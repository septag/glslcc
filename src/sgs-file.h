//
// Copyright 2018 Sepehr Taghdisian (septag@github). All rights reserved.
// License: https://github.com/septag/glslcc#license-bsd-2-clause
//

//
// File version: 1.1.0
// File endianness: little
// 
// v1.1.0 CHANGES
//      - added num_storages_images, num_storage_buffers (CS specific) variables to sgs_chunk_refl
//
#pragma once

#include "sx/allocator.h"

#pragma pack(push, 1)

#define SGS_CHUNK           sx_makefourcc('S', 'G', 'S', ' ')
#define SGS_CHUNK_STAG      sx_makefourcc('S', 'T', 'A', 'G')
#define SGS_CHUNK_REFL      sx_makefourcc('R', 'E', 'F', 'L')
#define SGS_CHUNK_CODE      sx_makefourcc('C', 'O', 'D', 'E')
#define SGS_CHUNK_DATA      sx_makefourcc('D', 'A', 'T', 'A')

#define SGS_LANG_GLES sx_makefourcc('G', 'L', 'E', 'S')
#define SGS_LANG_HLSL sx_makefourcc('H', 'L', 'S', 'L')
#define SGS_LANG_GLSL sx_makefourcc('G', 'L', 'S', 'L')
#define SGS_LANG_MSL  sx_makefourcc('M', 'S', 'L', ' ')
#define SGS_LANG_GLES sx_makefourcc('G', 'L', 'E', 'S')

#define SGS_VERTEXFORMAT_FLOAT      sx_makefourcc('F', 'L', 'T', '1')
#define SGS_VERTEXFORMAT_FLOAT2     sx_makefourcc('F', 'L', 'T', '2')
#define SGS_VERTEXFORMAT_FLOAT3     sx_makefourcc('F', 'L', 'T', '3')
#define SGS_VERTEXFORMAT_FLOAT4     sx_makefourcc('F', 'L', 'T', '4')
#define SGS_VERTEXFORMAT_INT        sx_makefourcc('I', 'N', 'T', '1')
#define SGS_VERTEXFORMAT_INT2       sx_makefourcc('I', 'N', 'T', '2')
#define SGS_VERTEXFORMAT_INT3       sx_makefourcc('I', 'N', 'T', '3')
#define SGS_VERTEXFORMAT_INT4       sx_makefourcc('I', 'N', 'T', '4')

#define SGS_STAGE_VERTEX            sx_makefourcc('V', 'E', 'R', 'T')
#define SGS_STAGE_FRAGMENT          sx_makefourcc('F', 'R', 'A', 'G')
#define SGS_STAGE_COMPUTE           sx_makefourcc('C', 'O', 'M', 'P')

#define SGS_IMAGEDIM_1D            sx_makefourcc('1', 'D', ' ', ' ')
#define SGS_IMAGEDIM_2D            sx_makefourcc('2', 'D', ' ', ' ')
#define SGS_IMAGEDIM_3D            sx_makefourcc('3', 'D', ' ', ' ')
#define SGS_IMAGEDIM_CUBE          sx_makefourcc('C', 'U', 'B', 'E')
#define SGS_IMAGEDIM_RECT          sx_makefourcc('R', 'E', 'C', 'T')
#define SGS_IMAGEDIM_BUFFER        sx_makefourcc('B', 'U', 'F', 'F')
#define SGS_IMAGEDIM_SUBPASS       sx_makefourcc('S', 'U', 'B', 'P')

// SGS chunk
struct sgs_chunk {
    uint32_t lang;          // sgs_shader_lang
    uint32_t profile_ver;   // 
};

// REFL
struct sgs_chunk_refl {
    char     name[32];
    uint32_t num_inputs;
    uint32_t num_textures;
    uint32_t num_uniform_buffers;
    uint32_t num_storage_images;
    uint32_t num_storage_buffers;
    uint16_t flatten_ubos;
    uint16_t debug_info;

    // inputs: sgs_refl_input[num_inputs]
    // uniform-buffers: sgs_refl_uniformbuffer[num_uniform_buffers]
    // textures: sgs_refl_texture[num_textures]
    // storage_images: sgs_refl_texture[num_storage_images]
    // storage_buffers: sgs_refl_buffer[num_storage_buffers]
};

struct sgs_refl_input {
    char     name[32];
    int32_t  loc;
    char     semantic[32];
    uint32_t semantic_index;
    uint32_t format;
};

struct sgs_refl_texture {
    char     name[32];
    int32_t  binding;
    uint32_t image_dim;
    uint8_t  multisample;
    uint8_t  is_array;
};

struct sgs_refl_buffer {
    char    name[32];
    int32_t binding;
    uint32_t size_bytes;
    uint32_t array_stride;
}; 

struct sgs_refl_uniformbuffer {
    char     name[32];
    int32_t  binding;
    uint32_t size_bytes;
    uint16_t array_size;
};

#pragma pack(pop)

struct sgs_file;

sgs_file* sgs_create_file(const sx_alloc* alloc, const char* filepath, uint32_t lang, uint32_t profile_ver);
void      sgs_destroy_file(sgs_file* f);
void      sgs_add_stage_code(sgs_file* f, uint32_t stage, const char* code);
void      sgs_add_stage_code_bin(sgs_file* f, uint32_t stage, const void* bytecode, int len);
void      sgs_add_stage_reflect(sgs_file* f, uint32_t stage, const void* reflect, int reflect_size);
bool      sgs_commit(sgs_file* f);
