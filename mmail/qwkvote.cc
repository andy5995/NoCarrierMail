/*
 * NoCarrierMail offline mail reader
 * QWK VOTING.DAT parser (Synchronet polls and message votes)

 Distributed under the GNU General Public License, version 3 or later. */

#include "qwkvote.h"
#include "qwkhdr.h"
#include "misc.h"
#include "u8text.h"

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

    sectKind = SK_OTHER;
    marker = 0;
    secUp = secDown = 0;
    secMask = 0;
    secTarget = 0;
    curPoll = lastPoll = 0;
    ballots = closes = 0;
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

void qwkVoting::endSection()
{
    if (secTarget) {
        if (SK_VOTE == sectKind) {
            if (secMask) {
                pending *b = new pending;
                b->msgid = secTarget;
                b->mask = secMask;
                b->next = ballots;
                ballots = b;
                secTarget = 0;      // the pending entry owns it now
            } else if (secUp || secDown)
                add(secTarget, secUp, secDown);
        } else if (SK_CLOSE == sectKind) {
            pending *b = new pending;
            b->msgid = secTarget;
            b->mask = 0;
            b->next = closes;
            closes = b;
            secTarget = 0;
        }
    }

    delete[] secTarget;
    secTarget = 0;
    secUp = secDown = 0;
    secMask = 0;
}

void qwkVoting::parse(const char *text)
{
    if (!text)
        return;

    char *work = strdupplus(text);
    char *line = work;

    while (line) {
        char *nl = strchr(line, '\n');
        if (nl)
            *nl = '\0';

        parseLine(line);

        line = nl ? (nl + 1) : 0;
    }

    finish();
    delete[] work;
}

void qwkVoting::parseLine(char *line)
{
    char *t = iniTrim(line);

    if (*t == '[') {
        char *close = strchr(t, ']');
        if (close) {
            *close = '\0';

            endSection();
            curPoll = 0;

            // Section names are "vote:<id>", "poll:<id>" or "close:<id>".
            // Anything without a colon is a "[<hex offset>]" marker,
            // which names the record the next section belongs to.
            char *sep = strchr(t + 1, ':');
            if (!sep) {
                marker = strtoul(t + 1, 0, 16);
                sectKind = SK_OTHER;
            } else {
                *sep = '\0';
                if (iniKeyEq(t + 1, "vote"))
                    sectKind = SK_VOTE;
                else if (iniKeyEq(t + 1, "close"))
                    sectKind = SK_CLOSE;
                else if (iniKeyEq(t + 1, "poll")) {
                    sectKind = SK_POLL;
                    curPoll = new poll;
                    curPoll->offset = marker;
                    curPoll->msgid = strdupplus(sep + 1);
                    curPoll->subject = 0;
                    curPoll->maxVotes = 1;
                    curPoll->results = 0;
                    curPoll->ballots = 0;
                    curPoll->noOfAnswers = 0;
                    curPoll->utf8 = false;
                    curPoll->comments = 0;
                    curPoll->next = 0;
                    if (lastPoll)
                        lastPoll->next = curPoll;
                    else
                        polls = curPoll;
                    lastPoll = curPoll;
                    noPolls++;
                } else
                    sectKind = SK_OTHER;
            }
        }
    } else if (*t && (SK_OTHER != sectKind)) {
        char *key, *value;
        if (iniSplit(t, key, value)) {
            if (SK_VOTE == sectKind || SK_CLOSE == sectKind) {
                if (iniKeyEq(key, "In-Reply-To")) {
                    delete[] secTarget;
                    secTarget = strdupplus(value);
                } else if (iniKeyEq(key, "UpVote")) {
                    if (iniKeyEq(value, "true"))
                        secUp++;
                } else if (iniKeyEq(key, "DownVote")) {
                    if (iniKeyEq(value, "true"))
                        secDown++;
                } else if (iniKeyEq(key, "Votes"))
                    // A bit-field over the poll's answers, written in hex
                    // with an "0x" prefix that strtoul understands.
                    secMask = (unsigned) strtoul(value, 0, 16);
            } else if (iniKeyEq(key, "Utf8")) {
                // Checked before splitIndex(), which would eat the '8'.
                if (iniKeyEq(value, "true"))
                    curPoll->utf8 = true;
            } else {
                int n = splitIndex(key);

                if (iniKeyEq(key, "Subject")) {
                    delete[] curPoll->subject;
                    curPoll->subject = strdupplus(value);
                } else if (iniKeyEq(key, "MaxVotes"))
                    curPoll->maxVotes = atoi(value);
                else if (iniKeyEq(key, "Results"))
                    curPoll->results = atoi(value);
                else if (iniKeyEq(key, "PollAnswer")) {
                    // The ballot bit-field is indexed by this number, so
                    // place the answer by it rather than by arrival.
                    if (n < MAXANSWERS) {
                        while (curPoll->noOfAnswers <= n) {
                            curPoll->answers[curPoll->noOfAnswers] = 0;
                            curPoll->answerVotes[curPoll->noOfAnswers++] = 0;
                        }
                        delete[] curPoll->answers[n];
                        curPoll->answers[n] = strdupplus(value);
                    }
                } else if (iniKeyEq(key, "Comment")) {
                    comment *c = new comment;
                    c->text = strdupplus(value);
                    c->next = 0;
                    comment **at = &curPoll->comments;
                    while (*at)
                        at = &(*at)->next;
                    *at = c;
                }
            }
        }
    }
}

void qwkVoting::finish()
{
    endSection();
    curPoll = 0;
    sectKind = SK_OTHER;

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

char *qwkVoting::getPollSubject(unsigned long offset, bool latin) const
{
    poll *p = findPoll(offset);
    return p ? u8dupFold(p->subject, p->utf8, latin) : 0;
}

char *qwkVoting::getPollText(unsigned long offset, bool latin) const
{
    poll *p = findPoll(offset);
    if (!p)
        return 0;

    // Fold the answers first: the column they make is sized by their length.
    char *answers[MAXANSWERS];
    for (int x = 0; x < p->noOfAnswers; x++)
        answers[x] = u8dupFold(p->answers[x], p->utf8, latin);

    // Visibility of the results on the BBS, which says nothing about what
    // this packet happens to carry.
    static const char *seen[] = {"visible to voters", "open", "closed",
                                 "secret"};

    size_t size = 256;
    for (comment *c = p->comments; c; c = c->next)
        size += strlen(c->text) + 2;
    for (int x = 0; x < p->noOfAnswers; x++)
        size += (answers[x] ? strlen(answers[x]) : 0) +
                ANSWERWIDTH + BARWIDTH + 16;

    char *out = new char[size];
    char *o = out;

    o += sprintf(o, "Poll -- %d vote%s per ballot, results %s\n\n",
                 p->maxVotes, (1 == p->maxVotes) ? "" : "s",
                 seen[p->results & 3]);

    for (comment *c = p->comments; c; c = c->next) {
        char *t = u8dupFold(c->text, p->utf8, latin);

        o += sprintf(o, "%s\n", t);
        delete[] t;
    }
    if (p->comments)
        *o++ = '\n';

    int most = 0, width = 0;
    for (int x = 0; x < p->noOfAnswers; x++) {
        if (p->answerVotes[x] > most)
            most = p->answerVotes[x];
        int len = answers[x] ? (int) strlen(answers[x]) : 0;
        if (len > width)
            width = len;
    }
    if (width > ANSWERWIDTH)
        width = ANSWERWIDTH;

    for (int x = 0; x < p->noOfAnswers; x++) {
        int bar = most ? ((p->answerVotes[x] * BARWIDTH + most - 1) / most) : 0;

        o += sprintf(o, "  %-*.*s ", width, width,
                     answers[x] ? answers[x] : "");
        for (int y = 0; y < BARWIDTH; y++)
            *o++ = (y < bar) ? '#' : ' ';
        o += sprintf(o, "  %d\n", p->answerVotes[x]);
    }

    if (p->ballots)
        sprintf(o, "\n%d ballot%s in this packet.\n", p->ballots,
                (1 == p->ballots) ? "" : "s");
    else
        sprintf(o, "\nNo ballots for this poll in this packet.\n");

    for (int x = 0; x < p->noOfAnswers; x++)
        delete[] answers[x];

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
