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

bool iniSplit(char *line, char *&key, char *&value)
{
    char *colon = strchr(line, ':');
    char *equals = strchr(line, '=');
    char *sep;
    if (colon && equals)
        sep = (colon < equals) ? colon : equals;
    else
        sep = colon ? colon : equals;

    if (!sep)
        return false;

    *sep = '\0';
    key = iniTrim(line);
    value = iniTrim(sep + 1);
    return true;
}

bool iniReadLine(FILE *f, char *buf, int size)
{
    if (!fgets(buf, size, f))
        return false;

    if (!strchr(buf, '\n')) {
        int c;
        while (((c = fgetc(f)) != EOF) && (c != '\n'))
            ;
    }
    return true;
}

qwkHeaders::qwkHeaders()
{
    first = last = 0;
    sorted = true;
    file = 0;
    line = 0;
    wanted = 0;
    loaded = false;
    loadedOffset = 0;
    fields = 0;
}

qwkHeaders::~qwkHeaders()
{
    clearFields();
    while (first) {
        chunk *next = first->next;
        delete first;
        first = next;
    }
    delete[] line;
    if (file)
        fclose(file);
}

void qwkHeaders::keepOnly(const char *const *keys)
{
    wanted = keys;
}

void qwkHeaders::open(FILE *f)
{
    file = f;
    line = new char[LINELEN];

    unsigned long prev = 0;
    long pos = ftell(file);
    while (iniReadLine(file, line, LINELEN)) {
        char *t = iniTrim(line);

        // Section header: [<hex offset into MESSAGES.DAT>]
        if ((*t == '[') && strchr(t, ']')) {
            unsigned long offset = strtoul(t + 1, 0, 16);

            if (last && (offset <= prev))
                sorted = false;
            prev = offset;

            if (!last || (last->n == CHUNK)) {
                chunk *c = new chunk;
                c->n = 0;
                c->next = 0;
                if (last)
                    last->next = c;
                else
                    first = c;
                last = c;
            }
            last->offset[last->n] = offset;
            last->pos[last->n++] = pos;
        }
        pos = ftell(file);
    }
}

long qwkHeaders::find(unsigned long offset) const
{
    for (chunk *c = first; c; c = c->next) {
        if (sorted) {
            if (offset > c->offset[c->n - 1])
                continue;

            int lo = 0, hi = c->n - 1;
            while (lo <= hi) {
                int mid = (lo + hi) / 2;
                if (c->offset[mid] == offset)
                    return c->pos[mid];
                if (c->offset[mid] < offset)
                    lo = mid + 1;
                else
                    hi = mid - 1;
            }
            return -1;
        }
        for (int i = 0; i < c->n; i++)
            if (c->offset[i] == offset)
                return c->pos[i];
    }
    return -1;
}

bool qwkHeaders::has(unsigned long offset) const
{
    return find(offset) >= 0;
}

bool qwkHeaders::any() const
{
    return first != 0;
}

void qwkHeaders::clearFields()
{
    while (fields) {
        field *next = fields->next;
        delete[] fields->key;
        delete[] fields->value;
        delete fields;
        fields = next;
    }
}

void qwkHeaders::addField(char *t)
{
    char *key, *value;

    if (!iniSplit(t, key, value) || !*key)
        return;

    if (wanted) {
        bool keep = false;
        for (const char *const *w = wanted; *w && !keep; w++)
            keep = iniKeyEq(key, *w);
        if (!keep)
            return;
    }

    field *f = new field;
    f->key = strdupplus(key);
    f->value = strdupplus(value);
    f->next = fields;
    fields = f;
}

// Read one section's fields, the lines between its "[...]" line and the next
void qwkHeaders::load(unsigned long offset)
{
    clearFields();
    loaded = true;
    loadedOffset = offset;

    long pos = find(offset);
    if ((pos < 0) || fseek(file, pos, SEEK_SET))
        return;

    if (!iniReadLine(file, line, LINELEN))
        return;
    while (iniReadLine(file, line, LINELEN)) {
        char *t = iniTrim(line);

        if (*t == '[')
            break;
        if (*t)
            addField(t);
    }
}

const char *qwkHeaders::get(unsigned long offset, const char *key)
{
    if (!file)
        return 0;
    if (!loaded || (loadedOffset != offset))
        load(offset);

    for (field *f = fields; f; f = f->next)
        if (iniKeyEq(f->key, key))
            return f->value;
    return 0;
}
