cmake_minimum_required(VERSION 3.21)
project(MiniCam LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)

# Triplet x64-windows-static: wszystko linkowane statycznie, wynikiem jest jeden
# minicam.exe bez DLL i bez VC++ Redistributable (runtime /MT).
# Triplet x64-windows (preset "DLL"): biblioteki jako DLL, runtime /MD.
# Runtime programu MUSI zgadzać się z bibliotekami, inaczej linker zgłosi LNK2038.
if(MSVC)
    if(VCPKG_TARGET_TRIPLET MATCHES "-static$")
        set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
    else()
        set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>DLL")
    endif()
endif()

find_package(Qt6 REQUIRED COMPONENTS Widgets)
find_package(OpenCASCADE REQUIRED)

# ---- Rdzeń: czyste C++ + OCCT, bez Qt ----
# Linkujemy tylko te moduły OCCT, których naprawdę używamy (pozostałe dociągną się
# same jako zależności). ${OpenCASCADE_LIBRARIES} to WSZYSTKIE moduły – przy
# statycznym linkowaniu niepotrzebnie wydłuża to linkowanie, a w niektórych
# dystrybucjach zawiera moduły Draw, których nie ma na dysku.
# Od OCCT 7.8 import STEP/IGES (także z kolorami) siedzi w TKDESTEP/TKDEIGES;
# wcześniej był rozbity na TKSTEP/TKIGES i TKXDESTEP/TKXDEIGES.
if(TARGET TKDESTEP)
    set(MINICAM_OCC_IMPORT TKDESTEP TKDEIGES)
else()
    set(MINICAM_OCC_IMPORT TKXDESTEP TKXDEIGES TKSTEP TKIGES)
endif()

add_library(camcore STATIC
    core/ModelImport.cpp
    core/ModelImport.h
    core/ModelAlign.cpp
    core/ModelAlign.h
    core/ContourLevels.cpp
    core/ContourLevels.h
    core/FeatureRecognition.cpp
    core/FeatureRecognition.h
)
target_include_directories(camcore PUBLIC core ${OpenCASCADE_INCLUDE_DIR})
target_link_libraries(camcore PUBLIC
    ${MINICAM_OCC_IMPORT}
    TKXCAF TKLCAF TKCAF  # dokument XCAF: kolory i nazwy z pliku
    TKShHealing # ShapeAnalysis – łączenie krawędzi w kontury
    TKTopAlgo   # BRepBndLib – prostopadłościan otaczający
    TKBRep      # TopoDS, BRepTools
    TKG3d TKGeomBase TKMath TKernel
)

# ---- Aplikacja: GUI w Qt ----
# qt_add_executable przy statycznym Qt sam dołącza wtyczki (m.in. platformę Windows).
qt_add_executable(minicam
    app/main.cpp
    app/MainWindow.cpp
    app/MainWindow.h
    app/OccView.cpp
    app/OccView.h
    app/Ribbon.cpp
    app/Ribbon.h
    app/AlignSettingsDialog.cpp
    app/AlignSettingsDialog.h
)
target_link_libraries(minicam PRIVATE camcore Qt6::Widgets
    TKV3d TKService TKOpenGl   # wizualizacja 3D OCCT
    TKPrim                     # BRepPrimAPI_MakePrism – ścianki konturów
)

if(WIN32)
    set_target_properties(minicam PROPERTIES WIN32_EXECUTABLE ON)
    target_link_libraries(minicam PRIVATE opengl32)
endif()

# Przy dynamicznym Qt wtyczka platformy (qwindows.dll) musi leżeć w podfolderze
# platforms/ obok exe – inaczej: "no Qt platform plugin could be initialized".
# Przy statycznym Qt wtyczki są już wewnątrz exe, więc nic nie kopiujemy.
get_target_property(_qt_core_type Qt6::Core TYPE)
if(_qt_core_type STREQUAL "SHARED_LIBRARY" AND TARGET Qt6::QWindowsIntegrationPlugin)
    add_custom_command(TARGET minicam POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E make_directory "$<TARGET_FILE_DIR:minicam>/platforms"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
                "$<TARGET_FILE:Qt6::QWindowsIntegrationPlugin>"
                "$<TARGET_FILE_DIR:minicam>/platforms/"
        COMMENT "Kopiowanie wtyczki platformy Qt"
    )
endif()
