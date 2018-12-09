cmake_minimum_required(VERSION 3.0)

set(SOURCE_FILES "glslcc.cpp"
                 "config.h"
                 "config.cpp" 
                 "sgs-file.h" 
                 "sgs-file.cpp")

add_executable(glslcc ${SOURCE_FILES})

target_link_libraries(glslcc PRIVATE 
        sx
        glslang 
        OGLCompiler 
        OSDependent 
        SPIRV 
        SPVRemapper 
        spirv-cross-core 
        spirv-cross-cpp 
        spirv-cross-glsl 
        spirv-cross-hlsl 
        spirv-cross-reflect
        spirv-cross-util 
        spirv-cross-msl)  

if (MSVC)
        if (ENABLE_D3D11_COMPILER)
                add_definitions(-DD3D11_COMPILER)    
                target_link_libraries(glslcc PRIVATE d3dcompiler)    
        endif()
elseif (${CMAKE_SYSTEM_NAME} MATCHES "Linux")
        target_link_libraries(glslcc PRIVATE -static-libgcc -static-libstdc++)
endif()


install(TARGETS glslcc 
        CONFIGURATIONS Release
        RUNTIME DESTINATION bin)