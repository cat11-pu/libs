/*
    pixelfont_subpixel_test.c - comparison and benchmark for pixelfont_blit (integer pen)
    vs pixelfont_blit_subpixel (16.16 fixed-point pen, coverage anti-aliasing, fixed-point kerning).

    Builds a 10px-line-height pixelfont from the sysfont 8x8 glyphs (auto-trimmed side bearings),
    renders comparison images into artifacts/, and times rendering of 1000 characters for both
    renderers.
*/

#define PIXELFONT_IMPLEMENTATION
#define PIXELFONT_BUILDER_IMPLEMENTATION
#define PIXELFONT_SUBPIXEL_IMPLEMENTATION
#include "pixelfont.h"

#define SYSFONT_IMPLEMENTATION
#include "sysfont.h"

#include <stdio.h>
#include <string.h>
#include <time.h>

#define FONT_HEIGHT 10
#define FONT_BASELINE 8
#define FONT_LINE_SPACING 12

#define FP_ONE 65536

static pixelfont_t* build_font( pixelfont_builder_t* builder, int with_kerning )
    {
    for( int c = 32; c < 127; ++c )
        {
        char s[ 2 ] = { (char) c, '\0' };
        PIXELFONT_U8 cell[ 8 * 8 ];
        memset( cell, 0, sizeof( cell ) );
        sysfont_8x8_u8( cell, 8, 8, 0, 0, s, 255 );

        int col_first = 8, col_last = -1;
        for( int ix = 0; ix < 8; ++ix )
            for( int iy = 0; iy < 8; ++iy )
                if( cell[ ix + iy * 8 ] )
                    {
                    col_first = ix < col_first ? ix : col_first;
                    col_last = ix > col_last ? ix : col_last;
                    }

        if( col_last < col_first )
            {
            pixelfont_builder_glyph( builder, c, 0, NULL, 0, 0, 3 );
            }
        else
            {
            int w = col_last - col_first + 1;
            PIXELFONT_U8 pixels[ FONT_HEIGHT * 8 ];
            memset( pixels, 0, sizeof( pixels ) );
            for( int iy = 0; iy < 8; ++iy )
                for( int ix = 0; ix < w; ++ix )
                    pixels[ ix + iy * w ] = cell[ col_first + ix + iy * 8 ];
            pixelfont_builder_glyph( builder, c, w, pixels, w, 0, w );
            }
        }

    if( with_kerning )
        {
        char const* pairs[][ 2 ] = {
            { "T", "o" }, { "T", "e" }, { "A", "V" }, { "V", "A" }, { "W", "A" },
            { "A", "W" }, { "Y", "o" }, { "W", "e" }, { "T", "a" }, { "L", "i" },
            };
        for( size_t i = 0; i < sizeof( pairs ) / sizeof( *pairs ); ++i )
            pixelfont_builder_kerning( builder, pairs[ i ][ 0 ][ 0 ], pairs[ i ][ 1 ][ 0 ], -1 );
        }

    return pixelfont_builder_font( builder );
    }


static void save_pgm( char const* path, PIXELFONT_U8 const* pixels, int width, int height )
    {
    FILE* f = fopen( path, "wb" );
    if( !f ) return;
    fprintf( f, "P5\n%d %d\n255\n", width, height );
    fwrite( pixels, 1, (size_t) ( width * height ), f );
    fclose( f );
    }


static double now_ms( void )
    {
    struct timespec ts;
    clock_gettime( CLOCK_MONOTONIC, &ts );
    return (double) ts.tv_sec * 1000.0 + (double) ts.tv_nsec / 1000000.0;
    }


#define IMG_W 640
#define IMG_H 64

static char const* demo_text =
    "The quick brown fox jumps over the lazy dog.\n"
    "WAVE AVATAR To Yo Te We 0123456789\n"
    "Pack my box with five dozen liquor jugs.";

int main( void )
    {
    pixelfont_builder_t* builder_nokern = pixelfont_builder_create( FONT_HEIGHT, FONT_BASELINE,
        FONT_LINE_SPACING, NULL );
    pixelfont_t* font_nokern = build_font( builder_nokern, 0 );

    pixelfont_builder_t* builder_kern = pixelfont_builder_create( FONT_HEIGHT, FONT_BASELINE,
        FONT_LINE_SPACING, NULL );
    pixelfont_t* font_kern = build_font( builder_kern, 1 );

    static PIXELFONT_U8 img[ IMG_W * IMG_H ];

    struct mode { char const* name; pixelfont_t* font; int subpixel; int hspacing; };
    struct mode modes[] =
        {
        { "artifacts/pf_mode0_int_hsp0_nokern.pgm",    font_nokern, 0, 0 },
        { "artifacts/pf_mode1_int_hsp1_nokern.pgm",    font_nokern, 0, 1 },
        { "artifacts/pf_mode2_int_hsp1_kern.pgm",      font_kern,   0, 1 },
        { "artifacts/pf_mode3_subpx_hsp05_nokern.pgm", font_nokern, 1, FP_ONE / 2 },
        { "artifacts/pf_mode4_subpx_hsp05_kern.pgm",   font_kern,   1, FP_ONE / 2 },
        };
    for( size_t i = 0; i < sizeof( modes ) / sizeof( *modes ); ++i )
        {
        memset( img, 0, sizeof( img ) );
        if( modes[ i ].subpixel )
            pixelfont_blit_subpixel( modes[ i ].font, 4 * FP_ONE, 2, demo_text, 255, img, IMG_W, IMG_H,
                modes[ i ].hspacing, 0, NULL );
        else
            pixelfont_blit( modes[ i ].font, 4, 2, demo_text, 255, img, IMG_W, IMG_H,
                PIXELFONT_ALIGN_LEFT, 0, modes[ i ].hspacing, 0, -1, PIXELFONT_BOLD_OFF,
                PIXELFONT_ITALIC_OFF, PIXELFONT_UNDERLINE_OFF, NULL );
        save_pgm( modes[ i ].name, img, IMG_W, IMG_H );
        }

    /* subpixel positioning sweep: same word at increasing fractional x offsets */
    {
    int const sweep_w = 640, sweep_h = 110;
    static PIXELFONT_U8 sweep[ 640 * 110 ];
    memset( sweep, 0, sizeof( sweep ) );
    char const* word = "WAVE WAVE";
    pixelfont_blit( font_kern, 4, 2, word, 255, sweep, sweep_w, sweep_h, PIXELFONT_ALIGN_LEFT, 0, 1, 0,
        -1, PIXELFONT_BOLD_OFF, PIXELFONT_ITALIC_OFF, PIXELFONT_UNDERLINE_OFF, NULL );
    pixelfont_blit( font_kern, 5, 2 + FONT_LINE_SPACING, word, 255, sweep, sweep_w, sweep_h,
        PIXELFONT_ALIGN_LEFT, 0, 1, 0, -1, PIXELFONT_BOLD_OFF, PIXELFONT_ITALIC_OFF,
        PIXELFONT_UNDERLINE_OFF, NULL );
    for( int i = 0; i <= 4; ++i )
        pixelfont_blit_subpixel( font_kern, 4 * FP_ONE + i * FP_ONE / 4,
            2 + ( 2 + i ) * FONT_LINE_SPACING, word, 255, sweep, sweep_w, sweep_h, FP_ONE / 2, 0,
            NULL );
    save_pgm( "artifacts/pf_sweep.pgm", sweep, sweep_w, sweep_h );
    }

    /* benchmark: render 1000 characters, integer vs subpixel */
    {
    static char text[ 1001 ];
    char const* chunk = "The quick brown fox jumps over the lazy dog. WAVE AVATAR 0123456789. ";
    text[ 0 ] = '\0';
    while( strlen( text ) + strlen( chunk ) < 1000 )
        strcat( text, chunk );
    strncat( text, chunk, 1000 - strlen( text ) );

    int const bw = 9216, bh = 40;
    static PIXELFONT_U8 target[ 9216 * 40 ];
    int const iterations = 300;

    for( int pass = 0; pass < 2; ++pass )
        {
        double best = 1.0e30, total = 0.0;
        for( int i = 0; i < iterations; ++i )
            {
            double t0 = now_ms();
            if( pass == 0 )
                pixelfont_blit( font_kern, 0, 2, text, 255, target, bw, bh, PIXELFONT_ALIGN_LEFT, 0, 1,
                    0, -1, PIXELFONT_BOLD_OFF, PIXELFONT_ITALIC_OFF, PIXELFONT_UNDERLINE_OFF, NULL );
            else
                pixelfont_blit_subpixel( font_kern, 0, 2, text, 255, target, bw, bh, FP_ONE / 2, 0,
                    NULL );
            double t1 = now_ms();
            best = t1 - t0 < best ? t1 - t0 : best;
            total += t1 - t0;
            }
        unsigned long checksum = 0;
        for( int i = 0; i < bw * bh; i += 64 ) checksum += target[ i ];
        printf( "%-10s 1000 chars x %d runs: best %.3f ms, avg %.3f ms  (checksum %lu)\n",
            pass == 0 ? "integer" : "subpixel", iterations, best, total / iterations, checksum );
        }
    }

    pixelfont_builder_destroy( builder_nokern );
    pixelfont_builder_destroy( builder_kern );
    return 0;
    }
