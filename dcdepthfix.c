#define _FILE_OFFSET_BITS 64
#include "evofs.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dirent.h>
#include <sys/stat.h>
#include <stdarg.h>


static uint32_t rd32(const uint8_t *p)
{
    return (uint32_t)p[0] | ((uint32_t)p[1] << 8) |
           ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24);
}

static void wr32(uint8_t *p, uint32_t v)
{
    p[0] = (uint8_t)(v & 0xFF); p[1] = (uint8_t)((v >> 8) & 0xFF);
    p[2] = (uint8_t)((v >> 16) & 0xFF); p[3] = (uint8_t)(v >> 24);
}

static void out(const char *fmt, ...);

/* what the shader had before it was changed, so the line that names the... */
static unsigned g_was_w, g_was_h;

/* the edges did not come out right, so the shader is not usable */
static int g_edges_bad;

/* set while undoing, so the same walk puts the old words back instead */
static int g_undo;
static int g_undone;
static int g_already;
int dcfix_already(void);
int dcfix_sweep(const char *dir, void (*say)(const char *));
int dcfix_undo(const char *dir, void (*say)(const char *));

uint8_t *slurp_pub(const char *path, size_t *len)
{
    FILE *f = fopen(path, "rb");
    uint8_t *b;
    int64_t n;
    if (!f) return NULL;
    if (FSEEK(f, 0, SEEK_END) != 0) { fclose(f); return NULL; }
    n = (int64_t)FTELL(f);
    if (n <= 0 || FSEEK(f, 0, SEEK_SET) != 0) { fclose(f); return NULL; }
    b = (uint8_t *)malloc((size_t)n);
    if (!b) { fclose(f); return NULL; }
    if (fread(b, 1, (size_t)n, f) != (size_t)n) { free(b); fclose(f); return NULL; }
    fclose(f);
    *len = (size_t)n;
    return b;
}

static const char *OWNER = "msaadepthconversion";

/* Which resources the depth pass owns.
 *
 * Gathered once for the whole pack rather than asked again for every candidate:
 * a pack holds thousands of resources, and asking each time meant walking all
 * of them each time. */
#define OWNED_MAX 64
static uint64_t g_owned[OWNED_MAX];
static int g_nowned;

static void collect_owned(const Pack *pk, const uint8_t *buf, size_t len)
{
    uint32_t j;
    g_nowned = 0;
    for (j = 0; j < pk->count && g_nowned < OWNED_MAX; j++) {
        size_t q = pk->res[j].rec + 8;
        uint16_t nd, k;
        char tmp[256];
        const char *nm = rpk_name(pk, buf, j, tmp);
        if (!nm || !strstr(nm, OWNER)) continue;
        if (q + 2 > len) continue;
        nd = (uint16_t)(buf[q] | (buf[q + 1] << 8));
        for (k = 0; k < nd && g_nowned < OWNED_MAX &&
                    q + 2 + (size_t)(k + 1) * 8 <= len; k++) {
            uint64_t dep = 0;
            size_t b;
            for (b = 0; b < 8; b++)
                dep |= (uint64_t)buf[q + 2 + (size_t)k * 8 + b] << (b * 8);
            g_owned[g_nowned++] = dep;
        }
    }
}

static int owned_by_depth_pass(const Pack *pk, const uint8_t *buf, size_t len, uint32_t which)
{
    int j;
    (void)buf; (void)len;
    for (j = 0; j < g_nowned; j++)
        if (g_owned[j] == pk->res[which].uid) return 1;
    return 0;
}

static int fix_pack(uint8_t *buf, size_t len)
{
    Pack pk;
    uint32_t i;
    int done = 0;

    if (rpk_parse(&pk, buf, len) != 0) return -1;
    collect_owned(&pk, buf, len);
    for (i = 0; i < pk.count; i++) {
        uint8_t *p;
        size_t o, sz;
        if ((pk.res[i].uid >> 48) != 29) continue;
        if (pk.res[i].offset > len || pk.res[i].size > len - pk.res[i].offset) continue;
        if (!owned_by_depth_pass(&pk, buf, len, i)) continue;
        p = buf + pk.res[i].offset;
        sz = pk.res[i].size;

        /* The two words the pass compares against are the size of the buffer it... */
        {
            uint32_t base = 0;
            int found = 0;
            for (o = 0; o + 8 <= sz; o++) {
                uint32_t w = rd32(p + o);
                if ((w >> 26) != 0x3C) continue;
                if (((w >> 18) & 0x7F) != 8) continue;
                base = ((rd32(p + o + 4) >> 16) & 0x1F) * 4;
                found = 1;
                break;
            }
            if (!found) continue;
            /* undoing goes straight to the block below; patching here first
             * would write the very words that block then takes back out */
            for (o = 0; !g_undo && o + 28 <= sz; o++) {
                uint32_t a = rd32(p + o), b = rd32(p + o + 8);
                uint32_t la, lb, wx, wy;
                int k, hit = 0;
                if ((a >> 23) != 0x17D || (b >> 23) != 0x17D) continue;
                if (((a >> 8) & 0xFF) != 3 || ((b >> 8) & 0xFF) != 3) continue;
                if ((a & 0xFF) != 0xFF || (b & 0xFF) != 0xFF) continue;
                la = rd32(p + o + 4);
                lb = rd32(p + o + 12);
                if (la < 160 || la > 8192 || lb < 120 || lb > 8192 || lb >= la) continue;
                wx = (a >> 16) & 0x7F;
                wy = (b >> 16) & 0x7F;
                for (k = 0; k < 12 && o + 16 + (size_t)k * 4 + 8 <= sz; k++) {
                    size_t at = o + 16 + (size_t)k * 4;
                    uint32_t c = rd32(p + at);
                    if ((c >> 25) == 0x3E && ((c >> 17) & 0xFF) == 0xC4 &&
                        ((c & 0x1FF) == wx || (c & 0x1FF) == wy)) {
                        wr32(p + at, c + (2u << 17)); hit++;
                    } else if ((c >> 26) == 0x34 && ((c >> 17) & 0x1FF) == 0xC4) {
                        uint32_t s0 = rd32(p + at + 4) & 0x1FF;
                        if (s0 == wx || s0 == wy) { wr32(p + at, c + (2u << 17)); hit++; }
                    }
                }
                if (hit != 2) continue;
                wr32(p + o, 0x80000000u | (41u << 23) | (wx << 16) | (0xFFu << 8) | (base + 2));
                wr32(p + o + 4, 0x000E0000u);
                wr32(p + o + 8, 0x80000000u | (41u << 23) | (wy << 16) | (0xFFu << 8) | (base + 2));
                wr32(p + o + 12, 0x000E000Eu);
                g_was_w = la;
                g_was_h = lb;
                done++;
                break;
            }
            /* Undoing it: the extracts go back to the two constants the game
             * shipped, and the comparisons back to the narrower ones. */
            if (g_undo) {
                for (o = 0; o + 16 <= sz; o++) {
                    uint32_t a = rd32(p + o), b, wx, wy;
                    int k, hit = 0;
                    if ((a >> 30) != 2 || ((a >> 23) & 0x7F) != 41) continue;
                    if (((a >> 8) & 0xFF) != 0xFF) continue;
                    if (rd32(p + o + 4) != 0x000E0000u) continue;
                    b = rd32(p + o + 8);
                    if ((b >> 30) != 2 || ((b >> 23) & 0x7F) != 41) continue;
                    if (rd32(p + o + 12) != 0x000E000Eu) continue;
                    wx = (a >> 16) & 0x7F;
                    wy = (b >> 16) & 0x7F;
                    for (k = 0; k < 12 && o + 16 + (size_t)k * 4 + 8 <= sz; k++) {
                        size_t at = o + 16 + (size_t)k * 4;
                        uint32_t c = rd32(p + at);
                        if ((c >> 25) == 0x3E && ((c >> 17) & 0xFF) == 0xC6 &&
                            ((c & 0x1FF) == wx || (c & 0x1FF) == wy)) {
                            wr32(p + at, c - (2u << 17)); hit++;
                        } else if ((c >> 26) == 0x34 && ((c >> 17) & 0x1FF) == 0xC6) {
                            uint32_t s0 = rd32(p + at + 4) & 0x1FF;
                            if (s0 == wx || s0 == wy) { wr32(p + at, c - (2u << 17)); hit++; }
                        }
                    }
                    if (hit != 2) continue;
                    wr32(p + o, (0x17Du << 23) | (wx << 16) | (3u << 8) | 0xFFu);
                    wr32(p + o + 4, 960);
                    wr32(p + o + 8, (0x17Du << 23) | (wy << 16) | (3u << 8) | 0xFFu);
                    wr32(p + o + 12, 540);
                    g_undone++;
                    done++;
                    break;
                }
                if (done) continue;
            }
            /* Already done, and worth saying what is in there rather than only that... */
            for (o = 0; o + 8 <= sz && !done; o++) {
                uint32_t w = rd32(p + o);
                if ((w >> 30) == 2 && ((w >> 23) & 0x7F) == 41 &&
                    ((w >> 8) & 0xFF) == 0xFF && rd32(p + o + 4) == 0x000E0000u) {
                    g_already++;
                    {   /* half a patch is worse than none, so the comparisons
                         * are counted every time rather than only when asked */
                        int k, ge = 0;
                        for (k = 0; k < 16 && o + 16 + (size_t)k * 4 + 4 <= sz; k++) {
                            uint32_t c = rd32(p + o + 16 + (size_t)k * 4);
                            if ((c >> 25) == 0x3E && ((c >> 17) & 0xFF) == 0xC6) ge++;
                            else if ((c >> 26) == 0x34 && ((c >> 17) & 0x1FF) == 0xC6) ge++;
                        }
                        if (ge != 2) g_edges_bad = 1;
                    }
                    break;
                }
            }
        }

    }
    rpk_free(&pk);
    return done;
}

static int could_hold_it(const uint8_t *buf, size_t len)
{
    size_t i, n = strlen(OWNER);
    if (len < n) return 0;
    for (i = 0; i + n <= len; i++)
        if (buf[i] == (uint8_t)OWNER[0] && !memcmp(buf + i, OWNER, n)) return 1;
    return 0;
}

static int patch_loose(const char *path)
{
    size_t len = 0;
    uint8_t *buf = slurp_pub(path, &len);
    int n;
    FILE *w;
    if (!buf) return 0;
    if (!could_hold_it(buf, len)) { free(buf); return 0; }
    n = fix_pack(buf, len);
    if (n <= 0) { free(buf); return 0; }
    w = fopen(path, "r+b");
    if (!w) { free(buf); out("   cannot write %s\n", path); return 0; }
    if (fwrite(buf, 1, len, w) != len || fclose(w) != 0) {
        free(buf);
        out("   only part of %s could be written\n", path);
        return 0;
    }
    free(buf);
    return n;
}

/* A packed game keeps its resources compressed inside the archives, so a pack
 * cannot be changed where it lies: the new bytes would have to be squeezed
 * again and would no longer be the size the index promises.
 *
 * The game reads a loose file before it looks in an archive, so the pack is
 * taken out, changed, and written beside the archives under its own name. That
 * is what a mod does, and it is undone by deleting the file. */
static int rebuild_dat(const char *dat_path, const char *ndx_path,
                       Index *x, uint32_t which, uint8_t *pack, uint32_t size);

static int patch_in_dat(const char *dat, uint64_t at, uint32_t size,
                        const char *ndx_path, Index *x, uint32_t which)
{
    DatReader r;
    uint8_t *buf;
    int n;

    if (dat_open(&r, dat) != 0) return 0;
    buf = (uint8_t *)malloc(size ? size : 1);
    if (!buf) { dat_close(&r); return 0; }
    if (dat_read(&r, at, size, buf) != 0) { free(buf); dat_close(&r); return 0; }
    dat_close(&r);
    if (!could_hold_it(buf, size)) { free(buf); return 0; }
    n = fix_pack(buf, size);
    if (n <= 0) { free(buf); return 0; }
    if (!rebuild_dat(dat, ndx_path, x, which, buf, size)) { free(buf); return 0; }
    free(buf);
    out("   %s rewritten with the pack changed inside it\n", evo_basename(dat));
    return n;
}

/* Patching a packed game.
 *
 * A pack inside an archive is squeezed, so it cannot be changed where it lies.
 * What can be done is what the game's own tools do: read it out, change it,
 * and write the archive again with the new pack spliced in where the old one
 * was. The change is four words long and the pack comes out the same size, so
 * nothing after it moves and only its checksum in the index differs.
 *
 * The archive is written beside the original and swapped in only once it is
 * whole, so a failure part way leaves the game as it was.
 */
static int rebuild_dat(const char *dat_path, const char *ndx_path,
                       Index *x, uint32_t which, uint8_t *pack, uint32_t size)
{
    DatReader r;
    Segment segs[3];
    size_t nseg = 0;
    uint64_t at = x->entries[which].offset, endpos = 0;
    uint32_t i;
    char tmp_dat[1700], tmp_ndx[1700];

    for (i = 0; i < x->count; i++) {
        uint64_t e;
        if (x->entries[i].dat != x->entries[which].dat) continue;
        e = x->entries[i].offset + (uint64_t)x->entries[i].size;
        if (e > endpos) endpos = e;
    }
    if (dat_open(&r, dat_path) != 0) return 0;

    if (at > 0) {
        segs[nseg].kind = SEG_DAT; segs[nseg].src = &r;
        segs[nseg].offset = 0; segs[nseg].size = at; nseg++;
    }
    segs[nseg].kind = SEG_MEM; segs[nseg].mem = pack; segs[nseg].size = size; nseg++;
    if (endpos > at + size) {
        segs[nseg].kind = SEG_DAT; segs[nseg].src = &r;
        segs[nseg].offset = at + size; segs[nseg].size = endpos - (at + size); nseg++;
    }

    snprintf(tmp_dat, sizeof tmp_dat, "%.1600s.new", dat_path);
    snprintf(tmp_ndx, sizeof tmp_ndx, "%.1600s.new", ndx_path);
    if (dat_write(tmp_dat, segs, nseg, r.filetime, r.version, r.buffer_size,
                  r.toc_offset, NULL, NULL) != 0) {
        dat_close(&r);
        remove(tmp_dat);
        out("   could not write the new archive\n");
        return 0;
    }
    dat_close(&r);

    evo_md5(pack, size, x->entries[which].md5);
    if (ndx_write(x, tmp_ndx) != 0) {
        remove(tmp_dat); remove(tmp_ndx);
        out("   could not write the new index\n");
        return 0;
    }
    /* both are whole, so put them in place */
    remove(dat_path); remove(ndx_path);
    if (rename(tmp_dat, dat_path) != 0 || rename(tmp_ndx, ndx_path) != 0) {
        out("   the new files could not be put in place\n");
        return 0;
    }
    return 1;
}

static void (*g_say)(const char *);

static void out(const char *fmt, ...)
{
    char b[1024];
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(b, sizeof b, fmt, ap);
    va_end(ap);
    if (g_say) {
        char win[1200];
        size_t i, j = 0;
        for (i = 0; b[i] && j + 2 < sizeof win; i++) {
            if (b[i] == '\n') win[j++] = '\r';
            win[j++] = b[i];
        }
        win[j] = 0;
        g_say(win);
    } else fputs(b, stdout);
}

static int ends_with(const char *s, const char *tail)
{
    size_t a = strlen(s), b = strlen(tail);
    return a >= b && !_stricmp_(s + a - b, tail);
}

static int sweep(const char *dir, int depth)
{
    DIR *d = opendir(dir);
    struct dirent *de;
    int total = 0;
    (void)depth;
    if (!d) return 0;
    while ((de = readdir(d))) {
        char full[1600];
        struct stat st;
        if (de->d_name[0] == '.') continue;
        snprintf(full, sizeof full, "%s/%s", dir, de->d_name);
        if (stat(full, &st) != 0) continue;

        if (S_ISDIR(st.st_mode)) continue;
        if (ends_with(de->d_name, ".dat")) {
            Index x;
            uint32_t k;
            char ndxp[1700];
            unsigned mine;
            if (sscanf(de->d_name, "game%u.dat", &mine) != 1) continue;
            snprintf(ndxp, sizeof ndxp, "%.1500s/game.ndx", dir);
            if (ndx_open(&x, ndxp) != 0) continue;
            for (k = 0; k < x.count; k++) {
                Entry *e = &x.entries[k];
                char loose[1700];
                struct stat s2;
                int got;
                if (!ends_with(e->name, ".rpk")) continue;
                /* the index covers every archive, so only the entries that
                 * belong to this one may be written into it. without this an
                 * offset from another archive lands somewhere arbitrary here. */
                if (e->dat != mine) continue;
                snprintf(loose, sizeof loose, "%.1500s/%.150s", dir, e->name);
                /* a loose copy is what the game reads, so it is the one to work
                 * on, whether that means changing it or putting it back */
                if (stat(loose, &s2) == 0) { total += patch_loose(loose); continue; }
                got = patch_in_dat(full, e->offset, e->size, ndxp, &x, k);
                if (got) {
                    (void)0;
                    total += got;
                }
            }
            ndx_close(&x);
        } else if (ends_with(de->d_name, ".rpk")) {
            int n = patch_loose(full);
            if (n > 0) {
                (void)0;
                total += n;
            }
        } else if (ends_with(de->d_name, ".ndx")) {
            Index x;
            uint32_t i;
            if (ndx_open(&x, full) != 0) continue;
            for (i = 0; i < x.count; i++) {
                char datp[1600], loose[1600];
                struct stat s2;
                Entry *e = &x.entries[i];
                if (!ends_with(e->name, ".rpk")) continue;
                snprintf(loose, sizeof loose, "%s/%s", dir, e->name);
                if (stat(loose, &s2) == 0) { total += patch_loose(loose); continue; }
                snprintf(datp, sizeof datp, "%s/game%03u.dat", dir, e->dat);
                if (stat(datp, &s2) != 0) continue;
                {
                    int n = patch_in_dat(datp, e->offset, e->size, full, &x, i);
                    if (n > 0) {
                        (void)0;
                        total += n;
                    }
                }
            }
            ndx_close(&x);
        }
    }
    closedir(d);
    return total;
}

int dcfix_sweep(const char *path, void (*say)(const char *))
{
    struct stat st;
    g_say = say;
    g_already = 0;
    if (stat(path, &st) == 0 && S_ISREG(st.st_mode)) {
        if (ends_with(path, ".dat")) {
            char ndx[1600], ndxfull[1700], *cut;
            Index x;
            uint32_t i;
            int total = 0;
            snprintf(ndx, sizeof ndx, "%.1500s", path);
            cut = strrchr(ndx, '/');
#ifdef _WIN32
            { char *c2 = strrchr(ndx, '\\'); if (c2 > cut) cut = c2; }
#endif
            if (cut) *cut = 0; else snprintf(ndx, sizeof ndx, ".");
            snprintf(ndxfull, sizeof ndxfull, "%.1500s/game.ndx", ndx);
            {
                if (ndx_open(&x, ndxfull) != 0) {
                    out("  no game.ndx beside that archive, so there is no way to\n"
                        "  tell where anything in it starts\n");
                    return 0;
                }
            }
            for (i = 0; i < x.count; i++) {
                Entry *e = &x.entries[i];
                if (!ends_with(e->name, ".rpk")) continue;
                total += patch_in_dat(path, e->offset, e->size, ndxfull, &x, i);
                if (total) { (void)(
                                 evo_basename(path)); break; }
            }
            ndx_close(&x);
            return total;
        }
        return patch_loose(path);
    }
    return sweep(path, 0);
}

int dcfix_already(void) { return g_already; }

/* the same sweep, putting the game's own words back */
int dcfix_undo(const char *path, void (*say)(const char *))
{
    int n;
    g_undo = 1;
    g_undone = 0;
    n = dcfix_sweep(path, say);
    g_undo = 0;
    return n;
}

#ifndef DCFIX_NO_MAIN
int main(int argc, char **argv)
{
    const char *dir = argc >= 2 ? argv[1] : ".";
    int n;

    if (argc > 2 && (!strcmp(argv[2], "-u") || !strcmp(argv[2], "--undo"))) {
        printf("DC Res Patch Depth Fix\n\n");
        if (dcfix_undo(dir, NULL) > 0) {
            printf("  REVERTED! Delete the shader cache before you play.\n");
            return 0;
        }
        printf("  NOTHING TO REVERT!\n");
        return 1;
    }
    printf("DC Res Patch Depth Fix\n\n");
    n = dcfix_sweep(dir, NULL);
    if (!n) {
        if (dcfix_already()) {
            if (g_edges_bad)
                printf("  BROKEN! Patch it again from a clean copy.\n");
            else
                printf("  ALREADY PATCHED!\n");
        } else {
            printf("  NOTHING TO PATCH! Point it at your game patch folder,\n"
                   "  the one holding global.rpk.\n");
        }
        return dcfix_already() ? 0 : 1;
    }
    printf("  PATCHED SUCCESSFULLY!\n");
    printf("  Delete the shader cache before you play.\n");
    return 0;
}
#endif
