/*
 * NoCarrierMail offline mail reader
 * Display width and character stepping for UTF-8 or single-byte text

 Distributed under the GNU General Public License, version 3 or later. */

#ifndef U8TEXT_H
#define U8TEXT_H

/* Every function takes a 'utf8' flag saying how the text is encoded. When it
   is false, each byte is one character one column wide, which is how the
   program has always measured text, so a caller that passes false (the DOS
   build, a single-byte area or console) gets the old behaviour unchanged.

   When it is true, the text is decoded as UTF-8. A byte that is not part of a
   valid sequence counts as one character one column wide, as it would be
   drawn as a replacement character. Combining marks and other zero-width
   characters belong to the character before them: they add no width, and
   stepping or truncating never separates them from it.

   Widths come from a built-in table of the East Asian wide and zero-width
   ranges, not from the C library's wcwidth(), so results do not depend on the
   locale. A terminal whose own table differs (emoji are the usual case) may
   draw a character a column wider or narrower than measured here.

   Byte offsets and lengths are ints; 'len' arguments are byte counts, or -1
   to mean "up to the terminating NUL". Self-contained: no curses, no
   globals. */

// CP437 byte -> Unicode code point, control codes as their IBM glyphs.
extern const unsigned short cp437_to_uni[256];

// Decode one character at s, reading at most len bytes (len >= 1). Stores the
// code point in *cp (0xFFFD for an invalid sequence) and returns the number
// of bytes used, which is always at least 1.
int u8decode(const char *s, int len, unsigned long *cp);

// Columns a code point takes: 0, 1 or 2.
int u8cpWidth(unsigned long cp);

// Columns taken by the first len bytes of s.
int u8width(const char *s, int len, bool utf8);

// Byte length of the UTF-8 character at s with the marks combining with it,
// reading at most len bytes (len >= 1, or -1 to stop at the NUL without
// measuring the rest of the text). Stores its width in *cols.
int u8char(const char *s, int len, int *cols);

// Number of bytes of s that fit in maxcols columns and maxbytes bytes without
// splitting a character. A negative maxcols or maxbytes means no limit on
// that measure. A wide character that would only half fit is left out.
int u8fit(const char *s, int len, int maxcols, int maxbytes, bool utf8);

// Rewrite UTF-8 text in place as Latin-1 or as CP437, for code that keeps
// single-byte text. A character the set lacks becomes '?', except that
// dashes, curly quotes and the ellipsis become their ASCII look-alikes;
// zero-width marks are removed. The result is never longer than the input.
// Returns s.
char *u8toLatin1(char *s);
char *u8toCP437(char *s);

// A newly allocated copy of s (caller deletes[] it), folded to Latin-1 or to
// CP437 by the functions above when it is UTF-8; 0 when s is 0.
char *u8dupFold(const char *s, bool utf8, bool latin);

// The other way: a newly allocated UTF-8 copy of single-byte text, read as
// Latin-1 or as CP437; 0 when s is 0.
char *u8dupEncode(const char *s, bool latin);

#endif
