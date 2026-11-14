#define PALRLE_IMPLEMENTATION
#include <string.h>
#include "../palrle.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static unsigned int rng_state = 12345;
static unsigned int xrnd(void){ rng_state ^= rng_state << 13; rng_state ^= rng_state >> 17; rng_state ^= rng_state << 5; return rng_state; }

static int roundtrip_nomask(int w, int h, int ncolors, unsigned int seed){
    rng_state = seed;
    unsigned char* src = malloc((size_t)w*h);
    unsigned char* dst = malloc((size_t)w*h);
    unsigned int pal[256];
    for(int i=0;i<ncolors;i++) pal[i]=0xff000000u|(i*11);
    for(int i=0;i<w*h;i++) src[i]=(unsigned char)(xrnd()%ncolors);
    palrle_data_t* d = palrle_encode(src,w,h,pal,ncolors,0);
    memset(dst,0xAA,(size_t)w*h);
    palrle_decode(d,dst,0);
    int bad=0, firstx=-1, firsty=-1;
    for(int y=0;y<h;y++) for(int x=0;x<w;x++)
        if(dst[y*w+x]!=src[y*w+x]){ if(!bad){firstx=x;firsty=y;} bad++; }
    if(bad) printf("[nomask] w=%d h=%d colors=%d seed=%u  mismatches=%d/%d first@(%d,%d)\n",
        w,h,ncolors,seed,bad,w*h,firstx,firsty);
    free(src);free(dst);palrle_free(d,0);
    return bad;
}

int main(void){
    int total_bad=0;
    int widths[] = {1,2,3,4,5,7,8,15,16,17,31,63,127,128,129,130,200,254,255,256,257,300,500,511,512,513,1000};
    int heights[]= {1,2,3,5,10,33,100,257};
    for(unsigned wi=0;wi<sizeof(widths)/sizeof(int);wi++)
      for(unsigned hi=0;hi<sizeof(heights)/sizeof(int);hi++)
        for(int c=2;c<=6;c+=2)
          for(unsigned s=1;s<=4;s++)
            total_bad += (roundtrip_nomask(widths[wi],heights[hi],c,s*7919u+wi*131+hi)>0);
    printf("nomask done, failing cases=%d\n", total_bad);
    return 0;
}
