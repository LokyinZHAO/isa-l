/**********************************************************************
  Copyright(c) 2023 Intel Corporation All rights reserved.

  Redistribution and use in source and binary forms, with or without
  modification, are permitted provided that the following conditions
  are met:
    * Redistributions of source code must retain the above copyright
      notice, this list of conditions and the following disclaimer.
    * Redistributions in binary form must reproduce the above copyright
      notice, this list of conditions and the following disclaimer in
      the documentation and/or other materials provided with the
      distribution.
    * Neither the name of Intel Corporation nor the names of its
      contributors may be used to endorse or promote products derived
      from this software without specific prior written permission.

  THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
  "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
  LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
  A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
  OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
  SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
  LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
  DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
  THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
  (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
  OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
**********************************************************************/

/*
 * Test gf_4vect_dot_prod_avx2_gfni and gf_5vect_dot_prod_avx2_gfni by comparing
 * their output against the scalar base implementation.
 * Also tests ec_encode_data_avx2_gfni with 4, 5, and 9 row configurations.
 *
 * Requires GFNI support at runtime. Skips gracefully on non-GFNI machines.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "erasure_code.h"
#include "test.h"

#define TEST_LEN     8192
#define TEST_SOURCES 16
#define RANDOMS      20
#define MAX_ROWS     6

typedef unsigned char u8;

/* External declarations for functions under test */
extern void gf_4vect_dot_prod_avx2_gfni(int len, int k, unsigned char *g_tbls,
                                         unsigned char **data, unsigned char **coding);
extern void gf_5vect_dot_prod_avx2_gfni(int len, int k, unsigned char *g_tbls,
                                         unsigned char **data, unsigned char **coding);
extern void ec_encode_data_avx2_gfni(int len, int k, int rows, unsigned char *g_tbls,
                                      unsigned char **data, unsigned char **coding);
extern void ec_init_tables_gfni(int k, int rows, unsigned char *a, unsigned char *g_tbls);

/*
 * Run one comparison test: call the multi-vect GFNI function with given rows/len/k,
 * compare against gf_vect_dot_prod_base.  Returns 0 on success, -1 on failure.
 */
static int
test_gfni_vs_base(int len, int k, int rows)
{
        int i, j;
        void *buf;
        /* Table sizes: GFNI uses 8 bytes/entry; base uses 32 bytes/entry */
        u8 a[TEST_SOURCES * MAX_ROWS];
        u8 g_tbls_gfni[TEST_SOURCES * MAX_ROWS * 8];
        u8 g_tbls_base[TEST_SOURCES * MAX_ROWS * 32];
        u8 *buffs[TEST_SOURCES];
        u8 *dest_gfni[MAX_ROWS], *dest_ref[MAX_ROWS];

        for (i = 0; i < k; i++) {
                if (posix_memalign(&buf, 32, len) != 0)
                        return -1;
                buffs[i] = buf;
                for (j = 0; j < len; j++)
                        buffs[i][j] = rand();
        }
        for (i = 0; i < rows; i++) {
                if (posix_memalign(&buf, 32, len) != 0)
                        return -1;
                dest_gfni[i] = buf;
                if (posix_memalign(&buf, 32, len) != 0)
                        return -1;
                dest_ref[i] = buf;
        }

        for (i = 0; i < rows * k; i++)
                a[i] = rand() & 0xff;

        /* Compute reference output */
        ec_init_tables(k, rows, a, g_tbls_base);
        for (i = 0; i < rows; i++)
                gf_vect_dot_prod_base(len, k, &g_tbls_base[i * k * 32],
                                      buffs, dest_ref[i]);

        /* Compute GFNI output */
        ec_init_tables_gfni(k, rows, a, g_tbls_gfni);
        if (rows == 4)
                gf_4vect_dot_prod_avx2_gfni(len, k, g_tbls_gfni, buffs, dest_gfni);
        else if (rows == 5)
                gf_5vect_dot_prod_avx2_gfni(len, k, g_tbls_gfni, buffs, dest_gfni);
        else
                return -1;

        /* Compare outputs */
        for (i = 0; i < rows; i++) {
                if (memcmp(dest_ref[i], dest_gfni[i], len) != 0) {
                        printf("Fail: rows=%d k=%d len=%d dest[%d] mismatch\n",
                               rows, k, len, i);
                        break;
                }
        }
        /* i < rows means a mismatch was found */
        {
                int ret = (i < rows) ? -1 : 0;
                int ci;
                for (ci = 0; ci < k; ci++)
                        aligned_free(buffs[ci]);
                for (ci = 0; ci < rows; ci++) {
                        aligned_free(dest_gfni[ci]);
                        aligned_free(dest_ref[ci]);
                }
                return ret;
        }
}

/*
 * Test ec_encode_data_avx2_gfni with given number of rows.
 */
static int
test_encode_avx2_gfni(int len, int k, int rows)
{
        int i, j;
        void *buf;
        u8 a[TEST_SOURCES * TEST_SOURCES];
        u8 g_tbls_gfni[TEST_SOURCES * TEST_SOURCES * 8];
        u8 g_tbls_base[TEST_SOURCES * TEST_SOURCES * 32];
        u8 *buffs[TEST_SOURCES];
        u8 *dest_gfni[TEST_SOURCES], *dest_ref[TEST_SOURCES];

        for (i = 0; i < k; i++) {
                if (posix_memalign(&buf, 32, len) != 0)
                        return -1;
                buffs[i] = buf;
                for (j = 0; j < len; j++)
                        buffs[i][j] = rand();
        }
        for (i = 0; i < rows; i++) {
                if (posix_memalign(&buf, 32, len) != 0)
                        return -1;
                dest_gfni[i] = buf;
                if (posix_memalign(&buf, 32, len) != 0)
                        return -1;
                dest_ref[i] = buf;
        }

        for (i = 0; i < rows * k; i++)
                a[i] = rand() & 0xff;

        /* Reference */
        ec_init_tables(k, rows, a, g_tbls_base);
        for (i = 0; i < rows; i++)
                gf_vect_dot_prod_base(len, k, &g_tbls_base[i * k * 32],
                                      buffs, dest_ref[i]);

        /* GFNI dispatch */
        ec_init_tables_gfni(k, rows, a, g_tbls_gfni);
        ec_encode_data_avx2_gfni(len, k, rows, g_tbls_gfni, buffs, dest_gfni);

        for (i = 0; i < rows; i++) {
                if (memcmp(dest_ref[i], dest_gfni[i], len) != 0) {
                        printf("Fail encode: rows=%d k=%d len=%d dest[%d] mismatch\n",
                               rows, k, len, i);
                        break;
                }
        }
        /* i < rows means a mismatch was found */
        {
                int ret = (i < rows) ? -1 : 0;
                int ci;
                for (ci = 0; ci < k; ci++)
                        aligned_free(buffs[ci]);
                for (ci = 0; ci < rows; ci++) {
                        aligned_free(dest_gfni[ci]);
                        aligned_free(dest_ref[ci]);
                }
                return ret;
        }
}

int
main(int argc, char *argv[])
{
        int rtest, k, len, rows, failed = 0;

#if defined(__x86_64__) || defined(__i386__)
        if (!__builtin_cpu_supports("gfni")) {
                printf("gf_4vect/5vect_dot_prod_avx2_gfni: GFNI not supported, skip\n");
                return 0;
        }
#else
        printf("gf_4vect/5vect_dot_prod_avx2_gfni: non-x86 platform, skip\n");
        return 0;
#endif

        printf("gf_4vect_dot_prod_avx2_gfni / gf_5vect_dot_prod_avx2_gfni: ");

        srand(0x12345678);

        /* Test all buffer sizes 1..256 for both 4-vect and 5-vect */
        for (len = 1; len <= 256 && !failed; len++) {
                k = (len % (TEST_SOURCES - 1)) + 1;
                if (test_gfni_vs_base(len, k, 4) != 0) {
                        failed = 1;
                        break;
                }
                if (test_gfni_vs_base(len, k, 5) != 0) {
                        failed = 1;
                        break;
                }
        }

        /* Random parameter tests */
        for (rtest = 0; rtest < RANDOMS && !failed; rtest++) {
                k = (rand() % (TEST_SOURCES - 1)) + 1;
                len = ((rand() % TEST_LEN) & ~1) + 2;
                if (test_gfni_vs_base(len, k, 4) != 0)
                        failed = 1;
        }
        for (rtest = 0; rtest < RANDOMS && !failed; rtest++) {
                k = (rand() % (TEST_SOURCES - 1)) + 1;
                len = ((rand() % TEST_LEN) & ~1) + 2;
                if (test_gfni_vs_base(len, k, 5) != 0)
                        failed = 1;
        }

        /* Test ec_encode_data_avx2_gfni dispatch with 4, 5, and 9 rows */
        for (rows = 4; rows <= 9 && !failed; rows++) {
                for (rtest = 0; rtest < RANDOMS / 2 && !failed; rtest++) {
                        k = (rand() % (TEST_SOURCES / 2 - 1)) + 1;
                        len = ((rand() % (TEST_LEN / 2)) & ~1) + 2;
                        if (test_encode_avx2_gfni(len, k, rows) != 0)
                                failed = 1;
                }
        }

        if (failed) {
                printf("Fail\n");
                return 1;
        }

        printf("Pass\n");
        return 0;
}
