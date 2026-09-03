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

/* say what is in there, not only whether something is */
static int g_verbose;

/* the edges did not come out right, so the shader is not usable */
static int g_edges_bad;
static int g_already;
int dcfix_already(void);
int dcfix_sweep(const char *dir, void (*say)(const char *));

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

static int owned_by_depth_pass(const Pack *pk, const uint8_t *buf, size_t len, uint32_t which)
{
    uint32_t j;
    for (j = 0; j < pk->count; j++) {
        size_t q = pk->res[j].rec + 8;
        uint16_t nd, k;
        char tmp[256];
        const char *nm;
        if (q + 2 > len) continue;
        nd = (uint16_t)(buf[q] | (buf[q + 1] << 8));
        for (k = 0; k < nd && q + 2 + (size_t)(k + 1) * 8 <= len; k++) {
            uint64_t dep = 0;
            size_t b;
            for (b = 0; b < 8; b++)
                dep |= (uint64_t)buf[q + 2 + (size_t)k * 8 + b] << (b * 8);
            if (dep != pk->res[which].uid) continue;
            nm = rpk_name(pk, buf, j, tmp);
            if (nm && strstr(nm, OWNER)) return 1;
        }
    }
    return 0;
}

static int fix_pack(uint8_t *buf, size_t len)
{
    Pack pk;
    uint32_t i;
    int done = 0;

    if (rpk_parse(&pk, buf, len) != 0) return -1;
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
            for (o = 0; o + 28 <= sz; o++) {
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
            /* Already done, and worth saying what is in there rather than only that... */
            for (o = 0; o + 8 <= sz && !done; o++) {
                uint32_t w = rd32(p + o);
                if ((w >> 30) == 2 && ((w >> 23) & 0x7F) == 41 &&
                    ((w >> 8) & 0xFF) == 0xFF && rd32(p + o + 4) == 0x000E0000u) {
                    g_already++;
                    if (g_verbose) {
                        uint32_t wx = (w >> 16) & 0x7F, wy = 0;
                        int k, ge = 0;
                        if (o + 12 <= sz) {
                            uint32_t h = rd32(p + o + 8);
                            if ((h >> 30) == 2 && ((h >> 23) & 0x7F) == 41)
                                wy = (h >> 16) & 0x7F;
                        }
                        (void)wx; (void)wy;
                        for (k = 0; k < 16 && o + 16 + (size_t)k * 4 + 4 <= sz; k++) {
                            uint32_t c = rd32(p + o + 16 + (size_t)k * 4);
                            if ((c >> 25) == 0x3E && ((c >> 17) & 0xFF) == 0xC6) ge++;
                            else if ((c >> 26) == 0x34 && ((c >> 17) & 0x1FF) == 0xC6) ge++;
                        }
                        (void)0;
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

static int patch_in_dat(const char *dat, uint64_t at, uint32_t size)
{
    FILE *f = fopen(dat, "r+b");
    uint8_t *buf;
    int n;
    if (!f) return 0;
    buf = (uint8_t *)malloc(size);
    if (!buf) { fclose(f); return 0; }
    if (FSEEK(f, (int64_t)at, SEEK_SET) != 0 || fread(buf, 1, size, f) != size) {
        free(buf); fclose(f); return 0;
    }
    if (!could_hold_it(buf, size)) { free(buf); fclose(f); return 0; }
    n = fix_pack(buf, size);
    if (n <= 0) { free(buf); fclose(f); return 0; }
    if (FSEEK(f, (int64_t)at, SEEK_SET) != 0 || fwrite(buf, 1, size, f) != size) {
        free(buf); fclose(f);
        out("   could not write it back into %s\n", dat);
        return 0;
    }
    free(buf);
    if (fclose(f) != 0) { out("   %s did not close cleanly\n", dat); return 0; }
    return n;
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
            snprintf(ndxp, sizeof ndxp, "%.1500s/game.ndx", dir);
            if (ndx_open(&x, ndxp) != 0) continue;
            for (k = 0; k < x.count; k++) {
                Entry *e = &x.entries[k];
                char loose[1700];
                struct stat s2;
                int got;
                if (!ends_with(e->name, ".rpk")) continue;
                snprintf(loose, sizeof loose, "%.1500s/%.150s", dir, e->name);
                if (stat(loose, &s2) == 0) continue;
                got = patch_in_dat(full, e->offset, e->size);
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
                if (stat(loose, &s2) == 0) continue;
                snprintf(datp, sizeof datp, "%s/game%03u.dat", dir, e->dat);
                if (stat(datp, &s2) != 0) continue;
                {
                    int n = patch_in_dat(datp, e->offset, e->size);
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
            char ndx[1600], *cut;
            Index x;
            uint32_t i;
            int total = 0;
            snprintf(ndx, sizeof ndx, "%.1500s", path);
            cut = strrchr(ndx, '/');
#ifdef _WIN32
            { char *c2 = strrchr(ndx, '\\'); if (c2 > cut) cut = c2; }
#endif
            if (cut) *cut = 0; else snprintf(ndx, sizeof ndx, ".");
            {
                char full[1700];
                snprintf(full, sizeof full, "%.1500s/game.ndx", ndx);
                if (ndx_open(&x, full) != 0) {
                    out("  no game.ndx beside that archive, so there is no way to\n"
                        "  tell where anything in it starts\n");
                    return 0;
                }
            }
            for (i = 0; i < x.count; i++) {
                Entry *e = &x.entries[i];
                if (!ends_with(e->name, ".rpk")) continue;
                total += patch_in_dat(path, e->offset, e->size);
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

#ifndef DCFIX_NO_MAIN
int main(int argc, char **argv)
{
    const char *dir = argc >= 2 ? argv[1] : ".";
    int n;

    if (argc > 2 && (!strcmp(argv[2], "-v") || !strcmp(argv[2], "--check")))
        g_verbose = 1;
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
