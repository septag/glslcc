## glslcc: Cross-compiler for GLSL shader language (GLSL->HLSL,METAL,GLES)
[@septag](https://twitter.com/septagh)

**glslcc** is a command line tool that converts GLSL (version 4.5) code to HLSL, GLES (version 2.0 and 3.0) and Metal (MSL).  
It uses [glslang](https://github.com/KhronosGroup/glslang) for parsing GLSL and compiling SPIR-V. And [SPIRV-cross](https://github.com/KhronosGroup/SPIRV-Cross) for converting the code from SPIR-V to the target language.  

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

I'll have to write a more detailed documentation but for now checkout ```glslcc --help``` for command line options.  

Here's some examples:  

Example shader (write shaders GLSL 4.5) : 
You can also use ```#include```. The compiler automatically inserts ```#extension GL_GOOGLE_include_directive : require``` at the begining of any shader.  

Vertex Shader (shader.vert):

```glsl
#version 450

layout (location = 0) in vec3 aPos;
layout (location = 1) in vec4 aColor;
layout (location = 2) in vec2 aCoord;

layout (std140) uniform matrices
{
    mat4 projection;
    mat4 view;
    mat4 model;
};

layout (location = 0) out flat vec4 outColor;
layout (location = 1) out vec2 outCoord;

void main()
{
    gl_Position = projection * view * model * vec4(aPos, 1.0);
    outColor = aColor;
    outCoord = aCoord;
}  

```

Fragment shader (shader.frag): 

```glsl
#version 450

precision mediump float;

layout (location = 0) in flat vec4 inColor;
layout (location = 1) in vec2 inCoord;

layout (location = 0) out vec4 fragColor;

layout (binding = 0) uniform sampler2D colorMap;

void main() 
{
    lowp vec4 c = texture(colorMap, inCoord);
    fragColor = inColor * c;
}
```

Cross-compiles the vertex-shader and fragment-shader to HLSL files with reflection Json data
Output files will be _shader_vs.hlsl_, _shader_fs.hlsl_ for HLSL code, and _shader_vs.hlsl.json_, _shader_fs.hlsl.json_ for their reflection data.


```
glslcc --vert=shader.vert --frag=shader.frag --output=shader.hlsl --lang=hlsl --reflect
```

This command does the same thing, but outputs all the data to a C header file *shader.h*, with specified variable names *g_shader_vs*, *g_shader_fs*, *g_shader_vs_refl* and *g_shader_fs_refl* which are the same data in files in hex representation.

```
glslcc --vert=shader.vert --frag=shader.frag --output=shader.h --lang=hlsl --reflect --cvar=g_shader
```

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
	
	https://github.com/septag/glslcc
	
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
