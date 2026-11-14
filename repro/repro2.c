#define PALRLE_IMPLEMENTATION
#include <string.h>
#include "../palrle.h"
#include <stdio.h>
#include <stdlib.h>

static unsigned int rng_state;
static unsigned int xrnd(void){ rng_state ^= rng_state<<13; rng_state ^= rng_state>>17; rng_state ^= rng_state<<5; return rng_state; }

static int cmp_buf(const char* tag, const unsigned char* src, const unsigned char* dst, int w,int h){
    int bad=0,fx=-1,fy=-1;
    for(int y=0;y<h;y++)for(int x=0;x<w;x++)
        if(dst[y*w+x]!=src[y*w+x]){ if(!bad){fx=x;fy=y;} bad++; }
    if(bad) printf("[%s] w=%d h=%d mismatches=%d/%d first@(%d,%d)\n",tag,w,h,bad,w*h,fx,fy);
    return bad;
}

static int test_nomask(const char* tag, unsigned char* src,int w,int h,int ncol){
    unsigned int pal[256]; for(int i=0;i<ncol;i++) pal[i]=0xff000000u+i;
    unsigned char* dst=malloc((size_t)w*h);
    palrle_data_t* d=palrle_encode(src,w,h,pal,ncol,0);
    memset(dst,0xAA,(size_t)w*h);
    palrle_decode(d,dst,0);
    int bad=cmp_buf(tag,src,dst,w,h);
    free(dst); palrle_free(d,0);
    return bad;
}

static int test_mask(const char* tag, unsigned char* src, unsigned char* mask,int w,int h,int ncol){
    unsigned int pal[256]; for(int i=0;i<ncol;i++) pal[i]=0xff000000u+i;
    unsigned char* dst=malloc((size_t)w*h); unsigned char* dmask=malloc((size_t)w*h);
    palrle_data_t* d=palrle_encode_mask(src,mask,w,h,pal,ncol,0);
    memset(dst,0xAA,(size_t)w*h); memset(dmask,0x55,(size_t)w*h);
    palrle_decode(d,dst,dmask);
    int bad=0;
    for(int y=0;y<h;y++)for(int x=0;x<w;x++){
        unsigned char exp_px = mask[y*w+x]? src[y*w+x]:0;
        unsigned char exp_mk = mask[y*w+x]? 255:0;
        if(dst[y*w+x]!=exp_px || dmask[y*w+x]!=exp_mk){ if(!bad)printf("[%s] first mismatch@(%d,%d) px got %d exp %d mask got %d exp %d (w=%d h=%d)\n",tag,x,y,dst[y*w+x],exp_px,dmask[y*w+x],exp_mk,w,h); bad++; }
    }
    if(bad) printf("[%s] w=%d h=%d total mismatches=%d\n",tag,w,h,bad);
    free(dst);free(dmask);palrle_free(d,0);
    return bad;
}

int main(void){
    int fails=0;
    int Ws[]={126,127,128,129,130,254,255,256,257,300,511,512,513,1000,2000};
    int Hs[]={1,2,3,7,8,9,64,127,128,129,300};
    for(unsigned a=0;a<sizeof(Ws)/sizeof(int);a++)
      for(unsigned b=0;b<sizeof(Hs)/sizeof(int);b++){
        int w=Ws[a],h=Hs[b],n=w*h;
        unsigned char* s=malloc(n); unsigned char* m=malloc(n);
        // pattern 1: whole row solid, color alternates by row
        for(int y=0;y<h;y++)for(int x=0;x<w;x++) s[y*w+x]=(unsigned char)(y%7);
        fails+=test_nomask("solid-rows",s,w,h,8);
        // pattern 2: vertical stripes (every pixel unique-ish, 2 colors)
        for(int y=0;y<h;y++)for(int x=0;x<w;x++) s[y*w+x]=(unsigned char)(x%3);
        fails+=test_nomask("vstripes",s,w,h,8);
        // pattern 3: blocks that create runs of exactly 127/128
        for(int y=0;y<h;y++)for(int x=0;x<w;x++) s[y*w+x]=(unsigned char)((x/127)%4);
        fails+=test_nomask("run127",s,w,h,8);
        for(int y=0;y<h;y++)for(int x=0;x<w;x++) s[y*w+x]=(unsigned char)((x/128)%4);
        fails+=test_nomask("run128",s,w,h,8);
        // pattern 4: long runs split across uniques/run boundary (3+1)
        for(int y=0;y<h;y++)for(int x=0;x<w;x++) s[y*w+x]=(unsigned char)((x%4==3)?2:(x%2));
        fails+=test_nomask("mixed",s,w,h,8);
        // pattern 5: random with full 256 palette
        rng_state=w*7349u+h*131u+17;
        for(int i=0;i<n;i++) s[i]=(unsigned char)(xrnd()&0xff);
        fails+=test_nomask("rand256",s,w,h,256);
        // mask tests
        rng_state=w*131u+h*7u+5;
        for(int i=0;i<n;i++){ s[i]=(unsigned char)(xrnd()%9); m[i]=(xrnd()&3)?255:0; }
        fails+=test_mask("mask-rand",s,m,w,h,16);
        // mask with big empty spans and solid runs
        for(int y=0;y<h;y++)for(int x=0;x<w;x++){
            int active=(x>=20 && x<w-10) && ((y%3)!=0);
            m[y*w+x]=active?255:0; s[y*w+x]=(unsigned char)(y%5);
        }
        fails+=test_mask("mask-spans",s,m,w,h,8);
        free(s);free(m);
      }
    printf("TOTAL FAILING CASES: %d\n",fails);
    return fails?1:0;
}
