/*
 * NoCarrierMail offline mail reader
 * Unit tests for mmail/misc.cc (pure helpers; no curses, no globals)

 Distributed under the GNU General Public License, version 3 or later. */

#include "test.h"
#include "../config.h"   // DOSNAMES -- must match what misc.cc compiles
#include "../mmail/misc.h"

int main()
{
    unsigned char b[4];

    // Little-endian 16-bit: byte order and round-trip
    putshort(b, 0x1234);
    CHECK(b[0] == 0x34 && b[1] == 0x12);
    CHECK(getshort(b) == 0x1234u);
    putshort(b, 0xFFFF);
    CHECK(getshort(b) == 0xFFFFu);

    // Little-endian 32-bit
    putlong(b, 0x89ABCDEFUL);
    CHECK(b[0] == 0xEF && b[3] == 0x89);
    CHECK(getlong(b) == 0x89ABCDEFUL);

    // Big-endian 32-bit
    putblong(b, 0x89ABCDEFUL);
    CHECK(b[0] == 0x89 && b[3] == 0xEF);
    CHECK(getblong(b) == 0x89ABCDEFUL);

    // Same bytes, opposite endianness reads differently
    putlong(b, 0x00000001UL);
    CHECK(getblong(b) == 0x01000000UL);

    // strnzcpy: truncates to len, always NUL-terminates, returns end pointer
    char buf[16];
    char *end = strnzcpy(buf, "hello", 3);
    CHECK_STR(buf, "hel");
    CHECK(end == buf + 3 && *end == '\0');
    end = strnzcpy(buf, "hi", 10);
    CHECK_STR(buf, "hi");
    CHECK(end == buf + 2);

    // stripre: strips a leading, case-insensitive "re: ", repeatedly
    CHECK_STR(stripre("Re: hello"), "hello");
    CHECK_STR(stripre("RE: re: x"), "x");
    CHECK_STR(stripre("hello"), "hello");
    CHECK_STR(stripre("Reply"), "Reply");

    // formatDate: an explicit strftime format is locale-independent
    struct tm dt;
    memset(&dt, 0, sizeof dt);
    dt.tm_year = 126;   // 2026
    dt.tm_mon = 5;      // June
    dt.tm_mday = 5;
    dt.tm_hour = 14;
    dt.tm_min = 30;
    char ds[40];
    formatDate(ds, sizeof ds, &dt, "%Y-%m-%d %H:%M");
    CHECK_STR(ds, "2026-06-05 14:30");
    // mktime normalization fills the weekday: 2026-06-05 was a Friday
    formatDate(ds, sizeof ds, &dt, "%a");
    CHECK_STR(ds, "Fri");
    // an empty format falls back to a non-empty locale rendering
    formatDate(ds, sizeof ds, &dt, "");
    CHECK(ds[0] != '\0');

    // quotespace: a single shell-safe argument, metacharacters neutralized.
    // The POSIX form single-quotes; '\'' is the only in-quote escape.
    char *q;
#ifndef DOSNAMES
    q = quotespace("plain");
    CHECK_STR(q, "'plain'");
    delete[] q;
    q = quotespace("with space");
    CHECK_STR(q, "'with space'");
    delete[] q;
    q = quotespace("a;rm -rf ~|b`c`$d");
    CHECK_STR(q, "'a;rm -rf ~|b`c`$d'");
    delete[] q;
    q = quotespace("it's");
    CHECK_STR(q, "'it'\\''s'");
    delete[] q;
    q = quotespace("");
    CHECK_STR(q, "''");
    delete[] q;
#else
    q = quotespace("plain");
    CHECK_STR(q, "\"plain\"");
    delete[] q;
    q = quotespace("a&b|c");
    CHECK_STR(q, "\"a&b|c\"");
    delete[] q;
    q = quotespace("a\"b");
    CHECK_STR(q, "\"a\"\"b\"");
    delete[] q;
    q = quotespace("a%PATH%b");
    CHECK_STR(q, "\"aPATHb\"");
    delete[] q;
#endif

    return mm_test_report();
}
