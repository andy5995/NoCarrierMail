/*
 * NoCarrierMail offline mail reader
 * Unit tests for mmail/qwkhdr.cc (Synchronet HEADERS.DAT parser)

 Distributed under the GNU General Public License, version 3 or later. */

#include "test.h"
#include "../mmail/qwkhdr.h"

// Index text as HEADERS.DAT, through a temporary file as the reader does.
static void openText(qwkHeaders &h, const char *text)
{
    FILE *f = tmpfile();

    fputs(text, f);
    rewind(f);
    h.open(f);
}

int main()
{
    // A two-message HEADERS.DAT, as Synchronet's msgtoqwk.cc writes it:
    // hex-offset sections, RFC-style "Key: Value" plus ini-style "Key = Value",
    // CRLF line endings, and values that themselves contain ':' and '='.
    const char *sample =
        "[1a80]\r\n"
        "Utf8 = true\r\n"
        "Format = flowed\r\n"
        "Message-ID: <100:200/300@vert>\r\n"
        "WhenWritten:  Fri Jun 05 12:34:56 2026 0000\r\n"
        "Sender: Digital Man\r\n"
        "Subject: longer than the 25-char MESSAGES.DAT field = yes\r\n"
        "Recipient: All\r\n"
        "Conference: 12\r\n"
        "\r\n"
        "[2b00]\r\n"
        "Sender: Rob Swindell\r\n"
        "Subject: second message\r\n";

    qwkHeaders h;
    openText(h, sample);

    // Sections are keyed by hex offset into MESSAGES.DAT
    CHECK(h.has(0x1a80));
    CHECK(h.has(0x2b00));
    CHECK(!h.has(0x9999));

    // any(): true once a non-empty HEADERS.DAT has been parsed
    CHECK(h.any());

    // Full (untruncated) RFC-style ':' fields
    CHECK_STR(h.get(0x1a80, "Sender"), "Digital Man");
    CHECK_STR(h.get(0x1a80, "Recipient"), "All");
    CHECK_STR(h.get(0x1a80, "Conference"), "12");

    // ini-style '=' fields
    CHECK_STR(h.get(0x1a80, "Utf8"), "true");
    CHECK_STR(h.get(0x1a80, "Format"), "flowed");

    // A value containing ':' must split only on the first (key) separator
    CHECK_STR(h.get(0x1a80, "Message-ID"), "<100:200/300@vert>");
    CHECK_STR(h.get(0x1a80, "WhenWritten"), "Fri Jun 05 12:34:56 2026 0000");

    // A value containing '=' after the ':' separator stays intact
    CHECK_STR(h.get(0x1a80, "Subject"),
              "longer than the 25-char MESSAGES.DAT field = yes");

    // Keys are case-insensitive
    CHECK_STR(h.get(0x1a80, "sender"), "Digital Man");
    CHECK_STR(h.get(0x1a80, "MESSAGE-ID"), "<100:200/300@vert>");

    // The second section is independent
    CHECK_STR(h.get(0x2b00, "Sender"), "Rob Swindell");
    CHECK_STR(h.get(0x2b00, "Subject"), "second message");

    // Absent keys / offsets return 0
    CHECK(h.get(0x1a80, "Nonexistent") == 0);
    CHECK(h.get(0x9999, "Sender") == 0);

    // Asking about one section, then another, then the first again reads
    // each back from the file.
    CHECK_STR(h.get(0x1a80, "Sender"), "Digital Man");
    CHECK_STR(h.get(0x2b00, "Sender"), "Rob Swindell");
    CHECK_STR(h.get(0x1a80, "Subject"),
              "longer than the 25-char MESSAGES.DAT field = yes");

    // keepOnly(): unlisted keys are not stored, matched case-insensitively.
    static const char *const used[] = { "sender", "Utf8", 0 };
    qwkHeaders k;
    k.keepOnly(used);
    openText(k, sample);
    CHECK(k.has(0x2b00));
    CHECK_STR(k.get(0x1a80, "Sender"), "Digital Man");
    CHECK_STR(k.get(0x1a80, "Utf8"), "true");
    CHECK(!k.get(0x1a80, "Subject"));
    CHECK(!k.get(0x2b00, "Subject"));

    // Sections out of offset order are still found.
    qwkHeaders u;
    openText(u, "[300]\nSender: C\n[100]\nSender: A\n[200]\nSender: B\n");
    CHECK_STR(u.get(0x100, "Sender"), "A");
    CHECK_STR(u.get(0x200, "Sender"), "B");
    CHECK_STR(u.get(0x300, "Sender"), "C");
    CHECK(!u.has(0x400));

    // More sections than one index chunk holds
    char *many = new char[700 * 32];
    char *m = many;
    for (int i = 1; i <= 700; i++)
        m += sprintf(m, "[%x]\nSender: n%d\n", i * 0x80, i);
    qwkHeaders big;
    openText(big, many);
    delete[] many;
    CHECK_STR(big.get(0x80, "Sender"), "n1");
    CHECK_STR(big.get(300 * 0x80, "Sender"), "n300");
    CHECK_STR(big.get(700 * 0x80, "Sender"), "n700");
    CHECK(!big.has(701 * 0x80));
    CHECK(!big.has(0x81));

    // Empty input: nothing found, and nothing to read
    qwkHeaders empty;
    openText(empty, "");
    CHECK(!empty.has(0x1a80));
    CHECK(!empty.any());
    CHECK(!empty.get(0x1a80, "Sender"));
    qwkHeaders none;
    CHECK(!none.any());
    CHECK(!none.get(0x1a80, "Sender"));

    // Round-trip: the exact section format qwkreply::addRep1() writes for a
    // reply with long fields must parse back through this reader. Keeps the
    // REP write side (qwk.cc) and the read side from drifting apart.
    const char *written =
        "[80]\r\n"
        "Utf8: false\r\n"
        "Sender: A Sender Whose Name Exceeds 25 Characters\r\n"
        "To: A Recipient Whose Name Also Exceeds 25 Characters\r\n"
        "Subject: A subject line that is well past the 25-char limit\r\n"
        "\r\n";

    qwkHeaders rt;
    openText(rt, written);
    CHECK(rt.has(0x80));
    CHECK_STR(rt.get(0x80, "Utf8"), "false");
    CHECK_STR(rt.get(0x80, "Sender"),
              "A Sender Whose Name Exceeds 25 Characters");
    CHECK_STR(rt.get(0x80, "To"),
              "A Recipient Whose Name Also Exceeds 25 Characters");
    CHECK_STR(rt.get(0x80, "Subject"),
              "A subject line that is well past the 25-char limit");

    return mm_test_report();
}
