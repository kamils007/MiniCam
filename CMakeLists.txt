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

find_package(Qt6 REQUIRED COMPONENTS Widgets PrintSupport)
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
    core/ModelExport.cpp
    core/ModelExport.h
    core/ModelAlign.cpp
    core/ModelAlign.h
    core/Geometry.h
    core/Layers.h
    core/PartContours.cpp
    core/PartContours.h
    core/GeometryEdit.cpp
    core/GeometryEdit.h
    core/Sketch.cpp
    core/Sketch.h
    core/FeatureRecognition.cpp
    core/FeatureRecognition.h
)
target_include_directories(camcore PUBLIC core ${OpenCASCADE_INCLUDE_DIR})
target_link_libraries(camcore PUBLIC
    ${MINICAM_OCC_IMPORT}
    TKXCAF TKLCAF TKCAF  # dokument XCAF: kolory i nazwy z pliku
    TKHLR       # HLRBRep – rzut bryły z góry (obrys)
    TKBO        # BOPAlgo_Tools – obszary z krawędzi rzutu (obrys)
    TKShHealing # ShapeAnalysis – łączenie krawędzi w kontury
    TKGeomAlgo  # GeomAPI, Geom2dAPI – przecięcia przy Utnij
    TKTopAlgo   # BRepBndLib – prostopadłościan otaczający
    TKBRep      # TopoDS, BRepTools
    TKG3d TKGeomBase TKMath TKernel
)

# ---- Aplikacja: GUI w Qt ----
# qt_add_executable przy statycznym Qt sam dołącza wtyczki (m.in. platformę Windows).
qt_add_executable(minicam
    app/main.cpp
    app/MainWindow.cpp
    app/FileMenu.cpp
    app/MainWindow.h
    app/OccView.cpp
    app/OccView.h
    app/OccViewPick.cpp
    app/Ribbon.cpp
    app/Ribbon.h
    app/BottomBars.cpp
    app/BottomBars.h
    app/InputBar.cpp
    app/InputBar.h
    app/RadialMenu.cpp
    app/RadialMenu.h
    app/commands/Command.cpp
    app/commands/Command.h
    app/commands/DeleteCommand.cpp
    app/commands/DeleteCommand.h
    app/commands/DrawCommands.cpp
    app/commands/DrawCommands.h
    app/commands/JoinCommands.cpp
    app/commands/JoinCommands.h
    app/commands/MoveCommand.cpp
    app/commands/MoveCommand.h
    app/commands/TrimCommand.cpp
    app/commands/TrimCommand.h
    app/commands/ZLevelsCommand.cpp
    app/commands/ZLevelsCommand.h
    app/AlignSettingsDialog.cpp
    app/LayersPanel.cpp
    app/LayersPanel.h
    app/AlignSettingsDialog.h
    app/Icons.cpp
    app/Icons.h
)
# Ikony wstążki: PNG wyrenderowane z SVG (resources/ikony, skrypty w tools/ikony),
# wkompilowane w exe jako zasoby Qt pod ścieżką :/ikony/png/<nazwa>_<rozmiar>.png.
file(GLOB MINICAM_ICONS CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/resources/ikony/png/*.png")
qt_add_resources(minicam "ikony" PREFIX "/" BASE "${CMAKE_CURRENT_SOURCE_DIR}/resources"
    FILES ${MINICAM_ICONS})
# Ikona programu: PNG dla okna/paska zadań (:/ikona/minicam_<rozmiar>.png),
# a na Windows dodatkowo .ico w zasobach exe (app/minicam.rc), żeby Eksplorator pokazywał ikonę pliku.
file(GLOB MINICAM_APP_ICON CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/resources/ikona/*.png")
qt_add_resources(minicam "ikona_programu" PREFIX "/" BASE "${CMAKE_CURRENT_SOURCE_DIR}/resources"
    FILES ${MINICAM_APP_ICON})
if(WIN32)
    target_sources(minicam PRIVATE app/minicam.rc)
endif()
target_include_directories(minicam PRIVATE app) # polecenia w app/commands widzą InputBar.h, OccView.h
target_link_libraries(minicam PRIVATE camcore Qt6::Widgets Qt6::PrintSupport
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
