/*
 * NoCarrierMail offline mail reader
 * Unit tests for mmail/qwkvote.cc (Synchronet VOTING.DAT parser)

 Distributed under the GNU General Public License, version 3 or later. */

#include "test.h"
#include "../mmail/qwkvote.h"

int main()
{
    // A VOTING.DAT as Synchronet's msgtoqwk.cc writes it: each entry is an
    // empty "[<hex offset>]" marker followed by the section holding the data.
    // The offset belongs to the ballot's own bodyless record, so it is never
    // the message being voted on -- that is named by "In-Reply-To".
    const char *sample =
        "[180]\r\n"
        "[vote:<A1.debate@vert.synchro.net>]\r\n"
        "UpVote = true\r\n"
        "In-Reply-To: <TARGET.1@vert.synchro.net>\r\n"
        "WhenWritten:  20260723120951-0700  c1e0\r\n"
        "Sender: Digital Man\r\n"
        "Conference: 1003\r\n"
        "\r\n"
        "[e00]\r\n"
        "[vote:<A2.debate@vert.synchro.net>]\r\n"
        "UpVote = true\r\n"
        "In-Reply-To: <TARGET.1@vert.synchro.net>\r\n"
        "Sender: Codefenix\r\n"
        "\r\n"
        "[3a00]\r\n"
        "[vote:<A3.debate@vert.synchro.net>]\r\n"
        "DownVote = true\r\n"
        "In-Reply-To: <TARGET.1@vert.synchro.net>\r\n"
        "Sender: Arelor\r\n"
        "\r\n"
        "[7980]\r\n"
        "[vote:<A4.debate@vert.synchro.net>]\r\n"
        "DownVote = true\r\n"
        "In-Reply-To: ANETBBS_f2c2ce96_6a5e96df\r\n"
        "Sender: Bf2k+\r\n"
        "\r\n";

    qwkVoting v;
    v.parse(sample);
    CHECK(v.any());

    int up, down;

    // Ballots accumulate onto the message named by In-Reply-To
    v.get("<TARGET.1@vert.synchro.net>", up, down);
    CHECK(up == 2);
    CHECK(down == 1);

    // A non-RFC In-Reply-To is a real occurrence; treat it as opaque
    v.get("ANETBBS_f2c2ce96_6a5e96df", up, down);
    CHECK(up == 0);
    CHECK(down == 1);

    // The ballot's own message-ID is not a vote target
    v.get("<A1.debate@vert.synchro.net>", up, down);
    CHECK(up == 0);
    CHECK(down == 0);

    // Unknown, empty and null message-IDs are all zero, not a crash
    v.get("<nobody@nowhere>", up, down);
    CHECK(up == 0);
    CHECK(down == 0);
    v.get("", up, down);
    CHECK(up == 0);
    CHECK(down == 0);
    v.get(0, up, down);
    CHECK(up == 0);
    CHECK(down == 0);

    // Message-IDs match case-insensitively
    v.get("<target.1@VERT.synchro.net>", up, down);
    CHECK(up == 2);
    CHECK(down == 1);

    // Poll and closure sections must not contribute to message tallies. A
    // poll ballot carries "Votes = 0x<mask>" selecting poll answers, which is
    // not an up/down vote on the poll message.
    const char *polls =
        "[100]\r\n"
        "[poll:<POLL.1@vert.synchro.net>]\r\n"
        "MaxVotes = 1\r\n"
        "Results = 1\r\n"
        "PollAnswer0 = Yes\r\n"
        "PollAnswer1 = No\r\n"
        "PollAnswer2 = Used to be\r\n"
        "Subject: Are you a licenced amateur radio operator?\r\n"
        "Conference: 2001\r\n"
        "\r\n"
        "[200]\r\n"
        "[vote:<B1@conchaos.synchro.net>]\r\n"
        "Votes = 0x2\r\n"
        "In-Reply-To: <POLL.1@vert.synchro.net>\r\n"
        "\r\n"
        "[300]\r\n"
        "[close:<POLL.1@vert.synchro.net>]\r\n"
        "In-Reply-To: <POLL.1@vert.synchro.net>\r\n"
        "\r\n";

    qwkVoting p;
    p.parse(polls);
    CHECK(!p.any());
    p.get("<POLL.1@vert.synchro.net>", up, down);
    CHECK(up == 0);
    CHECK(down == 0);

    // The poll is found at the offset of the marker section preceding it --
    // its own (bodyless) record in MESSAGES.DAT.
    CHECK(p.getNoOfPolls() == 1);
    CHECK(p.isPoll(0x100));
    CHECK(!p.isPoll(0x200));        // the ballot's own record
    CHECK(!p.isPoll(0x300));        // the closure's

    // Rendered body: question keys off the answers, and the one ballot's
    // 0x2 mask selects answer 1 only.
    char *body = p.getPollText(0x100);
    CHECK(body != 0);
    // "results closed" comes from the [close:...] section -- the poll's own
    // snapshot said Results = 1 (open).
    CHECK(strstr(body, "results closed") != 0);
    CHECK(strstr(body, "1 vote per ballot") != 0);
    CHECK(strstr(body, "1 ballot in this packet") != 0);
    // The exact rendered rows: answer 1 got the one vote, 0 and 2 did not.
    CHECK(strstr(body, "  Yes                                  0\n") != 0);
    CHECK(strstr(body, "  No         ########################  1\n") != 0);
    CHECK(strstr(body, "  Used to be                           0\n") != 0);
    delete[] body;

    CHECK(p.getPollText(0x999) == 0);

    // A multi-bit mask selects several answers at once (real packets contain
    // masks like 0x1c). Ballots may also precede the poll they belong to.
    const char *multi =
        "[10]\r\n"
        "[vote:<C1@vert>]\r\n"
        "Votes = 0x5\r\n"
        "In-Reply-To: <POLL.2@vert>\r\n"
        "\r\n"
        "[80]\r\n"
        "[poll:<POLL.2@vert>]\r\n"
        "MaxVotes = 3\r\n"
        "Results = 1\r\n"
        "Comment0 = pick up to three\r\n"
        "PollAnswer0 = A\r\n"
        "PollAnswer1 = B\r\n"
        "PollAnswer2 = C\r\n"
        "\r\n"
        "[c0]\r\n"
        "[vote:<C2@vert>]\r\n"
        "Votes = 0x4\r\n"
        "In-Reply-To: <POLL.2@vert>\r\n"
        "\r\n";

    qwkVoting m;
    m.parse(multi);
    CHECK(m.getNoOfPolls() == 1);
    CHECK(m.isPoll(0x80));

    char *mbody = m.getPollText(0x80);
    CHECK(mbody != 0);
    CHECK(strstr(mbody, "3 votes per ballot") != 0);
    CHECK(strstr(mbody, "results open") != 0);
    CHECK(strstr(mbody, "pick up to three") != 0);
    CHECK(strstr(mbody, "2 ballots in this packet") != 0);
    // 0x5 = answers 0 and 2; 0x4 = answer 2. So A:1, B:0, C:2, with the bar
    // scaled to C's winning count.
    CHECK(strstr(mbody, "  A ############              1\n") != 0);
    CHECK(strstr(mbody, "  B                           0\n") != 0);
    CHECK(strstr(mbody, "  C ########################  2\n") != 0);
    delete[] mbody;

    // A poll flagged Utf8=true has its subject and answers converted to
    // Latin-1, wherever in the section the flag appears.
    const char *upoll =
        "[20]\r\n"
        "[poll:<POLL.U@vert>]\r\n"
        "PollAnswer0 = caf\xC3\xA9\r\n"
        "Subject: d\xC3\xA9j\xC3\xA0 vu?\r\n"
        "Utf8 = true\r\n";

    qwkVoting u;
    u.parse(upoll);
    CHECK_STR(u.getPollSubject(0x20), "d\xE9j\xE0 vu?");
    char *ubody = u.getPollText(0x20);
    CHECK(strstr(ubody, "caf\xE9") != 0);
    delete[] ubody;

    // A poll nobody voted on still renders, without dividing by zero on the
    // bar scale.
    const char *lonely =
        "[40]\r\n"
        "[poll:<POLL.3@vert>]\r\n"
        "PollAnswer0 = Only option\r\n";

    qwkVoting one;
    one.parse(lonely);
    char *lbody = one.getPollText(0x40);
    CHECK(lbody != 0);
    CHECK(strstr(lbody, "Only option") != 0);
    CHECK(strstr(lbody, "No ballots for this poll") != 0);
    CHECK(strstr(lbody, "#") == 0);         // nothing to draw
    delete[] lbody;

    // A ballot with no In-Reply-To has no target and is discarded, including
    // when it is the last section in the file (the end-of-input flush).
    const char *orphan =
        "[100]\r\n"
        "[vote:<C1@vert>]\r\n"
        "UpVote = true\r\n";

    qwkVoting o;
    o.parse(orphan);
    CHECK(!o.any());

    // LF-only line endings, "UpVote: true" spelt with the other separator,
    // and a final section with no trailing newline
    const char *lf =
        "[100]\n"
        "[vote:<D1@vert>]\n"
        "In-Reply-To: <TARGET.2@vert>\n"
        "UpVote: true";

    qwkVoting l;
    l.parse(lf);
    l.get("<TARGET.2@vert>", up, down);
    CHECK(up == 1);
    CHECK(down == 0);

    // "UpVote = false" is not a vote
    const char *no =
        "[100]\n"
        "[vote:<E1@vert>]\n"
        "In-Reply-To: <TARGET.3@vert>\n"
        "UpVote = false\n";

    qwkVoting n;
    n.parse(no);
    CHECK(!n.any());

    // Parsing empty / null input is harmless
    qwkVoting empty;
    empty.parse("");
    CHECK(!empty.any());
    empty.parse(0);
    empty.get("<anything>", up, down);
    CHECK(up == 0);

    return mm_test_report();
}
