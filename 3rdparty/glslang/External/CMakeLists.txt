# Suppress all warnings from external projects.
set_property(DIRECTORY APPEND PROPERTY COMPILE_OPTIONS -w)

if(ENABLE_OPT AND NOT TARGET SPIRV-Tools-opt)
    if(IS_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/spirv-tools)
        set(SPIRV_SKIP_TESTS ON CACHE BOOL "Skip building SPIRV-Tools tests")
        add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/spirv-tools spirv-tools)
    endif()
endif()

