/*
 * NoCarrierMail offline mail reader
 * QWK VOTING.DAT parser (Synchronet polls and message votes)

 Distributed under the GNU General Public License, version 3 or later. */

#ifndef QWKVOTE_H
#define QWKVOTE_H

/* Parses a Synchronet VOTING.DAT file. The file is .ini-style; each entry is
   an empty "[<hex offset>]" section followed by a "[vote:<id>]",
   "[poll:<id>]" or "[close:<id>]" section. Two kinds of thing are collected:

   - Message votes. A "[vote:...]" section carrying "UpVote = true" or
     "DownVote = true", where "In-Reply-To" names the message being voted on.
     Those are tallied per target message-ID; the offset in the marker section
     belongs to the ballot's own (bodyless) record and is of no use. For a
     readable message the matching message-ID comes from HEADERS.DAT.

   - Polls. A "[poll:...]" section holds the answers and the settings; its
     marker offset locates the poll's own record in MESSAGES.DAT, which is a
     header with no body. Poll ballots are "[vote:...]" sections carrying
     "Votes = 0x<mask>", a bit-field over the poll's answers, and they name
     their poll in "In-Reply-To" -- as does a "[close:...]" section, which
     marks the poll's results closed.

   See docs/synchronet-qwk-extensions.md. Packets carry thousands of ballots,
   so message-vote lookups are hashed rather than linear; polls number in the
   handful and are kept in a plain list. This class is self-contained (no
   curses, no globals) so it can be unit-tested in isolation. */

class qwkVoting {
    enum { HASHSIZE = 509, MAXANSWERS = 16 };
    enum sectkind { SK_OTHER, SK_VOTE, SK_POLL, SK_CLOSE };

    struct tally {
        char *msgid;            // message being voted on
        int up, down;
        tally *next;
    };

    struct comment {
        char *text;
        comment *next;
    };

    struct poll {
        unsigned long offset;   // the poll's header in MESSAGES.DAT
        char *msgid, *subject;
        int maxVotes, results, ballots, noOfAnswers;
        bool utf8;
        char *answers[MAXANSWERS];
        int answerVotes[MAXANSWERS];
        comment *comments;
        poll *next;
    };

    // A poll ballot (or closure) is not applied where it is read: its poll
    // may not have been parsed yet, so both are held until the whole file
    // has been read. A closure leaves mask unused.
    struct pending {
        char *msgid;
        unsigned mask;
        pending *next;
    };

    tally *buckets[HASHSIZE];
    poll *polls;
    int count, noPolls;

    static unsigned hash(const char *);
    tally *find(const char *) const;
    void add(const char *, int, int);
    poll *findPoll(unsigned long) const;
    poll *findPollByID(const char *) const;
    void endSection(int kind, char *&target, int &up, int &down,
                    unsigned &mask, pending *&ballots, pending *&closes);

 public:
    qwkVoting();
    ~qwkVoting();

    // Parse null-terminated VOTING.DAT text into this object.
    void parse(const char *text);

    // Did the packet ship a VOTING.DAT with any usable ballots?
    bool any() const;

    // Vote counts for a message-ID; both are set to 0 when it has none.
    void get(const char *msgid, int &up, int &down) const;

    // Polls are looked up by the offset of their (bodyless) header block in
    // MESSAGES.DAT -- the marker section that preceded them -- which is how
    // the QWK driver puts a poll into the letter index.
    int getNoOfPolls() const;
    bool isPoll(unsigned long offset) const;

    // The full question. A poll is not written to HEADERS.DAT, so its record
    // in MESSAGES.DAT has only the 25-char subject field. Returns 0 if no
    // poll sits at that offset, or if it named no subject.
    const char *getPollSubject(unsigned long offset) const;

    // The poll's question, answers and vote counts, rendered for display as
    // the body of the otherwise empty poll message. Returns a newly-allocated
    // string (caller deletes[] it), or 0 if no poll sits at that offset.
    char *getPollText(unsigned long offset) const;
};

#endif
