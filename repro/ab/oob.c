#include "ab.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
int main(void){
    unsigned int pal[8]; for(int i=0;i<8;i++)pal[i]=0xff000000u+i;
    int w=24,h=8,n=w*h;
    unsigned char* s=malloc(n);
    for(int i=0;i<n;i++) s[i]=(unsigned char)(i%6);
    new_palrle_data_t* g=new_palrle_encode(s,w,h,pal,6,0);
    size_t blob=g->size;
    unsigned char* raw=malloc(blob);            /* exact size -> tight redzone */
    memcpy(raw,g,blob);
    size_t pb=4u*(size_t)((new_palrle_data_t*)raw)->palette_count;
    unsigned int fr; memcpy(&fr,&((new_palrle_data_t*)raw)->data[pb],4);
    unsigned char* ch=&((new_palrle_data_t*)raw)->data[fr];
    ch[0]=0; ch[1]=(unsigned char)(-125);
    unsigned char* dst=calloc(n,1);
    printf("calling OLD decoder on corrupt exact-size blob...\n");
    old_palrle_decode((old_palrle_data_t*)raw,dst,0);
    printf("old decoder returned (unsafe)\n");
    free(dst);free(raw);new_palrle_free(g,0);free(s);
    return 0;
}
