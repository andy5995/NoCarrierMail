/*
 * NoCarrierMail offline mail reader
 * QWK HEADERS.DAT parser (Synchronet extended headers)

 Distributed under the GNU General Public License, version 3 or later. */

#include "qwkhdr.h"
#include "misc.h"

extern "C" {
#include <ctype.h>
#include <string.h>
#include <stdlib.h>
}

char *iniTrim(char *s)
{
    while (*s && isspace((unsigned char) *s))
        s++;

    char *end = s + strlen(s);
    while ((end > s) && isspace((unsigned char) end[-1]))
        *--end = '\0';

    return s;
}

bool iniKeyEq(const char *a, const char *b)
{
    while (*a && *b) {
        if (tolower((unsigned char) *a) != tolower((unsigned char) *b))
            return false;
        a++;
        b++;
    }
    return (*a == *b);
}

qwkHeaders::qwkHeaders()
{
    head = 0;
}

qwkHeaders::~qwkHeaders()
{
    section *s = head;
    while (s) {
        field *f = s->fields;
        while (f) {
            field *fnext = f->next;
            delete[] f->key;
            delete[] f->value;
            delete f;
            f = fnext;
        }
        section *snext = s->next;
        delete s;
        s = snext;
    }
}

void qwkHeaders::parse(const char *text)
{
    if (!text)
        return;

    char *work = strdupplus(text);
    section *cur = 0;
    char *line = work;

    while (line && *line) {
        char *nl = strchr(line, '\n');
        if (nl)
            *nl = '\0';

        char *t = iniTrim(line);

        if (*t == '[') {
            // Section header: [<hex offset into MESSAGES.DAT>]
            char *close = strchr(t, ']');
            if (close) {
                *close = '\0';
                section *s = new section;
                s->offset = strtoul(t + 1, 0, 16);
                s->fields = 0;
                s->next = head;
                head = s;
                cur = s;
            }
        } else if (*t && cur) {
            // Field line: "Key: Value" or "Key = Value". Split on whichever
            // separator comes first, so values may contain the other one.
            char *colon = strchr(t, ':');
            char *equals = strchr(t, '=');
            char *sep;
            if (colon && equals)
                sep = (colon < equals) ? colon : equals;
            else
                sep = colon ? colon : equals;

            if (sep) {
                *sep = '\0';
                char *key = iniTrim(t);
                char *value = iniTrim(sep + 1);
                if (*key) {
                    field *f = new field;
                    f->key = strdupplus(key);
                    f->value = strdupplus(value);
                    f->next = cur->fields;
                    cur->fields = f;
                }
            }
        }

        line = nl ? (nl + 1) : 0;
    }

    delete[] work;
}

qwkHeaders::section *qwkHeaders::findSection(unsigned long offset) const
{
    for (section *s = head; s; s = s->next)
        if (s->offset == offset)
            return s;
    return 0;
}

bool qwkHeaders::has(unsigned long offset) const
{
    return findSection(offset) != 0;
}

bool qwkHeaders::any() const
{
    return head != 0;
}

const char *qwkHeaders::get(unsigned long offset, const char *key) const
{
    section *s = findSection(offset);
    if (s)
        for (field *f = s->fields; f; f = f->next)
            if (iniKeyEq(f->key, key))
                return f->value;
    return 0;
}

char *utf8ToLatin1(const char *s)
{
    char *out = new char[strlen(s) + 1];
    char *o = out;

    while (*s) {
        unsigned char c = *s++;
        unsigned long cp;
        int extra;

        if (c < 0x80) {
            cp = c;
            extra = 0;
        } else if ((c & 0xE0) == 0xC0) {
            cp = c & 0x1F;
            extra = 1;
        } else if ((c & 0xF0) == 0xE0) {
            cp = c & 0x0F;
            extra = 2;
        } else if ((c & 0xF8) == 0xF0) {
            cp = c & 0x07;
            extra = 3;
        } else {
            cp = '?';        // invalid lead byte
            extra = 0;
        }

        while (extra-- && (((unsigned char) *s & 0xC0) == 0x80))
            cp = (cp << 6) | ((unsigned char) *s++ & 0x3F);

        *o++ = (cp <= 0xFF) ? (char) cp : '?';
    }
    *o = '\0';

    return out;
}
