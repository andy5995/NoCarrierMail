/*
 * NoCarrierMail offline mail reader
 * QWK message-header struct (qheader)

 Copyright 1997 John Zero <john@graphisoft.hu>
 Copyright 1997-2021 William McBrine <wmcbrine@gmail.com>
 Distributed under the GNU General Public License, version 3 or later. */

// Split out of qwk.cc so the read/write round-trip can be unit-tested without
// pulling in the curses-free-but-mm-dependent QWK driver classes. These
// methods only touch the packet's fixed-width header layout -- no globals,
// no DateFormat: qHead.date stays the raw "MM-DD-YY HH:MM" so output() can
// re-pack it verbatim (see the note in init()).

#include "qwk.h"

bool qheader::init(FILE *datFile)
{
    qwkmsg_header qh;
    char buf[9], *err;

    if (!fread(&qh, 1, sizeof qh, datFile))
        return false;

    get_field(from, qh.from, 25);
    get_field(to, qh.to, 25);
    get_field(subject, qh.subject, 25);

    // Keep the RAW "MM-DD-YY HH:MM" here. This field is re-packed verbatim
    // into the fixed-width QWK header by output() (the reply round-trip), so
    // it must stay the packet's format. The user's DateFormat is applied only
    // for display, in qwkDispDate() -- never stored back into qHead.date.
    memcpy(date, qh.date, 8);
    date[2] = '-';
    date[5] = '-';        // some packets ship broken separators
    date[8] = ' ';
    strnzcpy(date + 9, qh.time, 5);

    strnzcpy(buf, qh.refnum, 8);
    refnum = atol(buf);

    get_field(buf, qh.msgnum, 7);
    msgnum = strtol(buf, &err, 10);
    if (*err)
        return false;    // bogus message

    strnzcpy(buf, qh.chunks, 6);
    msglen = (atol(buf) - 1) * 128;

    privat = (qh.status == '*') || (qh.status == '+');
    status = qh.status;

    // Is this a block of net-status flags?
    netblock = !qh.status || (qh.status == '\xff');

    origArea = getshort(&qh.confLSB);

    return true;
}

// Just the fields needed for building the index:
bool qheader::init_short(FILE *datFile)
{
    qwkmsg_header qh;
    char buf[9], *err;
    long rawlen;

    if (!fread(&qh, 1, sizeof qh, datFile))
        return false;

    get_field(to, qh.to, 25);

    status = qh.status;

    strnzcpy(buf, qh.chunks, 6);
    rawlen = strtol(buf, &err, 10);
    if ((*err && *err != ' ') || (rawlen < 2)) {
        // A one-chunk record is a header with no body: how Synchronet writes
        // a poll or ballot (status 'V'). Anything else here is a bad header.
        // Either way keep scanning; readIndices() decides what to index, so
        // fill in the fields it needs to make that call.
        netblock = true;
        msglen = 0;
        origArea = getshort(&qh.confLSB);
        return true;
    }

    msglen = (rawlen - 1) << 7;

    // Is this a block of net-status flags?
    netblock = !qh.status || (qh.status == '\xff');

    origArea = getshort(&qh.confLSB);

    return true;
}

// Write the header to a file, except for the length:
void qheader::output(FILE *repFile)
{
    qwkmsg_header qh;
    char buf[10];
    size_t sublen, tolen, fromlen;

    sublen = strlen(subject);
    if (sublen > 25)
        sublen = 25;

    tolen = strlen(to);
    if (tolen > 25)
        tolen = 25;

    fromlen = strlen(from);
    if (fromlen > 25)
        fromlen = 25;

    memset(&qh, ' ', sizeof qh);

    sprintf(buf, " %-6ld", msgnum);
    memcpy(qh.msgnum, buf, 7);

    putshort(&qh.confLSB, msgnum);

    if (refnum) {
        sprintf(buf, " %-7ld", refnum);
        memcpy(qh.refnum, buf, 8);
    }
    memcpy(qh.to, to, tolen);
    memcpy(qh.from, from, fromlen);
    memcpy(qh.subject, subject, sublen);

    qh.alive = (char) 0xE1;

    memcpy(qh.date, date, 8);
    memcpy(qh.time, &date[9], 5);

    if (privat)
        qh.status = '*';

    fwrite(&qh, 1, sizeof qh, repFile);
}

// Pad out with spaces, as necessary, and write the length to the header:
void qheader::set_length(FILE *repFile, long headerpos, long curpos)
{
    long length;

    for (length = curpos - headerpos; (length & 0x7f); length++)
        fputc(' ', repFile);

    fseek(repFile, headerpos + CHUNK_OFFSET, SEEK_SET);
    fprintf(repFile, "%-6ld", (length >> 7));
    fseek(repFile, headerpos + length, SEEK_SET);
}

// Return space-padded field as string, stripped and null-terminated
void qheader::get_field(char *dest, const char *source, size_t len)
{
    while (len && (' ' == source[len - 1]))
        len--;
    memcpy(dest, source, len);
    dest[len] = '\0';
}
