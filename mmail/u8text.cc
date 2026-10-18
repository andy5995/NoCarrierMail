/*
 * NoCarrierMail offline mail reader
 * Display width and character stepping for UTF-8 or single-byte text

 Distributed under the GNU General Public License, version 3 or later. */

#include "u8text.h"

extern "C" {
#include <string.h>
}

struct u8range {
    unsigned long first, last;
};

// Zero-width: combining marks, joiners, variation selectors.
static const u8range zeroWidth[] = {
    { 0x0300, 0x036F }, { 0x0483, 0x0489 }, { 0x0591, 0x05BD },
    { 0x05BF, 0x05BF }, { 0x05C1, 0x05C2 }, { 0x05C4, 0x05C5 },
    { 0x05C7, 0x05C7 }, { 0x0610, 0x061A }, { 0x064B, 0x065F },
    { 0x0670, 0x0670 }, { 0x06D6, 0x06DC }, { 0x06DF, 0x06E4 },
    { 0x06E7, 0x06E8 }, { 0x06EA, 0x06ED }, { 0x0900, 0x0902 },
    { 0x093A, 0x093A }, { 0x093C, 0x093C }, { 0x0941, 0x0948 },
    { 0x094D, 0x094D }, { 0x0951, 0x0957 }, { 0x0E31, 0x0E31 },
    { 0x0E34, 0x0E3A }, { 0x0E47, 0x0E4E }, { 0x1AB0, 0x1AFF },
    { 0x1DC0, 0x1DFF }, { 0x200B, 0x200F }, { 0x202A, 0x202E },
    { 0x2060, 0x2064 }, { 0x20D0, 0x20FF }, { 0xFE00, 0xFE0F },
    { 0xFE20, 0xFE2F }, { 0xFEFF, 0xFEFF }, { 0xE0100, 0xE01EF }
};

// Two columns: East Asian wide and fullwidth, and the common emoji blocks.
static const u8range doubleWidth[] = {
    { 0x1100, 0x115F }, { 0x231A, 0x231B }, { 0x2329, 0x232A },
    { 0x23E9, 0x23EC }, { 0x23F0, 0x23F0 }, { 0x23F3, 0x23F3 },
    { 0x25FD, 0x25FE }, { 0x2614, 0x2615 }, { 0x2648, 0x2653 },
    { 0x267F, 0x267F }, { 0x2693, 0x2693 }, { 0x26A1, 0x26A1 },
    { 0x26AA, 0x26AB }, { 0x26BD, 0x26BE }, { 0x26C4, 0x26C5 },
    { 0x26CE, 0x26CE }, { 0x26D4, 0x26D4 }, { 0x26EA, 0x26EA },
    { 0x26F2, 0x26F3 }, { 0x26F5, 0x26F5 }, { 0x26FA, 0x26FA },
    { 0x26FD, 0x26FD }, { 0x2705, 0x2705 }, { 0x270A, 0x270B },
    { 0x2728, 0x2728 }, { 0x274C, 0x274C }, { 0x274E, 0x274E },
    { 0x2753, 0x2755 }, { 0x2757, 0x2757 }, { 0x2795, 0x2797 },
    { 0x27B0, 0x27B0 }, { 0x27BF, 0x27BF }, { 0x2B1B, 0x2B1C },
    { 0x2B50, 0x2B50 }, { 0x2B55, 0x2B55 }, { 0x2E80, 0x303E },
    { 0x3041, 0x33FF }, { 0x3400, 0x4DBF }, { 0x4E00, 0x9FFF },
    { 0xA000, 0xA4CF }, { 0xA960, 0xA97F }, { 0xAC00, 0xD7A3 },
    { 0xF900, 0xFAFF }, { 0xFE10, 0xFE19 }, { 0xFE30, 0xFE6F },
    { 0xFF00, 0xFF60 }, { 0xFFE0, 0xFFE6 }, { 0x16FE0, 0x16FE4 },
    { 0x17000, 0x18AFF }, { 0x1B000, 0x1B2FF }, { 0x1F004, 0x1F004 },
    { 0x1F0CF, 0x1F0CF }, { 0x1F18E, 0x1F18E }, { 0x1F191, 0x1F19A },
    { 0x1F200, 0x1F251 }, { 0x1F300, 0x1F64F }, { 0x1F680, 0x1F6FF },
    { 0x1F7E0, 0x1F7EB }, { 0x1F90C, 0x1F9FF }, { 0x1FA70, 0x1FAFF },
    { 0x20000, 0x2FFFD }, { 0x30000, 0x3FFFD }
};

static bool inTable(unsigned long cp, const u8range *t, int n)
{
    int lo = 0, hi = n - 1;

    if (cp < t[0].first || cp > t[hi].last)
        return false;
    while (lo <= hi) {
        int mid = (lo + hi) / 2;

        if (cp > t[mid].last)
            lo = mid + 1;
        else if (cp < t[mid].first)
            hi = mid - 1;
        else
            return true;
    }
    return false;
}

#define TABLESIZE(t) ((int) (sizeof t / sizeof t[0]))

int u8decode(const char *s, int len, unsigned long *cp)
{
    const unsigned char *u = (const unsigned char *) s;
    unsigned long c = u[0], min;
    int need, x;

    if (c < 0x80) {
        *cp = c;
        return 1;
    }
    if ((c & 0xE0) == 0xC0) {
        c &= 0x1F;
        need = 1;
        min = 0x80;
    } else if ((c & 0xF0) == 0xE0) {
        c &= 0x0F;
        need = 2;
        min = 0x800;
    } else if ((c & 0xF8) == 0xF0) {
        c &= 0x07;
        need = 3;
        min = 0x10000UL;
    } else {
        *cp = 0xFFFD;
        return 1;
    }

    if (need >= len) {
        *cp = 0xFFFD;
        return 1;
    }
    for (x = 1; x <= need; x++) {
        if ((u[x] & 0xC0) != 0x80) {
            *cp = 0xFFFD;
            return 1;
        }
        c = (c << 6) | (u[x] & 0x3F);
    }

    // overlong forms, surrogates and values past U+10FFFF are invalid
    if (c < min || (c >= 0xD800 && c <= 0xDFFF) || c > 0x10FFFFUL) {
        *cp = 0xFFFD;
        return 1;
    }
    *cp = c;
    return need + 1;
}

int u8cpWidth(unsigned long cp)
{
    if (inTable(cp, zeroWidth, TABLESIZE(zeroWidth)))
        return 0;
    if (inTable(cp, doubleWidth, TABLESIZE(doubleWidth)))
        return 2;
    return 1;
}

static int textLen(const char *s, int len)
{
    return (len < 0) ? (int) strlen(s) : len;
}

// With len -1, 4 bytes are read at a time: a NUL ends any sequence, and its
// width of 1 ends the run of marks.
int u8char(const char *s, int len, int *w)
{
    unsigned long cp;
    int n = u8decode(s, (len < 0) ? 4 : len, &cp);

    *w = u8cpWidth(cp);
    while ((len < 0) || (n < len)) {
        int m = u8decode(s + n, (len < 0) ? 4 : len - n, &cp);

        if (u8cpWidth(cp))
            break;
        n += m;
    }
    return n;
}

int u8width(const char *s, int len, bool utf8)
{
    len = textLen(s, len);
    if (!utf8)
        return len;

    int cols = 0, pos = 0;

    while (pos < len) {
        int w;

        pos += u8char(s + pos, len - pos, &w);
        cols += w;
    }
    return cols;
}

char *u8toLatin1(char *s)
{
    char *in = s, *out = s;
    int len = strlen(s);

    while (*in) {
        unsigned long cp;
        int n = u8decode(in, len, &cp);

        in += n;
        len -= n;
        if (!u8cpWidth(cp))
            continue;
        switch (cp) {
        case 0x2010: case 0x2011: case 0x2012: case 0x2013: case 0x2014:
        case 0x2015: case 0x2212:
            cp = '-';
            break;
        case 0x2018: case 0x2019: case 0x201A: case 0x2032:
            cp = '\'';
            break;
        case 0x201C: case 0x201D: case 0x201E: case 0x2033:
            cp = '"';
            break;
        case 0x2026:              // three bytes in, three out
            *out++ = '.';
            *out++ = '.';
            cp = '.';
        }
        *out++ = (cp > 0xFF) ? '?' : (char) cp;
    }
    *out = '\0';
    return s;
}

int u8fit(const char *s, int len, int maxcols, int maxbytes, bool utf8)
{
    int cols = 0, pos = 0;

    len = textLen(s, len);
    int cap = (maxbytes >= 0 && maxbytes < len) ? maxbytes : len;

    if (!utf8)
        return (maxcols >= 0 && maxcols < cap) ? maxcols : cap;

    // decode against the whole text, so a character cut by the byte budget
    // is left out rather than read as a broken sequence
    while (pos < cap) {
        int w, n = u8char(s + pos, len - pos, &w);

        if (pos + n > cap || (maxcols >= 0 && cols + w > maxcols))
            break;
        cols += w;
        pos += n;
    }
    return pos;
}
