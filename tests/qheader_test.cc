/*
 * NoCarrierMail offline mail reader
 * Unit tests for mmail/qheader.cc (QWK message-header round-trip)

 Distributed under the GNU General Public License, version 3 or later. */

#include "test.h"
#include "../mmail/qwk.h"

int main()
{
    // A reply header written by output() must read back through init()
    // unchanged -- in particular the date must stay the raw QWK
    // "MM-DD-YY HH:MM" form. Regression: a DateFormat change once reformatted
    // the date on read-back, so re-saving an existing reply overflowed the
    // fixed 8-byte date field and corrupted it (e.g. "06-05-26 01:57" came
    // back as "06-05-20" + a garbled time).
    qheader h1;
    strcpy(h1.from, "Andy Alt");
    strcpy(h1.to, "Alan Ianson");
    strcpy(h1.subject, "re:syncterm");
    strcpy(h1.date, "06-05-26 01:57");
    h1.msgnum = 3047;
    h1.refnum = 49627;
    h1.privat = false;

    FILE *f = tmpfile();
    CHECK(f != 0);
    h1.output(f);

    // On disk, the header carries the raw fixed-width date (8) + time (5).
    rewind(f);
    char hdr[128];
    CHECK(fread(hdr, 1, 128, f) == 128);
    CHECK(!memcmp(hdr + 8, "06-05-26", 8));
    CHECK(!memcmp(hdr + 16, "01:57", 5));

    // Reading it back reconstructs the raw string, not a DateFormat rendering.
    rewind(f);
    qheader h2;
    CHECK(h2.init(f));
    fclose(f);

    CHECK_STR(h2.date, "06-05-26 01:57");
    CHECK_STR(h2.to, "Alan Ianson");
    CHECK_STR(h2.from, "Andy Alt");
    CHECK_STR(h2.subject, "re:syncterm");
    CHECK(h2.msgnum == 3047);
    CHECK(h2.refnum == 49627);

    return mm_test_report();
}
