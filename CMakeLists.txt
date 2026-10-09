cmake_minimum_required(VERSION 3.21)
project(MiniCam LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)

find_package(Qt6 REQUIRED COMPONENTS Widgets)
find_package(OpenCASCADE REQUIRED)

# ---- Rdzeń: czyste C++ + OCCT, bez Qt ----
add_library(camcore STATIC
    core/ModelImport.cpp
    core/ModelImport.h
)
target_include_directories(camcore PUBLIC core ${OpenCASCADE_INCLUDE_DIR})
target_link_libraries(camcore PUBLIC ${OpenCASCADE_LIBRARIES})

# ---- Aplikacja: GUI w Qt ----
add_executable(minicam
    app/main.cpp
    app/MainWindow.cpp
    app/MainWindow.h
    app/OccView.cpp
    app/OccView.h
)
target_link_libraries(minicam PRIVATE camcore Qt6::Widgets)

if(WIN32)
    set_target_properties(minicam PROPERTIES WIN32_EXECUTABLE ON)
endif()
