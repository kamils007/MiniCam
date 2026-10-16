# Native ARD solid writer — port of Python test 4.9

Polish handoff, exact COM contracts, ARD record layout and the unchanged Python
reference with its tests: [docs/alphacam/integracja.md](alphacam/integracja.md).

This branch ports the proven `Feature_2025_TEST_4_9_KOMPLET` path to C++.
It is a draft integration: the known successful input is Parasolid, while the
editor currently holds an Open CASCADE shape and exports it as STEP. Direct
STEP import in this Feature.dll build returned `0x80020009` in testing.
Consequently, saving the current STEP-derived model is **not yet working**.
The editor reports the failure and never replaces the destination with a
document missing its solid. No alternative translator is invoked.

## Preserved native sequence

1. Verify the exact x64 DLL and Feature.po SHA-256 fingerprints.
2. Initialize STA COM; load Feature.dll with its matching dependencies.
3. GetSolidFeatures and QueryInterface(IDispatch).
4. Bind the application's host context with ObjectNumberOf.
5. Construct all 583 MFC CString objects and publish the original text-table layout.
6. Import a private input copy through ImportAs; require a nonempty Bodies collection.
7. Bodies.Save to a temporary binary Parasolid; verify its binary header.
8. ObjectQueryWrite2 with guarded name storage and version argument zero.
9. ObjectDoWrite2 with FILE*, fwrite, fflush and fclose from Feature.dll's own CRT imports.
10. Release interfaces, restore the text pointer, destroy strings and unload the DLL
    while the host objects remain alive.

The host provides the native application/drawing/frame/layer callbacks exercised
by the Python test. Unimplemented callbacks terminate the isolated helper instead
of inventing success. It does not reproduce the Python diagnostic harness's entire
IDispatch layer API. The verified DLL path uses native vtable calls.

The document writer inserts the unmodified returned record in a `0x22` block
named `LicomUKRJMParaSolid`: str16 name, object count 1, uint32 record length,
reserved uint32 zero, record bytes. This is the envelope in TEST_6_ARD.
It preserves existing 2D geometry, layers, thumbnail and the final back pointer.
It does not copy the example part or a previously saved ARD as a template.

## Build and configuration

Build the application's normal Windows x64 CMake target. It also builds
`minicam-feature-host.exe`; keep both executables together.

The existing ARD save action asks for Feature.dll and the original
`host_environment_2025.json` snapshot the first time. Defaults are in a `Feature`
directory beside the executable. It remembers the selected paths in QSettings.
Keep the matching PSKERNEL.dll, Feature.po and native dependencies available in
the selected library directory/installation. These proprietary files and the
user's snapshot are not committed to the repository.

Only Feature.dll SHA-256
`4bd2cbb8606a8a5c9dc5c44b9fb0de1e34ecfb01438b6a1b13898873f6878fec`
and Feature.po SHA-256
`5ebfd4fedec46583fa72fd2e95862a0b3fbe7eb41b6a9e49ae12895176130776`
are accepted. RVAs are valid for this build only. The SpecialKey value comes
from the original environment snapshot; no default or modified license state
is supplied. The full InitAlphacamAddIn initializer is not called.

## Reproduce the proven Parasolid test

Run the helper with these five arguments:

```
minicam-feature-host.exe Feature.dll host_environment_2025.json input.x_b feature_object.bin Feature.po
```

The output file must not exist. Run in a writable directory. The application
uses temporary files, cancellation, a 60-second timeout and QSaveFile for the
final document. Python is not required at runtime.

Build `ard-record-test` with BUILD_TESTING enabled and run:

```
ard-record-test feature_object.bin result.ard
```

It checks the record bytes and length, the 0x22 wrapper, the 2D prefix/suffix,
the document's back pointer and rejection of an invalid record.

## Verification in this development session

- MSVC x64 compiled the helper, document writer, record test and application bridge.
- The native helper imported the original Python test's Parasolid, executed
  Bodies.Save and ObjectDoWrite2, and exited successfully.
- The Python reference parser recognized version 3, one source, one solid body
  and the same 12-byte body-metadata tail in the native record.
- The generated ARD passed the structural/byte-preservation test.
- The reference input retained its original SHA-256.
- Direct STEP import failed with `0x80020009`. Full application build and
  reopening the newly generated ARD in ALPHACAM have not been verified.

The remaining requirement is a supported way to pass the current OCCT shape
into this verified native serialization path. The Python package alone does
not establish that conversion.
