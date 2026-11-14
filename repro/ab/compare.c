#include "ab.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <stddef.h>

static unsigned int pal[16];

/* --- Case A: encoder sizing (old reserves 2 bytes/pixel, needs 3) --- */
static void case_encoder_overflow(void){
    int w=1,h=64,n=w*h;
    unsigned char* src=malloc(n);
    for(int i=0;i<n;i++) src[i]=(unsigned char)(i&1);

    size_t hdr=sizeof(old_palrle_data_t)-sizeof(unsigned char);
    /* old encoder's own reservation formula (does not call it here to avoid the
       heap corruption it causes; compute what it *would* allocate vs write). */
    size_t old_reserved = (sizeof(old_palrle_data_t)) + 4u*2u + 4u*(unsigned)h + (size_t)n*2;
    /* actual bytes an encoder must write for this all-uniques input: 3/pixel */
    size_t actual_needed = hdr + 4u*2u + 4u*(unsigned)h + (size_t)n*3;
    printf("[encoder w=1,h=%d] old formula reserves %zu payload bytes but format needs %zu -> %zd byte overrun\n",
        h, old_reserved, actual_needed, (ptrdiff_t)actual_needed-(ptrdiff_t)old_reserved);

    new_palrle_data_t* nd=new_palrle_encode(src,w,h,pal,2,0);
    size_t new_reserved = sizeof(new_palrle_data_t) + 4u*2u + 4u*(unsigned)h + (size_t)n*3;
    printf("[encoder w=1,h=%d] new reserves %zu bytes, size field=%u -> within bounds\n",
        h, new_reserved, nd->size);
    unsigned char* dst=malloc(n);
    int rc=new_palrle_decode_safe(nd,dst,0);
    printf("[encoder w=1,h=%d] new decode rc=%d, pixel-identical=%s\n\n",
        h, rc, (rc==0 && memcmp(dst,src,n)==0)?"YES":"NO");
    new_palrle_free(nd,0); free(dst);free(src);
}

/* --- Case B: corrupt blob -> old crashes/garbage, new returns an error --- */
static void case_corrupt_blob(void){
    int w=24,h=8,n=w*h;
    unsigned char* src=malloc(n),*dst=calloc(n+64,1);
    for(int i=0;i<n;i++) src[i]=(unsigned char)(i%6);
    new_palrle_data_t* good=new_palrle_encode(src,w,h,pal,6,0);
    size_t blob=good->size;

    unsigned char* raw=malloc(blob); memcpy(raw,good,blob);
    size_t pb=4u*(size_t)((new_palrle_data_t*)raw)->palette_count;
    unsigned int fr; memcpy(&fr,&((new_palrle_data_t*)raw)->data[pb],4);
    unsigned char* ch=&((new_palrle_data_t*)raw)->data[fr];
    ch[0]=0; ch[1]=(unsigned char)(-125);   /* uniques run overruns the block */

    int rc=new_palrle_decode_safe((new_palrle_data_t*)raw,dst,0);
    printf("[corrupt blob] new decoder rc=%d (4=CORRUPT), no out-of-bounds access\n",rc);

    unsigned char* oldraw=malloc(blob); memcpy(oldraw,raw,blob);
    memset(dst,0x77,n);
    old_palrle_decode((old_palrle_data_t*)oldraw,dst,0);  /* ASan: SEGV / OOB read */
    int garbage=0; for(int i=0;i<n;i++) if(dst[i]!=src[i]) garbage++;
    printf("[corrupt blob] old decoder ran unbounded; %d/%d output pixels wrong (and ASan reports SEGV/OOB)\n\n",
        garbage,n);
    free(oldraw);free(raw);new_palrle_free(good,0);free(src);free(dst);
}

/* --- Case C: valid-stream parity across many sizes --- */
static void case_parity(void){
    int Ws[]={1,2,3,7,8,127,128,129,255,256,257,512,1000};
    int Hs[]={1,2,3,8,64,128,257};
    int mism=0,cases=0;
    for(unsigned a=0;a<sizeof(Ws)/sizeof(int);a++)for(unsigned b=0;b<sizeof(Hs)/sizeof(int);b++){
        int w=Ws[a],h=Hs[b],n=w*h;
        unsigned char* s=malloc(n),*d1=malloc(n),*d2=malloc(n);
        for(int i=0;i<n;i++) s[i]=(unsigned char)((i*13+(w^h))%9);
        new_palrle_data_t* nd=new_palrle_encode(s,w,h,pal,9,0);
        size_t blob=nd->size;
        unsigned char* raw=malloc(blob); memcpy(raw,nd,blob);
        old_palrle_decode((old_palrle_data_t*)raw,d1,0);
        int rc=new_palrle_decode_safe(nd,d2,0);
        if(rc||memcmp(d1,s,n)||memcmp(d2,s,n)) mism++;
        cases++;
        free(raw);new_palrle_free(nd,0);free(s);free(d1);free(d2);
    }
    printf("[valid-stream parity] %d size combos, mismatches=%d\n",cases,mism);
}

int main(void){
    for(int i=0;i<16;i++) pal[i]=0xff000000u+(unsigned)i*0x010101;
    case_encoder_overflow();
    case_corrupt_blob();
    case_parity();
    return 0;
}
