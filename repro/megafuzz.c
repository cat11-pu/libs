#define PALRLE_IMPLEMENTATION
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include "../palrle.h"
static unsigned int st; static unsigned int rnd(void){st^=st<<13;st^=st>>17;st^=st<<5;return st;}
int main(int argc,char**argv){
    int iters=argc>1?atoi(argv[1]):300000;
    unsigned int pal[256]; for(int i=0;i<256;i++)pal[i]=0xff000000u+i*11;
    int maxw=600,maxh=20;
    unsigned char*s=malloc(maxw*maxh),*m=malloc(maxw*maxh),*d=malloc(maxw*maxh),*dm=malloc(maxw*maxh);
    long bad=0;
    for(int it=0;it<iters;it++){
        st=it*40503u+12345u;
        int w=1+rnd()%maxw,h=1+rnd()%maxh,nc=1+rnd()%256;
        size_t n=(size_t)w*h; int domask=rnd()&1; int sparse=rnd()%3;
        for(size_t i=0;i<n;i++){ s[i]=(unsigned char)(rnd()%nc); m[i]= sparse==0? (rnd()&1?255:0) : sparse==1? ((rnd()%7)==0?255:0):255; }
        palrle_data_t* dd = domask? palrle_encode_mask(s,m,w,h,pal,nc,0) : palrle_encode(s,w,h,pal,nc,0);
        if(!dd){ printf("alloc fail it=%d\n",it); bad++; continue; }
        memset(d,0xC3,n); memset(dm,0x3C,n);
        int rc = domask? palrle_decode_safe(dd,d,dm): palrle_decode_safe(dd,d,0);
        if(rc!=PALRLE_OK){ printf("rc=%d it=%d w=%d h=%d\n",rc,it,w,h); bad++; palrle_free(dd,0); continue; }
        for(size_t i=0;i<n;i++){
            int active = domask? (m[i]!=0):1;
            unsigned char ep = active? s[i]:0, em = (domask&&active)?255:0;
            if(d[i]!=ep || (domask && dm[i]!=em)){ if(bad<5)printf("mismatch it=%d i=%zu domask=%d\n",it,i,domask); bad++; break; }
        }
        palrle_free(dd,0);
    }
    printf("iters=%d bad=%ld -> %s\n",iters,bad,bad?"FAIL":"OK");
    return bad?1:0;
}
