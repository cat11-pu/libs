#define PALRLE_IMPLEMENTATION
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include "../palrle.h"
static unsigned int st; static unsigned int rnd(void){st^=st<<13;st^=st>>17;st^=st<<5;return st;}
int main(void){
    st=99; int w=40,h=6; size_t n=(size_t)w*h;
    unsigned char*s=malloc(n),*d=malloc(n);
    for(size_t i=0;i<n;i++)s[i]=(unsigned char)(rnd()%6);
    unsigned int pal[256]; for(int i=0;i<256;i++)pal[i]=i;
    palrle_data_t* g=palrle_encode(s,w,h,pal,6,0);
    size_t blob=g->size;
    unsigned char* raw=malloc(blob); memcpy(raw,g,blob);
    palrle_data_t* t=(palrle_data_t*)raw;
    t->size=(PALRLE_U32)(blob+64);
    printf("rc oversize = %d (blob=%zu)\n", palrle_decode_safe(t,d,0), blob);
    return 0;
}
