/* EvoFS - Evolution Studios .ndx / .dat / .rpk tool (DriveClub PS4,... */

#define _FILE_OFFSET_BITS 64
#include "evofs.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

/* ---------------------------------------------------------------- md5 */

static const uint32_t md5_k[64] = {
0xd76aa478,0xe8c7b756,0x242070db,0xc1bdceee,0xf57c0faf,0x4787c62a,0xa8304613,0xfd469501,
0x698098d8,0x8b44f7af,0xffff5bb1,0x895cd7be,0x6b901122,0xfd987193,0xa679438e,0x49b40821,
0xf61e2562,0xc040b340,0x265e5a51,0xe9b6c7aa,0xd62f105d,0x02441453,0xd8a1e681,0xe7d3fbc8,
0x21e1cde6,0xc33707d6,0xf4d50d87,0x455a14ed,0xa9e3e905,0xfcefa3f8,0x676f02d9,0x8d2a4c8a,
0xfffa3942,0x8771f681,0x6d9d6122,0xfde5380c,0xa4beea44,0x4bdecfa9,0xf6bb4b60,0xbebfbc70,
0x289b7ec6,0xeaa127fa,0xd4ef3085,0x04881d05,0xd9d4d039,0xe6db99e5,0x1fa27cf8,0xc4ac5665,
0xf4292244,0x432aff97,0xab9423a7,0xfc93a039,0x655b59c3,0x8f0ccc92,0xffeff47d,0x85845dd1,
0x6fa87e4f,0xfe2ce6e0,0xa3014314,0x4e0811a1,0xf7537e82,0xbd3af235,0x2ad7d2bb,0xeb86d391};
static const uint8_t md5_r[64] = {
7,12,17,22,7,12,17,22,7,12,17,22,7,12,17,22, 5,9,14,20,5,9,14,20,5,9,14,20,5,9,14,20,
4,11,16,23,4,11,16,23,4,11,16,23,4,11,16,23, 6,10,15,21,6,10,15,21,6,10,15,21,6,10,15,21};

static uint32_t rol(uint32_t x, int c) { return (x << c) | (x >> (32 - c)); }

static void md5_block(MD5 *m, const uint8_t *p)
{
    uint32_t w[16], a = m->a, b = m->b, c = m->c, d = m->d, f, t;
    int i;
    for (i = 0; i < 16; i++)
        w[i] = (uint32_t)p[i*4] | ((uint32_t)p[i*4+1] << 8) |
               ((uint32_t)p[i*4+2] << 16) | ((uint32_t)p[i*4+3] << 24);
    /* one loop per round type: same arithmetic, without a branch per step */
    for (i = 0; i < 16; i++) {
        f = (b & c) | (~b & d);
        t = d; d = c; c = b;
        b = b + rol(a + f + md5_k[i] + w[i], md5_r[i]);
        a = t;
    }
    for (; i < 32; i++) {
        f = (d & b) | (~d & c);
        t = d; d = c; c = b;
        b = b + rol(a + f + md5_k[i] + w[(5*i + 1) & 15], md5_r[i]);
        a = t;
    }
    for (; i < 48; i++) {
        f = b ^ c ^ d;
        t = d; d = c; c = b;
        b = b + rol(a + f + md5_k[i] + w[(3*i + 5) & 15], md5_r[i]);
        a = t;
    }
    for (; i < 64; i++) {
        f = c ^ (b | ~d);
        t = d; d = c; c = b;
        b = b + rol(a + f + md5_k[i] + w[(7*i) & 15], md5_r[i]);
        a = t;
    }
    m->a += a; m->b += b; m->c += c; m->d += d;
}

void md5_init(MD5 *m)
{
    m->a = 0x67452301; m->b = 0xefcdab89; m->c = 0x98badcfe; m->d = 0x10325476;
    m->len = 0; m->n = 0;
}

void md5_update(MD5 *m, const void *data, size_t len)
{
    const uint8_t *p = (const uint8_t *)data;
    m->len += len;
    if (m->n) {                        /* finish the partial block first */
        size_t take = 64 - m->n;
        if (take > len) take = len;
        memcpy(m->buf + m->n, p, take);
        m->n += take; p += take; len -= take;
        if (m->n == 64) { md5_block(m, m->buf); m->n = 0; }
    }
    while (len >= 64) {                /* whole blocks straight from the caller */
        md5_block(m, p);
        p += 64; len -= 64;
    }
    if (len) { memcpy(m->buf, p, len); m->n = len; }
}

void md5_final(MD5 *m, uint8_t out[16])
{
    uint64_t bits = m->len * 8;
    size_t i;
    uint8_t pad = 0x80;
    md5_update(m, &pad, 1);
    pad = 0;
    while (m->n != 56) md5_update(m, &pad, 1);
    for (i = 0; i < 8; i++) { uint8_t b = (uint8_t)(bits >> (8 * i)); memcpy(m->buf + 56 + i, &b, 1); }
    md5_block(m, m->buf);
    for (i = 0; i < 4; i++) {
        out[i]      = (uint8_t)(m->a >> (8 * i));
        out[i + 4]  = (uint8_t)(m->b >> (8 * i));
        out[i + 8]  = (uint8_t)(m->c >> (8 * i));
        out[i + 12] = (uint8_t)(m->d >> (8 * i));
    }
}

void evo_md5(const void *data, size_t len, uint8_t out[16])
{
    MD5 m; md5_init(&m); md5_update(&m, data, len); md5_final(&m, out);
}

/* ---------------------------------------------------------------- lz4 block */

int lz4_decompress(const uint8_t *src, size_t slen, uint8_t *dst, size_t dlen,
                   size_t *consumed)
{
    size_t i = 0, o = 0;
    while (i < slen) {
        unsigned tok = src[i++];
        size_t ll = tok >> 4, ml;
        size_t off, start, k;
        if (ll == 15) {
            unsigned b;
            do { if (i >= slen) return -1; b = src[i++]; ll += b; } while (b == 255);
        }
        if (i + ll > slen || o + ll > dlen) return -1;
        memcpy(dst + o, src + i, ll);
        i += ll; o += ll;
        if (o >= dlen) break;
        if (i + 2 > slen) return -1;
        off = (size_t)src[i] | ((size_t)src[i+1] << 8);
        i += 2;
        if (off == 0 || off > o) return -1;
        ml = tok & 15;
        if (ml == 15) {
            unsigned b;
            do { if (i >= slen) return -1; b = src[i++]; ml += b; } while (b == 255);
        }
        ml += 4;
        if (o + ml > dlen) return -1;
        start = o - off;
        if (off >= ml) {
            memcpy(dst + o, dst + start, ml);      /* no overlap at all */
        } else if (off >= 8) {
            /* the source of each 8-byte block ends at or before the block's... */
            for (k = 0; k + 8 <= ml; k += 8) memcpy(dst + o + k, dst + start + k, 8);
            for (; k < ml; k++) dst[o + k] = dst[start + k];
        } else {
            for (k = 0; k < ml; k++) dst[o + k] = dst[start + k];   /* tight overlap */
        }
        o += ml;
    }
    if (consumed) *consumed = i;
    return o == dlen ? 0 : -1;
}

#define LZ4_HASH_LOG 16
#define LZ4_HASH_SIZE (1 << LZ4_HASH_LOG)

static uint32_t lz4_hash(uint32_t s) { return (s * 2654435761u) >> (32 - LZ4_HASH_LOG); }

static uint32_t rd32(const uint8_t *p)
{
    return (uint32_t)p[0] | ((uint32_t)p[1] << 8) |
           ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24);
}

static void emit_len(uint8_t *out, size_t *o, size_t v)
{
    while (v >= 255) { out[(*o)++] = 255; v -= 255; }
    out[(*o)++] = (uint8_t)v;
}

/* worst case output is slen + slen/255 + 16 */
size_t lz4_compress(const uint8_t *src, size_t slen, uint8_t *out, int32_t *table)
{
    size_t o = 0, ip = 0, anchor = 0;
    size_t limit = slen > 12 ? slen - 12 : 0;
    int i;
    for (i = 0; i < LZ4_HASH_SIZE; i++) table[i] = -1;
    while (ip < limit) {
        uint32_t h = lz4_hash(rd32(src + ip));
        int32_t ref = table[h];
        size_t ml, maxml, lit, extra, off;
        table[h] = (int32_t)ip;
        if (ref < 0 || ip - (size_t)ref > 0xFFFF ||
            rd32(src + ref) != rd32(src + ip)) { ip++; continue; }
        ml = 4;
        maxml = slen - 5 - ip;
        while (ml < maxml && src[ref + ml] == src[ip + ml]) ml++;
        lit = ip - anchor;
        extra = ml - 4;
        out[o++] = (uint8_t)(((lit < 15 ? lit : 15) << 4) | (extra < 15 ? extra : 15));
        if (lit >= 15) emit_len(out, &o, lit - 15);
        memcpy(out + o, src + anchor, lit); o += lit;
        off = ip - (size_t)ref;
        out[o++] = (uint8_t)(off & 0xFF);
        out[o++] = (uint8_t)(off >> 8);
        if (extra >= 15) emit_len(out, &o, extra - 15);
        ip += ml;
        anchor = ip;
    }
    {
        size_t lit = slen - anchor;
        out[o++] = (uint8_t)((lit < 15 ? lit : 15) << 4);
        if (lit >= 15) emit_len(out, &o, lit - 15);
        memcpy(out + o, src + anchor, lit); o += lit;
    }
    return o;
}

/* ---------------------------------------------------------------- errors */

static char g_err[512];
static void (*g_log)(const char *);
static int g_force_bits;

void evo_force_index_bits(int b) { g_force_bits = b; }

void evo_set_log(void (*fn)(const char *)) { g_log = fn; }

static void (*g_prog)(uint64_t, uint64_t);
static uint64_t g_prog_done, g_prog_total, g_prog_last;

void evo_set_progress(void (*fn)(uint64_t, uint64_t)) { g_prog = fn; }

/* reset between commands so a failed run cannot leave a stale total behind */
void evo_progress_reset(void) { g_prog_done = g_prog_total = g_prog_last = 0; }

void evo_progress_begin(uint64_t total)
{
    g_prog_done = 0; g_prog_total = total; g_prog_last = 0;
    if (g_prog) g_prog(0, total);
}

/* report at most every 50 MB, and always on the final byte */
void evo_progress_add(uint64_t bytes)
{
    uint64_t step;
    g_prog_done += bytes;
    if (!g_prog) return;
    /* every 50 MB, but at least a hundred updates so short jobs still move */
    step = g_prog_total / 100;
    if (step > (50u << 20)) step = (50u << 20);
    if (step < (64u << 10)) step = (64u << 10);
    if (g_prog_done - g_prog_last >= step || g_prog_done >= g_prog_total) {
        g_prog_last = g_prog_done;
        g_prog(g_prog_done, g_prog_total);
    }
}

void evo_progress_end(void)
{
    if (g_prog && g_prog_total) g_prog(g_prog_total, g_prog_total);
    g_prog_total = 0;
}

void evo_log(const char *fmt, ...)
{
    char b[2048];
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(b, sizeof b, fmt, ap);
    va_end(ap);
    if (g_log) g_log(b);
    else { fputs(b, stdout); fflush(stdout); }
}

const char *evo_error(void) { return g_err; }

int evo_fail(const char *fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(g_err, sizeof g_err, fmt, ap);
    va_end(ap);
    return -1;
}

/* ---------------------------------------------------------------- .dat */

static uint16_t rd16(const uint8_t *p) { return (uint16_t)(p[0] | (p[1] << 8)); }

static uint64_t rd64(const uint8_t *p)
{
    return (uint64_t)rd32(p) | ((uint64_t)rd32(p + 4) << 32);
}

static void wr32(uint8_t *p, uint32_t v)
{
    p[0]=(uint8_t)v; p[1]=(uint8_t)(v>>8); p[2]=(uint8_t)(v>>16); p[3]=(uint8_t)(v>>24);
}

static void wr64(uint8_t *p, uint64_t v) { wr32(p, (uint32_t)v); wr32(p + 4, (uint32_t)(v >> 32)); }

static int dat_open_inner(DatReader *r, const char *path)
{
    uint8_t head[0x28];
    uint32_t i;
    uint64_t acc = 0, fsize;
    memset(r, 0, sizeof *r);
    r->f = fopen(path, "rb");
    if (!r->f) return evo_fail("cannot open %s", path);
    snprintf(r->path, sizeof r->path, "%s", path);
    if (fread(head, 1, 0x24, r->f) != 0x24) return evo_fail("%s: too short", path);
    if (memcmp(head, "DATF", 4)) return evo_fail("%s: missing DATF magic", path);
    r->version = rd32(head + 4);
    if (r->version != 4300 && r->version != 3100)
        return evo_fail("%s: unsupported DATF version %u", path, r->version);
    r->filetime = rd64(head + 8);
    r->toc_offset = rd64(head + 0x10);
    r->chunk_count = rd32(head + 0x18);
    r->buffer_size = rd32(head + 0x1C);
    if (r->buffer_size == 0 || r->buffer_size >= 0x40000)
        return evo_fail("%s: bufferSize 0x%X out of range", path, r->buffer_size);
    if (memcmp(head + 0x20, "CHNK", 4)) return evo_fail("%s: missing CHNK magic", path);
    if (r->chunk_count == 0 || r->chunk_count > 0x4000000)
        return evo_fail("%s: chunk count %u out of range", path, r->chunk_count);
    r->chunk_sizes = (uint32_t *)malloc((size_t)r->chunk_count * 4);
    r->chunk_at = (uint64_t *)malloc(((size_t)r->chunk_count + 1) * 8);
    if (!r->chunk_sizes || !r->chunk_at) return evo_fail("out of memory");
    {
        uint8_t *raw = (uint8_t *)malloc((size_t)r->chunk_count * 4);
        if (!raw) return evo_fail("out of memory");
        if (fread(raw, 1, (size_t)r->chunk_count * 4, r->f) != (size_t)r->chunk_count * 4) {
            free(raw); return evo_fail("%s: truncated chunk table", path);
        }
        for (i = 0; i < r->chunk_count; i++) r->chunk_sizes[i] = rd32(raw + i * 4);
        free(raw);
    }
    if (fread(head, 1, 4, r->f) != 4 || memcmp(head, "DATA", 4))
        return evo_fail("%s: missing DATA magic", path);
    r->data_start = 0x28 + (uint64_t)r->chunk_count * 4;
    for (i = 0; i < r->chunk_count; i++) { r->chunk_at[i] = acc; acc += r->chunk_sizes[i]; }
    r->chunk_at[r->chunk_count] = acc;
    FSEEK(r->f, 0, SEEK_END);
    fsize = (uint64_t)FTELL(r->f);
    if (r->data_start + acc != fsize)
        return evo_fail("%s: chunk table spans 0x%llX but file is 0x%llX",
                        path, (unsigned long long)(r->data_start + acc),
                        (unsigned long long)fsize);
    r->logical_size = (uint64_t)r->chunk_count * r->buffer_size;
    r->cached = -1;
    r->buf = (uint8_t *)malloc(r->buffer_size);
    r->tmp = (uint8_t *)malloc(r->buffer_size);
    if (!r->buf || !r->tmp) return evo_fail("out of memory");
    return 0;
}

int dat_open(DatReader *r, const char *path)
{
    int rc = dat_open_inner(r, path);
    if (rc != 0) dat_close(r);
    return rc;
}

void dat_close(DatReader *r)
{
    if (r->f) fclose(r->f);
    free(r->chunk_sizes); free(r->chunk_at); free(r->buf); free(r->tmp);
    memset(r, 0, sizeof *r);
}

const uint8_t *dat_chunk(DatReader *r, uint32_t i)
{
    uint32_t cs;
    if ((int64_t)i == r->cached) return r->buf;
    if (i >= r->chunk_count) { evo_fail("chunk %u out of range", i); return NULL; }
    cs = r->chunk_sizes[i];
    FSEEK(r->f, (int64_t)(r->data_start + r->chunk_at[i]), SEEK_SET);
    if (cs == r->buffer_size) {
        if (fread(r->buf, 1, cs, r->f) != cs) { evo_fail("truncated chunk %u", i); return NULL; }
    } else if (cs < 5) {
        evo_fail("chunk %u has stored size %u, too small to be verbatim (0x%X) or "
                 "compressed", i, cs, r->buffer_size);
        return NULL;
    } else {
        size_t used = 0;
        uint32_t raw_size;
        if (fread(r->tmp, 1, cs, r->f) != cs) { evo_fail("truncated chunk %u", i); return NULL; }
        raw_size = rd32(r->tmp);
        if (raw_size != r->buffer_size) {
            evo_fail("chunk %u declares rawSize 0x%X, expected 0x%X", i, raw_size,
                     r->buffer_size);
            return NULL;
        }
        if (lz4_decompress(r->tmp + 4, cs - 4, r->buf, raw_size, &used) != 0) {
            evo_fail("chunk %u failed to decompress", i);
            return NULL;
        }
        if (used != cs - 4u) {
            evo_fail("chunk %u consumed %llu of %u input bytes", i,
                     (unsigned long long)used, cs - 4);
            return NULL;
        }
    }
    r->cached = (int64_t)i;
    return r->buf;
}

int dat_read(DatReader *r, uint64_t off, uint64_t len, uint8_t *dst)
{
    if (off + len > r->logical_size || off + len < off)
        return evo_fail("read 0x%llX+0x%llX outside logical size 0x%llX of %s - the "
                        "index does not match this archive",
                        (unsigned long long)off, (unsigned long long)len,
                        (unsigned long long)r->logical_size, r->path);
    while (len) {
        uint32_t ci = (uint32_t)(off / r->buffer_size);
        uint32_t co = (uint32_t)(off % r->buffer_size);
        uint64_t n = r->buffer_size - co;
        const uint8_t *c;
        if (n > len) n = len;
        c = dat_chunk(r, ci);
        if (!c) return -1;
        memcpy(dst, c + co, (size_t)n);
        dst += n; off += n; len -= n;
    }
    return 0;
}

/* ---------------------------------------------------------------- .dat write */

int dat_write(const char *path, const Segment *segs, size_t nseg, uint64_t filetime,
              uint32_t version, uint32_t bs, uint64_t toc_offset,
              void (*progress)(uint64_t, uint64_t, void *), void *ctx)
{
    uint64_t total = 0, done = 0;
    uint32_t chunk_count, written = 0;
    uint32_t *sizes;
    uint8_t *pending, *cbuf;
    int32_t *table;
    size_t pn = 0, i;
    FILE *out;
    uint8_t hdr[0x28];
    int rc = 0;

    for (i = 0; i < nseg; i++) total += segs[i].size;
    if (total == 0) return evo_fail("refusing to write an empty archive");
    chunk_count = (uint32_t)((total + bs - 1) / bs);

    sizes = (uint32_t *)calloc(chunk_count, 4);
    pending = (uint8_t *)malloc(bs);
    cbuf = (uint8_t *)malloc(bs + bs / 255 + 64);
    table = (int32_t *)malloc(LZ4_HASH_SIZE * sizeof(int32_t));
    if (!sizes || !pending || !cbuf || !table) {
        free(sizes); free(pending); free(cbuf); free(table);
        return evo_fail("out of memory");
    }
    out = fopen(path, "wb");
    if (!out) { free(sizes); free(pending); free(cbuf); free(table);
                return evo_fail("cannot write %s", path); }

    memcpy(hdr, "DATF", 4);
    wr32(hdr + 4, version);
    wr64(hdr + 8, filetime);
    wr64(hdr + 0x10, toc_offset);
    wr32(hdr + 0x18, chunk_count);
    wr32(hdr + 0x1C, bs);
    memcpy(hdr + 0x20, "CHNK", 4);
    fwrite(hdr, 1, 0x24, out);
    for (i = 0; i < chunk_count; i++) fwrite("\0\0\0\0", 1, 4, out);
    fwrite("DATA", 1, 4, out);

    #define FLUSH_CHUNK() do {                                                   \
        size_t cn = lz4_compress(pending, bs, cbuf, table);                       \
        if (cn + 4 >= bs) { fwrite(pending, 1, bs, out); sizes[written++] = bs; }  \
        else { uint8_t p4[4]; wr32(p4, bs); fwrite(p4, 1, 4, out);                 \
               fwrite(cbuf, 1, cn, out); sizes[written++] = (uint32_t)(cn + 4); }  \
        done += bs;                                                               \
        if (progress) progress(done, total, ctx);                                 \
    } while (0)

    for (i = 0; i < nseg && rc == 0; i++) {
        const Segment *s = &segs[i];
        if (s->kind == SEG_MEM) {
            uint64_t left = s->size, pos = 0;
            while (left) {
                size_t take = bs - pn;
                if (take > left) take = (size_t)left;
                memcpy(pending + pn, s->mem + pos, take);
                pn += take; pos += take; left -= take;
                if (pn == bs) { FLUSH_CHUNK(); pn = 0; }
            }
        } else {
            FILE *in = NULL;
            uint64_t left = s->size, off = s->offset;
            if (s->kind == SEG_FILE) {
                in = fopen(s->path, "rb");
                if (!in) { rc = evo_fail("cannot read %s", s->path); break; }
            }
            while (left) {
                size_t take = bs - pn;
                if (take > left) take = (size_t)left;
                if (in) {
                    if (fread(pending + pn, 1, take, in) != take) {
                        rc = evo_fail("short read from %s", s->path); break;
                    }
                } else {
                    if (dat_read(s->src, off, take, pending + pn) != 0) { rc = -1; break; }
                    off += take;
                }
                pn += take; left -= take;
                if (pn == bs) { FLUSH_CHUNK(); pn = 0; }
            }
            if (in) fclose(in);
        }
    }
    if (rc == 0 && pn) {
        memset(pending + pn, 0, bs - pn);
        FLUSH_CHUNK();
        pn = 0;
    }
    if (rc == 0 && written != chunk_count)
        rc = evo_fail("wrote %u chunks, planned %u", written, chunk_count);
    if (rc == 0) {
        uint8_t *tbl = (uint8_t *)malloc((size_t)chunk_count * 4);
        if (!tbl) rc = evo_fail("out of memory");
        else {
            for (i = 0; i < chunk_count; i++) wr32(tbl + i * 4, sizes[i]);
            FSEEK(out, 0x24, SEEK_SET);
            fwrite(tbl, 1, (size_t)chunk_count * 4, out);
            free(tbl);
        }
    }
    fclose(out);
    free(sizes); free(pending); free(cbuf); free(table);
    return rc;
}

/* ---------------------------------------------------------------- .ndx */

uint32_t evo_name_hash(const char *name, uint32_t ha, uint32_t hb)
{
    uint32_t a = ha, r = 0;
    const unsigned char *p = (const unsigned char *)name;
    for (; *p; p++) {
        unsigned c = *p;
        if (c >= 'a' && c <= 'z') c -= 32;
        if (c == '\\') c = '/';
        a = (a * hb) % 0x7FFFFFFEu;      /* uint32 wrap before the modulo */
        r = (r * a + c) % 0x7FFFFFFFu;
    }
    return r;
}

/* digram decoder: b|0x80 expands to (dic[b-0x80], dic[b]), stacked so it... */
static size_t decode_name(const uint8_t dic[256], const uint8_t *comp, size_t comp_len,
                          size_t pos, char *out, size_t cap, int *ok)
{
    uint8_t stack[512];
    size_t i = 0, n = 0, guard = 0;
    *ok = 1;
    out[0] = 0;
    for (;;) {
        unsigned data;
        if (++guard > 1u << 20) { *ok = 0; return pos; }
        for (;;) {
            if (i > 0) data = stack[--i];
            else {
                if (pos >= comp_len) { *ok = 0; return pos; }
                data = comp[pos++];
            }
            if (!(data & 0x80)) break;
            if (i + 2 > sizeof stack) { *ok = 0; return pos; }
            stack[i] = dic[data];
            stack[i + 1] = dic[(data - 0x80) & 0xFF];
            i += 2;
            if (++guard > 1u << 20) { *ok = 0; return pos; }
        }
        if (data == 0) { out[n] = 0; return pos; }
        if (n + 1 < cap) out[n++] = (char)data;
        else { out[cap - 1] = 0; *ok = 0; return pos; }
    }
}

static int ndx_layout_ok(const Index *x, int bits)
{
    /* an 8-bit read of a 16-bit field keeps datIndex valid but scales every... */
    uint64_t mask = (1ull << bits) - 1;
    int di;
    for (di = 0; di < (int)x->dat_count; di++) {
        uint64_t cursor = 0;
        int seen = 0;
        uint32_t i;
        /* entries are hash-ordered, so gather then walk in offset order */
        for (;;) {
            uint64_t best = (uint64_t)-1;
            int32_t besti = -1;
            for (i = 0; i < x->count; i++) {
                uint64_t v = rd64(x->raw + x->entries_at + (size_t)x->entry_size * i);
                int32_t sz = (int32_t)rd32(x->raw + x->entries_at +
                                           (size_t)x->entry_size * i + 8);
                uint64_t o = v >> bits;
                if ((v & mask) != (uint64_t)di) continue;
                if (sz < 0) return 0;
                if (o >= cursor && o < best) { best = o; besti = (int32_t)i; }
            }
            if (besti < 0) break;
            if (best != cursor) return 0;
            cursor += (uint32_t)rd32(x->raw + x->entries_at +
                                     (size_t)x->entry_size * besti + 8);
            seen = 1;
        }
        (void)seen;
    }
    return 1;
}

static int ndx_open_inner(Index *x, const char *path)
{
    FILE *f;
    long fsize;
    size_t p, names_len, pos;
    uint32_t i, count;
    uint8_t dict_size;
    uint8_t dictab[256];
    const uint8_t *dic, *names;

    memset(x, 0, sizeof *x);
    f = fopen(path, "rb");
    if (!f) return evo_fail("cannot open %s", path);
    FSEEK(f, 0, SEEK_END); fsize = (long)FTELL(f); FSEEK(f, 0, SEEK_SET);
    x->raw = (uint8_t *)malloc((size_t)fsize);
    if (!x->raw) { fclose(f); return evo_fail("out of memory"); }
    if (fread(x->raw, 1, (size_t)fsize, f) != (size_t)fsize) {
        fclose(f); return evo_fail("%s: short read", path);
    }
    fclose(f);
    x->raw_size = (size_t)fsize;
    x->force_bits = g_force_bits;
    snprintf(x->path, sizeof x->path, "%s", path);

    if (x->raw_size < 0x50) return evo_fail("%s: too short to be an index", path);
    if (memcmp(x->raw, "DATN", 4) && memcmp(x->raw, "DATX", 4))
        return evo_fail("%s: magic is not DATN/DATX", path);
    x->version = rd32(x->raw + 4);
    if (x->version != 4300 && x->version != 3100)
        return evo_fail("%s: unsupported index version %u", path, x->version);
    x->filetime = rd64(x->raw + 8);
    x->total_data_size = rd64(x->raw + 0x10);
    x->hash_a = rd32(x->raw + 0x18);
    x->hash_b = rd32(x->raw + 0x1C);
    x->compression = (int32_t)rd32(x->raw + 0x20);
    x->buffer_size = rd32(x->raw + 0x24);
    if (x->compression < 0 || x->compression > 7)
        return evo_fail("invalid compression format %d", x->compression);
    if (x->buffer_size >= 0x40000)
        return evo_fail("ReadBufferSize 0x%X exceeds the game limit", x->buffer_size);
    p = 0x28;
    if (x->version > 3100) {
        x->dat_count = rd32(x->raw + 0x30);
        p += 16;
    } else x->dat_count = 1;
    if (p + 4 > x->raw_size) return evo_fail("%s: truncated header", path);
    count = rd32(x->raw + p); p += 4;
    if (count == 0 || count >= 1000000) return evo_fail("entry count %u out of range", count);
    x->count = count;
    if (p >= x->raw_size) return evo_fail("%s: truncated header", path);
    dict_size = x->raw[p]; p += 1;
    if (dict_size > 0x80) return evo_fail("dict size 0x%X exceeds the game limit", dict_size);
    if (p + (size_t)dict_size * 2 + 4 > x->raw_size)
        return evo_fail("%s: truncated dictionary", path);
    memset(dictab, 0, sizeof dictab);
    memcpy(dictab, x->raw + p, (size_t)dict_size * 2 < 256 ? (size_t)dict_size * 2 : 256);
    dic = dictab;
    p += (size_t)dict_size * 2;
    names_len = rd32(x->raw + p); p += 4;
    if (names_len > x->raw_size || p + names_len + 4 > x->raw_size)
        return evo_fail("%s: filename blob runs past end of file", path);
    names = x->raw + p; p += names_len;
    if (rd32(x->raw + p) != 0x12345678u)
        return evo_fail("missing 0x12345678 marker before the entry table");
    p += 4;
    x->entries_at = p;
    x->entry_size = x->version > 3100 ? 32 : 16;
    if (x->entries_at + (size_t)x->entry_size * count + 4 > x->raw_size)
        return evo_fail("%s: entry table runs past end of file", path);

    if (x->version <= 3100) x->index_bits = 0;
    else if (x->force_bits) x->index_bits = x->force_bits;
    else {
        /* datIndex < 256 either way, so an 8-bit read of a 16-bit field still... */
        int r8 = 1, r16 = 1, c8, c16, a8 = 1, nz = 0;
        uint64_t max16 = 0;
        for (i = 0; i < count; i++) {
            uint64_t v = rd64(x->raw + x->entries_at + (size_t)x->entry_size * i);
            uint64_t d16 = v & 0xFFFF;
            if ((v & 0xFF) >= x->dat_count) r8 = 0;
            if (d16 >= x->dat_count) r16 = 0;
            if (d16 > max16) max16 = d16;
            if (v >> 8) { nz++; if ((v >> 8) & 0xFF) a8 = 0; }
        }
        /* decisive: an 8-bit field cannot hold 256 or more, so an index that... */
        if (r16 && max16 > 0xFF) { x->index_bits = 16; x->layout_contiguous = 0; }
        else {
        c8 = r8 && ndx_layout_ok(x, 8);
        c16 = r16 && ndx_layout_ok(x, 16);
        if (r8 && !r16) x->index_bits = 8;
        else if (r16 && !r8) x->index_bits = 16;
        else if (c8 && !c16) x->index_bits = 8;
        else if (c16 && !c8) x->index_bits = 16;
        else if (r8 || r16) x->index_bits = (nz && a8) ? 16 : 8;
        else return evo_fail("could not determine the datIndex width: neither an "
                             "8- nor a 16-bit reading keeps datIndex below %u",
                             x->dat_count);
        x->layout_contiguous = (x->index_bits == 8) ? c8 : c16;
        }
    }

    x->entries = (Entry *)calloc(count, sizeof(Entry));
    x->names = (char *)malloc((size_t)count * 160);
    if (!x->entries || !x->names) return evo_fail("out of memory");
    pos = 0;
    for (i = 0; i < count; i++) {
        size_t rec = x->entries_at + (size_t)x->entry_size * i;
        uint64_t v = rd64(x->raw + rec);
        Entry *e = &x->entries[i];
        e->rec = rec;
        e->dat = (uint32_t)(x->index_bits ? (v & ((1ull << x->index_bits) - 1)) : 0);
        e->offset = x->index_bits ? (v >> x->index_bits) : v;
        e->size = (int32_t)rd32(x->raw + rec + 8);
        e->hash = rd32(x->raw + rec + 12);
        if (x->version > 3100) memcpy(e->md5, x->raw + rec + 16, 16);
        int ok;
        e->name = x->names + (size_t)i * 160;
        pos = decode_name(dic, names, names_len, pos, e->name, 160, &ok);
        if (!ok) return evo_fail("%s: filename %u is malformed", path, i);
    }
    return 0;
}

int ndx_open(Index *x, const char *path)
{
    int rc = ndx_open_inner(x, path);
    if (rc != 0) ndx_close(x);
    return rc;
}

void ndx_close(Index *x)
{
    free(x->raw); free(x->entries); free(x->names);
    memset(x, 0, sizeof *x);
}

int ndx_write(const Index *x, const char *path)
{
    uint8_t *d;
    uint32_t i;
    FILE *f;
    if (x->version <= 3100)
        return evo_fail("writing version 3100 indexes is not supported");
    d = (uint8_t *)malloc(x->raw_size);
    if (!d) return evo_fail("out of memory");
    memcpy(d, x->raw, x->raw_size);
    for (i = 0; i < x->count; i++) {
        const Entry *e = &x->entries[i];
        uint64_t v;
        if (e->size < 0) { free(d); return evo_fail("%s: negative size", e->name); }
        if (e->dat >= (1u << x->index_bits)) {
            free(d); return evo_fail("%s: datIndex %u overflows %d bits",
                                     e->name, e->dat, x->index_bits);
        }
        if (x->index_bits < 64 && (e->offset >> (64 - x->index_bits))) {
            free(d); return evo_fail("%s: offset overflows the packed field", e->name);
        }
        v = (e->offset << x->index_bits) | e->dat;
        wr64(d + e->rec, v);
        wr32(d + e->rec + 8, (uint32_t)e->size);
        wr32(d + e->rec + 12, e->hash);
        memcpy(d + e->rec + 16, e->md5, 16);
    }
    f = fopen(path, "wb");
    if (!f) { free(d); return evo_fail("cannot write %s", path); }
    fwrite(d, 1, x->raw_size, f);
    fclose(f);
    free(d);
    return 0;
}

/* entries whose offset+size runs past the archive that holds them; the... */
int ndx_fit_archives(const Index *x, const char *dat_dir, int *checked)
{
    uint64_t *cap;
    uint32_t i;
    int bad = 0, seen = 0;
    if (!dat_dir || !x->dat_count) return 0;
    cap = (uint64_t *)calloc(x->dat_count, sizeof(uint64_t));
    if (!cap) return 0;
    for (i = 0; i < x->dat_count; i++) {
        char p[1024];
        DatReader r;
        snprintf(p, sizeof p, "%s/game%03u.dat", dat_dir, i);
        if (dat_open(&r, p) == 0) { cap[i] = r.logical_size; seen++; }
        dat_close(&r);
    }
    for (i = 0; i < x->count; i++) {
        const Entry *e = &x->entries[i];
        if (e->dat >= x->dat_count || !cap[e->dat]) continue;
        if (e->offset + (uint64_t)e->size > cap[e->dat]) bad++;
    }
    free(cap);
    if (checked) *checked = seen;
    return seen ? bad : 0;
}

int ndx_open_checked(Index *x, const char *ndx_path, const char *dat_dir)
{
    int bad, seen = 0, other, saved = g_force_bits;
    Index y;
    if (ndx_open(x, ndx_path) != 0) return -1;
    /* an explicit --bits is the user's call, so do not second-guess it */
    if (saved || !dat_dir || x->version <= 3100 || x->index_bits == 0) return 0;
    bad = ndx_fit_archives(x, dat_dir, &seen);
    if (!seen || !bad) return 0;
    other = x->index_bits == 8 ? 16 : 8;
    evo_force_index_bits(other);
    if (ndx_open(&y, ndx_path) == 0 && ndx_fit_archives(&y, dat_dir, NULL) == 0) {
        evo_force_index_bits(saved);
        evo_log("note: %d entr(ies) ran past their archive at %d bits, so the "
                "index is being read as %d bits\n", bad, x->index_bits, other);
        ndx_close(x);
        *x = y;
        return 0;
    }
    ndx_close(&y);
    evo_force_index_bits(saved);
    evo_log("warning: %d entr(ies) point past the end of their archive; the "
            "index and the .dat files may not belong together\n", bad);
    return 0;
}


/* ---------------------------------------------------------------- index... */

static uint8_t g_pair_tok[256][256];   /* (a,b) -> token, 0 when there is none */
static uint8_t g_dict[256];
static int g_pair_ready;

static void pair_init(const uint8_t dict[256])
{
    int b;
    memset(g_pair_tok, 0, sizeof g_pair_tok);
    memcpy(g_dict, dict, 256);
    for (b = 0x80; b < 0x100; b++) {
        uint8_t lo = dict[(b - 0x80) & 0xFF], hi = dict[b];
        if (!g_pair_tok[lo][hi]) g_pair_tok[lo][hi] = (uint8_t)b;
    }
    g_pair_ready = 1;
}

/* greedy digram substitution, applied repeatedly because the dictionary nests */
static size_t encode_name(const char *name, uint8_t *out, size_t cap)
{
    uint8_t a[1024], b[1024];
    size_t n = 0, i, m, round;
    for (i = 0; name[i]; i++) {
        if (n + 2 >= sizeof a) return 0;
        if ((unsigned char)name[i] & 0x80) return 0;   /* literals are 7-bit */
        a[n++] = (uint8_t)name[i];
    }
    a[n++] = 0;
    for (round = 0; round < 12; round++) {
        int hit = 0;
        m = 0;
        for (i = 0; i < n; ) {
            uint8_t tok = (i + 1 < n) ? g_pair_tok[a[i]][a[i+1]] : 0;
            if (tok) { b[m++] = tok; i += 2; hit = 1; }
            else b[m++] = a[i++];
        }
        memcpy(a, b, m);
        n = m;
        if (!hit) break;
    }
    if (n > cap) return 0;
    memcpy(out, a, n);
    return n;
}

typedef struct { const char *name; uint32_t dat; uint64_t offset;
                 int32_t size; uint32_t hash; uint8_t md5[16]; } OutEntry;

static int cmp_out(const void *p, const void *q)
{
    const OutEntry *a = (const OutEntry *)p, *b = (const OutEntry *)q;
    if (a->hash != b->hash) return a->hash < b->hash ? -1 : 1;
    return strcmp(a->name, b->name);
}

int ndx_rebuild(const Index *x, const NewFile *add, int nadd, uint32_t dat_count,
                const char *out_path)
{
    OutEntry *all;
    uint8_t *blob, *outbuf;
    size_t blob_len = 0, blob_cap, pos, dict_at, i;
    uint32_t total = x->count + (uint32_t)nadd, k;
    FILE *f;
    if (x->version <= 3100) return evo_fail("writing version 3100 indexes is not supported");
    dict_at = 0x3D;
    {   /* the dictionary sits right after dictSize and is copied verbatim */
        uint8_t dict[256];
        uint8_t ds = x->raw[0x3C];
        memset(dict, 0, sizeof dict);
        memcpy(dict, x->raw + dict_at, (size_t)ds * 2 < 256 ? (size_t)ds * 2 : 256);
        pair_init(dict);
    }
    all = (OutEntry *)calloc(total, sizeof(OutEntry));
    if (!all) return evo_fail("out of memory");
    for (k = 0; k < x->count; k++) {
        all[k].name = x->entries[k].name;
        all[k].dat = x->entries[k].dat;
        all[k].offset = x->entries[k].offset;
        all[k].size = x->entries[k].size;
        all[k].hash = x->entries[k].hash;
        memcpy(all[k].md5, x->entries[k].md5, 16);
    }
    for (i = 0; i < (size_t)nadd; i++) {
        OutEntry *e = &all[x->count + i];
        e->name = add[i].name;
        e->dat = add[i].dat;
        e->offset = add[i].offset;
        e->size = add[i].size;
        e->hash = evo_name_hash(add[i].name, x->hash_a, x->hash_b);
        memcpy(e->md5, add[i].md5, 16);
    }
    qsort(all, total, sizeof(OutEntry), cmp_out);
    for (k = 1; k < total; k++)
        if (!strcmp(all[k].name, all[k-1].name)) {
            char nm[512];
            snprintf(nm, sizeof nm, "%s", all[k].name);
            free(all);
            return evo_fail("%s appears twice; it is already in the index", nm);
        }

    blob_cap = (size_t)total * 320 + 1024;
    blob = (uint8_t *)malloc(blob_cap);
    if (!blob) { free(all); return evo_fail("out of memory"); }
    for (k = 0; k < total; k++) {
        size_t n = encode_name(all[k].name, blob + blob_len, blob_cap - blob_len);
        if (!n) {
            char nm[512];
            snprintf(nm, sizeof nm, "%s", all[k].name);
            free(all); free(blob);
            return evo_fail("cannot encode the name %s", nm);
        }
        blob_len += n;
    }

    {
        size_t ds2 = (size_t)x->raw[0x3C] * 2;
        size_t head = dict_at + ds2;
        size_t need = head + 4 + blob_len + 4 + (size_t)total * 32 + 8;
        outbuf = (uint8_t *)malloc(need);
        if (!outbuf) { free(all); free(blob); return evo_fail("out of memory"); }
        memcpy(outbuf, x->raw, head);
        wr32(outbuf + 0x30, dat_count);
        wr32(outbuf + 0x38, total);
        pos = head;
        wr32(outbuf + pos, (uint32_t)blob_len); pos += 4;
        memcpy(outbuf + pos, blob, blob_len); pos += blob_len;
        wr32(outbuf + pos, 0x12345678u); pos += 4;
        for (k = 0; k < total; k++) {
            uint64_t v = (all[k].offset << x->index_bits) | all[k].dat;
            wr64(outbuf + pos, v);
            wr32(outbuf + pos + 8, (uint32_t)all[k].size);
            wr32(outbuf + pos + 12, all[k].hash);
            memcpy(outbuf + pos + 16, all[k].md5, 16);
            pos += 32;
        }
        wr32(outbuf + pos, (uint32_t)pos); pos += 4;   /* tail tag is its own offset */
        wr32(outbuf + pos, 0); pos += 4;
        f = fopen(out_path, "wb");
        if (!f) { free(all); free(blob); free(outbuf);
                  return evo_fail("cannot write %s", out_path); }
        fwrite(outbuf, 1, pos, f);
        fclose(f);
        free(outbuf);
    }
    free(all);
    free(blob);
    return 0;
}

int ndx_verify_hashes(const Index *x)
{
    uint32_t i, ok = 0;
    for (i = 0; i < x->count; i++)
        if (evo_name_hash(x->entries[i].name, x->hash_a, x->hash_b) == x->entries[i].hash)
            ok++;
    return (int)ok;
}

/* ---------------------------------------------------------------- .rpk */

static const char PACK_MAGIC[] =
    "EVOSLITL\x12\x00\x00\x00Resource PacK file";
#define PACK_MAGIC_LEN (sizeof PACK_MAGIC - 1)

static int rpk_parse_inner(Pack *pk, const uint8_t *buf, size_t len)
{
    size_t p;
    uint32_t i, npool, apool, total_deps = 0;
    memset(pk, 0, sizeof *pk);
    if (len < 0x2A || memcmp(buf, PACK_MAGIC, PACK_MAGIC_LEN)) return evo_fail("not a resource pack");
    pk->toc_size = rd32(buf + 0x1E);
    pk->toc_offset = rd32(buf + 0x22);
    if (pk->toc_offset != 0x26) return evo_fail("unexpected tocOffset 0x%X", pk->toc_offset);
    p = pk->toc_offset;
    if (p + 24 > len) return evo_fail("pack truncated");
    if (memcmp(buf + p, "EVOSLITL", 8)) return evo_fail("EVOS magic missing in TOC");
    p += 8;
    pk->info_uid = rd64(buf + p); p += 8;
    if (pk->info_uid >> 48 != 1) return evo_fail("TOC identifier is not RESOURCE_INFO_BLOCK");
    pk->version = rd32(buf + p); p += 4;
    if (rd32(buf + p) != 0x11111111u) return evo_fail("missing 0x11111111 marker");
    p += 4;
    pk->count = rd32(buf + p); p += 4;
    pk->alias_count = rd32(buf + p); p += 4;
    pk->dependency_count = rd32(buf + p); p += 4;
    npool = rd32(buf + p); p += 4;
    apool = rd32(buf + p); p += 4;
    if (pk->version >= 0x40002) {
        pk->unk3_count = rd32(buf + p); p += 4;
        pk->required_count = rd32(buf + p); p += 4;
    }
    pk->root_uid = rd64(buf + p); p += 8;
    pk->name_pool = p; pk->name_pool_size = npool; p += npool;
    pk->asset_pool = p; pk->asset_pool_size = apool; p += apool;
    pk->rec_start = p;
    if (p > len) return evo_fail("pack truncated in string pools");
    if (pk->count == 0 || pk->count > 4000000) return evo_fail("resource count %u out of range",
                                                              pk->count);
    pk->res = (ResInfo *)calloc(pk->count, sizeof(ResInfo));
    if (!pk->res) return evo_fail("out of memory");
    for (i = 0; i < pk->count; i++) {
        uint32_t nd, nn, na;
        if (p + 18 > len) return evo_fail("pack truncated in TOC");
        pk->res[i].uid = rd64(buf + p); p += 8;
        pk->res[i].rec = p;
        pk->res[i].size = rd32(buf + p); p += 4;
        pk->res[i].offset = rd32(buf + p); p += 4;
        nd = rd16(buf + p); p += 2; total_deps += nd; p += (size_t)nd * 8;
        if (p + 2 > len) return evo_fail("pack truncated in TOC");
        nn = rd16(buf + p); p += 2;
        pk->res[i].name_off = nn ? rd32(buf + p) : 0xFFFFFFFFu;
        p += (size_t)nn * 4;
        if (p + 2 > len) return evo_fail("pack truncated in TOC");
        na = rd16(buf + p); p += 2; p += (size_t)na * 4;
        if (p > len) return evo_fail("pack truncated in TOC");
    }
    pk->rec_end = p;
    if (total_deps != pk->dependency_count)
        return evo_fail("dependency count %u does not match the %u actually listed",
                        pk->dependency_count, total_deps);
    p += (size_t)pk->unk3_count * 12;
    p += (size_t)pk->required_count * 4;
    if (p > len) return evo_fail("pack truncated after TOC");
    if (p != 0x22 + (size_t)pk->toc_size)
        return evo_fail("tocSize mismatch: parsed 0x%llX declared 0x%llX",
                        (unsigned long long)p,
                        (unsigned long long)(0x22 + (size_t)pk->toc_size));
    if (rd32(buf + p) != 0x99999999u) return evo_fail("missing 0x99999999 marker");
    p += 4;
    pk->data_start = (uint32_t)p;
    pk->size = 0;
    for (i = 0; i < pk->count; i++) {
        uint64_t end = (uint64_t)pk->res[i].offset + pk->res[i].size;
        if (end > pk->size) pk->size = end;
    }
    return 0;
}

int rpk_parse(Pack *pk, const uint8_t *buf, size_t len)
{
    int rc = rpk_parse_inner(pk, buf, len);
    if (rc != 0) rpk_free(pk);
    return rc;
}

void rpk_free(Pack *pk) { free(pk->res); memset(pk, 0, sizeof *pk); }

const char *rpk_name(const Pack *pk, const uint8_t *buf, uint32_t i, char *tmp)
{
    if (pk->res[i].name_off == 0xFFFFFFFFu ||
        pk->res[i].name_off >= pk->name_pool_size ||
        buf[pk->name_pool + pk->res[i].name_off] == 0) {
        snprintf(tmp, 32, "%016llX", (unsigned long long)pk->res[i].uid);
        return tmp;
    }
    return (const char *)buf + pk->name_pool + pk->res[i].name_off;
}

int rpk_check_layout(const Pack *pk)
{
    uint32_t i, *ord = (uint32_t *)malloc(pk->count * 4);
    uint64_t cursor;
    int bad = 0;
    if (!ord) return evo_fail("out of memory");
    for (i = 0; i < pk->count; i++) ord[i] = i;
    for (i = 1; i < pk->count; i++) {
        uint32_t k = ord[i], j = i;
        while (j && pk->res[ord[j-1]].offset > pk->res[k].offset) { ord[j] = ord[j-1]; j--; }
        ord[j] = k;
    }
    cursor = pk->data_start;
    for (i = 0; i < pk->count; i++) {
        if (pk->res[ord[i]].offset != cursor) { bad = 1; break; }
        cursor += pk->res[ord[i]].size;
    }
    free(ord);
    if (bad) return evo_fail("resource payloads are not contiguous");
    return 0;
}

int evo_find_packs(const uint8_t *buf, size_t len, size_t *out, int max)
{
    int n = 0;
    size_t i;
    if (len < 30) return 0;
    for (i = 0; i + 30 <= len && n < max; i++) {
        Pack pk;
        if (memcmp(buf + i, PACK_MAGIC, PACK_MAGIC_LEN)) continue;
        if (rpk_parse(&pk, buf + i, len - i) == 0) { out[n++] = i; rpk_free(&pk); }
        else rpk_free(&pk);
    }
    return n;
}

int dat_scan_packs(DatReader *r, uint64_t *out, int max)
{
    const size_t TAIL = 29;
    uint8_t prev[32];
    size_t prevn = 0;
    uint32_t i;
    int n = 0, k, keep = 0;
    uint8_t *win = (uint8_t *)malloc(r->buffer_size + 32);
    uint8_t *toc;
    if (!win) return evo_fail("out of memory");
    for (i = 0; i < r->chunk_count && n < max; i++) {
        const uint8_t *c = dat_chunk(r, i);
        size_t len, j;
        uint64_t base;
        if (!c) { prevn = 0; continue; }
        memcpy(win, prev, prevn);
        memcpy(win + prevn, c, r->buffer_size);
        len = prevn + r->buffer_size;
        base = (uint64_t)i * r->buffer_size - prevn;
        for (j = 0; j + 30 <= len && n < max; j++)
            if (!memcmp(win + j, PACK_MAGIC, PACK_MAGIC_LEN)) out[n++] = base + j;
        prevn = len >= TAIL ? TAIL : len;
        memcpy(prev, win + len - prevn, prevn);
    }
    free(win);

    /* validate each hit by parsing its header and TOC out of the archive */
    toc = (uint8_t *)malloc(1 << 20);
    if (!toc) return evo_fail("out of memory");
    for (k = 0; k < n; k++) {
        uint8_t head[0x26];
        uint32_t toc_size;
        uint64_t need;
        Pack pk;
        if (dat_read(r, out[k], 0x26, head) != 0) continue;
        toc_size = rd32(head + 0x1E);
        need = 0x22 + (uint64_t)toc_size + 4;
        if (need > (1u << 20) || out[k] + need > r->logical_size) continue;
        if (dat_read(r, out[k], need, toc) != 0) continue;
        if (rpk_parse(&pk, toc, (size_t)need) != 0) { rpk_free(&pk); continue; }
        if (out[k] + pk.size <= r->logical_size) out[keep++] = out[k];
        rpk_free(&pk);
    }
    free(toc);
    return keep;
}

void evo_mkpath(const char *path)
{
    char t[1024];
    size_t i;
    snprintf(t, sizeof t, "%s", path);
    for (i = 1; t[i]; i++)
        if (t[i] == '/' || t[i] == '\\') { char c = t[i]; t[i] = 0; MKDIR(t); t[i] = c; }
    MKDIR(t);
}

/* last separator of either kind; Windows paths use backslashes */
const char *evo_basename(const char *p)
{
    const char *a = strrchr(p, '/'), *b = strrchr(p, '\\');
    const char *s = (a && b) ? (a > b ? a : b) : (a ? a : b);
    return s ? s + 1 : p;
}

int _strnicmp_(const char *a, const char *b, size_t n)
{
    size_t i;
    for (i = 0; i < n; i++) {
        int x = a[i], y = b[i];
        if (x >= 'A' && x <= 'Z') x += 32;
        if (y >= 'A' && y <= 'Z') y += 32;
        if (x != y) return x - y;
        if (!x) return 0;
    }
    return 0;
}

int _stricmp_(const char *a, const char *b)
{
    for (; *a && *b; a++, b++) {
        int x = *a, y = *b;
        if (x >= 'A' && x <= 'Z') x += 32;
        if (y >= 'A' && y <= 'Z') y += 32;
        if (x != y) return x - y;
    }
    return (unsigned char)*a - (unsigned char)*b;
}

int evo_sniff(const char *path)
{
    uint8_t h[30];
    size_t n;
    FILE *f = fopen(path, "rb");
    if (!f) return EVO_UNKNOWN;
    n = fread(h, 1, 30, f);
    fclose(f);
    if (n >= 4 && !memcmp(h, "DATF", 4)) return EVO_DAT;
    if (n >= 4 && (!memcmp(h, "DATN", 4) || !memcmp(h, "DATX", 4))) return EVO_NDX;
    if (n >= (int)PACK_MAGIC_LEN && !memcmp(h, PACK_MAGIC, PACK_MAGIC_LEN)) return EVO_RPK;
    return EVO_UNKNOWN;
}
