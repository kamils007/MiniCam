"""Offline integration checks for prepared CMake paths and compiler guards."""
import hashlib
import json
import os
from pathlib import Path
import shutil
import subprocess
import tempfile
import unittest


SOURCE = Path(__file__).resolve().parents[2]
CMAKE = os.environ.get("MINICAM_TEST_CMAKE") or shutil.which("cmake")


@unittest.skipUnless(CMAKE, "CMake required")
class StationToolchainTests(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory(prefix="minicam station ")
        self.root = Path(self.temp.name)
        (self.root / "cmake").mkdir()
        for name in ("StationVcpkg.cmake", "CheckStationCompiler.cmake"):
            shutil.copyfile(SOURCE / "cmake" / name, self.root / "cmake" / name)
        shutil.copytree(SOURCE / "tools/windows/triplets", self.root / "tools/windows/triplets")
        self.lock = json.loads((SOURCE / "tools/windows/toolchain.lock.json").read_text())
        self.vcpkg = self.root / "vcpkg managed"
        self.vcpkg.mkdir()
        for args in (
            ["init", "--quiet"],
            ["-c", "user.name=Test", "-c", "user.email=test@example.invalid", "commit", "--quiet", "--allow-empty", "-m", "fixture"],
        ):
            subprocess.run(["git", "-C", str(self.vcpkg), *args], check=True, capture_output=True)
        self.lock["vcpkgCommit"] = subprocess.check_output(
            ["git", "-C", str(self.vcpkg), "rev-parse", "HEAD"], text=True
        ).strip()
        self.lock_path = self.root / "tools/windows/toolchain.lock.json"
        self.lock_path.write_text(json.dumps(self.lock), encoding="utf-8")
        self.state_path = self.root / "out/station/environment.json"
        self.state_path.parent.mkdir(parents=True)
        self.cache = self.root / "shared cache"
        self.state = {
            "jobs": 2,
            "lockHash": hashlib.sha256(self.lock_path.read_bytes()).hexdigest(),
            "vcpkgRoot": self.vcpkg.as_posix(),
            "cacheRoot": self.cache.as_posix(),
            "visualStudio": (self.root / "VS").as_posix(),
            "ninja": (self.root / "ninja.exe").as_posix(),
            "git": Path(shutil.which("git")).as_posix(),
        }
        self.write_state()
        fake_toolchain = self.vcpkg / "scripts/buildsystems/vcpkg.cmake"
        fake_toolchain.parent.mkdir(parents=True)
        fake_toolchain.write_text(
            'file(WRITE "${CMAKE_CURRENT_LIST_DIR}/captured.json" '
            '"{\\\"root\\\":\\\"$ENV{VCPKG_ROOT}\\\",'
            '\\\"downloads\\\":\\\"$ENV{VCPKG_DOWNLOADS}\\\",'
            '\\\"binary\\\":\\\"$ENV{VCPKG_BINARY_SOURCES}\\\",'
            '\\\"assets\\\":\\\"$ENV{X_VCPKG_ASSET_SOURCES}\\\"}")\n',
            encoding="utf-8",
        )

    def tearDown(self):
        self.temp.cleanup()

    def write_state(self):
        self.state_path.write_text(json.dumps(self.state), encoding="utf-8")

    def run_cmake(self, script, success=True):
        entry = self.root / "check.cmake"
        entry.write_text(script, encoding="utf-8")
        # The toolchain must not use the caller's source/build directory.
        result = subprocess.run(
            [CMAKE, "-DCMAKE_SOURCE_DIR=unrelated-try-compile", "-P", str(entry)],
            cwd=self.root, text=True, capture_output=True,
        )
        self.assertEqual(result.returncode == 0, success, result.stdout + result.stderr)
        return result.stdout + result.stderr

    def include_toolchain(self, success=True):
        return self.run_cmake(f'include("{self.root.as_posix()}/cmake/StationVcpkg.cmake")\n', success)

    def test_prepared_toolchain_and_cache_with_spaces(self):
        self.include_toolchain()
        captured = json.loads((self.vcpkg / "scripts/buildsystems/captured.json").read_text())
        self.assertEqual(captured["root"], self.vcpkg.as_posix())
        self.assertEqual(captured["downloads"], self.cache.as_posix() + "/downloads")
        self.assertEqual(captured["binary"], "clear;files," + self.cache.as_posix() + "/binaries,readwrite")
        self.assertIn("shared%20cache/assets/,,readwrite", captured["assets"])

    def test_missing_preparation(self):
        self.state_path.unlink()
        self.assertIn("PRZYGOTUJ_KOMPILACJE.cmd", self.include_toolchain(False))

    def test_lock_change_requires_preparation(self):
        self.lock_path.write_text(json.dumps({**self.lock, "msvcVersion": "14.99"}))
        self.assertIn("Toolchain lock changed", self.include_toolchain(False))

    def test_wrong_vcpkg_head_is_rejected(self):
        self.lock["vcpkgCommit"] = "0" * 40
        self.lock_path.write_text(json.dumps(self.lock))
        self.state["lockHash"] = hashlib.sha256(self.lock_path.read_bytes()).hexdigest()
        self.write_state()
        self.assertIn("does not match the lock", self.include_toolchain(False))

    def test_overlay_triplets_pin_both_linkages(self):
        (self.vcpkg / "triplets").mkdir()
        for name, linkage in (("x64-windows", "dynamic"), ("x64-windows-static", "static")):
            with self.subTest(name=name):
                (self.vcpkg / f"triplets/{name}.cmake").write_text(f"set(VCPKG_LIBRARY_LINKAGE {linkage})\n")
                script = f'''set(VCPKG_ROOT "{self.vcpkg.as_posix()}")
include("{self.root.as_posix()}/tools/windows/triplets/{name}.cmake")
if(NOT VCPKG_PLATFORM_TOOLSET STREQUAL "v143" OR
   NOT VCPKG_PLATFORM_TOOLSET_VERSION STREQUAL "14.44" OR
   NOT VCPKG_CMAKE_SYSTEM_VERSION STREQUAL "10.0.26100.0" OR
   NOT VCPKG_LIBRARY_LINKAGE STREQUAL "{linkage}")
  message(FATAL_ERROR "triplet lost the common compiler, SDK or linkage")
endif()
'''
                self.run_cmake(script)

    def check_compiler(self, version, msvc="ON", pointer=8, success=True):
        return self.run_cmake(f'''set(MINICAM_STATION_BUILD ON)
set(MSVC {msvc})
set(CMAKE_CXX_COMPILER_VERSION "{version}")
set(CMAKE_SIZEOF_VOID_P {pointer})
include("{self.root.as_posix()}/cmake/CheckStationCompiler.cmake")
''', success)

    def test_common_compiler_allowed(self):
        self.check_compiler("19.44.35207")

    def test_other_compiler_and_architecture_rejected(self):
        for version, msvc, pointer in (("19.43.1", "ON", 8), ("19.44.1", "OFF", 8), ("19.44.1", "ON", 4)):
            with self.subTest(version=version, msvc=msvc, pointer=pointer):
                self.assertIn("Station presets require", self.check_compiler(version, msvc, pointer, False))

    def test_existing_builds_do_not_use_guard(self):
        self.run_cmake(f'include("{self.root.as_posix()}/cmake/CheckStationCompiler.cmake")\n')


if __name__ == "__main__":
    unittest.main()
