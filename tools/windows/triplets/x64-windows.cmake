# Keep this triplet self-contained: vcpkg's dependency-info pass has no
# VCPKG_ROOT CMake variable. Match the builtin at the pinned vcpkg commit.
set(VCPKG_TARGET_ARCHITECTURE x64)
set(VCPKG_CRT_LINKAGE dynamic)
set(VCPKG_LIBRARY_LINKAGE dynamic)
set(VCPKG_PROVIDED_FORTRAN ON)
file(READ "${CMAKE_CURRENT_LIST_DIR}/../toolchain.lock.json" _minicam_lock)
string(JSON VCPKG_PLATFORM_TOOLSET_VERSION GET "${_minicam_lock}" msvcVersion)
string(JSON VCPKG_CMAKE_SYSTEM_VERSION GET "${_minicam_lock}" windowsSdk)
set(VCPKG_PLATFORM_TOOLSET v143)
