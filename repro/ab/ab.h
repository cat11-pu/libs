#ifndef AB_H
#define AB_H
#include "palrle_orig.h"
typedef palrle_data_t old_palrle_data_t;
typedef palrle_data_t new_palrle_data_t;
extern old_palrle_data_t* old_palrle_encode(unsigned char*,int,int,unsigned int*,int,void*);
extern void old_palrle_decode(old_palrle_data_t*,unsigned char*,unsigned char*);
extern void old_palrle_free(old_palrle_data_t*,void*);
extern new_palrle_data_t* new_palrle_encode(unsigned char*,int,int,unsigned int*,int,void*);
extern int new_palrle_decode_safe(new_palrle_data_t*,unsigned char*,unsigned char*);
extern void new_palrle_free(new_palrle_data_t*,void*);
#endif
