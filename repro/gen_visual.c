#define PALRLE_IMPLEMENTATION
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include "../palrle.h"

/* Emit raw index grids + encoded RLE byte accounting for a visual repro. */
int main(void){
    unsigned int pal[8]; for(int i=0;i<8;i++) pal[i]=0xff000000u+i;
    /* A tall, narrow image (width=1) is the sharpest trigger: every pixel is its
       own 3-byte uniques chunk while the encoder only budgeted 2 bytes/pixel. */
    int w=1,h=16,n=w*h;
    unsigned char* src=malloc(n),*dst=malloc(n);
    for(int i=0;i<n;i++) src[i]=(unsigned char)(i&1);
    palrle_data_t* d=palrle_encode(src,w,h,pal,2,0);
    int rc=palrle_decode_safe(d,dst,0);
    FILE* f=fopen("repro/grid_w1.bin","wb");
    fwrite(src,1,n,f); fclose(f);
    printf("W1 h=%d decode_rc=%d identical=%d size=%u\n",h,rc,memcmp(src,dst,n)==0,d->size);
    palrle_free(d,0);

    /* A more visible 2D case: checkerboard, long rows, boundary height. */
    w=48;h=24;n=w*h;
    unsigned char* s2=malloc(n),*d2=malloc(n);
    for(int y=0;y<h;y++)for(int x=0;x<w;x++) s2[y*w+x]=(unsigned char)(((x/8)+(y/4))%6);
    palrle_data_t* d3=palrle_encode(s2,w,h,pal,6,0);
    int rc2=palrle_decode_safe(d3,d2,0);
    FILE* g=fopen("repro/grid_2d_src.bin","wb"); fwrite(s2,1,n,g); fclose(g);
    FILE* gg=fopen("repro/grid_2d_dst.bin","wb"); fwrite(d2,1,n,gg); fclose(gg);
    printf("2D w=%d h=%d decode_rc=%d identical=%d size=%u\n",w,h,rc2,memcmp(s2,d2,n)==0,d3->size);
    palrle_free(d3,0);
    free(src);free(dst);free(s2);free(d2);
    return 0;
}
