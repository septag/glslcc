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

function(remove_compile_options DEST_VAR COMPILER_FLAGS FLAGS)
        separate_arguments(FLAGS)
        foreach(FLAG ${FLAGS})
            string(REPLACE "${FLAG}" "" COMPILER_FLAGS "${COMPILER_FLAGS}")
        endforeach()
        set(${DEST_VAR} ${COMPILER_FLAGS} PARENT_SCOPE)
endfunction()
    

if (MSVC)
        if (ENABLE_D3D11_COMPILER)
                add_definitions(-DD3D11_COMPILER)    
                target_link_libraries(glslcc PRIVATE d3dcompiler)    
        endif()
        remove_compile_options(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG}" "/MDd")
        remove_compile_options(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE}" "/MDd")
        remove_compile_options(CMAKE_C_FLAGS_DEBUG "${CMAKE_C_FLAGS_DEBUG}" "/MDd")
        remove_compile_options(CMAKE_C_FLAGS_RELEASE "${CMAKE_C_FLAGS_RELEASE}" "/MDd")
        set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} /MT")
        set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} /MTd")
        set(CMAKE_C_FLAGS_RELEASE "${CMAKE_C_FLAGS_RELEASE} /MT")
        set(CMAKE_C_FLAGS_DEBUG "${CMAKE_C_FLAGS_DEBUG} /MTd")
elseif (${CMAKE_SYSTEM_NAME} MATCHES "Linux")
        target_link_libraries(glslcc PRIVATE -static-libgcc -static-libstdc++)
endif()


install(TARGETS glslcc 
        CONFIGURATIONS Release
        RUNTIME DESTINATION bin)