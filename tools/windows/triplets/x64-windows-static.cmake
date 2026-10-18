# Same library linkage as the original preset, with a common compiler and SDK.
include("${VCPKG_ROOT}/triplets/x64-windows-static.cmake")
file(READ "${CMAKE_CURRENT_LIST_DIR}/../toolchain.lock.json" _minicam_lock)
string(JSON VCPKG_PLATFORM_TOOLSET_VERSION GET "${_minicam_lock}" msvcVersion)
string(JSON VCPKG_CMAKE_SYSTEM_VERSION GET "${_minicam_lock}" windowsSdk)
set(VCPKG_PLATFORM_TOOLSET v143)
