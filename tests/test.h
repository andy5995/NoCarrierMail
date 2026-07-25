/*
 * NoCarrierMail offline mail reader
 * Minimal unit-test harness

 Distributed under the GNU General Public License, version 3 or later. */

#ifndef MM_TEST_H
#define MM_TEST_H

#include <cstdio>
#include <cstring>

static int mm_checks, mm_fails;

#define CHECK(cond) \
    do { \
        ++mm_checks; \
        if (!(cond)) { \
            ++mm_fails; \
            fprintf(stderr, "  FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond); \
        } \
    } while (0)

#define CHECK_STR(got, want) CHECK(!strcmp((got), (want)))

static inline int mm_test_report()
{
    fprintf(stderr, "%d checks, %d failure%s\n",
            mm_checks, mm_fails, (mm_fails == 1) ? "" : "s");
    return mm_fails ? 1 : 0;
}

#endif
