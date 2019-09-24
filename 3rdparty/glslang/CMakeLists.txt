# increase to 3.1 once all major distributions
# include a version of CMake >= 3.1
cmake_minimum_required(VERSION 2.8.12)
if (POLICY CMP0048)
  cmake_policy(SET CMP0048 NEW)
endif()
set_property(GLOBAL PROPERTY USE_FOLDERS ON)

set(LIB_TYPE STATIC)

# Precompiled header macro. Parameters are source file list and filename for pch cpp file.
macro(glslang_pch SRCS PCHCPP)
  if(MSVC AND CMAKE_GENERATOR MATCHES "^Visual Studio")
    set(PCH_NAME "$(IntDir)\\pch.pch")
    # make source files use/depend on PCH_NAME
    set_source_files_properties(${${SRCS}} PROPERTIES COMPILE_FLAGS "/Yupch.h /FIpch.h /Fp${PCH_NAME} /Zm300" OBJECT_DEPENDS "${PCH_NAME}")
    # make PCHCPP file compile and generate PCH_NAME
    set_source_files_properties(${PCHCPP} PROPERTIES COMPILE_FLAGS "/Ycpch.h /Fp${PCH_NAME} /Zm300" OBJECT_OUTPUTS "${PCH_NAME}")
    list(APPEND ${SRCS} "${PCHCPP}")
  endif()
endmacro(glslang_pch)

project(glslang)

if(WIN32)
    add_definitions(-DGLSLANG_OSINCLUDE_WIN32)
elseif(UNIX)
    add_definitions(-DGLSLANG_OSINCLUDE_UNIX)
else(WIN32)
    message("unknown platform")
endif(WIN32)

if(${CMAKE_CXX_COMPILER_ID} MATCHES "GNU")
    add_compile_options(-Wall -Wmaybe-uninitialized -Wuninitialized -Wunused -Wunused-local-typedefs
                        -Wunused-parameter -Wunused-value  -Wunused-variable -Wunused-but-set-parameter -Wunused-but-set-variable -fno-exceptions)
    add_compile_options(-Wno-reorder)  # disable this from -Wall, since it happens all over.
    add_compile_options(-fno-rtti)
elseif(${CMAKE_CXX_COMPILER_ID} MATCHES "Clang")
    add_compile_options(-Wall -Wuninitialized -Wunused -Wunused-local-typedefs
                        -Wunused-parameter -Wunused-value  -Wunused-variable)
    add_compile_options(-Wno-reorder)  # disable this from -Wall, since it happens all over.
    add_compile_options(-fno-rtti)
elseif(${CMAKE_CXX_COMPILER_ID} MATCHES "MSVC")
    add_compile_options(/GR-) # Disable RTTI
endif()

if(EMSCRIPTEN)
    add_compile_options(-Os -fno-exceptions)
    add_compile_options("SHELL: -s WASM=1")
    add_compile_options("SHELL: -s WASM_OBJECT_FILES=0")
    add_link_options(-Os)
    add_link_options("SHELL: -s FILESYSTEM=0")
    add_link_options("SHELL: --llvm-lto 1")
    add_link_options("SHELL: --closure 1")
    add_link_options("SHELL: -s ALLOW_MEMORY_GROWTH=1")

    if(ENABLE_EMSCRIPTEN_SINGLE_FILE)
        add_link_options("SHELL: -s SINGLE_FILE=1")
    endif(ENABLE_EMSCRIPTEN_SINGLE_FILE)
else()
    if(ENABLE_GLSLANG_WEB)
        if(MSVC)
            add_compile_options(/Os /GR-)
        else()
            add_compile_options(-Os -fno-exceptions)
            add_link_options(-Os)
        endif()
    endif(ENABLE_GLSLANG_WEB)
endif(EMSCRIPTEN)

# Request C++11
if(${CMAKE_VERSION} VERSION_LESS 3.1)
    # CMake versions before 3.1 do not understand CMAKE_CXX_STANDARD
    # remove this block once CMake >=3.1 has fixated in the ecosystem
    add_compile_options(-std=c++11)
else()
    set(CMAKE_CXX_STANDARD 11)
    set(CMAKE_CXX_STANDARD_REQUIRED ON)
    set(CMAKE_CXX_EXTENSIONS OFF)
endif()

function(glslang_set_link_args TARGET)
    # For MinGW compiles, statically link against the GCC and C++ runtimes.
    # This avoids the need to ship those runtimes as DLLs.
    if(WIN32 AND ${CMAKE_CXX_COMPILER_ID} MATCHES "GNU")
        set_target_properties(${TARGET} PROPERTIES
                              LINK_FLAGS "-static -static-libgcc -static-libstdc++")
    endif()
endfunction(glslang_set_link_args)

# We depend on these for later projects, so they should come first.
add_subdirectory(External)

# TODO
if(NOT TARGET SPIRV-Tools-opt)
    set(ENABLE_OPT OFF)
endif()

if(ENABLE_OPT)
    message(STATUS "optimizer enabled")
    add_definitions(-DENABLE_OPT=1)
else()
    if(ENABLE_HLSL)
        message(STATUS "spirv-tools not linked - illegal SPIRV may be generated for HLSL")
    endif()
    add_definitions(-DENABLE_OPT=0)
endif()

add_subdirectory(glslang)
add_subdirectory(OGLCompilersDLL)
add_subdirectory(SPIRV)
if(ENABLE_HLSL)
    add_subdirectory(hlsl)
endif(ENABLE_HLSL)
