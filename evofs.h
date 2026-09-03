/* EvoFS core - Evolution Studios .ndx / .dat / .rpk (DriveClub PS4, v4300) */
#ifndef EVOFS_H
#define EVOFS_H

#define _FILE_OFFSET_BITS 64
#include <stdint.h>
#include <stdio.h>
#include <stdarg.h>

#ifdef _WIN32
  #define FSEEK _fseeki64
  #define FTELL _ftelli64
  #include <direct.h>
  #define MKDIR(p) _mkdir(p)
#else
  #define FSEEK fseeko
  #define FTELL ftello
  #include <sys/stat.h>
  #define MKDIR(p) mkdir((p), 0777)
#endif

#define EVO_BUFFER_SIZE 0x10000
#define EVO_UNKNOWN 0
#define EVO_DAT 1
#define EVO_NDX 2
#define EVO_RPK 3

const char *evo_error(void);
int evo_fail(const char *fmt, ...);
int evo_sniff(const char *path);
void evo_log(const char *fmt, ...);
void evo_set_log(void (*fn)(const char *));
void evo_set_progress(void (*fn)(uint64_t done, uint64_t total));
void evo_progress_begin(uint64_t total);
void evo_progress_add(uint64_t bytes);
void evo_progress_end(void);
void evo_progress_reset(void);
void evo_force_index_bits(int bits);

int cmd_info(const char *path);
int cmd_list(const char *path, const char *needle);
int cmd_check_packs(const char *root);
int cmd_verify(const char *ndx_path, const char *dat_dir);
int cmd_extract(const char *ndx, const char *datdir, const char *needle, const char *out);
int cmd_extract_list(const char *ndx_path, const char *dat_dir, char **names,
                     int nname, const char *out_dir);
int cmd_unpack(const char *path, const char *out_dir);
int cmd_repack(const char *pack, const char *resdir, const char *out);
int cmd_build(const char *ndx, const char *datdir, const char *moddir, const char *out);
int selftest(const char *ndx_path, const char *dat_dir);
int cmd_compare(const char *a_path, const char *b_path);
int cmd_newpack(const char *res_dir, const char *out_path, const char *root_arg);
int cmd_unlock(const char *csv_path);
uint8_t *rpk_add_buffer(const uint8_t *buf, size_t len, uint64_t like,
                        uint32_t count, uint64_t *new_uid, size_t *out_len);
int cmd_clone_resource(const char *pack_path, const char *src_name,
                       const char *new_name, const char *out_path);
int pack_is_a_car(const char *game_dir, const char *name);
int cmd_clonecar(const char *game_dir, const char *car, const char *newname,
                 const char *out_dir);
extern int CLONE_NO_REMAP;
extern char g_vdef_name[256];
extern char CLONE_SEED_DIR[1024];
void clone_map_reset(void);
int clone_check_name(const char *game_dir, const char *car, const char *newname,
                     char *why, size_t cap);
void make_code(const char *name, size_t budget, char *out, size_t cap);
int csv_source_field(const char *path, const char *src_asset, const char *column,
                     char *out, size_t cap);
int rpk_rename_any(const uint8_t *buf, size_t len, const char *from, const char *to,
                   uint8_t **out_buf, size_t *out_len);
int cmd_meshimport(const char *pack_path, const char *part, const char *obj_path,
                   const char *out_path);
typedef struct { float *v, *vt, *vn; uint32_t *idx; int nv, ni, cv, ci; } Model;
int model_load(const char *path, Model *m);
void model_free(Model *m);

typedef struct { float *v, *vt, *vn; int nv, nvt, nvn, cv, cvt, cvn; int *f; int nf, cf; } Obj;
int evofs_put_part(uint8_t **bufp, size_t *lenp, const char *part, Obj *o);
void evofs_set_local(int on);
int evo_find_file(const char *root, const char *want, char *out, size_t cap);
int evofs_part_box(const uint8_t *buf, size_t len, const char *part, float *size, float *centre);
int evofs_part_size(const uint8_t *buf, size_t len, const char *part, float *size);
int evofs_part_headroom(const uint8_t *buf, size_t len, const char *part);
int cmd_checkcar(const char *pack_path);
float half_to_float(uint16_t h);
int cmd_reshape(const char *model_root, const char *pack_path, const char *out_path, int lod);
int cmd_wholecar(const char *model_root, const char *pack_path, const char *out_path, int lod);
int cmd_buildcar(const char *model_root, const char *pack_path, const char *out_path, int lod);

int cmd_mesh(const char *pack_path, const char *out_dir);
int cmd_references(const char *root, const char *needle);
int cmd_renamepack(const char *pack_path, const char *oldname, const char *newname,
                   const char *out_path);
int cmd_clonecar_audio(const char *game_dir, const char *car, const char *newname,
                       const char *out_dir, char *aud_out, char *ai_out, size_t cap);
int csv_code_taken(const char *path, const char *code);
int csv_source_code(const char *path, const char *src_asset, char *out, size_t cap);
int clone_map_collect(const uint8_t *buf, size_t len, uint64_t salt);
int cmd_clonecar_code_files(const char *game_dir, const char *oldcode,
                            const char *newcode, const char *out_dir);
int cmd_clonecar_files(const char *game_dir, const char *car, const char *newname,
                       const char *out_dir);
int cmd_clonecar_register(const char *game_dir, const char *car, const char *newname,
                          const char *out_dir);
void evo_clear_archive_dirs(void);
void evo_add_archive_dir(const char *dir);
extern int TEX_OVERWRITE;
int csv_field(const char *hdr, const char *want);
int csv_span(const char *line, int col, size_t *at, size_t *len);
int cmd_clone_merge(const char *clone_root, const char *out_dir);
int cmd_clone_check(const char *dir);
int registry_drop_car(const char *dir, const char *asset, const char *code);
int cmd_clone_check_report(const char *dir, char *problems, size_t cap);
int cmd_clone_check_list(const char *dir, char *problems, size_t cap,
                         char (*bad)[128], char (*badcode)[64], int *nbad);
uint8_t *slurp_pub(const char *path, size_t *len);
extern int EXTRACT_PACK_FOLDERS;
int cmd_sound_export(const char *res_dir, const char *out_dir);
int cmd_sound_import(const char *res_dir, const char *in_dir);
uint8_t *fsb5_from_parts(const uint8_t *src, size_t slen, uint8_t **blob,
                         size_t *blen, size_t *out_len);
int cmd_sound(const char *audio_path, const char *res_dir);
int cmd_depthdynamic(const char *pack_path, const char *out_path);
int cmd_depthfix(const char *pack_path, int width, int height, const char *out_path);
int cmd_placeholder_textures(const char *pack_path, const char *out_path);
int cmd_textures(const char *res_root, const char *out_dir);
int cmd_textures_aux(const char *res_root, const char *aux_root, const char *out_dir);
int cmd_textures_import(const char *res_root, const char *dds_dir);
int cmd_textures_import_aux(const char *res_root, const char *aux_root,
                            const char *dds_dir);
void evo_mkpath(const char *path);
int _stricmp_(const char *a, const char *b);
int _strnicmp_(const char *a, const char *b, size_t n);
const char *evo_basename(const char *p);
int bc_decode(int dxgi, const uint8_t *src, uint32_t w, uint32_t h, uint8_t *rgba);
int jpeg_write(const char *path, const uint8_t *rgba, uint32_t w, uint32_t h, int q);

void evo_md5(const void *data, size_t len, uint8_t out[16]);
typedef struct { uint32_t a,b,c,d; uint64_t len; uint8_t buf[64]; size_t n; } MD5;
void md5_init(MD5 *m);
void md5_update(MD5 *m, const void *data, size_t len);
void md5_final(MD5 *m, uint8_t out[16]);

int lz4_decompress(const uint8_t *src, size_t slen, uint8_t *dst, size_t dlen,
                   size_t *consumed);
size_t lz4_compress(const uint8_t *src, size_t slen, uint8_t *out, int32_t *table);
#define LZ4_TABLE_BYTES (65536 * (int)sizeof(int32_t))

typedef struct {
    FILE *f;
    char path[512];
    uint32_t version, chunk_count, buffer_size;
    uint64_t filetime, toc_offset, data_start, logical_size;
    uint32_t *chunk_sizes;
    uint64_t *chunk_at;
    uint8_t *buf, *tmp;
    int64_t cached;
} DatReader;

int dat_open(DatReader *r, const char *path);
void dat_close(DatReader *r);
const uint8_t *dat_chunk(DatReader *r, uint32_t i);
int dat_read(DatReader *r, uint64_t off, uint64_t len, uint8_t *dst);

enum { SEG_DAT, SEG_FILE, SEG_MEM };
typedef struct {
    int kind;
    uint64_t size;
    DatReader *src;      /* SEG_DAT */
    uint64_t offset;     /* SEG_DAT */
    const char *path;    /* SEG_FILE */
    const uint8_t *mem;  /* SEG_MEM */
} Segment;

int dat_write(const char *path, const Segment *segs, size_t nseg, uint64_t filetime,
              uint32_t version, uint32_t bs, uint64_t toc_offset,
              void (*progress)(uint64_t, uint64_t, void *), void *ctx);

typedef struct {
    uint32_t dat;
    uint64_t offset;
    int32_t size;
    uint32_t hash;
    uint8_t md5[16];
    char *name;
    size_t rec;
} Entry;

typedef struct {
    uint8_t *raw;
    size_t raw_size, entries_at;
    char path[512];
    uint32_t version, count, dat_count, buffer_size, hash_a, hash_b;
    int32_t compression;
    int entry_size, index_bits, force_bits, layout_contiguous;
    uint64_t filetime, total_data_size;
    Entry *entries;
    char *names;
} Index;

int ndx_open(Index *x, const char *path);
void ndx_close(Index *x);
int ndx_write(const Index *x, const char *path);

typedef struct { const char *name; uint32_t dat; uint64_t offset;
                 int32_t size; uint8_t md5[16]; } NewFile;
int ndx_rebuild(const Index *x, const NewFile *add, int nadd, uint32_t dat_count,
                const char *out_path);
int cmd_add(const char *ndx_path, const char *dat_dir, const char *files_root,
            const char *out_dir);
int ndx_verify_hashes(const Index *x);
int ndx_fit_archives(const Index *x, const char *dat_dir, int *checked);
int ndx_open_checked(Index *x, const char *ndx_path, const char *dat_dir);
uint32_t evo_name_hash(const char *name, uint32_t ha, uint32_t hb);

typedef struct {
    uint64_t uid;
    uint32_t size, offset, name_off;
    size_t rec;
} ResInfo;

typedef struct {
    uint32_t toc_size, toc_offset, version, count, alias_count, dependency_count;
    uint32_t unk3_count, required_count, data_start;
    uint64_t info_uid, root_uid, size;
    size_t name_pool, name_pool_size, asset_pool, asset_pool_size;
    size_t rec_start, rec_end;      /* byte range of the ResourceInfo array */
    ResInfo *res;
} Pack;

int evofs_put_part_slab(uint8_t *buf, size_t len, Pack *pk, const void *slab, Obj *o);
int cmd_newcar(const char *obj_path, const char *donor_path, const char *out_path);
int cmd_cutmodel(const char *model_root, const char *out_path, int lod);
int cmd_objcar(const char *obj_path, const char *pack_path, const char *out_path);
int cmd_cartemplate(const char *pack_path, const char *out_dir);
int cmd_partcar(const char *model_root, const char *pack_path, const char *out_path, int lod);
int cmd_fitcar(const char *model_root, const char *pack_path, const char *out_path, int lod);

typedef struct { float m[16]; int parent; char name[160]; } Node;
Node *hier_load(const Pack *pk, const uint8_t *buf, size_t len, int *out_n);

int rpk_parse(Pack *pk, const uint8_t *buf, size_t len);
void rpk_free(Pack *pk);
const char *rpk_name(const Pack *pk, const uint8_t *buf, uint32_t i, char *tmp);
int rpk_check_layout(const Pack *pk);
int evo_find_packs(const uint8_t *buf, size_t len, size_t *out, int max);
int dat_scan_packs(DatReader *r, uint64_t *out, int max);
int rpk_open_at(DatReader *r, uint64_t base, Pack *pk, uint8_t **toc);
const char *evo_type_name(uint32_t type_id);

#endif
