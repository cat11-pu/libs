#define PALRLE_IMPLEMENTATION
#include <string.h>
#include "../palrle.h"
#include <stdio.h>
#include <stdlib.h>

static unsigned int rng_state;
static unsigned int xrnd(void){ rng_state ^= rng_state<<13; rng_state ^= rng_state>>17; rng_state ^= rng_state<<5; return rng_state; }

static unsigned char* buf;
static int g_w, g_h, g_nc;
static long g_fails, g_trials;

static int verify_nomask(void){
    unsigned int pal[256]; for(int i=0;i<256;i++) pal[i]=0xff000000u+i;
    unsigned char* dst=malloc((size_t)g_w*g_h);
    palrle_data_t* d=palrle_encode(buf,g_w,g_h,pal,g_nc,0);
    memset(dst,0xAA,(size_t)g_w*g_h);
    palrle_decode(d,dst,0);
    int bad=0;
    for(int i=0;i<g_w*g_h;i++) if(dst[i]!=buf[i]){bad++; if(bad<3)printf("  px %d (x=%d y=%d) got %d exp %d\n",i,i%g_w,i/g_w,dst[i],buf[i]);}
    free(dst); palrle_free(d,0);
    return bad;
}

int main(int argc,char**argv){
    int iters = argc>1?atoi(argv[1]):200000;
    int maxw=700, maxh=12;
    buf=malloc(maxw*maxh);
    int special[]={1,2,3,126,127,128,129,254,255,256,257};
    for(int it=0; it<iters; it++){
        rng_state=it*2654435761u+1;
        g_w=1+xrnd()%maxw;
        g_h=1+xrnd()%maxh;
        g_nc=1+xrnd()%8;
        int mode=xrnd()%4;
        for(int y=0;y<g_h;y++){
            int x=0, c=xrnd()%g_nc;
            while(x<g_w){
                int len;
                if(mode==0) len=1+xrnd()%4;
                else if(mode==1) len=1+xrnd()%300;
                else if(mode==2) len=special[xrnd()%(int)(sizeof(special)/sizeof(int))];
                else len=1+xrnd()%g_w;
                if(x+len>g_w) len=g_w-x;
                for(int t=0;t<len;t++) buf[y*g_w+x+t]=(unsigned char)c;
                x+=len; c=(c+1+xrnd()%(g_nc>1?g_nc-1:1))%g_nc;
            }
        }
        g_trials++;
        if(verify_nomask()){
            g_fails++;
            printf("FAIL iter=%d w=%d h=%d nc=%d mode=%d\n",it,g_w,g_h,g_nc,mode);
            if(g_fails>10) break;
        }
    }
    printf("trials=%ld fails=%ld\n",g_trials,g_fails);
    return g_fails?1:0;
}
