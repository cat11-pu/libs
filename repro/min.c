#define PALRLE_IMPLEMENTATION
#include <string.h>
#include "../palrle.h"
#include <stdio.h>
#include <stdlib.h>

static unsigned int rng_state;
static unsigned int xrnd(void){ rng_state ^= rng_state<<13; rng_state ^= rng_state>>17; rng_state ^= rng_state<<5; return rng_state; }

int main(int argc,char**argv){
    unsigned int start = argc>1?(unsigned)strtoul(argv[1],0,10):0;
    int maxw=700, maxh=12;
    unsigned char* buf=malloc(maxw*maxh);
    int special[]={1,2,3,126,127,128,129,254,255,256,257};
    for(int it=start;;it++){
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
        unsigned char* dst=malloc((size_t)w*h);
        palrle_data_t* d=palrle_encode(buf,w,h,pal,nc,0);
        memset(dst,0xAA,(size_t)w*h);
        palrle_decode(d,dst,0);
        int bad=0;
        for(int i=0;i<w*h;i++) if(dst[i]!=buf[i]) bad++;
        if(bad){
            printf("ROUNDTRIP FAIL iter=%d w=%d h=%d nc=%d mode=%d bad=%d/%d\n",it,w,h,nc,mode,bad,w*h);
            printf("row0: "); for(int x=0;x<(w<40?w:40);x++) printf("%d ",buf[x]); printf("\n");
            free(dst);palrle_free(d,0);free(buf);
            return 0;
        }
        free(dst); palrle_free(d,0);
    }
}
