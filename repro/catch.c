#define PALRLE_IMPLEMENTATION
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <stddef.h>
#include "../palrle.h"

static unsigned int rng_state;
static unsigned int xrnd(void){ rng_state ^= rng_state<<13; rng_state ^= rng_state>>17; rng_state ^= rng_state<<5; return rng_state; }

int main(void){
    int maxw=700, maxh=12;
    unsigned char* buf=malloc(maxw*maxh);
    int special[]={1,2,3,126,127,128,129,254,255,256,257};
    for(int it=0;;it++){
        rng_state=it*2654435761u+1;
        int w=1+xrnd()%maxw, h=1+xrnd()%maxh, nc=1+xrnd()%8;
        int mode=xrnd()%4;
        for(int y=0;y<h;y++){
            int x=0,c=xrnd()%nc;
            while(x<w){
                int len = mode==0?1+xrnd()%4 : mode==1?1+xrnd()%300 :
                          mode==2?special[xrnd()%(int)(sizeof(special)/sizeof(int))] : 1+xrnd()%w;
                if(x+len>w) len=w-x;
                for(int t=0;t<len;t++) buf[y*w+x+t]=(unsigned char)c;
                x+=len; c=(c+1+xrnd()%(nc>1?nc-1:1))%nc;
            }
        }
        unsigned int pal[256]; for(int i=0;i<256;i++) pal[i]=0xff000000u+i;
        palrle_data_t* d=palrle_encode(buf,w,h,pal,nc,0);
        size_t hdr=sizeof(palrle_data_t)-sizeof(unsigned char);
        size_t rle_used = d->size - (hdr + 4u*(unsigned)nc);
        size_t rle_reserved = 4u*(unsigned)h + (size_t)w*h*2;
        if(rle_used > rle_reserved){
            printf("OVERRUN iter=%d w=%d h=%d nc=%d mode=%d\n",it,w,h,nc,mode);
            printf("RLE reserved=%zu written=%zu overrun=%zd\n",
                rle_reserved, rle_used, (ptrdiff_t)rle_used-(ptrdiff_t)rle_reserved);
            printf("chunk bytes/pixel = %.3f\n", (double)rle_used/((size_t)w*h));
            palrle_free(d,0); free(buf); return 0;
        }
        palrle_free(d,0);
    }
}
