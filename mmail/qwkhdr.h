/*
 * NoCarrierMail offline mail reader
 * QWK HEADERS.DAT parser (Synchronet extended headers)

 Distributed under the GNU General Public License, version 3 or later. */

#ifndef QWKHDR_H
#define QWKHDR_H

/* Parses a Synchronet HEADERS.DAT file: an .ini-style file whose sections are
   named by the hex byte-offset of a message's header in MESSAGES.DAT, holding
   long/extended header fields ("Sender", "Recipient", "Subject", "WhenWritten",
   "Message-ID", the "Utf8" flag, etc.). Field lines use either ':' or '=' as
   the key/value separator. This class is self-contained (no curses, no
   globals) so it can be unit-tested in isolation. */

// Shared with the VOTING.DAT parser, which reads the same .ini-ish syntax.
// Trim leading/trailing whitespace in place; returns the trimmed start.
char *iniTrim(char *);
// Case-insensitive whole-string compare (no strcasecmp/stricmp dependency).
bool iniKeyEq(const char *, const char *);
// Split a "Key: Value" / "Key = Value" line in place, on whichever separator
// comes first (so values may contain the other one). Trims both parts.
// Returns false when the line has no separator.
bool iniSplit(char *line, char *&key, char *&value);

class qwkHeaders {
    struct field {
        char *key, *value;
        field *next;
    };
    struct section {
        unsigned long offset;
        field *fields;
        section *next;
    };
    section *head;

    section *findSection(unsigned long) const;

 public:
    qwkHeaders();
    ~qwkHeaders();

    // Parse null-terminated HEADERS.DAT text into this object.
    void parse(const char *text);

    // Is there a section for this MESSAGES.DAT (byte) offset?
    bool has(unsigned long offset) const;

    // Did the packet ship a non-empty HEADERS.DAT (any sections parsed)?
    bool any() const;

    // Value for a key (case-insensitive) within a section, or 0 if absent.
    const char *get(unsigned long offset, const char *key) const;
};

// Convert a UTF-8 string to Latin-1 (codepoints above 0xFF become '?').
// Returns a newly-allocated string (caller deletes[] it); used for HEADERS.DAT
// fields flagged Utf8=true.
char *utf8ToLatin1(const char *s);

#endif
