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
elseif (NOT WIN32)
	
endif()

# static linking
if (STATIC_LINK AND NOT MSVC)
	if (CMAKE_BUILD_TYPE STREQUAL Release)
		set(CMAKE_EXE_LINKER_FLAGS  "${CMAKE_EXE_LINKER_FLAGS} -static-libgcc -static-libstdc++")
		#if (APPLE)
		#	add_custom_command(TARGET glslcc POST_BUILD COMMAND strip -X -x ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/glslcc)
		#elseif (NOT WIN32)
		#	add_custom_command(TARGET glslcc POST_BUILD COMMAND strip -g -X -x ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/glslcc)
		#endif()
	endif()
endif()	

install(TARGETS glslcc 
        CONFIGURATIONS Release
        RUNTIME DESTINATION bin)
		
		