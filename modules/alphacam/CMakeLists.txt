add_library(minicam-alphacam-export STATIC AlphacamExport.cpp AlphacamExport.h)
target_include_directories(minicam-alphacam-export PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}")
target_link_libraries(minicam-alphacam-export PRIVATE camcore Qt6::Widgets)
if(MSVC)
    target_compile_options(minicam-alphacam-export PRIVATE /utf-8 /permissive- /Zc:__cplusplus)
endif()

# Optional prepared runtime. Python/vendor binaries are not build dependencies.
set(MINICAM_ALPHACAM_RUNTIME "" CACHE PATH "Prepared alphacam-export runtime directory")
if(MINICAM_ALPHACAM_RUNTIME)
    if(NOT EXISTS "${MINICAM_ALPHACAM_RUNTIME}/step_ard/export_record.py" OR
       NOT EXISTS "${MINICAM_ALPHACAM_RUNTIME}/step_ard/data/1_test.ard")
        message(FATAL_ERROR "MINICAM_ALPHACAM_RUNTIME must point to a prepared alphacam-export directory")
    endif()
endif()
