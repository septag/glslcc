## glslcc: Cross-compiler for GLSL shader language (GLSL->HLSL,METAL,GLES)
[@septag](https://twitter.com/septagh)

**glslcc** is a command line tool that converts GLSL (version 4.5) code to HLSL, GLES (version 2.0 and 3.0) and Metal (MSL).  
It uses [glslang](https://github.com/KhronosGroup/glslang) for parsing and [SPIRV-cross](https://github.com/KhronosGroup/SPIRV-Cross) for converting the code from SPIR-V to the target language.  

### Features

- Currently, vertex, fragment and compute shaders are supported
- Flatten UBOs, useful for ES2 shaders
- Show preprocessor result
- Add defines
- Add include directories
- shader reflection data in Json format
- Can output to a native binary file format (.sgs), that holds all the shaders and reflection data for pipeline
- Can output to individual files
- Can output all pipeline shaders (vertex+fragment) and their reflection data to .c file variables
- Supports both GLES2 and GLES3 shaders

### Build
_glslcc_ uses CMake. Currently tested with Visual Studio 2015.  
The code is portable and other platforms should be built without errors, although I have to test and confirm this.

### Usage

_TODO_. for now checkout ```glslcc --help``` for command line options

### TODO

- More documentation
- Support for offline compiling of HLSL files (D3DCompiler)
- LZ4 compression for SGS binary files
- Support for more shader stages. I didn't have any use for geometry and tesselation shaders, but the _glslang_ supports all of them, so adding new shader stages shouldn't be a problem

[License (BSD 2-clause)](https://github.com/septag/glslcc/blob/master/LICENSE)
--------------------------------------------------------------------------

<a href="http://opensource.org/licenses/BSD-2-Clause" target="_blank">
<img align="right" src="http://opensource.org/trademarks/opensource/OSI-Approved-License-100x137.png">
</a>

	Copyright 2018 Sepehr Taghdisian. All rights reserved.
	
	https://github.com/septag/sx
	
	Redistribution and use in source and binary forms, with or without
	modification, are permitted provided that the following conditions are met:
	
	   1. Redistributions of source code must retain the above copyright notice,
	      this list of conditions and the following disclaimer.
	
	   2. Redistributions in binary form must reproduce the above copyright notice,
	      this list of conditions and the following disclaimer in the documentation
	      and/or other materials provided with the distribution.
	
	THIS SOFTWARE IS PROVIDED BY COPYRIGHT HOLDER ``AS IS'' AND ANY EXPRESS OR
	IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
	MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO
	EVENT SHALL COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT,
	INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING,
	BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
	DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF
	LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE
	OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF
	ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
