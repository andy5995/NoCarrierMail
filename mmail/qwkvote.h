/*
 * NoCarrierMail offline mail reader
 * QWK VOTING.DAT parser (Synchronet message votes)

 Distributed under the GNU General Public License, version 3 or later. */

#ifndef QWKVOTE_H
#define QWKVOTE_H

/* Parses a Synchronet VOTING.DAT file and tallies up/down votes per target
   message. The file is .ini-style; each entry is an empty "[<hex offset>]"
   section followed by a "[vote:<id>]", "[poll:<id>]" or "[close:<id>]"
   section. Only message ballots are collected here: a "[vote:...]" section
   carrying "UpVote = true" or "DownVote = true", where "In-Reply-To" names
   the message being voted on. Poll ballots ("Votes = 0x<mask>") select poll
   answers rather than voting on a message, so they are skipped.

   The offset in the marker section refers to the ballot's own (bodyless)
   record, not to the message being voted on, so it is of no use to us --
   targets are identified by message-ID, which for a readable message comes
   from HEADERS.DAT. See docs/synchronet-qwk-extensions.md.

   Packets carry thousands of ballots, so lookups are hashed rather than
   linear. This class is self-contained (no curses, no globals) so it can be
   unit-tested in isolation. */

class qwkVoting {
    enum { HASHSIZE = 509 };

    struct tally {
        char *msgid;            // message being voted on
        int up, down;
        tally *next;
    };
    tally *buckets[HASHSIZE];
    int count;

    static unsigned hash(const char *);
    tally *find(const char *) const;
    void add(const char *, int, int);

 public:
    qwkVoting();
    ~qwkVoting();

    // Parse null-terminated VOTING.DAT text into this object.
    void parse(const char *text);

    // Did the packet ship a VOTING.DAT with any usable ballots?
    bool any() const;

    // Vote counts for a message-ID; both are set to 0 when it has none.
    void get(const char *msgid, int &up, int &down) const;
};

#endif
