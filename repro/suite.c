#define PALRLE_IMPLEMENTATION
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include "../palrle.h"

static unsigned int st;
static unsigned int rnd(void){ st^=st<<13; st^=st>>17; st^=st<<5; return st; }

static long checks, fails;
#define CHECK(cond,msg) do{ checks++; if(!(cond)){ fails++; printf("FAIL: %s (%s:%d)\n",msg,__FILE__,__LINE__);} }while(0)

static unsigned int pal[256];

static void roundtrip_nomask(int w,int h,int nc,unsigned seed){
    st=seed?seed:1;
    size_t n=(size_t)w*h;
    unsigned char* src=(unsigned char*)malloc(n),*dst=(unsigned char*)malloc(n);
    for(size_t i=0;i<n;i++) src[i]=(unsigned char)(rnd()%nc);
    palrle_data_t* d=palrle_encode(src,w,h,pal,nc,0);
    CHECK(d!=0,"encode alloc");
    if(!d){ free(src);free(dst); return; }
    CHECK((size_t)d->size <= sizeof(palrle_data_t)+(size_t)nc*4+(size_t)h*4+(size_t)n*3,"size within 3n bound");
    memset(dst,0xA5,n);
    int rc=palrle_decode_safe(d,dst,0);
    CHECK(rc==PALRLE_OK,"decode rc");
    CHECK(memcmp(src,dst,n)==0,"nomask pixel identical");
    palrle_free(d,0); free(src);free(dst);
}

static void roundtrip_mask(int w,int h,int nc,unsigned seed,int sparse){
    st=seed?seed:7;
    size_t n=(size_t)w*h;
    unsigned char* src=(unsigned char*)malloc(n),*msk=(unsigned char*)malloc(n),*dst=(unsigned char*)malloc(n),*dm=(unsigned char*)malloc(n);
    for(size_t i=0;i<n;i++){ src[i]=(unsigned char)(rnd()%nc); msk[i]= sparse ? ((rnd()%5)==0?255:0) : (rnd()&1?255:0); }
    palrle_data_t* d=palrle_encode_mask(src,msk,w,h,pal,nc,0);
    CHECK(d!=0,"mask encode alloc");
    if(!d){ free(src);free(msk);free(dst);free(dm);return; }
    CHECK((size_t)d->size <= sizeof(palrle_data_t)+(size_t)nc*4+(size_t)h*4+(size_t)n*5,"size within 5n bound");
    memset(dst,0xA5,n); memset(dm,0x5A,n);
    int rc=palrle_decode_safe(d,dst,dm);
    CHECK(rc==PALRLE_OK,"mask decode rc");
    int bad=0;
    for(size_t i=0;i<n;i++){
        unsigned char ep=msk[i]?src[i]:0, em=msk[i]?255:0;
        if(dst[i]!=ep||dm[i]!=em) bad++;
    }
    CHECK(bad==0,"mask pixel+mask identical");
    palrle_free(d,0); free(src);free(msk);free(dst);free(dm);
}

int main(void){
    for(int i=0;i<256;i++) pal[i]=0xff000000u+(unsigned)i*7;

    /* 1. boundary dimensions incl. the exact original overflow (w=1,h=4) and 127/128/255/256/257 */
    int Ws[]={1,2,3,4,5,7,8,15,16,17,31,63,126,127,128,129,130,254,255,256,257,300,511,512,513,1000,2000};
    int Hs[]={1,2,3,4,7,8,9,63,64,127,128,129,255,256,257,300};
    for(unsigned a=0;a<sizeof(Ws)/sizeof(int);a++)
      for(unsigned b=0;b<sizeof(Hs)/sizeof(int);b++){
        roundtrip_nomask(Ws[a],Hs[b],1+(a+b)%9, 100u+a*131+b*17);
        roundtrip_nomask(Ws[a],Hs[b],256, 200u+a*7+b);
        roundtrip_mask(Ws[a],Hs[b],1+(a+b)%9, 300u+a*13+b,0);
        roundtrip_mask(Ws[a],Hs[b],16, 400u+a*17+b,1);
      }

    /* 2. structured patterns: solid rows, vertical stripes, exact-127/128 runs, checkerboard (worst case 3 B/px) */
    for(unsigned a=0;a<sizeof(Ws)/sizeof(int);a++)
      for(unsigned b=0;b<sizeof(Hs)/sizeof(int);b++){
        int w=Ws[a],h=Hs[b]; size_t n=(size_t)w*h;
        unsigned char* s=(unsigned char*)malloc(n),*d=(unsigned char*)malloc(n);
        for(int pat=0;pat<5;pat++){
          for(int y=0;y<h;y++)for(int x=0;x<w;x++)
            s[(size_t)y*w+x]=(unsigned char)(
                pat==0? y%7 : pat==1? x%3 : pat==2? (x/127)%4 :
                pat==3? (x/128)%4 : (x+y)&1 );
          palrle_data_t* dd=palrle_encode(s,w,h,pal,8,0);
          CHECK(dd!=0,"pattern encode");
          memset(d,0x11,n);
          CHECK(palrle_decode_safe(dd,d,0)==PALRLE_OK,"pattern decode rc");
          CHECK(memcmp(s,d,n)==0,"pattern pixels identical");
          palrle_free(dd,0);
        }
        free(s);free(d);
      }

    /* 3. invalid encoder geometry */
    unsigned char px[16]={0};
    CHECK(palrle_encode(px,0,4,pal,2,0)==0,"w=0 rejected");
    CHECK(palrle_encode(px,4,0,pal,2,0)==0,"h=0 rejected");
    CHECK(palrle_encode(px,-1,4,pal,2,0)==0,"negative w rejected");
    CHECK(palrle_encode(px,4,4,pal,-1,0)==0,"negative palette rejected");
    {
        CHECK(palrle_encode_mask(px,px,0,4,pal,2,0)==0,"mask w=0 rejected");
        CHECK(palrle_encode_mask(px,0,4,4,pal,2,0)==0,"NULL mask rejected");
        /* fully transparent mask is valid and yields an empty bitmap */
        unsigned char zero_mask[16]={0};
        palrle_data_t* empty = palrle_encode_mask(px,zero_mask,4,4,pal,2,0);
        CHECK(empty!=0 && empty->hpitch==0,"empty mask -> empty bitmap");
        unsigned char out[16]={0};
        CHECK(palrle_decode_safe(empty,out,0)==PALRLE_OK,"empty bitmap decodes");
        palrle_free(empty,0);
    }

    /* 4. malformed / truncated blobs must return errors, never crash */
    {
        st=99; int w=40,h=6; size_t n=(size_t)w*h;
        unsigned char* s=(unsigned char*)malloc(n),*d=(unsigned char*)malloc(n);
        for(size_t i=0;i<n;i++) s[i]=(unsigned char)(rnd()%6);
        palrle_data_t* good=palrle_encode(s,w,h,pal,6,0);
        size_t blob=good->size;
        unsigned char* raw=(unsigned char*)malloc(blob);
        memcpy(raw,good,blob);
        /* 4a. truncation at several cut points */
        for(size_t cut=sizeof(palrle_data_t); cut<blob; cut+=7){
            palrle_data_t* t=(palrle_data_t*)raw;
            PALRLE_U32 saved=t->size; t->size=(PALRLE_U32)cut;
            int rc=palrle_decode_safe(t,d,0);
            CHECK(rc!=PALRLE_OK,"truncated blob rejected");
            t->size=saved;
        }
        /* 4b. row offset pointing outside the (honest) declared region is
           rejected. A size field that lies about the underlying allocation
           cannot be detected from inside the function, just like any C struct. */
        {
            palrle_data_t* t=(palrle_data_t*)raw;
            size_t pb=4u*(size_t)t->palette_count;
            PALRLE_U32 bogus=t->size; /* one past the last valid byte */
            memcpy(&t->data[pb],&bogus,4);
            CHECK(palrle_decode_safe(t,d,0)==PALRLE_ERR_CORRUPT,"out-of-range row offset rejected");
        }
        /* 4b2. header claims a palette/table larger than the whole blob */
        memcpy(raw,good,blob);
        ((palrle_data_t*)raw)->palette_count=(PALRLE_U16)0xffff;
        CHECK(palrle_decode_safe((palrle_data_t*)raw,d,0)==PALRLE_ERR_CORRUPT,"huge palette_count rejected");
        /* 4c. corrupt hpitch beyond width -> write out of geometry */
        memcpy(raw,good,blob);
        ((palrle_data_t*)raw)->hpitch=(PALRLE_U16)(w+50);
        CHECK(palrle_decode_safe((palrle_data_t*)raw,d,0)!=PALRLE_OK,"oversized hpitch rejected");
        /* 4d. corrupt row offset pointing outside region */
        memcpy(raw,good,blob);
        {
            palrle_data_t* t=(palrle_data_t*)raw;
            size_t pb=4u*(size_t)t->palette_count;
            unsigned char* off=&t->data[pb];
            PALRLE_U32 bogus=(PALRLE_U32)(t->size+10);
            memcpy(off+4,&bogus,4); /* second row offset */
            CHECK(palrle_decode_safe(t,d,0)==PALRLE_ERR_CORRUPT,"bad row offset rejected");
        }
        /* 4e. empty-byte overflow inside a chunk (empty > remaining pitch) */
        memcpy(raw,good,blob);
        {
            palrle_data_t* t=(palrle_data_t*)raw;
            size_t pb=4u*(size_t)t->palette_count;
            PALRLE_U32 rs; memcpy(&rs,&t->data[pb],4);
            t->data[rs]=255; /* first empty marker absurd */
            CHECK(palrle_decode_safe(t,d,0)!=PALRLE_OK,"empty overrun rejected");
        }
        palrle_free(good,0); free(raw);free(s);free(d);
    }

    /* 5. NULL handling */
    CHECK(palrle_decode_safe(0,px,0)==PALRLE_ERR_NULL,"null blob decode");
    {
        palrle_data_t* d=palrle_encode(px,4,4,pal,2,0);
        CHECK(palrle_decode_safe(d,0,0)==PALRLE_ERR_NULL,"null pixels decode");
        CHECK(palrle_blit_safe(0,0,0,px,4,4)==PALRLE_ERR_NULL,"null blit");
        palrle_free(d,0);
    }

    /* 6. blit clipping: draw at offsets that push content outside the target, incl. negative */
    {
        int w=12,h=10; size_t n=(size_t)w*h;
        unsigned char* s=(unsigned char*)malloc(n),*canvas=(unsigned char*)malloc(n);
        for(size_t i=0;i<n;i++) s[i]=(unsigned char)(1+(i%5));
        palrle_data_t* d=palrle_encode(s,w,h,pal,8,0);
        int offs[][2]={{0,0},{5,5},{-5,-3},{w,h},{-w,-h},{20,-20}};
        for(unsigned k=0;k<sizeof(offs)/sizeof(offs[0]);k++){
            memset(canvas,0,n);
            int rc=palrle_blit_safe(d,offs[k][0],offs[k][1],canvas,w,h);
            CHECK(rc==PALRLE_OK,"blit rc");
            /* verify every written pixel matches source at corresponding coord */
            int consistent=1;
            for(int yy=0;yy<h;yy++)for(int xx=0;xx<w;xx++){
                int sx=xx-offs[k][0], sy=yy-offs[k][1];
                if(sx>=0&&sy>=0&&sx<w&&sy<h){ if(canvas[yy*w+xx]!=s[sy*w+sx]) consistent=0; }
            }
            CHECK(consistent,"blit content correct");
        }
        palrle_free(d,0);free(s);free(canvas);
    }

    /* 7. legacy void wrappers still round-trip */
    {
        int w=33,h=9; size_t n=(size_t)w*h;
        unsigned char* s=(unsigned char*)malloc(n),*d=(unsigned char*)malloc(n);
        for(size_t i=0;i<n;i++) s[i]=(unsigned char)(i%7);
        palrle_data_t* dd=palrle_encode(s,w,h,pal,8,0);
        palrle_decode(dd,d,0);
        CHECK(memcmp(s,d,n)==0,"legacy decode wrapper");
        palrle_free(dd,0);free(s);free(d);
    }

    printf("\nchecks=%ld fails=%ld -> %s\n",checks,fails,fails?"FAILED":"ALL PASS");
    return fails?1:0;
}
