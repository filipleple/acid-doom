#include "doomtype.h"
#include "v_video.h"
#include "i_video.h"
#include "v_video.h"   // SCREENWIDTH, SCREENHEIGHT, I_VideoBuffer
#include "w_wad.h"
#include <string.h>

extern int viewwindowx, viewwindowy, viewwidth, viewheight;

#include "w_wad.h"
#include "z_zone.h"
#include "deh_main.h"    // DEH_String for PLAYPAL name

static byte *pp_tinttable = NULL;   // our pointer (don’t use V_LoadTintTable/tinttable)
static int   pp_tinttable_ready = 0;




static void PP_EnsureTintTable(void)
{
    if (pp_tinttable_ready) return;

    // 1) try to load TINTTAB if present
    int lump = W_CheckNumForName("TINTTAB");
    if (lump >= 0) {
        pp_tinttable = W_CacheLumpNum(lump, PU_STATIC);
        pp_tinttable_ready = 1;
        return;
    }

    // 2) generate from PLAYPAL (first palette)
    //    allocate 256*256 table
    pp_tinttable = Z_Malloc(256 * 256, PU_STATIC, NULL);

    // load palette (768 bytes = 256 * (r,g,b))
    const byte *playpal = W_CacheLumpName(DEH_String("PLAYPAL"), PU_STATIC);

    // build RGB arrays for faster access
    int pr[256], pg[256], pb[256];
    for (int i = 0; i < 256; ++i) {
        pr[i] = playpal[i*3 + 0];
        pg[i] = playpal[i*3 + 1];
        pb[i] = playpal[i*3 + 2];
    }

    // for every pair (a,b), compute 50/50 blend in RGB and map to nearest palette index
    for (int a = 0; a < 256; ++a) {
        for (int b = 0; b < 256; ++b) {
            int r = (pr[a] + pr[b]) >> 1;
            int g = (pg[a] + pg[b]) >> 1;
            int bl = (pb[a] + pb[b]) >> 1;

            // find nearest palette color (euclidean in RGB)
            int best = 0;
            int bestd = 1<<30;
            for (int i = 0; i < 256; ++i) {
                int dr = r - pr[i];
                int dg = g - pg[i];
                int db = bl - pb[i];
                int d = dr*dr + dg*dg + db*db;
                if (d < bestd) { bestd = d; best = i; }
            }
            pp_tinttable[(a<<8) | b] = (byte)best;
        }
    }

    pp_tinttable_ready = 1;
}




static inline byte blend50(byte a, byte b)
{
    PP_EnsureTintTable();
    if (!pp_tinttable) return a; // fail-safe
    return pp_tinttable[(a << 8) | b];
}




void PP_BoxBlur8(byte *buf, int w, int h, int stride, int radius)
{
    if (!buf || radius <= 0) return;

    // Horizontal passes
    for (int pass = 0; pass < radius; ++pass) {
        for (int y = 0; y < h; ++y) {
            byte *row = buf + y * stride;
            byte prev = row[0];
            for (int x = 1; x < w; ++x) { byte cur = row[x]; byte m = blend50(cur, prev); row[x] = m; prev = m; }
            byte next = row[w-1];
            for (int x = w-2; x >= 0; --x) { byte cur = row[x]; byte m = blend50(cur, next); row[x] = m; next = m; }
        }
    }

    // Vertical passes
    for (int pass = 0; pass < radius; ++pass) {
        for (int x = 0; x < w; ++x) {
            int idx = x;
            byte prev = buf[idx];
            for (int y = 1; y < h; ++y) { idx += stride; byte cur = buf[idx]; byte m = blend50(cur, prev); buf[idx] = m; prev = m; }
            int idx2 = (h-1)*stride + x;
            byte next = buf[idx2];
            for (int y = h-2; y >= 0; --y) { idx2 -= stride; byte cur = buf[idx2]; byte m = blend50(cur, next); buf[idx2] = m; next = m; }
        }
    }
}


// At file scope:
static byte *pp_tmp = NULL;

static void ensure_tmp(void) {
    if (!pp_tmp) pp_tmp = Z_Malloc(SCREENWIDTH * SCREENHEIGHT, PU_STATIC, NULL);
}

// Separable 1D 50/50 “box” blur using tmp as the other buffer.
static inline void blur_h(byte *dst, byte *src, int w, int h, int stride) {
    for (int y = 0; y < h; ++y) {
        byte *s = src + y*stride, *d = dst + y*stride;
        d[0] = s[0];
        for (int x = 1; x < w-1; ++x) {
            // 0.5*left + 0.5*right, approximated with two blends
            byte m = blend50(s[x-1], s[x+1]);
            d[x] = m;
        }
        d[w-1] = s[w-1];
    }
}
static inline void blur_v(byte *dst, byte *src, int w, int h, int stride) {
    for (int x = 0; x < w; ++x) {
        dst[x] = src[x];
        for (int y = 1; y < h-1; ++y) {
            byte a = src[(y-1)*stride + x];
            byte b = src[(y+1)*stride + x];
            dst[y*stride + x] = blend50(a, b);
        }
        dst[(h-1)*stride + x] = src[(h-1)*stride + x];
    }
}
static void PP_Blur2(byte *view, int w, int h, int stride, int passes) {
    ensure_tmp();
    // copy view rect to tmp
    for (int y = 0; y < h; ++y) memcpy(pp_tmp + y*stride, view + y*stride, w);
    for (int i = 0; i < passes; ++i) {
        blur_h(view, pp_tmp, w, h, stride);
        blur_v(pp_tmp, view, w, h, stride);
    }
    // final is in pp_tmp after last vertical; copy back
    for (int y = 0; y < h; ++y) memcpy(view + y*stride, pp_tmp + y*stride, w);
}




void ApplyPost(byte *video)
{
    // Operate only on the view window so the status bar / borders stay crisp
    byte *view = video + viewwindowy * SCREENWIDTH + viewwindowx;
    // PP_BoxBlur8(view, viewwidth, viewheight, SCREENWIDTH, 2);
    PP_Blur2(view, viewwidth, viewheight, SCREENWIDTH, 2);
}

