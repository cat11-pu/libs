/*
------------------------------------------------------------------------------
          Licensing information can be found at the end of the file.
------------------------------------------------------------------------------

palrle.h - v0.1 - Run-length encoding of palettized bitmaps, for C/C++.

Do this:
    #define PALRLE_IMPLEMENTATION
before you include this file in *one* C/C++ file to create the implementation.
*/

#ifndef palrle_h
#define palrle_h

#ifndef PALRLE_U8
    #define PALRLE_U8 unsigned char
#endif
#ifndef PALRLE_U16
    #define PALRLE_U16 unsigned short
#endif
#ifndef PALRLE_U32
    #define PALRLE_U32 unsigned int
#endif

typedef struct palrle_data_t {
    PALRLE_U32 size;
    PALRLE_U16 width;
    PALRLE_U16 height;
    PALRLE_U16 xoffset;
    PALRLE_U16 yoffset;
    PALRLE_U16 hpitch;
    PALRLE_U16 vpitch;
    PALRLE_U16 palette_count;
    PALRLE_U8 data[ 1 ]; // "open" array
} palrle_data_t;

palrle_data_t* palrle_encode( PALRLE_U8* pixels, int width, int height, PALRLE_U32* palette, int palette_count, void* memctx );
palrle_data_t* palrle_encode_mask( PALRLE_U8* pixels, PALRLE_U8* mask, int width, int height, PALRLE_U32* palette, int palette_count, void* memctx );
//palrle_data_t* palrle_encode_transparency_index( PALRLE_U8* pixels, PALRLE_U8 transparency_index, int width, int height, PALRLE_U32* palette, int palette_count, void* memctx );

// Result codes returned by the bounds-checked decode/blit functions.
#define PALRLE_OK              0
#define PALRLE_ERR_NULL        1
#define PALRLE_ERR_DIMENSIONS  2
#define PALRLE_ERR_OVERFLOW    3
#define PALRLE_ERR_CORRUPT     4
#define PALRLE_ERR_TRUNCATED   5

// Decodes an RLE blob into caller-provided buffers, validating every read and
// write against the blob size and destination dimensions. Returns PALRLE_OK on
// success or one of the PALRLE_ERR_* codes above for out-of-range/corrupt input.
int palrle_decode_safe( palrle_data_t* rle_data, PALRLE_U8* pixels, PALRLE_U8* mask );

// Bounds-checked blit. `pixels` is a width*height index buffer; pixels landing
// outside that rectangle are skipped instead of corrupting memory.
int palrle_blit_safe( palrle_data_t* rle_data, int x, int y, PALRLE_U8* pixels, int width, int height );

// Legacy entry points. They keep their original signatures for source
// compatibility, but simply forward to the safe implementations.
void palrle_decode( palrle_data_t* rle_data, PALRLE_U8* pixels, PALRLE_U8* mask );

void palrle_blit( palrle_data_t* rle_data, int x, int y, PALRLE_U8* pixels, int width, int height );

void palrle_free( palrle_data_t* rle_data, void* memctx );


#endif /* palrle_h */

/*
----------------------
    IMPLEMENTATION
----------------------
*/

#ifdef PALRLE_IMPLEMENTATION
#undef PALRLE_IMPLEMENTATION

#ifndef PALRLE_MALLOC
    #define _CRT_NONSTDC_NO_DEPRECATE
    #define _CRT_SECURE_NO_WARNINGS
    #include <stdlib.h>
    #include <string.h>
    #define PALRLE_MALLOC( ctx, size ) ( malloc( size ) )
    #define PALRLE_FREE( ctx, ptr ) ( free( ptr ) )
#endif

#ifndef PALRLE_S8
    #define PALRLE_S8 signed char
#endif

// Bytes required by the worst-case RLE stream.
//
// No mask: any pixel can end up in its own uniques chunk [empty][count][color],
// which is 3 bytes per pixel. The old code reserved only 2 bytes per pixel and
// overran the heap for narrow images (e.g. width == 1) and noisy patterns.
//
// With a mask: empty spans cost an extra [empty][0] pair per 255 empty pixels,
// so the absolute worst case is 2 additional bytes per pixel -> 5 per pixel.
#define PALRLE_RLE_CAP_NOMASK( pixels ) ( (size_t) ( pixels ) * 3 )
#define PALRLE_RLE_CAP_MASK( pixels )   ( (size_t) ( pixels ) * 5 )

// The row offset table is stored as raw PALRLE_U32 values inside the byte
// array, so it is not naturally aligned. Access it through memcpy to avoid
// misaligned loads/stores on strict-alignment targets.
static void palrle_put_u32( PALRLE_U8* dst, PALRLE_U32 value ) {
    memcpy( dst, &value, sizeof( PALRLE_U32 ) );
}

static PALRLE_U32 palrle_get_u32( const PALRLE_U8* src ) {
    PALRLE_U32 value;
    memcpy( &value, src, sizeof( PALRLE_U32 ) );
    return value;
}

// Validates the dimensions that both encoders share and that fit the on-disk
// U16 header fields. Returns the total pixel count through out_pixels, or 0 if
// the request is invalid or would overflow size_t.
static size_t palrle_validate_geometry( int width, int height, int palette_count ) {
    if( width <= 0 || height <= 0 || palette_count < 0 ) return 0;
    if( width > 0xffff || height > 0xffff || palette_count > 0xffff ) return 0;
    size_t pixels = (size_t) width * (size_t) height;
    if( pixels > ( (size_t) 0xffffffff ) / 5 ) return 0;
    return pixels;
}


palrle_data_t* palrle_encode( PALRLE_U8* pixels, int width, int height, PALRLE_U32* palette, int palette_count, void* memctx ) {
    (void) memctx;

    size_t total_pixels = palrle_validate_geometry( width, height, palette_count );
    if( !total_pixels ) return 0;

    int xmin = 0;
    int xmax = width - 1;
    int ymin = 0;
    int ymax = height - 1;
    int hpitch = xmax - xmin + 1;
    int vpitch = ymax - ymin + 1;

    // Bitmap is completely empty, so just store the palette and dimensions
    if( hpitch <= 0 || vpitch <= 0 ) {
        size_t size = sizeof( palrle_data_t ) - sizeof( PALRLE_U8 ) +
            sizeof( PALRLE_U32 ) * (size_t) palette_count;
        palrle_data_t* data = (palrle_data_t*) PALRLE_MALLOC( memctx, size );
        if( !data ) return 0;
        data->size = (PALRLE_U32) size;
        data->width = (PALRLE_U16) width;
        data->height = (PALRLE_U16) height;
        data->xoffset = 0;
        data->yoffset = 0;
        data->hpitch = 0;
        data->vpitch = 0;
        data->palette_count = (PALRLE_U16) palette_count;
        if( palette ) memcpy( data->data, palette, sizeof( PALRLE_U32 ) * (size_t) palette_count );
        return data;
    }

    size_t rle_cap = PALRLE_RLE_CAP_NOMASK( (size_t) hpitch * (size_t) vpitch );
    size_t palette_bytes = sizeof( PALRLE_U32 ) * (size_t) palette_count;
    size_t offset_bytes = sizeof( PALRLE_U32 ) * (size_t) vpitch;
    size_t alloc_size = sizeof( palrle_data_t ) + palette_bytes + offset_bytes + rle_cap;
    palrle_data_t* data = (palrle_data_t*) PALRLE_MALLOC( memctx, alloc_size );
    if( !data ) return 0;
    memset( data, 0, alloc_size );

    size_t header_size = sizeof( palrle_data_t ) - sizeof( PALRLE_U8 );
    data->size = (PALRLE_U32) header_size;
    data->width = (PALRLE_U16) width;
    data->height = (PALRLE_U16) height;
    data->xoffset = (PALRLE_U16) xmin;
    data->yoffset = (PALRLE_U16) ymin;
    data->hpitch = (PALRLE_U16) hpitch;
    data->vpitch = (PALRLE_U16) vpitch;
    data->palette_count = (PALRLE_U16) palette_count;
    if( palette ) memcpy( data->data, palette, palette_bytes );

    size_t row_offset = palette_bytes;
    size_t rle_offset = row_offset + offset_bytes;
    size_t rle_end = rle_offset + rle_cap;
    for( int y = 0; y < vpitch; ++y ) {
        palrle_put_u32( &data->data[ row_offset ], (PALRLE_U32) rle_offset );
        row_offset += sizeof( PALRLE_U32 );
        int x = 0;
        while( x < hpitch ) {
            // add empty pixel count
            data->data[ rle_offset++ ] = (PALRLE_U8) 0;

            // add non-empty pixels
            int color = pixels[ x + xmin + ( y + ymin ) * width ];
            int count = 0;
            int tx = x;
            while( tx < hpitch ) {
                if( pixels[ tx + xmin + ( y + ymin ) * width ] != color ) break;
                if( count >= 127 ) break;
                ++count;
                ++tx;
            }

            if( count == 0 ) {
                data->data[ rle_offset++ ] = 0;
                continue;
            }

            if( count > 2 ) { // store as a run
                data->data[ rle_offset++ ] = (PALRLE_U8) count; // number of pixels
                data->data[ rle_offset++ ] = (PALRLE_U8) color; // of palette index `color`
                x = tx;
            } else { // store as unique values
                PALRLE_U8* uniques_out = &data->data[ rle_offset++ ];
                int uniques = 1;
                data->data[ rle_offset++ ] = (PALRLE_U8) color;
                ++x;

                // add pixexls until we encounter a run
                while( x < hpitch ) {
                    // check the length of the next run
                    color = pixels[ x + xmin + ( y + ymin ) * width ];
                    count = 0;
                    tx = x;
                    while( tx < hpitch ) {
                        if( pixels[ tx + xmin + ( y + ymin ) * width ] != color ) break;
                        if( count > 2 ) break; // No need to keep counting, we know this is a run
                        ++count;
                        ++tx;
                    }
                    if( count > 2 || count == 0 ) {
                        break;
                    } else {
                        if( uniques >= 128 ) break;
                        ++uniques;
                        data->data[ rle_offset++ ] = (PALRLE_U8) color;
                        ++x;
                    }
                }
                *uniques_out = (PALRLE_U8)( (char) -uniques );
            }
        }
    }
    // The 3-bytes-per-pixel bound above covers every encoding path; assert it
    // as a tripwire if a future change breaks that assumption.
    if( rle_offset > rle_end ) {
        PALRLE_FREE( memctx, data );
        return 0;
    }
    size_t final_size = header_size + rle_offset;
    if( final_size > 0xffffffff ) {
        PALRLE_FREE( memctx, data );
        return 0;
    }
    data->size = (PALRLE_U32) final_size;

    return data;
}


palrle_data_t* palrle_encode_mask( PALRLE_U8* pixels, PALRLE_U8* mask, int width, int height, PALRLE_U32* palette, int palette_count, void* memctx ) {
    (void) memctx;

    size_t total_pixels = palrle_validate_geometry( width, height, palette_count );
    if( !total_pixels || !pixels || !mask ) return 0;

    // Crop to smallest non-empty region
    int xmin = width;
    int xmax = 0;
    int ymin = height;
    int ymax = 0;
    for( int y = 0; y < height; ++y ) {
        for( int x = 0; x < width; ++x ) {
            if( mask[ x + y * width ] ) {
                if( x < xmin ) xmin = x;
                if( y < ymin ) ymin = y;
                if( x > xmax ) xmax = x;
                if( y > ymax ) ymax = y;
            }
        }
    }
    int hpitch = xmax - xmin + 1;
    int vpitch = ymax - ymin + 1;

    // Bitmap is completely empty, so just store the palette and dimensions
    if( hpitch <= 0 || vpitch <= 0 ) {
        size_t size = sizeof( palrle_data_t ) - sizeof( PALRLE_U8 ) +
            sizeof( PALRLE_U32 ) * (size_t) palette_count;
        palrle_data_t* data = (palrle_data_t*) PALRLE_MALLOC( memctx, size );
        if( !data ) return 0;
        data->size = (PALRLE_U32) size;
        data->width = (PALRLE_U16) width;
        data->height = (PALRLE_U16) height;
        data->xoffset = 0;
        data->yoffset = 0;
        data->hpitch = 0;
        data->vpitch = 0;
        data->palette_count = (PALRLE_U16) palette_count;
        if( palette ) memcpy( data->data, palette, sizeof( PALRLE_U32 ) * (size_t) palette_count );
        return data;
    }

    size_t rle_cap = PALRLE_RLE_CAP_MASK( (size_t) hpitch * (size_t) vpitch );
    size_t palette_bytes = sizeof( PALRLE_U32 ) * (size_t) palette_count;
    size_t offset_bytes = sizeof( PALRLE_U32 ) * (size_t) vpitch;
    size_t alloc_size = sizeof( palrle_data_t ) + palette_bytes + offset_bytes + rle_cap;
    palrle_data_t* data = (palrle_data_t*) PALRLE_MALLOC( memctx, alloc_size );
    if( !data ) return 0;
    memset( data, 0, alloc_size );

    size_t header_size = sizeof( palrle_data_t ) - sizeof( PALRLE_U8 );
    data->size = (PALRLE_U32) header_size;
    data->width = (PALRLE_U16) width;
    data->height = (PALRLE_U16) height;
    data->xoffset = (PALRLE_U16) xmin;
    data->yoffset = (PALRLE_U16) ymin;
    data->hpitch = (PALRLE_U16) hpitch;
    data->vpitch = (PALRLE_U16) vpitch;
    data->palette_count = (PALRLE_U16) palette_count;
    if( palette ) memcpy( data->data, palette, palette_bytes );

    size_t row_offset = palette_bytes;
    size_t rle_offset = row_offset + offset_bytes;
    size_t rle_end = rle_offset + rle_cap;
    for( int y = 0; y < vpitch; ++y ) {
        palrle_put_u32( &data->data[ row_offset ], (PALRLE_U32) rle_offset );
        row_offset += sizeof( PALRLE_U32 );
        int x = 0;
        while( x < hpitch ) {
            // add empty pixel count
            int empty = 0;
            while( x < hpitch ) {
                if( mask[ x + xmin + ( y + ymin ) * width ] != 0 ) break;
                if( empty >= 255 ) {
                    data->data[ rle_offset++ ] = 255; // 255 empty pixels
                    data->data[ rle_offset++ ] = 0; // 0 non-empty pixels
                    empty = 0;
                }
                ++empty;
                ++x;
            }
            data->data[ rle_offset++ ] = (PALRLE_U8) empty;

            // add non-empty pixels
            int color = pixels[ x + xmin + ( y + ymin ) * width ];
            int count = 0;
            int tx = x;
            while( tx < hpitch ) {
                if( mask[ tx + xmin + ( y + ymin ) * width ] == 0 ) break;
                if( pixels[ tx + xmin + ( y + ymin ) * width ] != color ) break;
                if( count >= 127 ) break;
                ++count;
                ++tx;
            }

            if( count == 0 ) {
                data->data[ rle_offset++ ] = 0;
                continue;
            }

            if( count > 2 ) { // store as a run
                data->data[ rle_offset++ ] = (PALRLE_U8) count; // number of pixels
                data->data[ rle_offset++ ] = (PALRLE_U8) color; // of palette index `color`
                x = tx;
            } else { // store as unique values
                PALRLE_U8* uniques_out = &data->data[ rle_offset++ ];
                int uniques = 1;
                data->data[ rle_offset++ ] = (PALRLE_U8) color;
                ++x;

                // add pixexls until we encounter a run
                while( x < hpitch ) {
                    // check the length of the next run
                    color = pixels[ x + xmin + ( y + ymin ) * width ];
                    count = 0;
                    tx = x;
                    while( tx < hpitch ) {
                        if( mask[ tx + xmin + ( y + ymin ) * width ] == 0 ) break;
                        if( pixels[ tx + xmin + ( y + ymin ) * width ] != color ) break;
                        if( count > 2 ) break; // No need to keep counting, we know this is a run
                        ++count;
                        ++tx;
                    }
                    if( count > 2 || count == 0 ) {
                        break;
                    } else {
                        if( uniques >= 128 ) break;
                        ++uniques;
                        data->data[ rle_offset++ ] = (PALRLE_U8) color;
                        ++x;
                    }
                }
                *uniques_out = (PALRLE_U8)( (char) -uniques );
            }
        }
    }
    // 5 bytes per pixel covers every combination of empty-run markers and
    // pixel chunks emitted by the masked encoder.
    if( rle_offset > rle_end ) {
        PALRLE_FREE( memctx, data );
        return 0;
    }
    size_t final_size = header_size + rle_offset;
    if( final_size > 0xffffffff ) {
        PALRLE_FREE( memctx, data );
        return 0;
    }
    data->size = (PALRLE_U32) final_size;

    return data;
}


/*
palrle_data_t* palrle_encode_transparency_index( PALRLE_U8* pixels, PALRLE_U8 transparency_index, int width, int height, PALRLE_U32* palette, int palette_count, void* memctx ) {
    (void) pixels, transparency_index, width, height, memctx;
    return 0;
}
*/



// Shared, fully bounds-checked RLE walker. In decode mode it fills the
// bitmap described by the blob header; in blit mode it draws into the
// caller's width*height buffer at an (x,y) offset and clips to its edges.
//
// Every byte of the RLE stream is validated against the blob size and every
// pixel is validated against the destination geometry, so a truncated or
// malformed blob returns an error code instead of reading or writing out of
// bounds. Each row is parsed independently through the row offset table, which
// prevents an error on one row from cascading into the rows below.
static int palrle_walk( palrle_data_t* rle_data, int blit, int x, int y,
                        PALRLE_U8* pixels, int width, int height, PALRLE_U8* mask ) {
    if( !rle_data || !pixels ) return PALRLE_ERR_NULL;
    if( width <= 0 || height <= 0 ) return PALRLE_ERR_DIMENSIONS;

    int dst_w = blit ? width  : rle_data->width;
    int dst_h = blit ? height : rle_data->height;
    if( dst_w <= 0 || dst_h <= 0 ) return PALRLE_ERR_DIMENSIONS;

    size_t dest_pixels = (size_t) dst_w * (size_t) dst_h;
    if( dest_pixels > (size_t) 0xffffffff ) return PALRLE_ERR_OVERFLOW;

    // Decode mode always starts from a blank bitmap, even when the encoded
    // content is empty, so clear before the early returns below.
    if( !blit ) {
        size_t clear_pixels = (size_t) rle_data->width * (size_t) rle_data->height;
        memset( pixels, 0, clear_pixels * sizeof( PALRLE_U8 ) );
        if( mask ) memset( mask, 0, clear_pixels * sizeof( PALRLE_U8 ) );
    }

    if( rle_data->hpitch == 0 || rle_data->vpitch == 0 ) return PALRLE_OK;
    if( rle_data->xoffset >= dst_w || rle_data->yoffset >= dst_h ) return PALRLE_OK;

    // The byte array contains: palette, then the per-row offset table, then the
    // RLE stream. Reject blobs whose header claims more data than was allocated.
    size_t palette_bytes = sizeof( PALRLE_U32 ) * (size_t) rle_data->palette_count;
    size_t table_bytes   = sizeof( PALRLE_U32 ) * (size_t) rle_data->vpitch;
    size_t data_bytes    = (size_t) rle_data->size -
                           ( sizeof( palrle_data_t ) - sizeof( PALRLE_U8 ) );
    if( palette_bytes + table_bytes > data_bytes ) return PALRLE_ERR_CORRUPT;
    size_t rle_begin = palette_bytes + table_bytes;
    size_t rle_limit = data_bytes;

    PALRLE_U32 prev_offset = 0;
    for( int row = 0; row < rle_data->vpitch; ++row ) {
        PALRLE_U32 row_start = palrle_get_u32(
            &rle_data->data[ palette_bytes + (size_t) row * sizeof( PALRLE_U32 ) ] );
        // Offsets are absolute positions inside the byte array and must stay
        // within the RLE region, with rows laid out in ascending order.
        if( row_start < rle_begin || row_start >= rle_limit ) return PALRLE_ERR_CORRUPT;
        if( row_start < prev_offset ) return PALRLE_ERR_CORRUPT;
        prev_offset = row_start;

        size_t pos = row_start;
        int col = 0;
        while( col < rle_data->hpitch ) {
            // Invariant for the "remaining pixels" math below; the loop
            // condition guarantees it, kept explicit to guard future edits.
            if( col > rle_data->hpitch ) return PALRLE_ERR_CORRUPT;
            if( pos >= rle_limit ) return PALRLE_ERR_TRUNCATED;
            int empty = rle_data->data[ pos++ ];
            if( empty > rle_data->hpitch - col ) return PALRLE_ERR_CORRUPT;
            col += empty;

            if( pos >= rle_limit ) return PALRLE_ERR_TRUNCATED;
            int count = (PALRLE_S8) rle_data->data[ pos++ ];
            if( count > 0 ) {
                if( pos >= rle_limit ) return PALRLE_ERR_TRUNCATED;
                PALRLE_U8 color = rle_data->data[ pos++ ];
                if( count > rle_data->hpitch - col ) return PALRLE_ERR_CORRUPT;
                for( int i = 0; i < count; ++i ) {
                    int local_x = rle_data->xoffset + col;
                    int local_y = rle_data->yoffset + row;
                    if( blit ) {
                        int xp = local_x + x;
                        int yp = local_y + y;
                        if( xp >= 0 && yp >= 0 && xp < dst_w && yp < dst_h )
                            pixels[ (size_t) yp * (size_t) dst_w + (size_t) xp ] = color;
                    } else {
                        if( local_x < 0 || local_y < 0 ||
                            local_x >= dst_w || local_y >= dst_h ) return PALRLE_ERR_CORRUPT;
                        size_t index = (size_t) local_y * (size_t) dst_w + (size_t) local_x;
                        pixels[ index ] = color;
                        if( mask ) mask[ index ] = 255;
                    }
                    ++col;
                }
            } else if( count < 0 ) {
                int uniques = -count;
                if( uniques > rle_data->hpitch - col ) return PALRLE_ERR_CORRUPT;
                for( int i = 0; i < uniques; ++i ) {
                    if( pos >= rle_limit ) return PALRLE_ERR_TRUNCATED;
                    PALRLE_U8 color = rle_data->data[ pos++ ];
                    int local_x = rle_data->xoffset + col;
                    int local_y = rle_data->yoffset + row;
                    if( blit ) {
                        int xp = local_x + x;
                        int yp = local_y + y;
                        if( xp >= 0 && yp >= 0 && xp < dst_w && yp < dst_h )
                            pixels[ (size_t) yp * (size_t) dst_w + (size_t) xp ] = color;
                    } else {
                        if( local_x < 0 || local_y < 0 ||
                            local_x >= dst_w || local_y >= dst_h ) return PALRLE_ERR_CORRUPT;
                        size_t index = (size_t) local_y * (size_t) dst_w + (size_t) local_x;
                        pixels[ index ] = color;
                        if( mask ) mask[ index ] = 255;
                    }
                    ++col;
                }
            }
            // count == 0 is a valid masked "empty run" marker and simply
            // advances to the next chunk.
        }
    }

    return PALRLE_OK;
}


int palrle_decode_safe( palrle_data_t* rle_data, PALRLE_U8* pixels, PALRLE_U8* mask ) {
    if( !rle_data || !pixels ) return PALRLE_ERR_NULL;
    return palrle_walk( rle_data, 0, 0, 0, pixels, rle_data->width, rle_data->height, mask );
}


int palrle_blit_safe( palrle_data_t* rle_data, int x, int y, PALRLE_U8* pixels, int width, int height ) {
    return palrle_walk( rle_data, 1, x, y, pixels, width, height, 0 );
}


void palrle_decode( palrle_data_t* rle_data, PALRLE_U8* pixels, PALRLE_U8* mask ) {
    (void) palrle_decode_safe( rle_data, pixels, mask );
}


void palrle_blit( palrle_data_t* rle_data, int x, int y, PALRLE_U8* pixels, int width, int height ) {
    (void) palrle_blit_safe( rle_data, x, y, pixels, width, height );
}


void palrle_free( palrle_data_t* rle_data, void* memctx ) {
    (void) rle_data, (void) memctx;
    PALRLE_FREE( memctx, rle_data );
}

#endif /* PALRLE_IMPLEMENTATION */


/*
------------------------------------------------------------------------------

This software is available under 2 licenses - you may choose the one you like.

------------------------------------------------------------------------------

ALTERNATIVE A - MIT License

Copyright (c) 2019 Mattias Gustavsson

Permission is hereby granted, free of charge, to any person obtaining a copy of
this software and associated documentation files (the "Software"), to deal in
the Software without restriction, including without limitation the rights to
use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies
of the Software, and to permit persons to whom the Software is furnished to do
so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.

------------------------------------------------------------------------------

ALTERNATIVE B - Public Domain (www.unlicense.org)

This is free and unencumbered software released into the public domain.

Anyone is free to copy, modify, publish, use, compile, sell, or distribute this
software, either in source code form or as a compiled binary, for any purpose,
commercial or non-commercial, and by any means.

In jurisdictions that recognize copyright laws, the author or authors of this
software dedicate any and all copyright interest in the software to the public
domain. We make this dedication for the benefit of the public at large and to
the detriment of our heirs and successors. We intend this dedication to be an
overt act of relinquishment in perpetuity of all present and future rights to
this software under copyright law.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN
ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION
WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.

------------------------------------------------------------------------------
*/
