/*
 * NoCarrierMail offline mail reader
 * QWK VOTING.DAT parser (Synchronet message votes)

 Distributed under the GNU General Public License, version 3 or later. */

#include "qwkvote.h"
#include "qwkhdr.h"
#include "misc.h"

extern "C" {
#include <ctype.h>
#include <string.h>
}

qwkVoting::qwkVoting()
{
    for (int x = 0; x < HASHSIZE; x++)
        buckets[x] = 0;
    count = 0;
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

void qwkVoting::parse(const char *text)
{
    if (!text)
        return;

    char *work = strdupplus(text);
    char *line = work;

    // Fields accumulate until the section ends, because a ballot's target
    // ("In-Reply-To") is written after the Up/DownVote line.
    bool isVote = false;
    int up = 0, down = 0;
    char *target = 0;

    while (line) {
        char *nl = strchr(line, '\n');
        if (nl)
            *nl = '\0';

        char *t = iniTrim(line);

        if (*t == '[') {
            char *close = strchr(t, ']');
            if (close) {
                *close = '\0';

                if (isVote && target && (up || down))
                    add(target, up, down);

                delete[] target;
                target = 0;
                up = down = 0;

                // Section names are "vote:<id>", "poll:<id>" or "close:<id>";
                // only ballots interest us. (Also "[<hex offset>]" markers,
                // which have no colon and so match nothing.)
                isVote = false;
                char *kind = strchr(t + 1, ':');
                if (kind) {
                    *kind = '\0';
                    isVote = iniKeyEq(t + 1, "vote");
                }
            }
        } else if (*t && isVote) {
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

                if (iniKeyEq(key, "In-Reply-To")) {
                    delete[] target;
                    target = strdupplus(value);
                } else if (iniKeyEq(key, "UpVote")) {
                    if (iniKeyEq(value, "true"))
                        up++;
                } else if (iniKeyEq(key, "DownVote")) {
                    if (iniKeyEq(value, "true"))
                        down++;
                }

                // "Votes = 0x<mask>" marks a poll ballot, which selects poll
                // answers instead of voting on a message. Nothing to tally.
            }
        }

        line = nl ? (nl + 1) : 0;
    }

    if (isVote && target && (up || down))
        add(target, up, down);

    delete[] target;
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
