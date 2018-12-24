//
// Copyright 2018 Sepehr Taghdisian (septag@github). All rights reserved.
// License: https://github.com/septag/glslcc#license-bsd-2-clause
//

#include "sgs-file.h"

#include "sx/io.h"
#include "sx/array.h"
#include "sx/os.h"
#include "sx/string.h"

#include <string>

struct sgs_file
{
    const sx_alloc* alloc               = nullptr;
    std::string     filepath            = {};
    sgs_file_header hdr                 = {};
    sgs_file_stage* stages              = nullptr;  
    int             reflect_block_size  = 0;
    int             code_block_size     = 0;
    char*           reflect_block       = nullptr;
    char*           code_block          = nullptr;
};

sgs_file* sgs_create_file(const sx_alloc* alloc, const char* filepath, sgs_shader_lang lang, int profile_ver)
{
    sgs_file* sgs = new (sx_malloc(alloc, sizeof(sgs_file))) sgs_file;
    sgs->alloc = alloc;
    sgs->filepath = filepath;

    sgs->hdr.sig = SGS_FILE_SIG;
    sgs->hdr.version = SGS_FILE_VERSION;
    sgs->hdr.lang = lang;
    sgs->hdr.profile_ver = profile_ver;
    
    return sgs;
}

void sgs_destroy_file(sgs_file* f)
{
    sx_assert(f);
    f->~sgs_file();
    sx_free(f->alloc, f);
}

void sgs_add_stage_code(sgs_file* f, sgs_shader_stage stage, const char* code)
{
    sgs_file_stage* s = nullptr;
    // search in stages and see if find it
    for (int i = 0; i < sx_array_count(f->stages); i++) {
        if (f->stages[i].stage == (int)stage) {
            s = &f->stages[i];
            break;
        }
    }

    if (!s) {
        s = sx_array_add(f->alloc, f->stages, 1);
        sx_memset(s, 0x0, sizeof(sgs_file_stage));
        s->stage = stage;
    }

    int len = sx_strlen(code) + 1;
    f->code_block = (char*)sx_realloc(f->alloc, f->code_block, f->code_block_size + len);
    s->code_offset = f->code_block_size;
    s->code_size = len;
    
    sx_memcpy(f->code_block + s->code_offset, code, len);
    f->code_block_size += len;
}

void sgs_add_stage_code_bin(sgs_file* f, sgs_shader_stage stage, const void* bytecode, int len)
{
    sx_assert(len > 0);

    sgs_file_stage* s = nullptr;
    // search in stages and see if find it
    for (int i = 0; i < sx_array_count(f->stages); i++) {
        if (f->stages[i].stage == (int)stage) {
            s = &f->stages[i];
            break;
        }
    }

    if (!s) {
        s = sx_array_add(f->alloc, f->stages, 1);
        sx_memset(s, 0x0, sizeof(sgs_file_stage));
        s->stage = stage;
    }

    f->code_block = (char*)sx_realloc(f->alloc, f->code_block, f->code_block_size + len);
    s->code_offset = f->code_block_size;
    s->code_size = len;
    
    sx_memcpy(f->code_block + s->code_offset, bytecode, len);
    f->code_block_size += len;
}

void sgs_add_stage_reflect(sgs_file* f, sgs_shader_stage stage, const char* reflect)
{
    sgs_file_stage* s = nullptr;
    // search in stages and see if find it
    for (int i = 0; i < sx_array_count(f->stages); i++) {
        if (f->stages[i].stage == (int)stage) {
            s = &f->stages[i];
            break;
        }
    }

    if (!s) {
        s = sx_array_add(f->alloc, f->stages, 1);
        sx_memset(s, 0x0, sizeof(sgs_file_stage));
        s->stage = stage;
        ++f->hdr.num_stages;
    }

    int len = sx_strlen(reflect) + 1;
    f->reflect_block = (char*)sx_realloc(f->alloc, f->reflect_block, f->reflect_block_size + len);
    s->reflect_offset = f->reflect_block_size;
    s->reflect_size = len;
    
    sx_memcpy(f->reflect_block + s->reflect_offset, reflect, len);
    f->reflect_block_size += len;
}

bool sgs_commit(sgs_file* f)
{
    sx_file_writer writer;
    if (!sx_file_open_writer(&writer, f->filepath.c_str(), 0))
        return false;

    int reflect_start_offset = sizeof(sgs_file_header) + sizeof(sgs_file_stage)*sx_array_count(f->stages);
    int data_start_offset = reflect_start_offset + f->reflect_block_size;
    f->hdr.num_stages = sx_array_count(f->stages);
    
    sx_file_write(&writer, &f->hdr, sizeof(sgs_file_header));
    if (f->stages) {
        for (int i = 0; i < sx_array_count(f->stages); i++) {
            // Fix the offsets to absolute position of the file
            f->stages[i].reflect_offset += reflect_start_offset;
            f->stages[i].code_offset += data_start_offset;
            sx_file_write(&writer, &f->stages[i], sizeof(sgs_file_stage));
        }
    }
    if (f->reflect_block)
        sx_file_write(&writer, f->reflect_block, f->reflect_block_size);
    if (f->code_block)
        sx_file_write(&writer, f->code_block, f->code_block_size);
    sx_file_close_writer(&writer);

    return true;
}

