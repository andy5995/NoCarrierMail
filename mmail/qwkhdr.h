/*
 * NoCarrierMail offline mail reader
 * QWK HEADERS.DAT parser (Synchronet extended headers)

 Distributed under the GNU General Public License, version 3 or later. */

#ifndef QWKHDR_H
#define QWKHDR_H

extern "C" {
#include <stdio.h>
}

/* Reads a Synchronet HEADERS.DAT file: an .ini-style file whose sections are
   named by the hex byte-offset of a message's header in MESSAGES.DAT, holding
   long/extended header fields ("Sender", "To", "Subject", "WhenWritten",
   "Message-ID", the "Utf8" flag, etc.). Field lines use either ':' or '=' as
   the key/value separator. This class is self-contained (no curses, no
   globals) so it can be unit-tested in isolation.

   Only an index of where each section starts is kept in memory; a section's
   fields are read from the file when asked for. A big packet's headers run
   to hundreds of kilobytes, which on DOS would leave too little memory to
   run the archiver or the editor. */

// Shared with the VOTING.DAT parser, which reads the same .ini-ish syntax.
// Trim leading/trailing whitespace in place; returns the trimmed start.
char *iniTrim(char *);
// Case-insensitive whole-string compare (no strcasecmp/stricmp dependency).
bool iniKeyEq(const char *, const char *);
// Split a "Key: Value" / "Key = Value" line in place, on whichever separator
// comes first (so values may contain the other one). Trims both parts.
// Returns false when the line has no separator.
bool iniSplit(char *line, char *&key, char *&value);
// Read one line into buf (size bytes), newline kept; the rest of a line too
// long for buf is skipped. Returns false at the end of the file.
bool iniReadLine(FILE *f, char *buf, int size);
// Line buffer size both parsers pass to iniReadLine()
enum { INILINE = 1024 };
// Is this trimmed line a "[...]" section header?
bool iniSection(const char *);

class qwkHeaders {
    enum { CHUNK = 256 };

    // The index, in chunks so that no one block passes 64 KB on a 16-bit
    // build. Synchronet writes the sections in offset order, which allows
    // a binary search; otherwise lookups fall back to a scan.
    struct chunk {
        unsigned long offset[CHUNK];    // section name
        long pos[CHUNK];                // where its "[...]" line starts
        int n;
        chunk *next;
    };
    struct field {
        char *key, *value;
        field *next;
    };

    chunk *first, *last;
    bool sorted;
    FILE *file;
    char *line;

    // The one section held in memory, the last one asked for
    bool loaded;
    unsigned long loadedOffset;
    field *fields;

    long find(unsigned long) const;
    void addField(char *);
    void clearFields();
    void load(unsigned long);

 public:
    qwkHeaders();
    ~qwkHeaders();

    // Index an open HEADERS.DAT. The file stays open, and this object
    // closes it.
    void open(FILE *f);

    // Is there a section for this MESSAGES.DAT (byte) offset?
    bool has(unsigned long offset) const;

    // Did the packet ship a non-empty HEADERS.DAT (any sections found)?
    bool any() const;

    // Value for a key (case-insensitive) within a section, or 0 if absent.
    // The pointer stays valid until get() is asked about another section.
    const char *get(unsigned long offset, const char *key);
};

#endif
