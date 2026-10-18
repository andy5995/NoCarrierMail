/*
 * NoCarrierMail offline mail reader
 * Unit tests for mmail/u8text.cc (pure helpers; no curses, no globals)

 Distributed under the GNU General Public License, version 3 or later. */

#include "test.h"
#include "../mmail/u8text.h"

int main()
{
    unsigned long cp;

    // Decoding: ASCII, 2/3/4-byte forms
    CHECK(u8decode("A", 1, &cp) == 1 && cp == 'A');
    CHECK(u8decode("\xC3\xA9", 2, &cp) == 2 && cp == 0xE9);
    CHECK(u8decode("\xE2\x82\xAC", 3, &cp) == 3 && cp == 0x20AC);
    CHECK(u8decode("\xF0\x9F\x98\x80", 4, &cp) == 4 && cp == 0x1F600UL);

    // Invalid input takes one byte and decodes as U+FFFD
    CHECK(u8decode("\x80", 1, &cp) == 1 && cp == 0xFFFD);       // stray tail
    CHECK(u8decode("\xC3", 1, &cp) == 1 && cp == 0xFFFD);       // cut short
    CHECK(u8decode("\xE2\x82" "A", 3, &cp) == 1 && cp == 0xFFFD);
    CHECK(u8decode("\xC0\xAF", 2, &cp) == 1 && cp == 0xFFFD);   // overlong
    CHECK(u8decode("\xED\xA0\x80", 3, &cp) == 1 && cp == 0xFFFD); // surrogate
    CHECK(u8decode("\xF4\x90\x80\x80", 4, &cp) == 1 && cp == 0xFFFD);
    CHECK(u8decode("\xFF", 1, &cp) == 1 && cp == 0xFFFD);

    // Code point widths
    CHECK(u8cpWidth('a') == 1);
    CHECK(u8cpWidth(0xE9) == 1);
    CHECK(u8cpWidth(0x0301) == 0);         // combining acute
    CHECK(u8cpWidth(0x200D) == 0);         // zero width joiner
    CHECK(u8cpWidth(0x4E2D) == 2);         // CJK
    CHECK(u8cpWidth(0xAC00) == 2);         // Hangul
    CHECK(u8cpWidth(0x1F600UL) == 2);      // emoji
    CHECK(u8cpWidth(0x2500) == 1);         // box drawing stays narrow
    CHECK(u8cpWidth(0x0416) == 1);         // Cyrillic

    // Width of a string
    const char *mixed = "a\xE4\xB8\xAD" "e\xCC\x81" "b";  // a, CJK, e+acute, b
    CHECK(u8width(mixed, -1, true) == 5);
    CHECK(u8width(mixed, -1, false) == 8);  // one column per byte
    CHECK(u8width("", -1, true) == 0);
    CHECK(u8width("\x80\x80", -1, true) == 2);
    CHECK(u8width(mixed, 4, true) == 3);   // "a" + CJK

    // One character at a time, a combining mark kept with its base
    int w;
    CHECK(u8char(mixed, -1, &w) == 1 && w == 1);
    CHECK(u8char(mixed + 1, -1, &w) == 3 && w == 2);
    CHECK(u8char(mixed + 4, -1, &w) == 3 && w == 1);   // 'e' plus U+0301
    CHECK(u8char(mixed + 4, 1, &w) == 1 && w == 1);    // mark past len
    CHECK(u8char(mixed + 7, -1, &w) == 1 && w == 1);   // stops at the NUL
    CHECK(u8char("e\xCC\x81\xCC\x81", -1, &w) == 5); // two marks

    // Fitting a column budget never splits a character
    CHECK(u8fit(mixed, -1, 1, -1, true) == 1);
    CHECK(u8fit(mixed, -1, 2, -1, true) == 1);   // CJK only half fits
    CHECK(u8fit(mixed, -1, 3, -1, true) == 4);
    CHECK(u8fit(mixed, -1, 4, -1, true) == 7);   // mark comes along
    CHECK(u8fit(mixed, -1, 99, -1, true) == 8);
    CHECK(u8fit(mixed, -1, 0, -1, true) == 0);

    // ... nor a byte budget, as a fixed packet field would impose
    CHECK(u8fit(mixed, -1, -1, 3, true) == 1);
    CHECK(u8fit(mixed, -1, -1, 4, true) == 4);
    CHECK(u8fit(mixed, -1, -1, 6, true) == 4);   // 'e' but not its mark
    CHECK(u8fit("\xC3\xA9\xC3\xA9", -1, -1, 3, true) == 2);
    CHECK(u8fit(mixed, -1, 99, 99, true) == 8);

    // Both budgets: whichever is tighter wins
    CHECK(u8fit(mixed, -1, 3, 7, true) == 4);
    CHECK(u8fit(mixed, -1, 9, 2, true) == 1);

    // Single-byte mode is plain byte arithmetic
    CHECK(u8fit(mixed, -1, 3, -1, false) == 3);
    CHECK(u8fit(mixed, -1, 5, 2, false) == 2);
    CHECK(u8fit(mixed, -1, -1, -1, false) == 8);

    // Folding to Latin-1 for a single-byte display
    char fold[32];
    strcpy(fold, "caf\xC3\xA9 \xE2\x80\x94 \xE4\xB8\xAD" "e\xCC\x81");
    CHECK_STR(u8toLatin1(fold), "caf\xE9 - ?e");
    strcpy(fold, "\xE2\x80\x9Chi\xE2\x80\x9D it\xE2\x80\x99s\xE2\x80\xA6");
    CHECK_STR(u8toLatin1(fold), "\"hi\" it's...");
    strcpy(fold, "\x80" "ok");
    CHECK_STR(u8toLatin1(fold), "?ok");     // U+FFFD has no Latin-1 form
    strcpy(fold, "");
    CHECK_STR(u8toLatin1(fold), "");

    return mm_test_report();
}
