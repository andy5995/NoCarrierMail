/*
 * NoCarrierMail offline mail reader
 * QWK VOTING.DAT parser (Synchronet polls and message votes)

 Distributed under the GNU General Public License, version 3 or later. */

#include "qwkvote.h"
#include "qwkhdr.h"
#include "misc.h"

extern "C" {
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
}

// Longest answer label kept on one line, and the longest bar drawn for the
// most-voted answer. Both together stay inside an 80-column letter window.
#define ANSWERWIDTH 34
#define BARWIDTH 24

// Poll fields are numbered -- "PollAnswer0", "Comment3". Returns the index
// and cuts the digits off, leaving the key ready for iniKeyEq(). (strncasecmp
// isn't used anywhere in mmail/; the DOSish ports have stricmp instead.)
static int splitIndex(char *key)
{
    char *end = key + strlen(key);

    while ((end > key) && isdigit((unsigned char) end[-1]))
        end--;

    int n = atoi(end);
    *end = '\0';

    return n;
}

qwkVoting::qwkVoting()
{
    for (int x = 0; x < HASHSIZE; x++)
        buckets[x] = 0;
    polls = 0;
    count = noPolls = 0;
}

qwkVoting::~qwkVoting()
{
    for (int x = 0; x < HASHSIZE; x++) {
        tally *t = buckets[x];
        while (t) {
            tally *next = t->next;
            delete[] t->msgid;
            delete t;
            t = next;
        }
    }

    poll *p = polls;
    while (p) {
        poll *next = p->next;
        comment *c = p->comments;
        while (c) {
            comment *cnext = c->next;
            delete[] c->text;
            delete c;
            c = cnext;
        }
        for (int x = 0; x < p->noOfAnswers; x++)
            delete[] p->answers[x];
        delete[] p->subject;
        delete[] p->msgid;
        delete p;
        p = next;
    }
}

// Case-insensitive, to match iniKeyEq()'s treatment of the message-IDs it
// compares. Both sides come from the same BBS, but nothing guarantees case
// is preserved across a gateway.

unsigned qwkVoting::hash(const char *s)
{
    unsigned h = 5381;

    while (*s)
        h = (h * 33) ^ (unsigned) tolower((unsigned char) *s++);

    return h % HASHSIZE;
}

qwkVoting::tally *qwkVoting::find(const char *msgid) const
{
    for (tally *t = buckets[hash(msgid)]; t; t = t->next)
        if (iniKeyEq(t->msgid, msgid))
            return t;
    return 0;
}

void qwkVoting::add(const char *msgid, int up, int down)
{
    tally *t = find(msgid);

    if (!t) {
        unsigned b = hash(msgid);
        t = new tally;
        t->msgid = strdupplus(msgid);
        t->up = t->down = 0;
        t->next = buckets[b];
        buckets[b] = t;
        count++;
    }

    t->up += up;
    t->down += down;
}

qwkVoting::poll *qwkVoting::findPoll(unsigned long offset) const
{
    for (poll *p = polls; p; p = p->next)
        if (p->offset == offset)
            return p;
    return 0;
}

qwkVoting::poll *qwkVoting::findPollByID(const char *msgid) const
{
    for (poll *p = polls; p; p = p->next)
        if (iniKeyEq(p->msgid, msgid))
            return p;
    return 0;
}

// A section's fields accumulate until the next section header (or the end of
// the file), because a ballot's target ("In-Reply-To") is written after its
// vote lines. This settles the finished section and resets the accumulators.

void qwkVoting::endSection(int kind, char *&target, int &up, int &down,
                           unsigned &mask, pending *&ballots,
                           pending *&closes)
{
    if (target) {
        if (SK_VOTE == kind) {
            if (mask) {
                pending *b = new pending;
                b->msgid = target;
                b->mask = mask;
                b->next = ballots;
                ballots = b;
                target = 0;         // the pending entry owns it now
            } else if (up || down)
                add(target, up, down);
        } else if (SK_CLOSE == kind) {
            pending *b = new pending;
            b->msgid = target;
            b->mask = 0;
            b->next = closes;
            closes = b;
            target = 0;
        }
    }

    delete[] target;
    target = 0;
    up = down = 0;
    mask = 0;
}

void qwkVoting::parse(const char *text)
{
    if (!text)
        return;

    char *work = strdupplus(text);
    char *line = work;

    int kind = SK_OTHER;

    unsigned long marker = 0;
    int up = 0, down = 0;
    unsigned mask = 0;
    char *target = 0;
    poll *cur = 0, *tail = 0;
    pending *ballots = 0, *closes = 0;

    while (line) {
        char *nl = strchr(line, '\n');
        if (nl)
            *nl = '\0';

        char *t = iniTrim(line);

        if (*t == '[') {
            char *close = strchr(t, ']');
            if (close) {
                *close = '\0';

                endSection(kind, target, up, down, mask, ballots, closes);
                cur = 0;

                // Section names are "vote:<id>", "poll:<id>" or "close:<id>".
                // Anything without a colon is a "[<hex offset>]" marker,
                // which names the record the next section belongs to.
                char *sep = strchr(t + 1, ':');
                if (!sep) {
                    marker = strtoul(t + 1, 0, 16);
                    kind = SK_OTHER;
                } else {
                    *sep = '\0';
                    if (iniKeyEq(t + 1, "vote"))
                        kind = SK_VOTE;
                    else if (iniKeyEq(t + 1, "close"))
                        kind = SK_CLOSE;
                    else if (iniKeyEq(t + 1, "poll")) {
                        kind = SK_POLL;
                        cur = new poll;
                        cur->offset = marker;
                        cur->msgid = strdupplus(sep + 1);
                        cur->subject = 0;
                        cur->maxVotes = 1;
                        cur->results = 0;
                        cur->ballots = 0;
                        cur->noOfAnswers = 0;
                        cur->utf8 = false;
                        cur->comments = 0;
                        cur->next = 0;
                        if (tail)
                            tail->next = cur;
                        else
                            polls = cur;
                        tail = cur;
                        noPolls++;
                    } else
                        kind = SK_OTHER;
                }
            }
        } else if (*t && (SK_OTHER != kind)) {
            char *key, *value;
            if (iniSplit(t, key, value)) {
                if (SK_VOTE == kind || SK_CLOSE == kind) {
                    if (iniKeyEq(key, "In-Reply-To")) {
                        delete[] target;
                        target = strdupplus(value);
                    } else if (iniKeyEq(key, "UpVote")) {
                        if (iniKeyEq(value, "true"))
                            up++;
                    } else if (iniKeyEq(key, "DownVote")) {
                        if (iniKeyEq(value, "true"))
                            down++;
                    } else if (iniKeyEq(key, "Votes"))
                        // A bit-field over the poll's answers, written in hex
                        // with an "0x" prefix that strtoul understands.
                        mask = (unsigned) strtoul(value, 0, 16);
                } else if (iniKeyEq(key, "Utf8")) {
                    // Checked before splitIndex(), which would eat the '8'.
                    if (iniKeyEq(value, "true"))
                        cur->utf8 = true;
                } else {
                    int n = splitIndex(key);

                    if (iniKeyEq(key, "Subject")) {
                        delete[] cur->subject;
                        cur->subject = strdupplus(value);
                    } else if (iniKeyEq(key, "MaxVotes"))
                        cur->maxVotes = atoi(value);
                    else if (iniKeyEq(key, "Results"))
                        cur->results = atoi(value);
                    else if (iniKeyEq(key, "PollAnswer")) {
                        // The ballot bit-field is indexed by this number, so
                        // place the answer by it rather than by arrival.
                        if (n < MAXANSWERS) {
                            while (cur->noOfAnswers <= n) {
                                cur->answers[cur->noOfAnswers] = 0;
                                cur->answerVotes[cur->noOfAnswers++] = 0;
                            }
                            delete[] cur->answers[n];
                            cur->answers[n] = strdupplus(value);
                        }
                    } else if (iniKeyEq(key, "Comment")) {
                        comment *c = new comment;
                        c->text = strdupplus(value);
                        c->next = 0;
                        comment **at = &cur->comments;
                        while (*at)
                            at = &(*at)->next;
                        *at = c;
                    }
                }
            }
        }

        line = nl ? (nl + 1) : 0;
    }

    endSection(kind, target, up, down, mask, ballots, closes);

    // A poll flagged Utf8=true carries UTF-8 text; convert it to Latin-1
    // here, once, so the accessors can hand fields out as-is. Done after the
    // whole file is read rather than per-line, so it cannot matter where in
    // the section the flag was written.
    for (poll *p = polls; p; p = p->next)
        if (p->utf8) {
            char *conv;
            if (p->subject) {
                conv = utf8ToLatin1(p->subject);
                delete[] p->subject;
                p->subject = conv;
            }
            for (int x = 0; x < p->noOfAnswers; x++)
                if (p->answers[x]) {
                    conv = utf8ToLatin1(p->answers[x]);
                    delete[] p->answers[x];
                    p->answers[x] = conv;
                }
            for (comment *c = p->comments; c; c = c->next) {
                conv = utf8ToLatin1(c->text);
                delete[] c->text;
                c->text = conv;
            }
        }

    // Poll ballots are resolved last: a ballot may be written before the poll
    // it selects answers in.
    while (ballots) {
        pending *next = ballots->next;
        poll *p = findPollByID(ballots->msgid);
        if (p) {
            p->ballots++;
            for (int x = 0; x < p->noOfAnswers; x++)
                if (ballots->mask & (1U << x))
                    p->answerVotes[x]++;
        }
        delete[] ballots->msgid;
        delete ballots;
        ballots = next;
    }

    // Closures likewise: mark the poll's results closed.
    while (closes) {
        pending *next = closes->next;
        poll *p = findPollByID(closes->msgid);
        if (p)
            p->results = 2;
        delete[] closes->msgid;
        delete closes;
        closes = next;
    }

    delete[] work;
}

bool qwkVoting::any() const
{
    return count != 0;
}

void qwkVoting::get(const char *msgid, int &up, int &down) const
{
    up = down = 0;

    if (msgid && *msgid) {
        tally *t = find(msgid);
        if (t) {
            up = t->up;
            down = t->down;
        }
    }
}

int qwkVoting::getNoOfPolls() const
{
    return noPolls;
}

bool qwkVoting::isPoll(unsigned long offset) const
{
    return findPoll(offset) != 0;
}

const char *qwkVoting::getPollSubject(unsigned long offset) const
{
    poll *p = findPoll(offset);
    return p ? p->subject : 0;
}

char *qwkVoting::getPollText(unsigned long offset) const
{
    poll *p = findPoll(offset);
    if (!p)
        return 0;

    // Visibility of the results on the BBS, which says nothing about what
    // this packet happens to carry.
    static const char *seen[] = {"visible to voters", "open", "closed",
                                 "secret"};

    size_t size = 256;
    for (comment *c = p->comments; c; c = c->next)
        size += strlen(c->text) + 2;
    for (int x = 0; x < p->noOfAnswers; x++)
        size += (p->answers[x] ? strlen(p->answers[x]) : 0) +
                ANSWERWIDTH + BARWIDTH + 16;

    char *out = new char[size];
    char *o = out;

    o += sprintf(o, "Poll -- %d vote%s per ballot, results %s\n\n",
                 p->maxVotes, (1 == p->maxVotes) ? "" : "s",
                 seen[p->results & 3]);

    for (comment *c = p->comments; c; c = c->next)
        o += sprintf(o, "%s\n", c->text);
    if (p->comments)
        *o++ = '\n';

    int most = 0, width = 0;
    for (int x = 0; x < p->noOfAnswers; x++) {
        if (p->answerVotes[x] > most)
            most = p->answerVotes[x];
        int len = p->answers[x] ? (int) strlen(p->answers[x]) : 0;
        if (len > width)
            width = len;
    }
    if (width > ANSWERWIDTH)
        width = ANSWERWIDTH;

    for (int x = 0; x < p->noOfAnswers; x++) {
        int bar = most ? ((p->answerVotes[x] * BARWIDTH + most - 1) / most) : 0;

        o += sprintf(o, "  %-*.*s ", width, width,
                     p->answers[x] ? p->answers[x] : "");
        for (int y = 0; y < BARWIDTH; y++)
            *o++ = (y < bar) ? '#' : ' ';
        o += sprintf(o, "  %d\n", p->answerVotes[x]);
    }

    if (p->ballots)
        sprintf(o, "\n%d ballot%s in this packet.\n", p->ballots,
                (1 == p->ballots) ? "" : "s");
    else
        sprintf(o, "\nNo ballots for this poll in this packet.\n");

    return out;
}

void qwkVoteDate(char *dest, time_t t)
{
    struct tm lt = *localtime(&t);
    struct tm gt = *gmtime(&t);

    long off = ((long) lt.tm_hour - gt.tm_hour) * 60 +
               lt.tm_min - gt.tm_min;

    // Around midnight the two can sit on different days; at New Year the
    // year wraps too, flipping tm_yday's sign.
    int daydiff = lt.tm_yday - gt.tm_yday;
    if (daydiff > 1)
        daydiff = -1;
    else if (daydiff < -1)
        daydiff = 1;
    off += daydiff * 24L * 60;

    char sign = (off < 0) ? '-' : '+';
    if (off < 0)
        off = -off;

    sprintf(dest, "%04d%02d%02d%02d%02d%02d%c%02ld%02ld",
            lt.tm_year + 1900, lt.tm_mon + 1, lt.tm_mday,
            lt.tm_hour, lt.tm_min, lt.tm_sec, sign, off / 60, off % 60);
}
