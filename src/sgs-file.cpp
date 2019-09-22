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

struct sgs_stage {
    uint32_t    stage;
    union {
        char*   code;
        void*   data;
    };  
    uint32_t    data_size;     // =0 if it's not bytecode

    void*       refl;
    uint32_t    refl_size;

    void*       cs_refl;
    uint32_t    cs_refl_size;
};

struct sgs_file
{
    const sx_alloc* alloc               = nullptr;
    std::string     filepath            = {};
    uint32_t        lang                = 0;
    uint16_t        profile_ver         = 0;
    sgs_stage*      stages              = nullptr;
};

sgs_file* sgs_create_file(const sx_alloc* alloc, const char* filepath, uint32_t lang, uint32_t profile_ver)
{
    sgs_file* sgs = new (sx_malloc(alloc, sizeof(sgs_file))) sgs_file;
    sgs->alloc = alloc;
    sgs->filepath = filepath;
    sgs->lang = lang;
    sgs->profile_ver = profile_ver;

    return sgs;
}

void sgs_destroy_file(sgs_file* f)
{
    sx_assert(f);
    sx_array_free(f->alloc, f->stages);
    f->~sgs_file();
    sx_free(f->alloc, f);
}

void sgs_add_stage_code(sgs_file* f, uint32_t stage, const char* code)
{
    sgs_stage* s = nullptr;
    // search in stages and see if find it
    for (int i = 0; i < sx_array_count(f->stages); i++) {
        if (f->stages[i].stage == stage) {
            s = &f->stages[i];
            break;
        }
    }

    if (!s) {
        s = sx_array_add(f->alloc, f->stages, 1);
        sx_memset(s, 0x0, sizeof(sgs_stage));
        s->stage = stage;
    }

    int len = sx_strlen(code) + 1;
    sx_assert(s->code == nullptr);
    sx_assert(s->data_size == 0);

    s->code = (char*)sx_malloc(f->alloc, len);
    sx_assert(s->code);
    sx_memcpy(s->code, code, len);
}

void sgs_add_stage_code_bin(sgs_file* f, uint32_t stage, const void* bytecode, int len)
{
    sx_assert(len > 0);

    sgs_stage* s = nullptr;
    // search in stages and see if find it
    for (int i = 0; i < sx_array_count(f->stages); i++) {
        if (f->stages[i].stage == (int)stage) {
            s = &f->stages[i];
            break;
        }
    }

    if (!s) {
        s = sx_array_add(f->alloc, f->stages, 1);
        sx_memset(s, 0x0, sizeof(sgs_stage));
        s->stage = stage;
    }
    
    sx_assert(s->data == nullptr);
    sx_assert(s->data_size == 0);

    s->data = (char*)sx_malloc(f->alloc, len);
    sx_memcpy(s->data, bytecode, len);
    s->data_size = len;
}

void sgs_add_stage_reflect(sgs_file* f, uint32_t stage, const void* reflect, int refl_size)
{
    sgs_stage* s = nullptr;
    // search in stages and see if find it
    for (int i = 0; i < sx_array_count(f->stages); i++) {
        if (f->stages[i].stage == (int)stage) {
            s = &f->stages[i];
            break;
        }
    }

    if (!s) {
        s = sx_array_add(f->alloc, f->stages, 1);
        sx_memset(s, 0x0, sizeof(sgs_stage));
        s->stage = stage;
    }

    sx_assert(s->refl == nullptr);
    sx_assert(s->refl_size == 0);

    s->refl = sx_malloc(f->alloc, refl_size);
    sx_memcpy(s->refl, reflect, refl_size);
    s->refl_size = refl_size;
}

bool sgs_commit(sgs_file* f)
{
    sx_file_writer writer;
    if (!sx_file_open_writer(&writer, f->filepath.c_str(), 0))
        return false;

    // write main chunk
    const uint32_t _sgs = SGS_CHUNK;
    const uint32_t _sgs_size = 0;       // doesn't matter
    sx_file_write_var(&writer, _sgs);
    sx_file_write_var(&writer, _sgs_size);

    sgs_chunk sgs;
    sgs.lang = f->lang;
    sgs.profile_ver = f->profile_ver;
    sx_file_write_var(&writer, sgs);

    // write stages
    for (int i = 0; i < sx_array_count(f->stages); i++) {
        const sgs_stage* s = &f->stages[i];

        const uint32_t code_size = (s->data_size == 0 ? (sx_strlen(s->code)+1) : 0);
        const uint32_t data_size = s->data_size;
        sx_assert(code_size || data_size);

        const uint32_t stage_size = 
            (s->refl ? (8 + s->refl_size) : 0) +
            (8 + code_size + data_size) +
            sizeof(uint32_t);
        
        // `STAG`
        const uint32_t _stage = SGS_CHUNK_STAG;
        sx_file_write_var(&writer, _stage);
        sx_file_write_var(&writer, stage_size);
        sx_file_write_var(&writer, s->stage);

        if (code_size) {
            // `CODE`
            const uint32_t _code = SGS_CHUNK_CODE;
            const uint32_t code_size = sx_strlen(s->code) + 1;
            sx_file_write_var(&writer, _code);
            sx_file_write_var(&writer, code_size);
            sx_file_write(&writer, s->code, code_size);
        } else if (data_size) {
            // `DATA`
            const uint32_t _data = SGS_CHUNK_DATA;
            sx_file_write_var(&writer, _data);
            sx_file_write_var(&writer, s->data_size);
            sx_file_write(&writer, s->data, s->data_size);
        }

        // `REFL`
        if (s->refl) {
            const uint32_t _refl = SGS_CHUNK_REFL;
            sx_file_write_var(&writer, _refl);
            sx_file_write_var(&writer, s->refl_size);
            sx_file_write(&writer, s->refl, s->refl_size);
        }
    }

    sx_file_close_writer(&writer);

    return true;
}

