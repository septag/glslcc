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