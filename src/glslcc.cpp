//
// Copyright 2018 Sepehr Taghdisian (septag@github). All rights reserved.
// License: https://github.com/septag/glslcc#license-bsd-2-clause
//
//
// Version History
//      1.0.0       Initial release
//      1.1.0       SGS file support (native binary format that holds all shaders and reflection data)
//      1.2.0       Added HLSL vertex semantics
//      1.2.1       Linux build
//      1.2.2       shader filename fix
//      1.2.3       memory corruption fix
//      1.3.0       D3D11 compiler for windows
//      1.3.1       Added -g for byte-code compiler flags
//      1.3.2       Fixed vertex semantic names for reflection
//      1.4.0       Added GLSL shader support
//      1.4.1       More reflection data, like uniform block members and types
//      1.4.2       Small bug fixed in flatten uniform block array size
//
#define _ALLOW_KEYWORD_MACROS

#include "sx/cmdline.h"
#include "sx/string.h"
#include "sx/array.h"
#include "sx/os.h"
#include "sx/io.h"

#include <stdio.h>
#include <stdlib.h>

#include <string>

#include "ShaderLang.h"
#include "SPIRV/SpvTools.h"
#include "SPIRV/GlslangToSpv.h"
#include "SPIRV/disassemble.h"
#include "SPIRV/spirv.hpp"

#include "spirv_cross.hpp"
#include "spirv_glsl.hpp"
#include "spirv_msl.hpp"
#include "spirv_hlsl.hpp"

#include "config.h"
#include "sgs-file.h"

#ifdef D3D11_COMPILER 
#   include <d3dcompiler.h>
#   define BYTECODE_COMPILATION
#endif

// sjson
#define sjson_malloc(user, size)        sx_malloc((const sx_alloc*)user, size)
#define sjson_free(user, ptr)           sx_free((const sx_alloc*)user, ptr)
#define sjson_realloc(user, ptr, size)  sx_realloc((const sx_alloc*)user, ptr, size)
#define sjson_assert(e)                 sx_assert(e);
#define sjson_snprintf                  sx_snprintf
#define sjson_strcpy(dst, n, src)       sx_strcpy(dst, n, src)
#define SJSON_IMPLEMENTATION
#include "../3rdparty/sjson/sjson.h"

#define VERSION_MAJOR  1
#define VERSION_MINOR  4
#define VERSION_SUB    2

static const sx_alloc* g_alloc = sx_alloc_malloc;
static sgs_file* g_sgs         = nullptr;

struct p_define
{
    char* def;
    char* val;
};

enum shader_lang
{
    SHADER_LANG_GLES = 0,
    SHADER_LANG_HLSL,
    SHADER_LANG_MSL,
    SHADER_LANG_GLSL,
    SHADER_LANG_COUNT
};

static const char* k_shader_types[SHADER_LANG_COUNT] = {
    "gles",
    "hlsl",
    "msl",
    "glsl"
};

enum vertex_attribs
{
    VERTEX_POSITION = 0,
    VERTEX_NORMAL,
    VERTEX_TEXCOORD0,
    VERTEX_TEXCOORD1,
    VERTEX_TEXCOORD2,
    VERTEX_TEXCOORD3,
    VERTEX_TEXCOORD4,
    VERTEX_TEXCOORD5,
    VERTEX_TEXCOORD6,
    VERTEX_TEXCOORD7,
    VERTEX_COLOR0,
    VERTEX_COLOR1,
    VERTEX_COLOR2,
    VERTEX_COLOR3,
    VERTEX_TANGENT,
    VERTEX_BITANGENT,
    VERTEX_INDICES,
    VERTEX_WEIGHTS,
    VERTEX_ATTRIB_COUNT
};

static const char* k_attrib_names[VERTEX_ATTRIB_COUNT] = {
    "POSITION",
    "NORMAL",
    "TEXCOORD0",
    "TEXCOORD1",
    "TEXCOORD2",
    "TEXCOORD3",
    "TEXCOORD4",
    "TEXCOORD5",
    "TEXCOORD6",
    "TEXCOORD7",
    "COLOR0",
    "COLOR1",
    "COLOR2",
    "COLOR3",
    "TANGENT",
    "BINORMAL",
    "BLENDINDICES",
    "BLENDWEIGHT"
};

static const char* k_attrib_sem_names[VERTEX_ATTRIB_COUNT] = {
    "POSITION",
    "NORMAL",
    "TEXCOORD",
    "TEXCOORD",
    "TEXCOORD",
    "TEXCOORD",
    "TEXCOORD",
    "TEXCOORD",
    "TEXCOORD",
    "TEXCOORD",
    "COLOR",
    "COLOR",
    "COLOR",
    "COLOR",
    "TANGENT",
    "BINORMAL",
    "BLENDINDICES",
    "BLENDWEIGHT"
};

static int k_attrib_sem_indices[VERTEX_ATTRIB_COUNT] = {
    0,
    0,
    0,
    1,
    2,
    3,
    4,
    5,
    6,
    7,
    0,
    1,
    2,
    3,
    0,
    0,
    0,
    0
};

// Includer
class Includer : public glslang::TShader::Includer 
{
public:
    virtual ~Includer() {}    

    IncludeResult* includeSystem(const char* headerName, 
                                 const char* includerName, 
                                 size_t inclusionDepth) override
    { 
        for (auto i = m_systemDirs.begin(); i != m_systemDirs.end(); ++i) {
            std::string header_path(*i);    
            if (header_path.back() != '/')
                header_path += "/";
            header_path += headerName;

            if (sx_os_stat(header_path.c_str()).type == SX_FILE_TYPE_REGULAR) {
                sx_mem_block* mem = sx_file_load_bin(g_alloc, header_path.c_str());
                if (mem)  {
                    return new(sx_malloc(g_alloc, sizeof(IncludeResult))) 
                        IncludeResult(header_path, (const char*)mem->data, (size_t)mem->size, mem);
                }
            }
        }
        return nullptr;
    }

    IncludeResult* includeLocal(const char* headerName,
                                const char* includerName,
                                size_t inclusionDepth) override 
    { 
        char cur_dir[256];
        sx_os_path_pwd(cur_dir, sizeof(cur_dir));
        std::string header_path(cur_dir);
        std::replace(header_path.begin(), header_path.end(), '\\', '/');
        if (header_path.back() != '/')
            header_path += "/";
        header_path += headerName;

        sx_mem_block* mem = sx_file_load_bin(g_alloc, header_path.c_str());
        if (mem)  {
            return new(sx_malloc(g_alloc, sizeof(IncludeResult))) 
                IncludeResult(header_path, (const char*)mem->data, (size_t)mem->size, mem);
        } 
        return nullptr;
    }

    // Signals that the parser will no longer use the contents of the
    // specified IncludeResult.
    void releaseInclude(IncludeResult* result) override
    {
        if (result) {
            sx_mem_block* mem = (sx_mem_block*)result->userData;
            if (mem)
                sx_mem_destroy_block(mem);
            result->~IncludeResult();
            sx_free(g_alloc, result);
        }
    }

    void addSystemDir(const char* dir)
    {
        std::string std_dir(dir);
        std::replace(std_dir.begin(), std_dir.end(), '\\', '/');
        m_systemDirs.push_back(std_dir);
    }

private:
    std::vector<std::string> m_systemDirs;
};

struct cmd_args 
{
    const char* vs_filepath;
    const char* fs_filepath;
    const char* cs_filepath;
    const char* out_filepath;
    shader_lang lang;
    p_define*   defines;
    Includer    includer;
    int         profile_ver;
    int         invert_y;
    int         preprocess;
    int         flatten_ubos;
    int         sgs_file;
    int         reflect;
    int         compile_bin;
    int         debug_bin;
    const char* cvar;
    const char* reflect_filepath;
};

static void print_version()
{
    printf("glslcc v%d.%d.%d\n", VERSION_MAJOR, VERSION_MINOR, VERSION_SUB);
    puts("http://www.github.com/septag/glslcc");
}

static void print_help(sx_cmdline_context* ctx)
{
    char buffer[4096];
    print_version();
    puts("");
    puts(sx_cmdline_create_help_string(ctx, buffer, sizeof(buffer)));
    puts("Current supported shader stages are:\n"
         "\t- Vertex shader (--vert)\n"
         "\t- Fragment shader (--frag)\n"
         "\t- Compute shader (--comp)\n");
    exit(0);
}

static shader_lang parse_shader_lang(const char* arg) 
{
    if (sx_strequalnocase(arg, "metal"))
        arg = "msl";

    for (int i = 0; i < SHADER_LANG_COUNT; i++) {
        if (sx_strequalnocase(k_shader_types[i], arg)) {
            return (shader_lang)i;
        }
    }

    puts("Invalid shader type");
    exit(-1);
}

static void parse_defines(cmd_args* args, const char* defines)
{
    sx_assert(defines);
    const char* def = defines;

    do {
        def = sx_skip_whitespace(def);
        if (def[0]) {
            const char* next_def = sx_strchar(def, ',');
            if (!next_def)
                next_def = sx_strchar(def, ';');
            int len;
            if (next_def) {
                len = (int)(uintptr_t)(next_def - def);
            } else {
                len = sx_strlen(def);
            }

            if (len > 0) {
                p_define d = {0x0};
                d.def = (char*)sx_malloc(g_alloc, len + 1);
                sx_strncpy(d.def, len+1, def, len);
                sx_trim_whitespace(d.def, len+1, d.def);

                // Check def=value pair
                char* equal = (char*)sx_strchar(d.def, ':');
                if (!equal)
                    equal = (char*)sx_strchar(d.def, '=');
                if (equal) {
                    *equal = 0;
                    d.val = equal + 1;
                }

                sx_array_push(g_alloc, args->defines, d);
            }
            
            def = sx_strchar(def, ',');
            if (def)
                def++;
        }
    } while (def);
}

#ifdef D3D11_COMPILER
static sx_mem_block* compile_binary(const char* code, const char* filename, 
                                    int profile_version, EShLanguage stage, int debug)
{
    ID3DBlob* output = NULL;
    ID3DBlob* errors = NULL;

    int major_ver = profile_version / 10;
    int minor_ver = profile_version % 10;
    char target[32];
    switch (stage) {
    case EShLangVertex:
        sx_snprintf(target, sizeof(target), "vs_%d_%d", major_ver, minor_ver);      break;
    case EShLangFragment:
        sx_snprintf(target, sizeof(target), "ps_%d_%d", major_ver, minor_ver);      break;
    case EShLangCompute:
        sx_snprintf(target, sizeof(target), "cs_%d_%d", major_ver, minor_ver);      break;
    }

    uint32_t compile_flags;
    if (!debug)
        compile_flags = D3DCOMPILE_OPTIMIZATION_LEVEL3;
    else 
        compile_flags = D3DCOMPILE_DEBUG | D3DCOMPILE_SKIP_OPTIMIZATION;

    HRESULT hr = D3DCompile(
        code,                           /* pSrcData */
        sx_strlen(code),                /* SrcDataSize */
        filename,                       /* pSourceName */
        NULL,                           /* pDefines */
        NULL,                           /* pInclude */
        "main",                         /* pEntryPoint */
        target,                         /* pTarget (vs_5_0 or ps_5_0) */
        compile_flags,                  /* Flags1 */
        0,                              /* Flags2 */
        &output,                        /* ppCode */
        &errors);                       /* ppErrorMsgs */
        
    if (FAILED(hr)) {
        if (errors) {
            puts((LPCSTR)errors->GetBufferPointer());
            errors->Release(); 
        }
        return nullptr;
    }

    sx_mem_block* mem = sx_mem_create_block(g_alloc, 
                                            (int)output->GetBufferSize(),
                                            output->GetBufferPointer());
    output->Release();                                
    return mem;
}
#endif

static void cleanup_args(cmd_args* args)
{
    for (int i = 0; i < sx_array_count(args->defines); i++) {
        if (args->defines[i].def)
            sx_free(g_alloc, args->defines[i].def);
    }
    sx_array_free(g_alloc, args->defines);
}

static const char* get_stage_name(EShLanguage stage)
{
    switch (stage) {
    case EShLangVertex:
        return "vs";
    case EShLangFragment:
        return "fs";
    case EShLangCompute:
        return "cs";
    default:
        sx_assert(0);
        return nullptr;
    }
}

static void parse_includes(cmd_args* args, const char* includes)
{
    sx_assert(includes);
    const char* inc = includes;

    do {
        inc = sx_skip_whitespace(inc);
        if (inc[0]) {
            const char* next_inc = sx_strchar(inc, ';');
            int len;
            if (next_inc) {
                len = (int)(uintptr_t)(next_inc - inc);
            } else {
                len = sx_strlen(inc);
            }

            if (len > 0) {
                char* inc_str = (char*)sx_malloc(g_alloc, len + 1);
                sx_assert(inc_str);
                sx_strncpy(inc_str, len+1, inc, len);
                sx_trim_whitespace(inc_str, len+1, inc_str);
                args->includer.addSystemDir(inc_str);
                sx_free(g_alloc, inc_str);
            }
            
            inc = sx_strchar(inc, ',');
            if (inc)
                inc++;
        }
    } while (inc);    
}

static void add_defines(glslang::TShader* shader, const cmd_args& args, std::string& def)
{
    std::vector<std::string> processes;

    for (int i = 0; i < sx_array_count(args.defines); i++) {
        const p_define& d = args.defines[i];
        def += "#define " + std::string(d.def);
        if (d.val) {
            def += std::string(" ") + std::string(d.val);
        }
        def += std::string("\n");

        char process[256];
        sx_snprintf(process, sizeof(process), "D%s", d.def);
        processes.push_back(process);
    }

    shader->setPreamble(def.c_str());
    shader->addProcesses(processes);
}

enum resource_type
{
    RES_TYPE_REGULAR = 0,
    RES_TYPE_SSBO,
    RES_TYPE_VERTEX_INPUT,
    RES_TYPE_TEXTURE,
    RES_TYPE_UNIFORM_BUFFER
};

struct uniform_type_mapping
{
    spirv_cross::SPIRType::BaseType base_type;
    int                             vec_size;
    int                             columns;
    const char*                     type_str;
};

static const uniform_type_mapping k_uniform_map[] = {
    {spirv_cross::SPIRType::Float,  1, 1, "float"},
    {spirv_cross::SPIRType::Float,  2, 1, "float2"},
    {spirv_cross::SPIRType::Float,  3, 1, "float3"},
    {spirv_cross::SPIRType::Float,  4, 1, "float4"},
    {spirv_cross::SPIRType::Float,  3, 4, "mat3x4"},
    {spirv_cross::SPIRType::Float,  3, 4, "mat4x3"},
    {spirv_cross::SPIRType::Float,  3, 3, "mat3x3"},
    {spirv_cross::SPIRType::Float,  4, 4, "mat4"},
    {spirv_cross::SPIRType::Int,  1, 1, "int"},
    {spirv_cross::SPIRType::Int,  2, 1, "int2"},
    {spirv_cross::SPIRType::Int,  3, 1, "int3"},
    {spirv_cross::SPIRType::Int,  4, 1, "int4"},
    {spirv_cross::SPIRType::Half,  4, 1, "float"},
    {spirv_cross::SPIRType::Half,  4, 2, "float2"},
    {spirv_cross::SPIRType::Half,  4, 3, "float3"},
    {spirv_cross::SPIRType::Half,  4, 4, "float4"}
};

enum ImageFormat {
    ImageFormatUnknown = 0,
    ImageFormatRgba32f = 1,
    ImageFormatRgba16f = 2,
    ImageFormatR32f = 3,
    ImageFormatRgba8 = 4,
    ImageFormatRgba8Snorm = 5,
    ImageFormatRg32f = 6,
    ImageFormatRg16f = 7,
    ImageFormatR11fG11fB10f = 8,
    ImageFormatR16f = 9,
    ImageFormatRgba16 = 10,
    ImageFormatRgb10A2 = 11,
    ImageFormatRg16 = 12,
    ImageFormatRg8 = 13,
    ImageFormatR16 = 14,
    ImageFormatR8 = 15,
    ImageFormatRgba16Snorm = 16,
    ImageFormatRg16Snorm = 17,
    ImageFormatRg8Snorm = 18,
    ImageFormatR16Snorm = 19,
    ImageFormatR8Snorm = 20,
    ImageFormatRgba32i = 21,
    ImageFormatRgba16i = 22,
    ImageFormatRgba8i = 23,
    ImageFormatR32i = 24,
    ImageFormatRg32i = 25,
    ImageFormatRg16i = 26,
    ImageFormatRg8i = 27,
    ImageFormatR16i = 28,
    ImageFormatR8i = 29,
    ImageFormatRgba32ui = 30,
    ImageFormatRgba16ui = 31,
    ImageFormatRgba8ui = 32,
    ImageFormatR32ui = 33,
    ImageFormatRgb10a2ui = 34,
    ImageFormatRg32ui = 35,
    ImageFormatRg16ui = 36,
    ImageFormatRg8ui = 37,
    ImageFormatR16ui = 38,
    ImageFormatR8ui = 39,
    ImageFormatMax = 0x7fffffff,
};

enum Dim {
    Dim1D = 0,
    Dim2D = 1,
    Dim3D = 2,
    DimCube = 3,
    DimRect = 4,
    DimBuffer = 5,
    DimSubpassData = 6,
    DimMax = 0x7fffffff,
};

const char* k_texture_format_str[spv::ImageFormatR8ui+1] = {
    "unknown",
    "Rgba32f",
    "Rgba16f",
    "R32f",
    "Rgba8",
    "Rgba8Snorm",
    "Rg32f",
    "Rg16f",
    "R11fG11fB10f",
    "R16f",
    "Rgba16",
    "Rgb10A2",
    "Rg16",
    "Rg8",
    "R16",
    "R8",
    "Rgba16Snorm",
    "Rg16Snorm",
    "Rg8Snorm",
    "R16Snorm",
    "R8Snorm",
    "Rgba32i",
    "Rgba16i",
    "Rgba8i",
    "R32i",
    "Rg32i",
    "Rg16i",
    "Rg8i",
    "R16i",
    "R8i",
    "Rgba32ui",
    "Rgba16ui",
    "Rgba8ui",
    "R32ui",
    "Rgb10a2ui",
    "Rg32ui",
    "Rg16ui",
    "Rg8ui",
    "R16ui",
    "R8ui"
};

const char* k_texture_dim_str[spv::DimSubpassData+1] = {
    "1d",
    "2d",
    "3d",
    "cube",
    "rect",
    "buffer",
    "subpass_data"
};

// https://github.com/KhronosGroup/SPIRV-Cross/wiki/Reflection-API-user-guide
static void output_resource_info(sjson_context* jctx, sjson_node* jparent,
                                 const spirv_cross::Compiler& compiler, 
                                 const std::vector<spirv_cross::Resource>& ress,                                 
                                 resource_type res_type = RES_TYPE_REGULAR,
                                 bool flatten_ubos = false)
{
    
    auto resolve_variable_type = [](const spirv_cross::SPIRType& type)->const char* {
        int count = sizeof(k_uniform_map)/sizeof(uniform_type_mapping);
        for (int i = 0; i < count; i++) {
            if (k_uniform_map[i].base_type == type.basetype &&
                k_uniform_map[i].vec_size == type.vecsize && 
                k_uniform_map[i].columns == type.columns)
            {
                return k_uniform_map[i].type_str;
            }
        }
        return "unknown";
    };

	for (auto &res : ress) {
        sjson_node* jres = sjson_mkobject(jctx);
        auto &type = compiler.get_type(res.type_id);
		if (res_type == RES_TYPE_SSBO && compiler.buffer_is_hlsl_counter_buffer(res.id))
			continue;
        

		// If we don't have a name, use the fallback for the type instead of the variable
		// for SSBOs and UBOs since those are the only meaningful names to use externally.
		// Push constant blocks are still accessed by name and not block name, even though they are technically Blocks.
		bool is_push_constant = compiler.get_storage_class(res.id) == spv::StorageClassPushConstant;
		bool is_block = compiler.get_decoration_bitset(type.self).get(spv::DecorationBlock) ||
		                compiler.get_decoration_bitset(type.self).get(spv::DecorationBufferBlock);
		bool is_sized_block = is_block && (compiler.get_storage_class(res.id) == spv::StorageClassUniform ||
		                                   compiler.get_storage_class(res.id) == spv::StorageClassUniformConstant);
		uint32_t fallback_id = !is_push_constant && is_block ? res.base_type_id : res.id;

		uint32_t block_size = 0;
		uint32_t runtime_array_stride = 0;
		if (is_sized_block) {
			auto &base_type = compiler.get_type(res.base_type_id);
			block_size = uint32_t(compiler.get_declared_struct_size(base_type));
			runtime_array_stride = uint32_t(compiler.get_declared_struct_size_runtime_array(base_type, 1) -
			                                compiler.get_declared_struct_size_runtime_array(base_type, 0));
		}

	    spirv_cross::Bitset mask;
		if (res_type == RES_TYPE_SSBO)
			mask = compiler.get_buffer_block_flags(res.id);
		else
			mask = compiler.get_decoration_bitset(res.id);

        sjson_put_int(jctx, jres, "id", res.id);
        sjson_put_string(jctx, jres, "name", 
            !res.name.empty() ? res.name.c_str() : compiler.get_fallback_name(fallback_id).c_str());

        if (!type.array.empty()) {
            int arr_sz = 0;
            for (auto arr : type.array)
                arr_sz += arr;
            sjson_put_int(jctx, jres, "array", arr_sz);
        }        

        int loc = -1;
		if (mask.get(spv::DecorationLocation)) {
            loc = compiler.get_decoration(res.id, spv::DecorationLocation);
			sjson_put_int(jctx, jres, "location", loc);
        }

        if (mask.get(spv::DecorationDescriptorSet)) {
			sjson_put_int(jctx, jres, "set", 
                compiler.get_decoration(res.id, spv::DecorationDescriptorSet));
        }
		if (mask.get(spv::DecorationBinding)) {
			sjson_put_int(jctx, jres, "binding", 
                compiler.get_decoration(res.id, spv::DecorationBinding));
        }
		if (mask.get(spv::DecorationInputAttachmentIndex)) {
			sjson_put_int(jctx, jres, "attachment", 
                compiler.get_decoration(res.id, spv::DecorationInputAttachmentIndex));
        }
		if (mask.get(spv::DecorationNonReadable))
			sjson_put_bool(jctx, jres, "writeonly", true);
		if (mask.get(spv::DecorationNonWritable))
			sjson_put_bool(jctx, jres, "readonly", true);
		if (is_sized_block) {
			sjson_put_int(jctx, jres, "block_size", block_size);
			if (runtime_array_stride)
				sjson_put_int(jctx, jres, "unsized_array_stride", runtime_array_stride);
		}

        if (res_type == RES_TYPE_VERTEX_INPUT && loc != -1) {
            sjson_put_string(jctx, jres, "semantic", k_attrib_sem_names[loc]);
            sjson_put_int(jctx, jres, "semantic_index", k_attrib_sem_indices[loc]);
        }

		uint32_t counter_id = 0;
		if (res_type == RES_TYPE_SSBO && compiler.buffer_get_hlsl_counter_buffer(res.id, counter_id))
			sjson_put_int(jctx, jres, "hlsl_counter_buffer_id", counter_id);

        // Some extra 
        if (res_type == RES_TYPE_UNIFORM_BUFFER) {
            if (flatten_ubos) {
                sjson_put_string(jctx, jres, "type", "float4");
                sjson_put_int(jctx, jres, "array", sx_max((int)block_size, 16)/16);
            }

            sjson_node* jmembers = sjson_put_array(jctx, jres, "members");
            // members
            int member_idx = 0;
            for (auto &member_id : type.member_types) {
                sjson_node* jmember = sjson_mkobject(jctx);
                auto& member_type = compiler.get_type(member_id);

                sjson_put_string(jctx, jmember, "name", compiler.get_member_name(type.self, member_idx).c_str());
                sjson_put_string(jctx, jmember, "type", resolve_variable_type(member_type));
                sjson_put_int(jctx, jmember, "offset", compiler.type_struct_member_offset(type, member_idx));
                sjson_put_int(jctx, jmember, "size", (int)compiler.get_declared_struct_member_size(type, member_idx));
                if (!member_type.array.empty()) {
                    int arr_sz = 0;
                    for (auto arr : member_type.array)
                        arr_sz += arr;
                    sjson_put_int(jctx, jmember, "array", arr_sz);
                }

                sjson_append_element(jmembers, jmember);
                member_idx ++;
            }
        } else if (res_type == RES_TYPE_TEXTURE) {
            sjson_put_string(jctx, jres, "dimension", k_texture_dim_str[type.image.dim]);
            sjson_put_string(jctx, jres, "format", k_texture_format_str[type.image.format]);
            if (type.image.ms)
                sjson_put_bool(jctx, jres, "multisample", true);
            if (type.image.arrayed)
                sjson_put_bool(jctx, jres, "array", true);
        } else if (res_type == RES_TYPE_VERTEX_INPUT) {
            sjson_put_string(jctx, jres, "type", resolve_variable_type(type));
        }

        sjson_append_element(jparent, jres);
	}
}

static void output_reflection(const cmd_args& args, const spirv_cross::Compiler& compiler, 
                              const spirv_cross::ShaderResources& ress, 
                              const char* filename,
                              EShLanguage stage, std::string* reflect_json, bool pretty = false)
{
    sjson_context* jctx = sjson_create_context(0, 0, (void*)g_alloc);
    sx_assert(jctx);

    sjson_node* jroot = sjson_mkobject(jctx);
    sjson_put_string(jctx, jroot, "language", k_shader_types[args.lang]);
    sjson_put_int(jctx, jroot, "profile_version", args.profile_ver);
    if (args.compile_bin)
        sjson_put_bool(jctx, jroot, "bytecode", true);
    if (args.debug_bin)
        sjson_put_bool(jctx, jroot, "debug_info", true);
    if (args.flatten_ubos)
        sjson_put_bool(jctx, jroot, "flatten_ubos", true);

    sjson_node* jshader = sjson_put_obj(jctx, jroot, get_stage_name(stage));
    sjson_put_string(jctx, jshader, "file", filename);

    if (!ress.subpass_inputs.empty())
        output_resource_info(jctx, sjson_put_array(jctx, jshader, "subpass_inputs"), compiler, ress.subpass_inputs);
    if (!ress.stage_inputs.empty())
        output_resource_info(jctx, sjson_put_array(jctx, jshader, "inputs"), compiler, ress.stage_inputs, 
            (stage == EShLangVertex) ? RES_TYPE_VERTEX_INPUT : RES_TYPE_REGULAR);
    if (!ress.stage_outputs.empty())
        output_resource_info(jctx, sjson_put_array(jctx, jshader, "outputs"), compiler, ress.stage_outputs);
    if (!ress.sampled_images.empty())
        output_resource_info(jctx, sjson_put_array(jctx, jshader, "textures"), compiler, ress.sampled_images, RES_TYPE_TEXTURE);
    if (!ress.separate_images.empty())
        output_resource_info(jctx, sjson_put_array(jctx, jshader, "sep_images"), compiler, ress.separate_images);
    if (!ress.separate_samplers.empty())
        output_resource_info(jctx, sjson_put_array(jctx, jshader, "sep_samplers"), compiler, ress.separate_samplers);
    if (!ress.storage_images.empty())
        output_resource_info(jctx, sjson_put_array(jctx, jshader, "storage_images"), compiler, ress.storage_images);
    if (!ress.storage_buffers.empty())
        output_resource_info(jctx, sjson_put_array(jctx, jshader, "storage_buffers"), compiler, ress.storage_buffers, RES_TYPE_SSBO);
    if (!ress.uniform_buffers.empty()) {
        output_resource_info(jctx, sjson_put_array(jctx, jshader, "uniform_buffers"), compiler, ress.uniform_buffers,
                             RES_TYPE_UNIFORM_BUFFER, args.flatten_ubos ? true : false);
    }
    if (!ress.push_constant_buffers.empty())
        output_resource_info(jctx, sjson_put_array(jctx, jshader, "push_cbs"), compiler, ress.push_constant_buffers);
    if (!ress.atomic_counters.empty())
        output_resource_info(jctx, sjson_put_array(jctx, jshader, "counters"), compiler, ress.atomic_counters);
    
    char* json_str;
    if (!pretty)
        json_str = sjson_encode(jctx, jroot);
    else
        json_str = sjson_stringify(jctx, jroot, "  ");
    *reflect_json = json_str;
    sjson_free_string(jctx, json_str);
    sjson_destroy_context(jctx);
}

// if binary_size > 0, then we assume the data is binary
static bool write_file(const char* filepath, const char* data, const char* cvar, 
                       bool append = false, int binary_size = -1)
{
    sx_file_writer writer;
    if (!sx_file_open_writer(&writer, filepath, append ? SX_FILE_OPEN_APPEND : 0))
        return false;
    
    if (cvar && cvar[0]) {
        const int chars_per_line = 16;

        // .C file
        if (!append) {
            // file header
            char header[512];
            sx_snprintf(header, sizeof(header),
                        "// This file is automatically created by glslcc v%d.%d.%d\n"
                        "// http://www.github.com/septag/glslcc\n"
                        "// \n"
                        "#pragma once\n\n", VERSION_MAJOR, VERSION_MINOR, VERSION_SUB);
            sx_file_write_text(&writer, header);
        }

        char var[128];
        int len;
        int char_offset = 0;
        char hex[32];
        
        if (binary_size > 0) 
            len = binary_size;
        else
            len = sx_strlen(data) + 1;   // include the '\0' at the end to null-terminate the string

        sx_snprintf(var, sizeof(var), "static const unsigned char %s[%d] = {\n\t", cvar, len);
        sx_file_write_text(&writer, var);

        for (int i = 0; i < len; i++) {
            if (i != len - 1) {
                sx_snprintf(hex, sizeof(hex), "0x%02x, ", (uint8_t)data[i]);
            } else {
                sx_snprintf(hex, sizeof(hex), "0x%02x };\n", (uint8_t)data[i]);
            }
            sx_file_write_text(&writer, hex);

            ++char_offset;
            if (char_offset == chars_per_line) {
                sx_file_write_text(&writer, "\n\t");
                char_offset = 0;
            }
        }
        
        sx_file_write_text(&writer, "\n");
    } else {
        if (binary_size > 0)
            sx_file_write(&writer, data, binary_size);
        else
            sx_file_write(&writer, data, sx_strlen(data));
    }

    sx_file_close_writer(&writer);
    return true;
}

static int cross_compile(const cmd_args& args, std::vector<uint32_t>& spirv, 
                         const char* filename, EShLanguage stage, int file_index)
{
    sx_assert(!spirv.empty());
    // Using SPIRV-cross

    try {
        std::unique_ptr<spirv_cross::CompilerGLSL> compiler;
        // Use spirv-cross to convert to other types of shader
        if (args.lang == SHADER_LANG_GLES || args.lang == SHADER_LANG_GLSL) {
            compiler = std::unique_ptr<spirv_cross::CompilerGLSL>(new spirv_cross::CompilerGLSL(spirv));
        } else if (args.lang == SHADER_LANG_MSL) {
            compiler = std::unique_ptr<spirv_cross::CompilerMSL>(new spirv_cross::CompilerMSL(spirv));
        } else if (args.lang == SHADER_LANG_HLSL) {
            compiler = std::unique_ptr<spirv_cross::CompilerHLSL>(new spirv_cross::CompilerHLSL(spirv));
        } else {
            sx_assert(0 && "Language not implemented");
        }

        spirv_cross::ShaderResources ress = compiler->get_shader_resources();

        spirv_cross::CompilerGLSL::Options opts = compiler->get_common_options();
        opts.flatten_multidimensional_arrays = true;
        if (args.lang == SHADER_LANG_GLES) {
            opts.es = true;
            opts.version = args.profile_ver;
        } else if (args.lang == SHADER_LANG_GLSL) {
            opts.enable_420pack_extension = false;
            opts.es = false;
            opts.version = args.profile_ver;
        } else if (args.lang == SHADER_LANG_HLSL) {
            spirv_cross::CompilerHLSL* hlsl = (spirv_cross::CompilerHLSL*)compiler.get();
            spirv_cross::CompilerHLSL::Options hlsl_opts = hlsl->get_hlsl_options();

            hlsl_opts.shader_model = args.profile_ver;
            hlsl_opts.point_size_compat = true;
            hlsl_opts.point_coord_compat = true;

            hlsl->set_hlsl_options(hlsl_opts);

            uint32_t new_builtin = hlsl->remap_num_workgroups_builtin();
            if (new_builtin) {
                hlsl->set_decoration(new_builtin, spv::DecorationDescriptorSet, 0);
                hlsl->set_decoration(new_builtin, spv::DecorationBinding, 0);
            }
        }

        // Flatten ubos
        if (args.flatten_ubos) {
            for (auto &ubo : ress.uniform_buffers) 
                compiler->flatten_buffer_block(ubo.id);
            for (auto &ubo : ress.push_constant_buffers)
                compiler->flatten_buffer_block(ubo.id);
        }

        compiler->set_common_options(opts);

        std::string code;
        // Prepare vertex attribute remap for HLSL
        if (args.lang == SHADER_LANG_HLSL) {
            std::vector<spirv_cross::HLSLVertexAttributeRemap> remaps;
            for (int i = 0; i < VERTEX_ATTRIB_COUNT; i++) {
                spirv_cross::HLSLVertexAttributeRemap remap = {(uint32_t)i , k_attrib_names[i]};
                remaps.push_back(std::move(remap));
            }

            code = ((spirv_cross::CompilerHLSL*)compiler.get())->compile(std::move(remaps));
        } else {
            code = compiler->compile();
        }

        // Output code
        if (g_sgs) {
            sgs_shader_stage sstage;
            switch (stage) {
            case EShLangVertex:         sstage = SGS_STAGE_VERTEX;      break;
            case EShLangFragment:       sstage = SGS_STAGE_FRAGMENT;    break;
            case EShLangCompute:        sstage = SGS_STAGE_COMPUTE;     break;
            }    

            if (args.compile_bin) {
#ifdef BYTECODE_COMPILATION
                sx_mem_block* mem = compile_binary(code.c_str(), args.out_filepath, args.profile_ver, 
                                                   stage, args.debug_bin);
                if (!mem) {
                    printf("Bytecode compilation of '%s' failed\n", args.out_filepath);
                    return -1;
                }    

                sgs_add_stage_code_bin(g_sgs, sstage, mem->data, mem->size);
                sx_mem_destroy_block(mem);
#endif
            } else {
                sgs_add_stage_code(g_sgs, sstage, code.c_str());
            }


            std::string json_str;
            output_reflection(args, *compiler, ress, args.out_filepath, stage, &json_str);
            sgs_add_stage_reflect(g_sgs, sstage, json_str.c_str());
        } else {
            std::string cvar_code = args.cvar ? args.cvar : "";
            std::string filepath; 
            if (!cvar_code.empty()) {
                cvar_code += "_";
                cvar_code += get_stage_name(stage);
                filepath = args.out_filepath;
            } else {
                char ext[32];
                char basename[512];
                sx_os_path_splitext(ext, sizeof(ext), basename, sizeof(basename), args.out_filepath);
                filepath = std::string(basename) + std::string("_") + std::string(get_stage_name(stage)) + std::string(ext);
            }
            bool append = !cvar_code.empty() & (file_index > 0);

            // Check if we have to compile byte-code or output the source only
            if (args.compile_bin) {
#ifdef BYTECODE_COMPILATION
                sx_mem_block* mem = compile_binary(code.c_str(), filepath.c_str(), args.profile_ver, 
                                                   stage, args.debug_bin);
                if (!mem) {
                    printf("Bytecode compilation of '%s' failed\n", filepath.c_str());
                    return -1;
                }

                if (!write_file(filepath.c_str(), (const char*)mem->data, cvar_code.c_str(), append, mem->size)) {
                    printf("Writing to '%s' failed\n", filepath.c_str());
                    return -1;
                }

                sx_mem_destroy_block(mem);
#endif
            } else {
                // output code file
                if (!write_file(filepath.c_str(), code.c_str(), cvar_code.c_str(), append)) {
                    printf("Writing to '%s' failed\n", filepath.c_str());
                    return -1;
                }
            }

            if (args.reflect) {
                // output json reflection file
                // if --reflect is defined, we just output to that file
                // if --reflect is not defined, check cvar (.C file), and if set, output to the same file (out_filepath)
                // if --reflect is not defined and there is no cvar, output to out_filepath.json
                std::string json_str;

                output_reflection(args, *compiler, ress, filepath.c_str(), stage, &json_str, cvar_code.empty());

                std::string reflect_filepath;
                if (args.reflect_filepath) {
                    reflect_filepath = args.reflect_filepath;
                } else if (!cvar_code.empty()) {
                    reflect_filepath = filepath;
                    append = true;
                } else {
                    reflect_filepath = filepath;
                    reflect_filepath += ".json";
                }

                std::string cvar_refl = !cvar_code.empty() ? (cvar_code + "_refl") : "";
                if (!write_file(reflect_filepath.c_str(), json_str.c_str(), cvar_refl.c_str(), append)) {
                    printf("Writing to '%s' failed", reflect_filepath.c_str());
                    return -1;
                }
            }
        }
        
        puts(filename); // SUCCESS
        return 0;
    } catch (const std::exception& e) {
        printf("SPIRV-cross: %s\n", e.what());
        return -1;
    }
}

struct compile_file_desc
{
    EShLanguage stage;
    const char* filename;
};

#define compile_files_ret(_code)        \
        destroy_shaders(shaders);       \
        sx_array_free(g_alloc, files);  \
        prog->~TProgram();              \
        sx_free(g_alloc, prog);         \
        glslang::FinalizeProcess();     \
        return _code;                   

static int compile_files(cmd_args& args, const TBuiltInResource& limits_conf)
{
    auto destroy_shaders = [](glslang::TShader**& shaders) {
        for (int i = 0; i < sx_array_count(shaders); i++) {
            if (shaders[i]) {
                shaders[i]->~TShader();
                sx_free(g_alloc, shaders[i]);
            }
        }
        sx_array_free(g_alloc, shaders);
    };

    glslang::InitializeProcess();

    // Gather files for compilation
    compile_file_desc* files = nullptr;
    if (args.vs_filepath) {
        compile_file_desc d = {EShLangVertex, args.vs_filepath};
        sx_array_push(g_alloc, files, d);
    }

    if (args.fs_filepath) {
        compile_file_desc d = {EShLangFragment, args.fs_filepath};
        sx_array_push(g_alloc, files, d);
    }

    if (args.cs_filepath) {
        compile_file_desc d = {EShLangCompute, args.cs_filepath};
        sx_array_push(g_alloc, files, d);
    }

    glslang::TProgram* prog = new(sx_malloc(g_alloc, sizeof(glslang::TProgram))) glslang::TProgram();
    glslang::TShader** shaders = nullptr;

    // TODO: add more options for messaging options
    EShMessages messages = EShMsgDefault;
    int default_version = 100; // 110 for desktop

    // construct semantics mapping defines
    // to be used in layout(location = SEMANTIC) inside GLSL
    std::string semantics_def;
    for (int i = 0; i < VERTEX_ATTRIB_COUNT; i++) {
        char sem_line[128];
        sx_snprintf(sem_line, sizeof(sem_line), "#define %s %d\n", k_attrib_names[i], i);
        semantics_def += std::string(sem_line);
    }

    // Add SV_Target semantics for more HLSL compatibility
    for (int i = 0; i < 8; i++) {
        char sv_target_line[128];
        sx_snprintf(sv_target_line, sizeof(sv_target_line), "#define SV_Target%d %d\n", i, i);
        semantics_def += std::string(sv_target_line);
    }

    for (int i = 0; i < sx_array_count(files); i++) {
        // Always set include_directive in the preamble, because we may need to include shaders
        std::string def("#extension GL_GOOGLE_include_directive : require\n");
        def += semantics_def;

        // Read target file
        sx_mem_block* mem = sx_file_load_bin(g_alloc, files[i].filename);
        if (!mem) {
            printf("opening file '%s' failed\n", files[i].filename);
            compile_files_ret(-1);
        }

        glslang::TShader* shader = new(sx_malloc(g_alloc, sizeof(glslang::TShader))) glslang::TShader(files[i].stage);
        sx_assert(shader);
        sx_array_push(g_alloc, shaders, shader);
        
        char* shader_str = (char*)mem->data;
        int shader_len = (int)mem->size;
        shader->setStringsWithLengthsAndNames(&shader_str, &shader_len, &files[i].filename, 1);
        shader->setInvertY(args.invert_y ? true : false);
        shader->setEnvInput(glslang::EShSourceGlsl, files[i].stage, glslang::EShClientVulkan, default_version);
        shader->setEnvClient(glslang::EShClientVulkan, glslang::EShTargetVulkan_1_1);
        //shader->setEnvInput(glslang::EShSourceGlsl, files[i].stage, glslang::EShClientOpenGL, default_version);
        //shader->setEnvClient(glslang::EShClientOpenGL, glslang::EShTargetOpenGL_450);
        shader->setEnvTarget(glslang::EShTargetSpv, glslang::EShTargetSpv_1_0);
        add_defines(shader, args, def);

        if (args.preprocess) {
            std::string prep_str;
            if (shader->preprocess(&limits_conf, default_version, ENoProfile, false, false, messages, &prep_str,
                                args.includer)) {
                puts("-------------------");
                printf("%s:\n", files[i].filename);
                puts("-------------------");
                puts(prep_str.c_str());
                puts("");
            } else {
                const char* info_log = shader->getInfoLog();
                const char* info_log_dbg = shader->getInfoDebugLog();
                if (info_log && info_log[0])
                    fprintf(stderr, "%s\n", info_log);
                if (info_log_dbg && info_log_dbg[0])
                    fprintf(stderr, "%s\n", info_log_dbg);
                sx_mem_destroy_block(mem);
                compile_files_ret(-1);
            }
        } else {
            if (!shader->parse(&limits_conf, default_version, false, messages, args.includer)) {
                const char* info_log = shader->getInfoLog();
                const char* info_log_dbg = shader->getInfoDebugLog();
                if (info_log && info_log[0])
                    fprintf(stderr, "%s\n", info_log);
                if (info_log_dbg && info_log_dbg[0])
                    fprintf(stderr, "%s\n", info_log_dbg);
                sx_mem_destroy_block(mem);
                compile_files_ret(-1);
            }

            prog->addShader(shader);
        }

        sx_mem_destroy_block(mem);
    }   // foreach (file)

    // In preprocess mode, do not link, just exit
    if (args.preprocess) {
        compile_files_ret(0);
    }

    if (!prog->link(messages)) {
        puts("Link failed: ");
        fprintf(stderr, "%s\n", prog->getInfoLog());
        fprintf(stderr, "%s\n", prog->getInfoDebugLog());
        compile_files_ret(-1);
    }

    // Output and save SPIR-V for each shader
    for (int i = 0; i < sx_array_count(files); i++) {
        std::vector<uint32_t> spirv;

        glslang::SpvOptions spv_opts;
        spv_opts.validate = true;
        spv::SpvBuildLogger logger;
        sx_assert(prog->getIntermediate(files[i].stage));

        glslang::GlslangToSpv(*prog->getIntermediate(files[i].stage), spirv, &logger, &spv_opts);
        if (!logger.getAllMessages().empty())
            puts(logger.getAllMessages().c_str());

        if (cross_compile(args, spirv, files[i].filename, files[i].stage, i) != 0) {
            compile_files_ret(-1);
        }
    }

    destroy_shaders(shaders);
    prog->~TProgram();
    sx_free(g_alloc, prog);

    glslang::FinalizeProcess();
    sx_array_free(g_alloc, files);

    return 0;
}

int main(int argc, char* argv[])
{
    cmd_args args = {};
    args.lang = SHADER_LANG_COUNT;

    int version = 0;
    int dump_conf = 0;

    const sx_cmdline_opt opts[] = {
        {"help", 'h', SX_CMDLINE_OPTYPE_NO_ARG, 0x0, 'h', "Print this help text", 0x0},
        {"version", 'V', SX_CMDLINE_OPTYPE_FLAG_SET, &version, 1, "Print version", 0x0},
        {"vert", 'v', SX_CMDLINE_OPTYPE_REQUIRED, 0x0, 'v', "Vertex shader source file", "Filepath"},
        {"frag", 'f', SX_CMDLINE_OPTYPE_REQUIRED, 0x0, 'f', "Fragment shader source file", "Filepath"},
        {"compute", 'c', SX_CMDLINE_OPTYPE_REQUIRED, 0x0, 'c', "Compute shader source file", "Filepath"},
        {"output", 'o', SX_CMDLINE_OPTYPE_REQUIRED, 0x0, 'o', "Output file", "Filepath"},
        {"lang", 'l', SX_CMDLINE_OPTYPE_REQUIRED, 0x0, 'l', "Convert to shader language", "gles/msl/hlsl/glsl"},
        {"defines", 'D', SX_CMDLINE_OPTYPE_OPTIONAL, 0x0, 'D', "Preprocessor definitions, seperated by comma or ';'", "Defines"},
        {"invert-y", 'Y', SX_CMDLINE_OPTYPE_FLAG_SET, &args.invert_y, 1, "Invert position.y in vertex shader", 0x0},
        {"profile", 'p', SX_CMDLINE_OPTYPE_REQUIRED, 0x0, 'p', "Shader profile version (HLSL: 40, 50, 60), (ES: 200, 300), (GLSL: 330, 400, 420)", "ProfileVersion"},
        {"dumpc", 'C', SX_CMDLINE_OPTYPE_FLAG_SET, &dump_conf, 1, "Dump shader limits configuration", 0x0},
        {"include-dirs", 'I', SX_CMDLINE_OPTYPE_REQUIRED, 0x0, 'I', "Set include directory for <system> files, seperated by ';'", "Directory(s)"},
        {"preprocess", 'P', SX_CMDLINE_OPTYPE_FLAG_SET, &args.preprocess, 1, "Dump preprocessed result to terminal"},
        {"cvar", 'N', SX_CMDLINE_OPTYPE_REQUIRED, 0x0, 'N', "Outputs Hex data to a C include file with a variable name", "VariableName"},
        {"flatten-ubos", 'F', SX_CMDLINE_OPTYPE_FLAG_SET, &args.flatten_ubos, 1, "Flatten UBOs, useful for ES2 shaders", 0x0},
        {"reflect", 'r', SX_CMDLINE_OPTYPE_OPTIONAL, 0x0, 'r', "Output shader reflection information to a json file", "Filepath"},
        {"sgs", 'G', SX_CMDLINE_OPTYPE_FLAG_SET, &args.sgs_file, 1, "Output file should be packed SGS format", "Filepath"},
        {"bin", 'b', SX_CMDLINE_OPTYPE_FLAG_SET, &args.compile_bin, 1, "Compile to bytecode instead of source. requires ENABLE_D3D11_COMPILER build flag", 0x0},
        {"debug-bin", 'g', SX_CMDLINE_OPTYPE_FLAG_SET, &args.debug_bin, 1, "Generate debug info for binary compilation, should come with --bin", 0x0},
        SX_CMDLINE_OPT_END
    };
    sx_cmdline_context* cmdline = sx_cmdline_create_context(g_alloc, argc, (const char**)argv, opts);
    
    int opt;
    const char* arg;
    while ((opt = sx_cmdline_next(cmdline, NULL, &arg)) != -1) {
        switch (opt) {
            case '+': printf("Got argument without flag: %s\n", arg);           break;
            case '?': printf("Unknown argument: %s\n", arg);         exit(-1);  break;
            case '!': printf("Invalid use of argument: %s\n", arg);  exit(-1);  break;
            case 'v': args.vs_filepath = arg;                                   break;
            case 'f': args.fs_filepath = arg;                                   break;
            case 'c': args.cs_filepath = arg;                                   break;
            case 'o': args.out_filepath = arg;                                  break;
            case 'D': parse_defines(&args, arg);                                break;
            case 'l': args.lang = parse_shader_lang(arg);                       break;
            case 'h': print_help(cmdline);                                      break;
            case 'p': args.profile_ver = sx_toint(arg);                         break;
            case 'I': parse_includes(&args, arg);                               break;
            case 'N': args.cvar = arg;                                          break;
            case 'r': args.reflect_filepath = arg;  args.reflect = 1;           break;
            default:                                                            break;
        }
    }

    if (version) {
        print_version();
        exit(0);
    }

    if (dump_conf) {
        puts(get_default_conf_str().c_str());
        exit(0);
    }

    if ((args.vs_filepath && !sx_os_path_isfile(args.vs_filepath)) || 
        (args.fs_filepath && !sx_os_path_isfile(args.fs_filepath)) ||
        (args.cs_filepath && !sx_os_path_isfile(args.cs_filepath))) 
    {
        puts("input files are invalid");
        exit(-1);
    }

    if (!args.vs_filepath && !args.fs_filepath && !args.cs_filepath) {
        puts("you must at least define one input shader file");
        exit(-1);
    }

    if (args.cs_filepath && (args.vs_filepath || args.fs_filepath)) {
        puts("Cannot link compute-shader with either fragment shader or vertex shader");
        exit(-1);
    }

    if (args.out_filepath == nullptr && !args.preprocess) {
        puts("Output file is not specified");
        exit(-1);
    }

    if (args.lang == SHADER_LANG_COUNT && !args.preprocess) {
        puts("Shader language is not specified");
        exit(-1);
    }

    if (args.out_filepath) {
        // determine if we output SGS format automatically
        char ext[32];
        sx_os_path_ext(ext, sizeof(ext), args.out_filepath);
        if (sx_strequalnocase(ext, ".sgs"))
            args.sgs_file = 1;
    }

    // Set default shader profile version
    // HLSL: 50 (5.0)
    // GLSL: 200 (2.00)
    if (args.profile_ver == 0) {
        if (args.lang == SHADER_LANG_GLES)
            args.profile_ver = 200; 
        else if (args.lang == SHADER_LANG_HLSL)
            args.profile_ver = 50;      // D3D11
        else if (args.lang == SHADER_LANG_GLSL)
            args.profile_ver = 400;     // D3D11 level hardware
    }

#if SX_PLATFORM_WINDOWS 
    if (args.compile_bin && (args.lang != SHADER_LANG_HLSL || args.profile_ver >= 60)) {
        puts("ignoring --bin flag, byte-code compilation not implemented for this target");
        args.compile_bin = 0;
    } 
#   ifndef D3D11_COMPILER
    // Windows + HLSL -> works but requires ENABLE_D3D11_COMPILER
    else if (args.compile_bin) {
        puts("Cannot compile to byte-code, glslcc is not built with ENABLE_D3D11_COMPILER flag");
        exit(-1);
    }
#   endif    
#else
    if (args.compile_bin) {
        puts("ignoring --bin flag, byte-code compilation not implemented for this target");
        args.compile_bin = 0;
    }
#endif


    if (args.sgs_file && !args.preprocess) {
        sgs_shader_lang slang;
        switch (args.lang) {
            case SHADER_LANG_GLES:  slang = SGS_SHADER_GLES;    break;
            case SHADER_LANG_HLSL:  slang = SGS_SHADER_HLSL;    break;
            case SHADER_LANG_MSL:   slang = SGS_SHADER_MSL;     break;
            case SHADER_LANG_GLSL:  slang = SGS_SHADER_GLSL;    break;
        }
        g_sgs = sgs_create_file(g_alloc, args.out_filepath, slang, args.profile_ver);
        sx_assert(g_sgs);
    }

    int r = compile_files(args, k_default_conf);

    if (g_sgs) {
        if (r == 0 && !sgs_commit(g_sgs)) {
            printf("Writing SGS file '%s' failed", args.out_filepath);
        }
        sgs_destroy_file(g_sgs);
    }

    sx_cmdline_destroy_context(cmdline, g_alloc);
    cleanup_args(&args);
    return r;
}
