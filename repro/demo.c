#define PALRLE_IMPLEMENTATION
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <stddef.h>
#include "../palrle.h"

static void show(const char* tag, palrle_data_t* d, size_t rle_reserved){
    size_t hdr      = sizeof(palrle_data_t)-sizeof(unsigned char);
    size_t palette  = sizeof(unsigned int)*d->palette_count;
    size_t offsets  = sizeof(unsigned int)*d->vpitch;
    size_t rle_used = d->size - (hdr + palette);
    printf("[%s] size field=%u  hdr=%zu palette=%zu offsets=%zu\n", tag, d->size, hdr, palette, offsets);
    printf("[%s] RLE region reserved=%zu  RLE bytes actually written=%zu  overrun=%zd\n",
        tag, rle_reserved, rle_used, (ptrdiff_t)rle_used-(ptrdiff_t)rle_reserved);
}

int main(void){
    int w=32,h=4,n=w*h;
    unsigned char* src=malloc(n), *dst=malloc(n), *mask=malloc(n);
    for(int y=0;y<h;y++)for(int x=0;x<w;x++){ src[y*w+x]=(unsigned char)((x+y)&1); mask[y*w+x]=255; }
    unsigned int pal[2]={0xff0000,0x00ff00};

    palrle_data_t* d=palrle_encode(src,w,h,pal,2,0);
    show("nomask", d, (size_t)w*h*2);
    memset(dst,0xAA,n); palrle_decode(d,dst,0);
    int bad=0; for(int i=0;i<n;i++) if(dst[i]!=src[i]) bad++;
    printf("[nomask] decode mismatches=%d/%d (stream itself self-consistent; damage is to adjacent heap)\n\n",bad,n);
    palrle_free(d,0);

    palrle_data_t* dm=palrle_encode_mask(src,mask,w,h,pal,2,0);
    show("mask", dm, (size_t)w*h*2);
    free(src);free(dst);free(mask);
    return 0;
}
