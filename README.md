## glslcc: Cross-compiler for GLSL shader language (GLSL->HLSL,METAL,GLES,GLSLv3)
[@septag](https://twitter.com/septagh)

**glslcc** is a command line tool that converts GLSL code to HLSL, GLES (version 2.0 and 3.0), Metal (MSL) and also other GLSL versions (GLSL 330, GLSL 400, etc..).  
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
- Can output to other GLSL versions like 330
- Optional D3D11 byte code output for HLSL shaders

### Build
_glslcc_ uses CMake. built and tested on: 

- Windows 10 - Microsoft visual studio 2015 Update 3
- Ubuntu 16.04 - GCC 5.4.0
- MacOS - 10.14 sdk with clang-1000.11.45.2
  
### Usage

I'll have to write a more detailed documentation but for now checkout ```glslcc --help``` for command line options.  

Here's some examples:  

Example shader (write shaders GLSL 4.5) : 
You can also use ```#include```. The compiler automatically inserts ```#extension GL_GOOGLE_include_directive : require``` at the begining of any shader.  

Vertex Shader (shader.vert):

```glsl
#version 450

layout (location = POSITION) in vec3 aPos;
layout (location = COLOR0) in vec4 aColor;
layout (location = TEXCOORD0) in vec2 aCoord;

layout (std140) uniform matrices
{
    mat4 projection;
    mat4 view;
    mat4 model;
};

layout (location = COLOR0) out flat vec4 outColor;
layout (location = TEXCOORD0) out vec2 outCoord;

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

layout (location = COLOR0) in flat vec4 inColor;
layout (location = TEXCOORD0) in vec2 inCoord;

layout (location = SV_Target0) out vec4 fragColor;

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

This command does the same thing, but outputs all the data to a C header file *shader.h*, with specified variable names *g_shader_vs*, *g_shader_fs*, *g_shader_vs_refl* and *g_shader_fs_refl* which are the same data in files in hex representation. Also sets preprocessor values HLSL=1 and USE_TEXTURE3D=1 for both shaders.

```
glslcc --vert=shader.vert --frag=shader.frag --output=shader.h --lang=hlsl --reflect --cvar=g_shader --defines=HLSL:1,USE_TEXTURE3D:1
```

#### Reflection data
Reflection data comes in form of json files and activated with ```--reflect``` option. It includes all the information that you need to link your 3d Api to the shader

#### HLSL semantics

As you can see in the above example, I have used HLSL shader semantics for input and output layout. This must done for compatibility with HLSL shaders and also proper vertex assembly creation in D3D application. The reflection data also emits proper semantics for each vertex input for the application.  
Here are supported HLSL semantics that you should use with ```layout (location = SEMANTIC)```:

```
POSITION, NORMAL, TEXCOORD0, TEXCOORD1, TEXCOORD2, TEXCOORD3, TEXCOORD4, TEXCOORD5, TEXCOORD6, TEXCOORD7, COLOR0, COLOR1, COLOR2, COLOR3, TANGENT, BINORMAL, BLENDINDICES, BLENDWEIGHT

SV_Target0, SV_Target1, SV_Target2, SV_Target3
```

### SGK file format

There is also an option for exporting to .sgs files *(--sgs)* which is a simple binary format to hold all shaders (vs + fs + cs) with their reflection inside a binary blob. Check out ```sgs-file.h``` for the file format. It starts with the header *sgs_file_header*, following with an array of *sgs_file_stage* structs. Then you can determine the data offsets and size of code and reflection of each shader stage in the file.


### D3D11 Compiler
There is a support for compiling d3d11 shaders (ps_5_0, vs_5_0, cs_5_0) into D3D11 byte-code instead of HLSL source code. On windows with Windows SDK, set ```ENABLE_D3D11_COMPILER=ON``` flag for cmake, build the project and use ```--bin``` in the command line arguments to generate binary byte-code file.

### TODO

- More documentation
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
