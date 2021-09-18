## glslcc: Cross-compiler for GLSL shader language (GLSL->HLSL,METAL,GLES,GLSLv3)
[@septag](https://twitter.com/septagh)

**glslcc** is a command line tool that converts GLSL code to HLSL, GLES (version 2.0 and 3.0), Metal (MSL) and also other GLSL versions (GLSL 330, GLSL 400, etc..).  
It uses [glslang](https://github.com/KhronosGroup/glslang) for parsing GLSL and compiling SPIR-V. And [SPIRV-cross](https://github.com/KhronosGroup/SPIRV-Cross) for converting the code from SPIR-V to the target language.  

### Features

- Currently, vertex, fragment and compute shaders are supported
- Flatten UBOs, useful for ES2 shaders
- Show preprocessor result, show include files (for resolving shader dependencies in external tools)
- Add defines
- Add include directories
- shader reflection data in Json format
- Can output to a native binary file format (.sgs), that holds all the shaders and reflection data for pipeline
- Can output to individual files
- Can output all pipeline shaders (vertex+fragment) and their reflection data to .c file variables
- Supports both GLES2 and GLES3 shaders
- Can output to other GLSL versions like 330
- Optional D3D11 byte code output for HLSL shaders
- Support for special tags (begin_vert/begin_frag) in a single .glsl file (embed multiple sources)

### Build
_glslcc_ uses CMake. built and tested on: 

- Windows 10 - Microsoft visual studio 2015 Update 3
- Ubuntu 16.04 - GCC 5.4.0
- MacOS - 10.14 sdk with clang-1000.11.45.2

#### How to build on Windows
**Note:** These instructions assume you already have [Git](https://git-scm.com/download/win)
and [CMake](https://cmake.org/download/) installed.

**I assume that terminal users are using a Linux-style terminal for Windows;**
**therefore '/' is used in file paths. If you're running Windows cmd all**
**'/' need to replaced with '\\'**

1. Get the code

```
git clone https://github.com/septag/glslcc.git
```

2. Run CMake

```
./cmake -S path/to/glslcc/ -B path/to/glslcc/build
```

*If you don't include the `-B` flag, the build files will instead be*
*written to wherever `cmake.exe` is, which you probably don't want*

3. Build and Run the executable from the terminal and enjoy!
See below for further examples of command line args

```
./glslcc.exe --vert=path/to/shader.vert --frag=path/to/shader.frag --output=path/to/shader.hlsl --lang=hlsl --reflect
```

### Usage

I'll have to write a more detailed documentation but for these are the arguments: (```glslcc --help```)

```
-h --help                           - Print this help text
-V --version                        - Print version
-v --vert=<Filepath>                - Vertex shader source file
-f --frag=<Filepath>                - Fragment shader source file
-c --compute=<Filepath>             - Compute shader source file
-o --output=<Filepath>              - Output file
-l --lang=<gles/msl/hlsl/glsl>      - Convert to shader language
-D --defines(=Defines)              - Preprocessor definitions, seperated by comma or ';'
-Y --invert-y                       - Invert position.y in vertex shader
-p --profile=<ProfileVersion>       - Shader profile version (HLSL: 40, 50, 60), (ES: 200, 300), (GLSL: 330, 400, 420)
-C --dumpc                          - Dump shader limits configuration
-I --include-dirs=<Directory(s)>    - Set include directory for <system> files, seperated by ';'
-P --preprocess                     - Dump preprocessed result to terminal
-N --cvar=<VariableName>            - Outputs Hex data to a C include file with a variable name
-F --flatten-ubos                   - Flatten UBOs, useful for ES2 shaders
-r --reflect(=Filepath)             - Output shader reflection information to a json file
-G --sgs                            - Output file should be packed SGS format
-b --bin                            - Compile to bytecode instead of source. requires ENABLE_D3D11_COMPILER build flag
-g --debug                          - Generate debug info for binary compilation, should come with --bin
-O --optimize                       - Optimize shader for release compilation
-S --silent                         - Does not output filename(s) after compile success
-i --input=<(null)>                 - Input shader source file. determined by extension (.vert/.frag/.comp)
-0 --validate                       - Only performs shader validatation and error checking
-E --err-format=<glslang/msvc>      - Output error format
-L --list-includes                  - List include files in shaders, does not generate any output files

Current supported shader stages are:
        - Vertex shader (--vert)
        - Fragment shader (--frag)
        - Compute shader (--compute)
```

Here's some examples:  

Example shader (write shaders GLSL 4.5) : 
You can also use ```#include```. The compiler automatically inserts ```#extension GL_GOOGLE_include_directive : require``` at the begining of any shader.  

Vertex Shader (shader.vert):

```glsl
#version 450

layout (location = POSITION) in vec3 aPos;
layout (location = COLOR0) in vec4 aColor;
layout (location = TEXCOORD0) in vec2 aCoord;

layout (std140, binding=0) uniform matrices
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
glslcc --vert=shader.vert --frag=shader.frag --output=shader.h --lang=hlsl --reflect --cvar=g_shader --defines=HLSL=1;USE_TEXTURE3D=1
```

You can also pass files without explicitly defining input shaders in arguments. their shader type will be resolved by checking their file extensions. So `.vert`=vertex-shader, `.frag`=fragment-shader, `.comp`=compute-shader

```
glslcc shader.vert shader.frag --output=shader --lang=hlsl
```

To only validate a specific shader (useful for tools and IDEs), use `--validate` flag, with your specified output error format. By default, on windows, it outputs msvc's error format and on other platforms outputs gcc's error format, and only _glslang_'s format if explicitly defined:

```
glslcc shader.vert --validate --err-format=glslang
```

#### Embed multiple sources into a single file
To to this, you can use the special tags and write your vertex/fragment sources inside of them. 
Tags are `//@begin_vert` for vertex shader and `//@begin_frag` for fragment shader. Also remember to end the block with `//@end`. 
The compiler will extract the blocks and compile each source separately just like individual files. 

**Note** that this only works with *.glsl* files and no other extension:


```glsl
//@begin_vert
    #version 450

    layout (location = POSITION)  in  vec3 a_pos;
    layout (location = TEXCOORD0) in  vec2 a_coord;
    layout (location = TEXCOORD0) out vec2 f_coord;

    layout (binding=0, std140) uniform matrices {
        mat4 mvp;
    };
        
    void main() {
        gl_Position = mvp * vec4(a_pos, 1.0);
        f_coord = a_coord;
    }
//@end

//@begin_frag
    #version 450

    precision mediump float;

    layout (location = TEXCOORD0)  in  vec2 f_coord;
    layout (location = SV_Target0) out vec4 frag_color;

    layout (binding = 0) uniform sampler2D tex_image;

    void main() {
        frag_color = texture(tex_image, f_coord);
    }
//@end
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

### SGS file format

There is also an option for exporting to .sgs files *(--sgs)* which is a simple IFF like binary format to hold all shaders (vs + fs + cs) with their reflection inside a binary blob. 
Check out [sgs-file.h](https://github.com/septag/glslcc/blob/master/src/sgs-file.h) for the file format spec and headers.
The blocks are layed out like this:

The blocks are composed of a uint32_t fourcc code + uint32_t variable defining the size of the block. So each block header is 8 bytes. For each header structure, check out the header file.
- `SGS ` block
	- `struct sgs_chunk`
	- `STAG` blocks: defines each shader stage (vs + fs + ..)
		- `CODE`: actual code for the shader stage
		- `DATA`: binary (byte-code) data for the shader stage
		- `REFL`: Reflection data for the shader stage
			- `struct sgs_chunk_refl`: reflection data header
			- `struct sgs_refl_input[]`: array of vertex-shader input attributes (see `sgs_chunk_refl` for number of inputs)
			- `struct sgs_refl_uniform_buffer[]`: array of uniform buffer objects (see `sgs_chunk_refl` for number of uniform buffers)
			- `struct sgs_refl_texture[]`: array of texture objects (see `sgs_chunk_refl` for number of textures)
			- `struct sgs_refl_texture[]`: array of storage image objects (see `sgs_chunk_refl` for number of storage images)
			- `struct sgs_refl_buffer[]`: array of storage buffer objects (see `sgs_chunk_refl` for number of storage buffers)

### MSVC Linter

If you happen to work with msvc 2017 and higher, there is this extension called [GLSL language integration](https://marketplace.visualstudio.com/items?itemName=DanielScherzer.GLSL) ([github](https://github.com/danielscherzer/GLSL)) that this compiler is compatible with, so it can perform automating error checking in your editor. use these parameters in extensions's config:  

```
glslcc -0 -E glslang
```

### VSCode Linter

For visual studio code linting, I have modified existing [GLSL Lint](https://marketplace.visualstudio.com/items?itemName=CADENAS.vscode-glsllint) extension to work with this tool instead. you can find it [here](vscode).

Just install the tool manually in vscode, and add these options to vscode:

```
    "glsllint.glslangValidatorPath": "/path/to/glslcc.exe",
    "glsllint.glslangValidatorArgs": "--validate --err-format=glslang"
```

### D3D11 Compiler
There is a support for compiling d3d11 shaders (ps_5_0, vs_5_0, cs_5_0) into D3D11 byte-code instead of HLSL source code. On windows with Windows SDK, set ```ENABLE_D3D11_COMPILER=ON``` flag for cmake, build the project and use ```--bin``` in the command line arguments to generate binary byte-code file.

### CMake module
I've added [glslcc.cmake](https://github.com/septag/glslcc/blob/master/cmake/glslcc.cmake) module, to facilitate shader compilation in cmake projects. here's an example on how you can use it in your `CMakeLists.txt` to make shaders as C header files:  

```
include(glslcc)
set(shaders test.vert test.frag)
set_source_files_properties(${shaders} PROPERTIES GLSLCC_OUTPUT_DIRECTORY "shaders_inc")
glslcc_target_compile_shaders_h(project_name "${shaders}")
```

These properties can be assigned to shaders source files:

- `GLSLCC_OUTPUT_DIRECTORY`: output directory path
- `GLSLCC_SHADER_LANG`: shader language, `gles/msl/hlsl/glsl`. if not defined, it will be automatically selected by running platform
- `GLSLCC_SHADER_VERSION`: shader profile version. default: _hlsl:50_, _gles:200_, _glsl:330_
- `GLSLCC_OUTPUT_FILENAME`: compiled filename. default: `SOURCE_FILE.EXT.h`
- `GLSLCC_COMPILE_FLAGS`: extra compilation flags to pass to `glslcc`
- `COMPILE_DEFINITIONS`: compile definitions
- `INCLUDE_DIRECTORIES`: shader include directories

### TODO

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
