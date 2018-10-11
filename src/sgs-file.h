//
// Copyright 2018 Sepehr Taghdisian (septag@github). All rights reserved.
// License: https://github.com/septag/glslcc#license-bsd-2-clause
//

//
// File version: 1.0.0
//
#pragma once

#include "sx/allocator.h"

#pragma pack(push, 1)

#define SGS_FILE_SIG        0x53475331  // "SGS1"
#define SGS_FILE_VERSION    100

enum sgs_shader_lang
{
    SGS_SHADER_GLES = 1,
    SGS_SHADER_HLSL,
    SGS_SHADER_MSL,
    SGS_SHADER_GLSL
};

enum sgs_shader_stage
{
    SGS_STAGE_VERTEX = 0,
    SGS_STAGE_FRAGMENT,
    SGS_STAGE_COMPUTE,
    SGS_STAGE_COUNT 
};

struct sgs_file_stage
{
    int stage;                      // sgs_shader_stage
    int code_size;                  // code size in bytes
    int code_offset;                // begining of the code data
    int reflect_size;               // reflect size in bytes
    int reflect_offset;             // begining of the reflection data
};

struct sgs_file_header
{
    uint32_t        sig;            // SGS_FILE_SIG
    int             version;        // SGS_FILE_VERSION
    int             lang;           // sgs_shader_lang
    int             profile_ver;    // shader profile version
    int             num_stages;     // number of stages (fs + vs + cs ..)
    //                              // sgs_file_stage* (num_stages)
};

#pragma pack(pop)

struct sgs_file;

sgs_file* sgs_create_file(const sx_alloc* alloc, const char* filepath, sgs_shader_lang lang, int profile_ver);
void      sgs_destroy_file(sgs_file* f);
void      sgs_add_stage_code(sgs_file* f, sgs_shader_stage stage, const char* code);
void      sgs_add_stage_code_bin(sgs_file* f, sgs_shader_stage stage, const void* bytecode, int len);
void      sgs_add_stage_reflect(sgs_file* f, sgs_shader_stage stage, const char* reflect);
bool      sgs_commit(sgs_file* f);
