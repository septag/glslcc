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
//      1.4.3       Bug fixed for MSL shaders where vertex input attributes needed to be sequential (SEMANTIC conflict)
//      1.4.4       Minor bug fixes in SGS export
//      1.4.5       Improved binary header writer to uint32_t
//      1.5.0       Sgs format is now IFF like
//      1.5.1       updated sx lib
//      1.6.0       shader validation, output custom compiler error formats
//      1.61.0      gcc output error format
//      1.62.0      removed "#version 200 es" inclusion from spirv_glsl.cpp, 330 default for GLSL
//      1.63.0      added relfection data to compute shader for SGS files
//      1.7.0       spirv-cross / glslang update
//      1.7.1       bug fixed in spirv_glsl::emit_header for OpenGL 3+ GLSL shaders
//      1.7.2       bugs fixed in parsing --defines and --include-dirs flags
//      1.7.3       Added current file directory to the include-dirs
//      1.7.4       Added //@begin_vert //@begin_frag //@end tags in .glsl files
//      1.7.5       List include names in the shader with -L argument
//      1.7.6       Fixed bugs in parse output
//
#define _ALLOW_KEYWORD_MACROS

#include "sx/allocator.h"
#include "sx/array.h"
#include "sx/cmdline.h"
#include "sx/io.h"
#include "sx/os.h"
#include "sx/string.h"

#include <stdio.h>
#include <stdlib.h>

#include <string>

#include "SPIRV/GlslangToSpv.h"
#include "SPIRV/SpvTools.h"
#include "SPIRV/disassemble.h"
#include "SPIRV/spirv.hpp"

#include "spirv_cross.hpp"
#include "spirv_glsl.hpp"
#include "spirv_hlsl.hpp"
#include "spirv_msl.hpp"

#include "config.h"
#include "sgs-file.h"

#ifdef D3D11_COMPILER
#include <d3dcompiler.h>
#define BYTECODE_COMPILATION
#endif

// sjson
#define sjson_malloc(user, size) sx_malloc((const sx_alloc*)user, size)
#define sjson_free(user, ptr) sx_free((const sx_alloc*)user, ptr)
#define sjson_realloc(user, ptr, size) sx_realloc((const sx_alloc*)user, ptr, size)
#define sjson_assert(e) sx_assert(e);
#define sjson_snprintf sx_snprintf
#define sjson_strcpy(dst, n, src) sx_strcpy(dst, n, src)
#define SJSON_IMPLEMENTATION
#include "../3rdparty/sjson/sjson.h"

#define VERSION_MAJOR 1
#define VERSION_MINOR 7
#define VERSION_SUB 6

static const sx_alloc* g_alloc = sx_alloc_malloc();
static sgs_file* g_sgs = nullptr;

struct p_define {
    char* def;
    char* val;
};

enum shader_lang {
    SHADER_LANG_GLES = 0,
    SHADER_LANG_HLSL,
    SHADER_LANG_MSL,
    SHADER_LANG_GLSL,
    SHADER_LANG_COUNT
};

enum output_error_format {
    OUTPUT_ERRORFORMAT_GLSLANG = 0,
    OUTPUT_ERRORFORMAT_MSVC,
    OUTPUT_ERRORFORMAT_GCC
};

static const char* k_shader_types[SHADER_LANG_COUNT] = {
    "gles",
    "hlsl",
    "msl",
    "glsl"
};

static const uint32_t k_shader_langs_fourcc[SHADER_LANG_COUNT] = {
    SGS_LANG_GLES,
    SGS_LANG_HLSL,
    SGS_LANG_MSL,
    SGS_LANG_GLSL
};

enum vertex_attribs {
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
class Includer : public glslang::TShader::Includer {
public:
    Includer() : glslang::TShader::Includer()
    {
        m_listIncludes = false;
    }

    explicit Includer(bool list_files) : glslang::TShader::Includer()
    {
        m_listIncludes = list_files;
    }

    virtual ~Includer() {}

    IncludeResult* includeSystem(const char* headerName,
        const char* includerName,
        size_t inclusionDepth) override
    {
        for (auto i = m_systemDirs.begin(); i != m_systemDirs.end(); ++i) {
            std::string header_path(*i);
            if (!header_path.empty() && header_path.back() != '/')
                header_path += "/";
            header_path += headerName;

            if (sx_os_stat(header_path.c_str()).type == SX_FILE_TYPE_REGULAR) {
                sx_mem_block* mem = sx_file_load_bin(g_alloc, header_path.c_str());
                if (mem) {
                    if (m_listIncludes) {
                        puts(header_path.c_str());
                    }
                    return new (sx_malloc(g_alloc, sizeof(IncludeResult)))
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
        if (mem) {
            if (m_listIncludes) {
                puts(headerName);
            }
            return new (sx_malloc(g_alloc, sizeof(IncludeResult)))
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

    void addIncluder(const Includer& includer)
    {
        for (const std::string& inc : includer.m_systemDirs) {
            m_systemDirs.push_back(inc);
        }
    }

private:
    std::vector<std::string> m_systemDirs;
    bool m_listIncludes;
};

struct cmd_args {
    const char* vs_filepath;
    const char* fs_filepath;
    const char* cs_filepath;
    const char* out_filepath;
    shader_lang lang;
    p_define* defines;
    Includer includer;
    int profile_ver;
    int invert_y;
    int preprocess;
    int flatten_ubos;
    int sgs_file;
    int reflect;
    int compile_bin;
    int debug_bin;
    int optimize;
    int silent;
    int validate;
    int list_includes;
    output_error_format err_format;
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
         "\t- Compute shader (--compute)\n");
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

static output_error_format parse_output_errorformat(const char* arg)
{
    if (sx_strequalnocase(arg, "msvc")) {
        return OUTPUT_ERRORFORMAT_MSVC;
    } else if (sx_strequalnocase(arg, "glslang")) {
        return OUTPUT_ERRORFORMAT_GLSLANG;
    }

    return OUTPUT_ERRORFORMAT_GLSLANG;
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
            int len = next_def ? (int)(uintptr_t)(next_def - def) : sx_strlen(def);

            if (len > 0) {
                p_define d = { 0x0 };
                d.def = (char*)sx_malloc(g_alloc, len + 1);
                sx_strncpy(d.def, len + 1, def, len);
                sx_trim_whitespace(d.def, len + 1, d.def);

                // Check def=value pair
                char* equal = (char*)sx_strchar(d.def, '=');
                if (equal) {
                    *equal = 0;
                    d.val = equal + 1;
                }

                sx_array_push(g_alloc, args->defines, d);
            }

            def = next_def;
            if (def) {
                def++;
            }
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
        sx_snprintf(target, sizeof(target), "vs_%d_%d", major_ver, minor_ver);
        break;
    case EShLangFragment:
        sx_snprintf(target, sizeof(target), "ps_%d_%d", major_ver, minor_ver);
        break;
    case EShLangCompute:
        sx_snprintf(target, sizeof(target), "cs_%d_%d", major_ver, minor_ver);
        break;
    }

    uint32_t compile_flags;
    if (!debug)
        compile_flags = D3DCOMPILE_OPTIMIZATION_LEVEL3;
    else
        compile_flags = D3DCOMPILE_DEBUG | D3DCOMPILE_SKIP_OPTIMIZATION;

    HRESULT hr = D3DCompile(
        code, /* pSrcData */
        sx_strlen(code), /* SrcDataSize */
        filename, /* pSourceName */
        NULL, /* pDefines */
        NULL, /* pInclude */
        "main", /* pEntryPoint */
        target, /* pTarget (vs_5_0 or ps_5_0) */
        compile_flags, /* Flags1 */
        0, /* Flags2 */
        &output, /* ppCode */
        &errors); /* ppErrorMsgs */

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
            int len = next_inc ? (int)(uintptr_t)(next_inc - inc) : sx_strlen(inc);

            if (len > 0) {
                char* inc_str = (char*)sx_malloc(g_alloc, len + 1);
                sx_assert(inc_str);
                sx_strncpy(inc_str, len + 1, inc, len);
                args->includer.addSystemDir(inc_str);
                sx_free(g_alloc, inc_str);
            }

            inc = next_inc;
            if (inc) {
                inc++;
            }
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

enum resource_type {
    RES_TYPE_REGULAR = 0,
    RES_TYPE_SSBO,
    RES_TYPE_VERTEX_INPUT,
    RES_TYPE_TEXTURE,
    RES_TYPE_UNIFORM_BUFFER
};

struct uniform_type_mapping {
    spirv_cross::SPIRType::BaseType base_type;
    int vec_size;
    int columns;
    const char* type_str;
    uint32_t fourcc;
};

static const uniform_type_mapping k_uniform_map[] = {
    { spirv_cross::SPIRType::Float, 1, 1, "float", SGS_VERTEXFORMAT_FLOAT },
    { spirv_cross::SPIRType::Float, 2, 1, "float2", SGS_VERTEXFORMAT_FLOAT2 },
    { spirv_cross::SPIRType::Float, 3, 1, "float3", SGS_VERTEXFORMAT_FLOAT3 },
    { spirv_cross::SPIRType::Float, 4, 1, "float4", SGS_VERTEXFORMAT_FLOAT4 },
    { spirv_cross::SPIRType::Float, 3, 4, "mat3x4", 0 },
    { spirv_cross::SPIRType::Float, 3, 4, "mat4x3", 0 },
    { spirv_cross::SPIRType::Float, 3, 3, "mat3x3", 0 },
    { spirv_cross::SPIRType::Float, 4, 4, "mat4", 0 },
    { spirv_cross::SPIRType::Int, 1, 1, "int", SGS_VERTEXFORMAT_INT },
    { spirv_cross::SPIRType::Int, 2, 1, "int2", SGS_VERTEXFORMAT_INT2 },
    { spirv_cross::SPIRType::Int, 3, 1, "int3", SGS_VERTEXFORMAT_INT3 },
    { spirv_cross::SPIRType::Int, 4, 1, "int4", SGS_VERTEXFORMAT_INT4 },
    { spirv_cross::SPIRType::Half, 4, 1, "float", SGS_VERTEXFORMAT_FLOAT },
    { spirv_cross::SPIRType::Half, 4, 2, "float2", SGS_VERTEXFORMAT_FLOAT2 },
    { spirv_cross::SPIRType::Half, 4, 3, "float3", SGS_VERTEXFORMAT_FLOAT3 },
    { spirv_cross::SPIRType::Half, 4, 4, "float4", SGS_VERTEXFORMAT_FLOAT4 }
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

const char* k_texture_format_str[spv::ImageFormatR8ui + 1] = {
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

const char* k_texture_dim_str[spv::DimSubpassData + 1] = {
    "1d",
    "2d",
    "3d",
    "cube",
    "rect",
    "buffer",
    "subpass_data"
};

const uint32_t k_texture_dim_fourcc[spv::DimSubpassData + 1] = {
    SGS_IMAGEDIM_1D,
    SGS_IMAGEDIM_2D,
    SGS_IMAGEDIM_3D,
    SGS_IMAGEDIM_CUBE,
    SGS_IMAGEDIM_RECT,
    SGS_IMAGEDIM_BUFFER,
    SGS_IMAGEDIM_SUBPASS
};

// https://github.com/KhronosGroup/SPIRV-Cross/wiki/Reflection-API-user-guide
static void output_resource_info_json(sjson_context* jctx, sjson_node* jparent,
    const spirv_cross::Compiler& compiler,
    const spirv_cross::SmallVector<spirv_cross::Resource>& ress,
    resource_type res_type = RES_TYPE_REGULAR,
    bool flatten_ubos = false)
{

    auto resolve_variable_type = [](const spirv_cross::SPIRType& type) -> const char* {
        int count = sizeof(k_uniform_map) / sizeof(uniform_type_mapping);
        for (int i = 0; i < count; i++) {
            if (k_uniform_map[i].base_type == type.basetype && k_uniform_map[i].vec_size == type.vecsize && k_uniform_map[i].columns == type.columns) {
                return k_uniform_map[i].type_str;
            }
        }
        return "unknown";
    };

    auto fill_members = [&jctx, &compiler, &resolve_variable_type] (sjson_node* jres, const spirv_cross::SPIRType& type){
      sjson_node *jmembers = sjson_put_array(jctx, jres, "members");
      // members
      int member_idx = 0;
      for (auto &member_id : type.member_types) {
        sjson_node *jmember = sjson_mkobject(jctx);
        auto &member_type = compiler.get_type(member_id);

        sjson_put_string(
            jctx, jmember, "name",
            compiler.get_member_name(type.self, member_idx).c_str());
        sjson_put_string(jctx, jmember, "type",
                         resolve_variable_type(member_type));
        sjson_put_int(jctx, jmember, "offset",
                      compiler.type_struct_member_offset(type, member_idx));
        sjson_put_int(jctx, jmember, "size",
                      (int)compiler.get_declared_struct_member_size(
                          type, member_idx));
        if (!member_type.array.empty()) {
          int arr_sz = 0;
          for (auto arr : member_type.array)
            arr_sz += arr;
          sjson_put_int(jctx, jmember, "array", arr_sz);
        }

        sjson_append_element(jmembers, jmember);
        member_idx++;
      }
    };

    for (auto& res : ress) {
        sjson_node* jres = sjson_mkobject(jctx);
        auto& type = compiler.get_type(res.type_id);
        if (res_type == RES_TYPE_SSBO && compiler.buffer_is_hlsl_counter_buffer(res.id))
            continue;

        // If we don't have a name, use the fallback for the type instead of the variable
        // for SSBOs and UBOs since those are the only meaningful names to use externally.
        // Push constant blocks are still accessed by name and not block name, even though they are technically Blocks.
        bool is_push_constant = compiler.get_storage_class(res.id) == spv::StorageClassPushConstant;
        bool is_block = compiler.get_decoration_bitset(type.self).get(spv::DecorationBlock) || compiler.get_decoration_bitset(type.self).get(spv::DecorationBufferBlock);
        bool is_sized_block = is_block && (compiler.get_storage_class(res.id) == spv::StorageClassUniform || compiler.get_storage_class(res.id) == spv::StorageClassUniformConstant);
        uint32_t fallback_id = !is_push_constant && is_block ? (uint32_t)res.base_type_id : (uint32_t)res.id;

        uint32_t block_size = 0;
        uint32_t runtime_array_stride = 0;
        if (is_sized_block) {
            auto& base_type = compiler.get_type(res.base_type_id);
            block_size = uint32_t(compiler.get_declared_struct_size(base_type));
            runtime_array_stride = uint32_t(compiler.get_declared_struct_size_runtime_array(base_type, 1) - compiler.get_declared_struct_size_runtime_array(base_type, 0));
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


        if (res_type == RES_TYPE_UNIFORM_BUFFER) {
          if (flatten_ubos) {
            sjson_put_string(jctx, jres, "type", "float4");
            sjson_put_int(jctx, jres, "array",
                          sx_max((int)block_size, 16) / 16);
          }

          fill_members(jres, type);

        } else if(is_push_constant && is_block) {
          fill_members(jres, type);
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

static void output_reflection_json(const cmd_args& args, const spirv_cross::Compiler& compiler,
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
        output_resource_info_json(jctx, sjson_put_array(jctx, jshader, "subpass_inputs"), compiler, ress.subpass_inputs);
    if (!ress.stage_inputs.empty())
        output_resource_info_json(jctx, sjson_put_array(jctx, jshader, "inputs"), compiler, ress.stage_inputs,
            (stage == EShLangVertex) ? RES_TYPE_VERTEX_INPUT : RES_TYPE_REGULAR);
    if (!ress.stage_outputs.empty())
        output_resource_info_json(jctx, sjson_put_array(jctx, jshader, "outputs"), compiler, ress.stage_outputs);
    if (!ress.sampled_images.empty())
        output_resource_info_json(jctx, sjson_put_array(jctx, jshader, "textures"), compiler, ress.sampled_images, RES_TYPE_TEXTURE);
    if (!ress.separate_images.empty())
        output_resource_info_json(jctx, sjson_put_array(jctx, jshader, "sep_images"), compiler, ress.separate_images);
    if (!ress.separate_samplers.empty())
        output_resource_info_json(jctx, sjson_put_array(jctx, jshader, "sep_samplers"), compiler, ress.separate_samplers);
    if (!ress.storage_images.empty())
        output_resource_info_json(jctx, sjson_put_array(jctx, jshader, "storage_images"), compiler, ress.storage_images, RES_TYPE_TEXTURE);
    if (!ress.storage_buffers.empty())
        output_resource_info_json(jctx, sjson_put_array(jctx, jshader, "storage_buffers"), compiler, ress.storage_buffers, RES_TYPE_SSBO);
    if (!ress.uniform_buffers.empty()) {
        output_resource_info_json(jctx, sjson_put_array(jctx, jshader, "uniform_buffers"), compiler, ress.uniform_buffers,
            RES_TYPE_UNIFORM_BUFFER, args.flatten_ubos ? true : false);
    }
    if (!ress.push_constant_buffers.empty())
        output_resource_info_json(jctx, sjson_put_array(jctx, jshader, "push_cbs"), compiler, ress.push_constant_buffers);
    if (!ress.atomic_counters.empty())
        output_resource_info_json(jctx, sjson_put_array(jctx, jshader, "counters"), compiler, ress.atomic_counters);

    char* json_str;
    if (!pretty)
        json_str = sjson_encode(jctx, jroot);
    else
        json_str = sjson_stringify(jctx, jroot, "  ");
    *reflect_json = json_str;
    sjson_free_string(jctx, json_str);
    sjson_destroy_context(jctx);
}

static void output_resource_info_bin(sx_mem_writer* w, uint32_t* num_values,
    const spirv_cross::Compiler& compiler,
    const spirv_cross::SmallVector<spirv_cross::Resource>& ress,
    resource_type res_type = RES_TYPE_REGULAR,
    bool flatten_ubos = false)
{
    auto resolve_variable_type = [](const spirv_cross::SPIRType& type) -> uint32_t {
        int count = sizeof(k_uniform_map) / sizeof(uniform_type_mapping);
        for (int i = 0; i < count; i++) {
            if (k_uniform_map[i].base_type == type.basetype && k_uniform_map[i].vec_size == type.vecsize && k_uniform_map[i].columns == type.columns) {
                return k_uniform_map[i].fourcc;
            }
        }
        return 0;
    };

    for (auto& res : ress) {
        auto& type = compiler.get_type(res.type_id);
        if (res_type == RES_TYPE_SSBO && compiler.buffer_is_hlsl_counter_buffer(res.id))
            continue;

        // If we don't have a name, use the fallback for the type instead of the variable
        // for SSBOs and UBOs since those are the only meaningful names to use externally.
        // Push constant blocks are still accessed by name and not block name, even though they are technically Blocks.
        bool is_push_constant = compiler.get_storage_class(res.id) == spv::StorageClassPushConstant;
        bool is_block = compiler.get_decoration_bitset(type.self).get(spv::DecorationBlock) || compiler.get_decoration_bitset(type.self).get(spv::DecorationBufferBlock);
        bool is_sized_block = is_block && (compiler.get_storage_class(res.id) == spv::StorageClassUniform || compiler.get_storage_class(res.id) == spv::StorageClassUniformConstant);
        uint32_t fallback_id = !is_push_constant && is_block ? (uint32_t)res.base_type_id : (uint32_t)res.id;

        uint32_t block_size = 0;
        uint32_t runtime_array_stride = 0;
        if (is_sized_block) {
            auto& base_type = compiler.get_type(res.base_type_id);
            block_size = uint32_t(compiler.get_declared_struct_size(base_type));
            runtime_array_stride = uint32_t(compiler.get_declared_struct_size_runtime_array(base_type, 1) - compiler.get_declared_struct_size_runtime_array(base_type, 0));
        }

        spirv_cross::Bitset mask;
        if (res_type == RES_TYPE_SSBO)
            mask = compiler.get_buffer_block_flags(res.id);
        else
            mask = compiler.get_decoration_bitset(res.id);

        const char* name = !res.name.empty() ? res.name.c_str() : compiler.get_fallback_name(fallback_id).c_str();

        int array_size = 1;
        if (!type.array.empty()) {
            int arr_sz = 0;
            for (auto arr : type.array)
                arr_sz += arr;
            array_size = arr_sz;
        }

        int loc = -1;
        int binding = -1;
        if (mask.get(spv::DecorationLocation)) {
            loc = compiler.get_decoration(res.id, spv::DecorationLocation);
        }

        if (mask.get(spv::DecorationBinding)) {
            binding = compiler.get_decoration(res.id, spv::DecorationBinding);
        }

        // Some extra
        if (res_type == RES_TYPE_UNIFORM_BUFFER) {
            sgs_refl_uniformbuffer u = { 0 };

            sx_strcpy(u.name, sizeof(u.name), name);
            u.binding = binding;
            u.size_bytes = block_size;
            if (flatten_ubos) {
                u.array_size = (uint16_t)sx_max((int)block_size, 16) / 16;
            } else {
                u.array_size = array_size;
            }
            sx_mem_write_var(w, u);
        } else if (res_type == RES_TYPE_TEXTURE) {
            sgs_refl_texture t = { 0 };

            sx_strcpy(t.name, sizeof(t.name), name);
            t.binding = binding;
            t.image_dim = k_texture_dim_fourcc[type.image.dim];
            t.multisample = type.image.ms ? 1 : 0;
            t.is_array = type.image.arrayed ? 1 : 0;
            sx_mem_write_var(w, t);
        } else if (res_type == RES_TYPE_VERTEX_INPUT) {
            sgs_refl_input i = { 0 };

            sx_strcpy(i.name, sizeof(i.name), name);
            i.loc = loc;
            sx_strcpy(i.semantic, sizeof(i.semantic), k_attrib_sem_names[loc]);
            i.semantic_index = k_attrib_sem_indices[loc];
            i.format = resolve_variable_type(type);
            sx_mem_write_var(w, i);
        } else if (res_type == RES_TYPE_SSBO) {
            sgs_refl_buffer b = { 0 };
            sx_strcpy(b.name, sizeof(b.name), name);
            b.binding = binding;
            b.size_bytes = block_size;
            if (runtime_array_stride) {
                b.array_stride = runtime_array_stride;
            }
            sx_mem_write_var(w, b);
        }

        ++(*num_values);
    }
}

static void output_reflection_bin(const cmd_args& args, const spirv_cross::Compiler& compiler,
    const spirv_cross::ShaderResources& ress,
    const char* filename,
    EShLanguage stage, sx_mem_block** refl_mem)
{
    sx_mem_writer w;
    sx_mem_init_writer(&w, g_alloc, 2048);

    sgs_chunk_refl refl;
    sx_memset(&refl, 0x0, sizeof(refl));
    sx_os_path_basename(refl.name, sizeof(refl.name), filename);
    refl.flatten_ubos = args.flatten_ubos;
    refl.debug_info = args.debug_bin;
    sx_mem_write_var(&w, refl);

    if (!ress.stage_inputs.empty() && stage == EShLangVertex) {
        output_resource_info_bin(&w, &refl.num_inputs, compiler, ress.stage_inputs, RES_TYPE_VERTEX_INPUT);
    }

    if (!ress.uniform_buffers.empty()) {
        output_resource_info_bin(&w, &refl.num_uniform_buffers, compiler, ress.uniform_buffers,
            RES_TYPE_UNIFORM_BUFFER, args.flatten_ubos ? true : false);
    }

    if (!ress.sampled_images.empty()) {
        output_resource_info_bin(&w, &refl.num_textures, compiler, ress.sampled_images, RES_TYPE_TEXTURE);
    }

    // compute shader reflection data
    if (stage == EShLangCompute) {
        if (!ress.storage_images.empty()) {
            output_resource_info_bin(&w, &refl.num_storage_images, compiler, ress.storage_images,
                RES_TYPE_TEXTURE);
        }

        if (!ress.storage_buffers.empty()) {
            output_resource_info_bin(&w, &refl.num_storage_buffers, compiler, ress.storage_buffers,
                RES_TYPE_SSBO);
        }
    }

    sx_mem_seekw(&w, 0, SX_WHENCE_BEGIN);
    sx_mem_write_var(&w, refl);

    *refl_mem = w.mem;
}

// if binary_size > 0, then we assume the data is binary
static bool write_file(const char* filepath, const char* data, const char* cvar,
    bool append = false, int binary_size = -1)
{
    sx_file_writer writer;
    if (!sx_file_open_writer(&writer, filepath, append ? SX_FILE_OPEN_APPEND : 0))
        return false;

    if (cvar && cvar[0]) {
        const int items_per_line = 8;

        // .C file
        if (!append) {
            // file header
            char header[512];
            sx_snprintf(header, sizeof(header),
                "// This file is automatically created by glslcc v%d.%d.%d\n"
                "// http://www.github.com/septag/glslcc\n"
                "// \n"
                "#pragma once\n\n",
                VERSION_MAJOR, VERSION_MINOR, VERSION_SUB);
            sx_file_write_text(&writer, header);
        }

        char var[128];
        int len;
        int char_offset = 0;
        char hex[32];

        if (binary_size > 0)
            len = binary_size;
        else
            len = sx_strlen(data) + 1; // include the '\0' at the end to null-terminate the string

        // align data to uint32_t (4)
        const uint32_t* aligned_data = (const uint32_t*)data;
        int aligned_len = sx_align_mask(len, 3);
        if (aligned_len > len) {
            uint32_t* tmp = (uint32_t*)sx_malloc(g_alloc, aligned_len);
            if (!tmp) {
                sx_out_of_memory();
                return false;
            }
            sx_memcpy(tmp, data, len);
            sx_memset((uint8_t*)tmp + len, 0x0, aligned_len - len);
            aligned_data = tmp;
        }
        const char* aligned_ptr = (const char*)aligned_data;

        sx_snprintf(var, sizeof(var), "static const unsigned int %s_size = %d;\n", cvar, len);
        sx_file_write_text(&writer, var);
        sx_snprintf(var, sizeof(var), "static const unsigned int %s_data[%d/4] = {\n\t", cvar, aligned_len);
        sx_file_write_text(&writer, var);

        sx_assert(aligned_len % sizeof(uint32_t) == 0);
        int uint_count = aligned_len / 4;
        for (int i = 0; i < uint_count; i++) {
            if (i != uint_count - 1) {
                sx_snprintf(hex, sizeof(hex), "0x%08x, ", *aligned_data);
            } else {
                sx_snprintf(hex, sizeof(hex), "0x%08x };\n", *aligned_data);
            }
            sx_file_write_text(&writer, hex);

            ++char_offset;
            if (char_offset == items_per_line) {
                sx_file_write_text(&writer, "\n\t");
                char_offset = 0;
            }

            ++aligned_data;
        }

        sx_file_write_text(&writer, "\n");

        if (aligned_ptr != data) {
            sx_free(g_alloc, const_cast<char*>(aligned_ptr));
        }
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
            for (auto& ubo : ress.uniform_buffers)
                compiler->flatten_buffer_block(ubo.id);
            for (auto& ubo : ress.push_constant_buffers)
                compiler->flatten_buffer_block(ubo.id);
        }

        // Reset vertex input locations for MSL
        std::vector<int> old_locs;
        if (args.lang == SHADER_LANG_MSL && stage == EShLangVertex) {
            for (int i = 0; i < ress.stage_inputs.size(); i++) {
                spirv_cross::Resource& res = ress.stage_inputs[i];
                spirv_cross::Bitset mask = compiler->get_decoration_bitset(res.id);

                if (mask.get(spv::DecorationLocation)) {
                    old_locs.push_back(compiler->get_decoration(res.id, spv::DecorationLocation));
                    compiler->set_decoration(res.id, spv::DecorationLocation, (uint32_t)i);
                } else {
                    old_locs.push_back(-1);
                }
            }
        }

        compiler->set_common_options(opts);

        std::string code;
        // Prepare vertex attribute remap for HLSL
        if (args.lang == SHADER_LANG_HLSL) {
            // std::vector<spirv_cross::HLSLVertexAttributeRemap> remaps;
            spirv_cross::CompilerHLSL* hlsl_compiler = (spirv_cross::CompilerHLSL*)compiler.get();
            for (int i = 0; i < VERTEX_ATTRIB_COUNT; i++) {
                spirv_cross::HLSLVertexAttributeRemap remap = { (uint32_t)i, k_attrib_names[i] };
                // remaps.push_back(std::move(remap));
                hlsl_compiler->add_vertex_attribute_remap(remap);
            }

            code = hlsl_compiler->compile();
        } else {
            code = compiler->compile();
        }

        // Output code
        if (g_sgs) {
            uint32_t sstage;
            switch (stage) {
            case EShLangVertex:
                sstage = SGS_STAGE_VERTEX;
                break;
            case EShLangFragment:
                sstage = SGS_STAGE_FRAGMENT;
                break;
            case EShLangCompute:
                sstage = SGS_STAGE_COMPUTE;
                break;
            default:
                sstage = 0;
                break;
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

            if (args.reflect) {
                // turn back location attributs for reflection
                if (!old_locs.empty()) {
                    sx_assert(old_locs.size() == ress.stage_inputs.size());
                    for (int i = 0; i < ress.stage_inputs.size(); i++) {
                        spirv_cross::Resource& res = ress.stage_inputs[i];
                        spirv_cross::Bitset mask = compiler->get_decoration_bitset(res.id);
                        if (old_locs[i] != -1) {
                            sx_assert(mask.get(spv::DecorationLocation));
                            old_locs.push_back(compiler->get_decoration(res.id, spv::DecorationLocation));
                            compiler->set_decoration(res.id, spv::DecorationLocation, old_locs[i]);
                        }
                    }
                }

                sx_mem_block* mem = nullptr;
                output_reflection_bin(args, *compiler, ress, args.out_filepath, stage, &mem);
                sgs_add_stage_reflect(g_sgs, sstage, mem->data, mem->size);
                sx_mem_destroy_block(mem);
            }
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
                // turn back location attributs for reflection
                if (!old_locs.empty()) {
                    sx_assert(old_locs.size() == ress.stage_inputs.size());
                    for (int i = 0; i < ress.stage_inputs.size(); i++) {
                        spirv_cross::Resource& res = ress.stage_inputs[i];
                        spirv_cross::Bitset mask = compiler->get_decoration_bitset(res.id);
                        if (old_locs[i] != -1) {
                            sx_assert(mask.get(spv::DecorationLocation));
                            old_locs.push_back(compiler->get_decoration(res.id, spv::DecorationLocation));
                            compiler->set_decoration(res.id, spv::DecorationLocation, old_locs[i]);
                        }
                    }
                }

                // output json reflection file
                // if --reflect is defined, we just output to that file
                // if --reflect is not defined, check cvar (.C file), and if set, output to the same file (out_filepath)
                // if --reflect is not defined and there is no cvar, output to out_filepath.json
                std::string json_str;
                output_reflection_json(args, *compiler, ress, filepath.c_str(), stage, &json_str, cvar_code.empty());

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

        if (!args.silent)
            puts(filename); // SUCCESS
        return 0;
    } catch (const std::exception& e) {
        printf("SPIRV-cross: %s\n", e.what());
        return -1;
    }
}

struct compile_file_desc {
    EShLanguage stage;
    const char* filename;
    uint32_t offset;
    uint32_t size;
};

#define compile_files_ret(_code)   \
    destroy_shaders(shaders);      \
    sx_array_free(g_alloc, files); \
    prog->~TProgram();             \
    sx_free(g_alloc, prog);        \
    glslang::FinalizeProcess();    \
    return _code;

struct output_parse_result {
    std::string file;
    std::string err;
    int line;
};

static bool parse_output_log_detect_line(const char** str)
{
    const char* err_header = "ERROR: ";
    const char* warn_header = "WARNING: ";

    if (sx_strstr(*str, err_header) == *str) {
        *str = *str + sx_strlen(err_header);
        return true;
    } else if (sx_strstr(*str, warn_header) == *str) {
        *str = *str + sx_strlen(warn_header);
        return true;
    } else {
        return false;
    }
}

static bool parse_output_log(const char* str, std::vector<output_parse_result>* r)
{
    while (parse_output_log_detect_line(&str)) {
        const char* divider = sx_strchar(str, ':');
        if (!divider)
            return r->size() > 0;
        output_parse_result lr;

        if (SX_PLATFORM_WINDOWS && divider == str + 1)
            divider = sx_strchar(divider + 1, ':');
        // sx_strncpy(file, file_sz, str, (intptr_t)(divider - str));
        lr.file.assign(str, divider);
        lr.line = sx_toint(divider + 1);
        const char* next_divider = sx_strchar(divider + 1, ':');
        sx_assert(next_divider);
        // sx_strcpy(desc, desc_sz, next_divider + 1);
        const char* line_sep = sx_strchar(next_divider + 1, '\n');
        std::string err;
        if (line_sep) {
            lr.err.assign(next_divider + 1, line_sep);
            str = line_sep + 1;
        } else {
            lr.err.assign(next_divider + 1);
            str += err.length();
        }

        r->push_back(lr);
    }
    return true;
}

static void output_error(const char* err_str, const cmd_args& args, const char* filename, int start_line = 0)
{
    if (err_str && err_str[0]) {
        std::vector<output_parse_result> lines;
        parse_output_log(err_str, &lines);
        if (args.err_format == OUTPUT_ERRORFORMAT_GLSLANG) {
            fprintf(stdout, "%s\n", filename);
            for (std::vector<output_parse_result>::iterator il = lines.begin();
                 il != lines.end(); ++il) {
                fprintf(stdout, "ERROR: 0:%d:%s\n", il->line + start_line, il->err.c_str());
            }
        } else if (args.err_format == OUTPUT_ERRORFORMAT_MSVC) {
            for (std::vector<output_parse_result>::iterator il = lines.begin();
                 il != lines.end(); ++il) {
                char fullpath[256];
                sx_os_path_abspath(fullpath, sizeof(fullpath), il->file.c_str());
                fprintf(stderr, "%s(%d,0): error:%s\n", fullpath, il->line + start_line, il->err.c_str());
            }
        } else if (args.err_format == OUTPUT_ERRORFORMAT_GCC) {
            for (std::vector<output_parse_result>::iterator il = lines.begin();
                 il != lines.end(); ++il) {
                char fullpath[256];
                sx_os_path_abspath(fullpath, sizeof(fullpath), il->file.c_str());
                fprintf(stderr, "%s:%d:0: error:%s\n", fullpath, il->line + start_line, il->err.c_str());
            }
        }
    }
}



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

    auto find_end_block = [](const char* text)->const char* {
        const char* end_block = sx_strstr(text, "//@end");
        if (end_block) {
            if (*(end_block - 1) == '\n') {
                if (!end_block[6] || sx_isspace(end_block[6])) {
                    return end_block;
                }
            }
            return nullptr;
        } else {
            return nullptr;
        }
    };

    auto calculate_start_line = [](const char* source, int end_offset)->int
    {
        int count = 0;
        const char* start = source;
        while (1) {
            source = sx_strchar(source, '\n');
            if (source && uintptr_t(source - start) < end_offset) {
                count++;
                ++source;
            } else {
                break;
            }
        }
        return count;
    };

    glslang::InitializeProcess();

    // Gather files for compilation
    compile_file_desc* files = nullptr;

    if ((args.vs_filepath || args.fs_filepath) && args.vs_filepath == args.fs_filepath) {
        char ext[32];
        sx_os_path_ext(ext, sizeof(ext), args.vs_filepath);
        sx_assert(sx_strequalnocase(ext, ".glsl"));

        // open the file and check for special tags
        sx_mem_block* mem = sx_file_load_text(g_alloc, args.vs_filepath);
        if (!mem) {
            printf("opening file '%s' failed\n", args.vs_filepath);
            return -1;
        }

        const char* text = (const char*)mem->data;
        text = sx_skip_whitespace(text);

        while (*text) {
            if (sx_strnequal(text, "//@begin_", 9)) {
                text += 9;
                if (sx_strnequal(text, "vert", 4) && (text[4]=='\n' || (text[4]=='\r'&&text[5]=='\n'))) {
                    text += (text[4]=='\r'&&text[5]=='\n') ? 6 : 5;
                    const char* end_block = find_end_block(text);
                    if (!end_block) {
                        printf("no matching //@end found with //@begin: %s\n", args.vs_filepath);
                        return -1;
                    }

                    compile_file_desc d = {
                        EShLangVertex, 
                        args.vs_filepath, 
                        static_cast<uint32_t>(text - (const char*)mem->data),
                        static_cast<uint32_t>(end_block - text)
                    };
                    sx_array_push(g_alloc, files, d);

                    text = end_block + 6;
                } else if (sx_strnequal(text, "frag", 4) && (text[4]=='\n' || (text[4]=='\r'&&text[5]=='\n'))) {
                    text += (text[4]=='\r'&&text[5]=='\n') ? 6 : 5;
                    const char* end_block = find_end_block(text);
                    if (!end_block) {
                        printf("no matching //@end found with //@begin: %s\n", args.fs_filepath);
                        return -1;
                    }
                    compile_file_desc d = {
                        EShLangFragment,
                        args.vs_filepath,
                        static_cast<uint32_t>(text - (const char*)mem->data),
                        static_cast<uint32_t>(end_block - text)
                    };
                    sx_array_push(g_alloc, files, d);

                    text = end_block + 6;
                } else {
                    printf("invalid @begin tag in '%s'\n", args.vs_filepath);
                }
            } 
            const char* next_text = sx_skip_whitespace(text);
            if (next_text == text) {
                break;
            }
            text = next_text;
        } // while(text)

        sx_mem_destroy_block(mem);

        // the offsets should not have any conflict with each other
        for (int i = 0; i < sx_array_count(files) - 1; i++) {
            if (files[i].offset + files[i].size > files[i+1].offset) {
                printf("invalid @begin @end shader blocks: %s\n", args.vs_filepath);
                return -1;
            }
        }
    } else {
        if (args.vs_filepath) {
            compile_file_desc d = { EShLangVertex, args.vs_filepath };
            sx_array_push(g_alloc, files, d);
        }

        if (args.fs_filepath) {
            compile_file_desc d = { EShLangFragment, args.fs_filepath };
            sx_array_push(g_alloc, files, d);
        }

        if (args.cs_filepath) {
            compile_file_desc d = { EShLangCompute, args.cs_filepath };
            sx_array_push(g_alloc, files, d);
        }
    }

    glslang::TProgram* prog = new (sx_malloc(g_alloc, sizeof(glslang::TProgram))) glslang::TProgram();
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

        if (args.lang == SHADER_LANG_GLES && args.profile_ver == 200) {
            def += std::string("#define flat\n");
        }

        // Read target file
        sx_mem_block* mem = sx_file_load_bin(g_alloc, files[i].filename);
        if (!mem) {
            printf("opening file '%s' failed\n", files[i].filename);
            compile_files_ret(-1);
        }

        glslang::TShader* shader = new (sx_malloc(g_alloc, sizeof(glslang::TShader))) glslang::TShader(files[i].stage);
        sx_assert(shader);
        sx_array_push(g_alloc, shaders, shader);

        char* shader_str;
        int shader_len;
        int start_line = 0;
        if (files[i].size == 0) {
            shader_str = (char*)mem->data;
            shader_len = (int)mem->size;
        } else {
            shader_str = (char*)mem->data + files[i].offset;
            shader_len = (int)files[i].size;
            start_line = calculate_start_line((const char*)mem->data, files[i].offset);

            #if 0   // TODO: remove later
            const char* version = "#version 450\n";
            int version_sz = sx_strlen(version);
            shader_str = (char*)sx_malloc(g_alloc, files[i].size + version_sz);
            sx_assert(shader_str);
            sx_memcpy(shader_str, version, version_sz);
            sx_memcpy(shader_str + version_sz, (char*)mem->data + files[i].offset, files[i].size);
            shader_len = files[i].size + version_sz;

            #endif
        }
        shader->setStringsWithLengthsAndNames(&shader_str, &shader_len, &files[i].filename, 1);
        shader->setInvertY(args.invert_y ? true : false);
        shader->setEnvInput(glslang::EShSourceGlsl, files[i].stage, glslang::EShClientVulkan, default_version);
        shader->setEnvClient(glslang::EShClientVulkan, glslang::EShTargetVulkan_1_1);
        shader->setEnvTarget(glslang::EShTargetSpv, glslang::EShTargetSpv_1_0);
        add_defines(shader, args, def);

        std::string prep_str;
        Includer includer(args.list_includes);
        char cur_file_dir[512];
        sx_os_path_dirname(cur_file_dir, sizeof(cur_file_dir), files[i].filename);
        includer.addSystemDir(cur_file_dir);
        includer.addIncluder(args.includer);

        if (args.preprocess || args.list_includes) {
            if (shader->preprocess(&limits_conf, default_version, ENoProfile, false, false, messages, &prep_str, includer)) {
                if (args.preprocess) {
                    puts("-------------------");
                    printf("%s:\n", files[i].filename);
                    puts("-------------------");
                    puts(prep_str.c_str());
                    puts("");
                }
            } else {
                output_error(shader->getInfoLog(), args, files[i].filename, start_line);
                sx_mem_destroy_block(mem);
                compile_files_ret(-1);
            }
        } else {
            if (!shader->parse(&limits_conf, default_version, false, messages, includer)) {
                output_error(shader->getInfoLog(), args, files[i].filename, start_line);
                sx_mem_destroy_block(mem);
                compile_files_ret(-1);
            }

            if (!args.validate)
                prog->addShader(shader);
        }

        sx_mem_destroy_block(mem);
    } // foreach (file)

    // In preprocess mode, do not link, just exit
    if (args.preprocess || args.validate || args.list_includes) {
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

static void detect_input_file(cmd_args* args, const char* file)
{
    char ext[32];
    sx_os_path_ext(ext, sizeof(ext), file);
    if (sx_strequalnocase(ext, ".vert")) {
        args->vs_filepath = file;
    } else if (sx_strequalnocase(ext, ".frag")) {
        args->fs_filepath = file;
    } else if (sx_strequalnocase(ext, ".comp")) {
        args->cs_filepath = file;
    } else if (sx_strequalnocase(ext, ".glsl")) {
        // take a wild guess and put it in both args
        // later, we will look for special tags inside the file to extract the source for each stage
        args->vs_filepath = file;
        args->fs_filepath = file;
    }
}

int main(int argc, char* argv[])
{
    cmd_args args = {};
    args.lang = SHADER_LANG_COUNT;
    args.err_format = SX_PLATFORM_WINDOWS ? OUTPUT_ERRORFORMAT_MSVC : OUTPUT_ERRORFORMAT_GCC;

    int version = 0;
    int dump_conf = 0;

    const sx_cmdline_opt opts[] = {
        { "help", 'h', SX_CMDLINE_OPTYPE_NO_ARG, 0x0, 'h', "Print this help text", 0x0 },
        { "version", 'V', SX_CMDLINE_OPTYPE_FLAG_SET, &version, 1, "Print version", 0x0 },
        { "vert", 'v', SX_CMDLINE_OPTYPE_REQUIRED, 0x0, 'v', "Vertex shader source file", "Filepath" },
        { "frag", 'f', SX_CMDLINE_OPTYPE_REQUIRED, 0x0, 'f', "Fragment shader source file", "Filepath" },
        { "compute", 'c', SX_CMDLINE_OPTYPE_REQUIRED, 0x0, 'c', "Compute shader source file", "Filepath" },
        { "output", 'o', SX_CMDLINE_OPTYPE_REQUIRED, 0x0, 'o', "Output file", "Filepath" },
        { "lang", 'l', SX_CMDLINE_OPTYPE_REQUIRED, 0x0, 'l', "Convert to shader language", "gles/msl/hlsl/glsl" },
        { "defines", 'D', SX_CMDLINE_OPTYPE_OPTIONAL, 0x0, 'D', "Preprocessor definitions, seperated by comma or ';'", "Defines" },
        { "invert-y", 'Y', SX_CMDLINE_OPTYPE_FLAG_SET, &args.invert_y, 1, "Invert position.y in vertex shader", 0x0 },
        { "profile", 'p', SX_CMDLINE_OPTYPE_REQUIRED, 0x0, 'p', "Shader profile version (HLSL: 40, 50, 60), (ES: 200, 300), (GLSL: 330, 400, 420)", "ProfileVersion" },
        { "dumpc", 'C', SX_CMDLINE_OPTYPE_FLAG_SET, &dump_conf, 1, "Dump shader limits configuration", 0x0 },
        { "include-dirs", 'I', SX_CMDLINE_OPTYPE_REQUIRED, 0x0, 'I', "Set include directory for <system> files, seperated by ';'", "Directory(s)" },
        { "preprocess", 'P', SX_CMDLINE_OPTYPE_FLAG_SET, &args.preprocess, 1, "Dump preprocessed result to terminal" },
        { "cvar", 'N', SX_CMDLINE_OPTYPE_REQUIRED, 0x0, 'N', "Outputs Hex data to a C include file with a variable name", "VariableName" },
        { "flatten-ubos", 'F', SX_CMDLINE_OPTYPE_FLAG_SET, &args.flatten_ubos, 1, "Flatten UBOs, useful for ES2 shaders", 0x0 },
        { "reflect", 'r', SX_CMDLINE_OPTYPE_OPTIONAL, 0x0, 'r', "Output shader reflection information to a json file", "Filepath" },
        { "sgs", 'G', SX_CMDLINE_OPTYPE_FLAG_SET, &args.sgs_file, 1, "Output file should be packed SGS format", "Filepath" },
        { "bin", 'b', SX_CMDLINE_OPTYPE_FLAG_SET, &args.compile_bin, 1, "Compile to bytecode instead of source. requires ENABLE_D3D11_COMPILER build flag", 0x0 },
        { "debug", 'g', SX_CMDLINE_OPTYPE_FLAG_SET, &args.debug_bin, 1, "Generate debug info for binary compilation, should come with --bin", 0x0 },
        { "optimize", 'O', SX_CMDLINE_OPTYPE_FLAG_SET, &args.optimize, 1, "Optimize shader for release compilation", 0x0 },
        { "silent", 'S', SX_CMDLINE_OPTYPE_FLAG_SET, &args.silent, 1, "Does not output filename(s) after compile success" },
        { "input", 'i', SX_CMDLINE_OPTYPE_REQUIRED, 0x0, 'i', "Input shader source file. determined by extension (.vert/.frag/.comp)", 0x0 },
        { "validate", '0', SX_CMDLINE_OPTYPE_FLAG_SET, &args.validate, 1, "Only performs shader validatation and error checking", 0x0 },
        { "err-format", 'E', SX_CMDLINE_OPTYPE_REQUIRED, 0x0, 'E', "Output error format", "glslang/msvc" },
        { "list-includes", 'L', SX_CMDLINE_OPTYPE_FLAG_SET, &args.list_includes, 1, "List include files in shaders, does not generate any output files", 0x0},
        SX_CMDLINE_OPT_END
    };
    sx_cmdline_context* cmdline = sx_cmdline_create_context(g_alloc, argc, (const char**)argv, opts);

    // always include the 

    int opt;
    const char* arg;
    while ((opt = sx_cmdline_next(cmdline, NULL, &arg)) != -1) {
        switch (opt) {
        case '+':
            detect_input_file(&args, arg);
            break;
        case '?':
            printf("Unknown argument: %s\n", arg);
            exit(-1);
            break;
        case '!':
            printf("Invalid use of argument: %s\n", arg);
            exit(-1);
            break;
        case 'v':
            args.vs_filepath = arg;
            break;
        case 'f':
            args.fs_filepath = arg;
            break;
        case 'c':
            args.cs_filepath = arg;
            break;
        case 'o':
            args.out_filepath = arg;
            break;
        case 'D':
            parse_defines(&args, arg);
            break;
        case 'l':
            args.lang = parse_shader_lang(arg);
            break;
        case 'h':
            print_help(cmdline);
            break;
        case 'p':
            args.profile_ver = sx_toint(arg);
            break;
        case 'I':
            parse_includes(&args, arg);
            break;
        case 'N':
            args.cvar = arg;
            break;
        case 'r':
            args.reflect_filepath = arg;
            args.reflect = 1;
            break;
        case 'i':
            detect_input_file(&args, arg);
            break;
        case 'E':
            args.err_format = parse_output_errorformat(arg);
            break;
        default:
            break;
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

    if ((args.vs_filepath && !sx_os_path_isfile(args.vs_filepath)) || (args.fs_filepath && !sx_os_path_isfile(args.fs_filepath)) || (args.cs_filepath && !sx_os_path_isfile(args.cs_filepath))) {
        puts("Input files are invalid");
        exit(-1);
    }

    if (!args.vs_filepath && !args.fs_filepath && !args.cs_filepath) {
        puts("You must at least define one input shader file");
        exit(-1);
    }

    if (args.cs_filepath && (args.vs_filepath || args.fs_filepath)) {
        puts("Cannot link compute-shader with either fragment shader or vertex shader");
        exit(-1);
    }

    if (args.out_filepath == nullptr && !(args.preprocess | args.validate | args.list_includes)) {
        puts("Output file is not specified");
        exit(-1);
    }

    if (args.lang == SHADER_LANG_COUNT && !(args.preprocess | args.validate | args.list_includes)) {
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
            args.profile_ver = 50; // D3D11
        else if (args.lang == SHADER_LANG_GLSL)
            args.profile_ver = 330;
    }

#if SX_PLATFORM_WINDOWS
    if (args.compile_bin && (args.lang != SHADER_LANG_HLSL || args.profile_ver >= 60)) {
        puts("ignoring --bin flag, byte-code compilation not implemented for this target");
        args.compile_bin = 0;
    }
#ifndef D3D11_COMPILER
    // Windows + HLSL -> works but requires ENABLE_D3D11_COMPILER
    else if (args.compile_bin) {
        puts("Cannot compile to byte-code, glslcc is not built with ENABLE_D3D11_COMPILER flag");
        exit(-1);
    }
#endif
#else
    if (args.compile_bin) {
        puts("Ignoring --bin flag, byte-code compilation not implemented for this target");
        args.compile_bin = 0;
    }
#endif

    if (args.sgs_file && !(args.preprocess | args.validate | args.list_includes)) {
        uint32_t slang = 0;
        switch (args.lang) {
        case SHADER_LANG_GLES:
            slang = SGS_LANG_GLES;
            break;
        case SHADER_LANG_HLSL:
            slang = SGS_LANG_HLSL;
            break;
        case SHADER_LANG_MSL:
            slang = SGS_LANG_MSL;
            break;
        case SHADER_LANG_GLSL:
            slang = SGS_LANG_GLSL;
            break;
        default:
            sx_assert(0);
            break;
        }
        g_sgs = sgs_create_file(g_alloc, args.out_filepath, slang, args.profile_ver);
        sx_assert(g_sgs);
    }

    int r = compile_files(args, k_default_conf);

    if (g_sgs) {
        if (r == 0 && !sgs_commit(g_sgs)) {
            printf("Writing SGS file '%s' failed\n", args.out_filepath);
        }
        sgs_destroy_file(g_sgs);
    }

    sx_cmdline_destroy_context(cmdline, g_alloc);
    cleanup_args(&args);
    return r;
}
