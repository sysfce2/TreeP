/* quirc -- QR-code recognition library
 * Copyright (C) 2010-2012 Daniel Beer <dlbeer@gmail.com>
 *
 * Permission to use, copy, modify, and/or distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <setjmp.h>
#include <jpeglib.h>
#include <assert.h>
#include "mjpeg.h"

struct huffman_table {
        uint8_t		bits[17];
        uint8_t		huffval[256];
};

static const struct huffman_table dc_lum = {
        .bits = {
                0x00, 0x00, 0x01, 0x05, 0x01, 0x01, 0x01, 0x01,
                0x01, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
                0x00
        },
        .huffval = {
                0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07,
                0x08, 0x09, 0x0a, 0x0b
        }
};

static const struct huffman_table ac_lum = {
        .bits = {
                0x00, 0x00, 0x02, 0x01, 0x03, 0x03, 0x02, 0x04,
                0x03, 0x05, 0x05, 0x04, 0x04, 0x00, 0x00, 0x01,
                0x7d
        },
        .huffval = {
                0x01, 0x02, 0x03, 0x00, 0x04, 0x11, 0x05, 0x12,
                0x21, 0x31, 0x41, 0x06, 0x13, 0x51, 0x61, 0x07,
                0x22, 0x71, 0x14, 0x32, 0x81, 0x91, 0xa1, 0x08,
                0x23, 0x42, 0xb1, 0xc1, 0x15, 0x52, 0xd1, 0xf0,
                0x24, 0x33, 0x62, 0x72, 0x82, 0x09, 0x0a, 0x16,
                0x17, 0x18, 0x19, 0x1a, 0x25, 0x26, 0x27, 0x28,
                0x29, 0x2a, 0x34, 0x35, 0x36, 0x37, 0x38, 0x39,
                0x3a, 0x43, 0x44, 0x45, 0x46, 0x47, 0x48, 0x49,
                0x4a, 0x53, 0x54, 0x55, 0x56, 0x57, 0x58, 0x59,
                0x5a, 0x63, 0x64, 0x65, 0x66, 0x67, 0x68, 0x69,
                0x6a, 0x73, 0x74, 0x75, 0x76, 0x77, 0x78, 0x79,
                0x7a, 0x83, 0x84, 0x85, 0x86, 0x87, 0x88, 0x89,
                0x8a, 0x92, 0x93, 0x94, 0x95, 0x96, 0x97, 0x98,
                0x99, 0x9a, 0xa2, 0xa3, 0xa4, 0xa5, 0xa6, 0xa7,
                0xa8, 0xa9, 0xaa, 0xb2, 0xb3, 0xb4, 0xb5, 0xb6,
                0xb7, 0xb8, 0xb9, 0xba, 0xc2, 0xc3, 0xc4, 0xc5,
                0xc6, 0xc7, 0xc8, 0xc9, 0xca, 0xd2, 0xd3, 0xd4,
                0xd5, 0xd6, 0xd7, 0xd8, 0xd9, 0xda, 0xe1, 0xe2,
                0xe3, 0xe4, 0xe5, 0xe6, 0xe7, 0xe8, 0xe9, 0xea,
                0xf1, 0xf2, 0xf3, 0xf4, 0xf5, 0xf6, 0xf7, 0xf8,
                0xf9, 0xfa
        }
};

static const struct huffman_table dc_chroma = {
        .bits = {
                0x00, 0x00, 0x03, 0x01, 0x01, 0x01, 0x01, 0x01,
                0x01, 0x01, 0x01, 0x01, 0x00, 0x00, 0x00, 0x00,
                0x00
        },
        .huffval = {
                0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07,
                0x08, 0x09, 0x0a, 0x0b
        }
};

static const struct huffman_table ac_chroma = {
        .bits = {
                0x00, 0x00, 0x02, 0x01, 0x02, 0x04, 0x04, 0x03,
                0x04, 0x07, 0x05, 0x04, 0x04, 0x00, 0x01, 0x02,
                0x77
        },
        .huffval = {
                0x00, 0x01, 0x02, 0x03, 0x11, 0x04, 0x05, 0x21,
                0x31, 0x06, 0x12, 0x41, 0x51, 0x07, 0x61, 0x71,
                0x13, 0x22, 0x32, 0x81, 0x08, 0x14, 0x42, 0x91,
                0xa1, 0xb1, 0xc1, 0x09, 0x23, 0x33, 0x52, 0xf0,
                0x15, 0x62, 0x72, 0xd1, 0x0a, 0x16, 0x24, 0x34,
                0xe1, 0x25, 0xf1, 0x17, 0x18, 0x19, 0x1a, 0x26,
                0x27, 0x28, 0x29, 0x2a, 0x35, 0x36, 0x37, 0x38,
                0x39, 0x3a, 0x43, 0x44, 0x45, 0x46, 0x47, 0x48,
                0x49, 0x4a, 0x53, 0x54, 0x55, 0x56, 0x57, 0x58,
                0x59, 0x5a, 0x63, 0x64, 0x65, 0x66, 0x67, 0x68,
                0x69, 0x6a, 0x73, 0x74, 0x75, 0x76, 0x77, 0x78,
                0x79, 0x7a, 0x82, 0x83, 0x84, 0x85, 0x86, 0x87,
                0x88, 0x89, 0x8a, 0x92, 0x93, 0x94, 0x95, 0x96,
                0x97, 0x98, 0x99, 0x9a, 0xa2, 0xa3, 0xa4, 0xa5,
                0xa6, 0xa7, 0xa8, 0xa9, 0xaa, 0xb2, 0xb3, 0xb4,
                0xb5, 0xb6, 0xb7, 0xb8, 0xb9, 0xba, 0xc2, 0xc3,
                0xc4, 0xc5, 0xc6, 0xc7, 0xc8, 0xc9, 0xca, 0xd2,
                0xd3, 0xd4, 0xd5, 0xd6, 0xd7, 0xd8, 0xd9, 0xda,
                0xe2, 0xe3, 0xe4, 0xe5, 0xe6, 0xe7, 0xe8, 0xe9,
                0xea, 0xf2, 0xf3, 0xf4, 0xf5, 0xf6, 0xf7, 0xf8,
                0xf9, 0xfa
        }
};

static void init_source(j_decompress_ptr cinfo)
{
}

static boolean fill_input_buffer(j_decompress_ptr cinfo)
{
        static const uint8_t eoi_marker[] = {0xff, 0xd9};

        cinfo->src->next_input_byte = eoi_marker;
        cinfo->src->bytes_in_buffer = 2;

        return TRUE;
}

static void skip_input_data(j_decompress_ptr cinfo, long num_bytes)
{
        if (num_bytes < 0)
                return;
        if (num_bytes > cinfo->src->bytes_in_buffer)
                num_bytes = cinfo->src->bytes_in_buffer;

        cinfo->src->bytes_in_buffer -= num_bytes;
        cinfo->src->next_input_byte += num_bytes;
}

static void term_source(j_decompress_ptr cinfo)
{
}

struct my_jpeg_error {
        struct jpeg_error_mgr   base;
        jmp_buf                 env;
};

static void my_output_message(struct jpeg_common_struct *com)
{
        struct mjpeg_decoder *mj = (struct mjpeg_decoder *)com->err;
        char buf[JMSG_LENGTH_MAX];

        mj->err.format_message(com, buf);
        fprintf(stderr, "MJPEG error: %s\n", buf);
}

static void my_error_exit(struct jpeg_common_struct *com)
{
        struct mjpeg_decoder *mj = (struct mjpeg_decoder *)com->err;

        my_output_message(com);
        longjmp(mj->env, 0);
}

static void setup_table(struct jpeg_decompress_struct *jpeg,
                        JHUFF_TBL **tbl_ptr, const struct huffman_table *tab)
{
        assert (*tbl_ptr == NULL);

        *tbl_ptr = jpeg_alloc_huff_table((j_common_ptr)jpeg);
        memcpy((*tbl_ptr)->bits, tab->bits, 17);
        memcpy((*tbl_ptr)->huffval, tab->huffval, 256);
}

void mjpeg_init(struct mjpeg_decoder *mj)
{
        memset(mj, 0, sizeof(*mj));

        /* Set up error management */
        mj->dinfo.err = jpeg_std_error(&mj->err);
        mj->err.error_exit = my_error_exit;
        mj->err.output_message = my_output_message;

        mj->src.init_source = init_source;
        mj->src.fill_input_buffer = fill_input_buffer;
        mj->src.skip_input_data = skip_input_data;
        mj->src.resync_to_restart = jpeg_resync_to_restart;
        mj->src.term_source = term_source;

        jpeg_create_decompress(&mj->dinfo);
        mj->dinfo.src = &mj->src;
        mj->dinfo.err = &mj->err;

        setup_table(&mj->dinfo, &mj->dinfo.dc_huff_tbl_ptrs[0], &dc_lum);
        setup_table(&mj->dinfo, &mj->dinfo.ac_huff_tbl_ptrs[0], &ac_lum);
        setup_table(&mj->dinfo, &mj->dinfo.dc_huff_tbl_ptrs[1], &dc_chroma);
        setup_table(&mj->dinfo, &mj->dinfo.ac_huff_tbl_ptrs[1], &ac_chroma);
}

void mjpeg_free(struct mjpeg_decoder *mj)
{
        jpeg_destroy_decompress(&mj->dinfo);
}

int mjpeg_decode_rgb32(struct mjpeg_decoder *mj,
                       const uint8_t *data, int datalen,
                       uint8_t *out, int pitch, int max_w, int max_h)
{
        if (setjmp(mj->env))
                return -1;

        mj->dinfo.src->bytes_in_buffer = datalen;
        mj->dinfo.src->next_input_byte = data;

        jpeg_read_header(&mj->dinfo, TRUE);
        mj->dinfo.output_components = 3;
        mj->dinfo.out_color_space = JCS_RGB;
        jpeg_start_decompress(&mj->dinfo);

        if (mj->dinfo.image_height > max_h ||
            mj->dinfo.image_width > max_w) {
                fprintf(stderr, "MJPEG: frame too big\n");
                return -1;
        }

        uint8_t *rgb = calloc(mj->dinfo.image_width, 3);
        if (!rgb) {
                fprintf(stderr, "memory allocation failed\n");
                return -1;
        }
        while (mj->dinfo.output_scanline < mj->dinfo.image_height) {
                uint8_t *scr = out + pitch * mj->dinfo.output_scanline;
                uint8_t *output = rgb;
                int i;

                jpeg_read_scanlines(&mj->dinfo, &output, 1);
                for (i = 0; i < mj->dinfo.image_width; i++) {
                        scr[0] = output[2];
                        scr[1] = output[1];
                        scr[2] = output[0];
                        scr += 4;
                        output += 3;
                }
        }
        free(rgb);

        jpeg_finish_decompress(&mj->dinfo);

        return 0;
}

int mjpeg_decode_gray(struct mjpeg_decoder *mj,
                      const uint8_t *data, int datalen,
                      uint8_t *out, int pitch, int max_w, int max_h)
{
        if (setjmp(mj->env))
                return -1;

        mj->dinfo.src->bytes_in_buffer = datalen;
        mj->dinfo.src->next_input_byte = data;

        jpeg_read_header(&mj->dinfo, TRUE);
        mj->dinfo.output_components = 1;
        mj->dinfo.out_color_space = JCS_GRAYSCALE;
        jpeg_start_decompress(&mj->dinfo);

        if (mj->dinfo.image_height > max_h ||
            mj->dinfo.image_width > max_w) {
                fprintf(stderr, "MJPEG: frame too big\n");
                return -1;
        }

        while (mj->dinfo.output_scanline < mj->dinfo.image_height) {
                uint8_t *scr = out + pitch * mj->dinfo.output_scanline;

                jpeg_read_scanlines(&mj->dinfo, &scr, 1);
        }

        jpeg_finish_decompress(&mj->dinfo);

        return 0;
}
