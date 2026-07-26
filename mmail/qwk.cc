/*
 * NoCarrierMail offline mail reader
 * QWK

 Copyright 1997 John Zero <john@graphisoft.hu>
 Copyright 1997-2021 William McBrine <wmcbrine@gmail.com>
 Distributed under the GNU General Public License, version 3 or later. */

#include "qwk.h"
#include "compress.h"

unsigned char *onecomp(unsigned char *p, char *dest, const char *comp)
{
    size_t len = strlen(comp);

    if (!strncasecmp((char *) p, comp, len)) {
        p += len;
        while (*p == ' ')
            p++;
        int x;
        for (x = 0; *p && (*p != '\n') && (x < 71); x++)
            dest[x] = *p++;
        dest[x] = '\0';

        while (*p == '\n')
            p++;
        return p;
    }
    return 0;
}

// Copy a HEADERS.DAT field value. When the section is flagged Utf8=true the
// value is UTF-8, so convert it to Latin-1; otherwise copy it as-is.
static char *hdrField(const char *s, bool utf8)
{
    return utf8 ? utf8ToLatin1(s) : strdupplus(s);
}

// -----------------------------------------------------------------
// The QWK methods
// -----------------------------------------------------------------

qwkpack::qwkpack() : pktbase()
{
    qwke = !(!mm.workList->exists("toreader.ext"));

    readControlDat();
    readDoorId();
    if (qwke)
        readToReader();

    infile = mm.workList->ftryopen("messages.dat");
    if (!infile)
        fatalError("Could not open MESSAGES.DAT");

    loadVoting();

    readIndices();

    loadHeaders();

    listBulletins((const char (*)[13]) newsfile, 1);
}

qwkpack::~qwkpack()
{
    cleanup();
}

unsigned long qwkpack::MSBINtolong(unsigned const char *ms)
{
    return ((((unsigned long) ms[0] + ((unsigned long) ms[1] << 8) +
        ((unsigned long) ms[2] << 16)) | 0x800000L) >> (24 + 0x80 - ms[3]));
}

area_header *qwkpack::getNextArea()
{
    int cMsgNum = areas[ID].nummsgs;
    bool x = (areas[ID].num == -1);

    // QWKE allows 71-char To/Subject; so does a Synchronet packet that ships
    // HEADERS.DAT (it can carry full-length headers) even when QWKE is off.
    int hdrlen = (qwke || headers.any()) ? 71 : 25;

    area_header *tmp = new area_header(ID + 1, areas[ID].numA,
        areas[ID].name, (x ? "Letters addressed to you" : areas[ID].name),
        (greekqwk ? (x ? "GreekQWK personal" : "GreekQWK") : (qwke ? (x
        ? "QWKE personal" : "QWKE") : (x ? "QWK personal" : "QWK"))),
        areas[ID].attr | hasOffConfig | (cMsgNum ? ACTIVE : 0), cMsgNum,
        0, hdrlen, hdrlen);

    ID++;
    return tmp;
}

bool qwkpack::isQWKE()
{
    return qwke;
}

bool qwkpack::isGreekQWK()
{
    return greekqwk;
}

bool qwkpack::hasHeaders()
{
    return headers.any();
}

bool qwkpack::externalIndex()
{
    const char *p;
    bool hasNdx;

    hasPers = !(!mm.workList->exists("personal.ndx"));

    p = mm.workList->exists(".ndx");
    hasNdx = !(!p);

    if (hasNdx) {
        struct {
            unsigned char MSB[4];
            unsigned char confnum;
        } ndx_rec;

        FILE *idxFile;
        char fname[13];

        if (!hasPers) {
            areas++;
            maxConf--;
        }

        // Store the size of the .DAT for future comparison as
        // a check against invalid .NDX entries.

        fseek(infile, 0, SEEK_END);
        unsigned long endpoint = (unsigned long) ftell(infile);
        if (endpoint > 128)
            endpoint -= 128;

        while (p) {
            int x, cMsgNum = 0;

            memcpy(fname, p, strlen(p) - 4);
            fname[strlen(p) - 4] = '\0';
            x = atoi(fname);

            if (!x) {
                if (!strcasecmp(fname, "personal"))
                    x = -1;
                else
                    if (strcmp(fname, "000"))
                        x = -2;      // fname is not a num
            }
            if (x != -2) {
                x = getXNum(x);
                if (-1 == x)         // fname is a num but not a
                    hasNdx = false;  // valid conference
            }

            if (x >= 0) {

                idxFile = mm.workList->ftryopen(p);
                if (idxFile) {
                    cMsgNum = mm.workList->getSize() / ndxRecLen;
                    body[x] = new bodytype[cMsgNum];
                    for (int y = 0; y < cMsgNum; y++) {
                        // If any .NDX entries appear corrupted, we're
                        // better off aborting this and using the new
                        // (purely .DAT-based) indexing method (in
                        // readIndices()).

                        if (!fread(&ndx_rec, ndxRecLen, 1, idxFile)) {
                            hasNdx = false;
                            break;
                        }
                        unsigned long temp = MSBINtolong(ndx_rec.MSB) << 7;

                        if ((temp < 256) || (temp > endpoint)) {
                            hasNdx = false;    // use other method
                            break;
                        }

                        // Range alone isn't enough: some packets ship .NDX
                        // files whose entries point into the middle of a
                        // message body instead of a header. Verify each
                        // pointer lands on a parseable header; if not, fall
                        // back to the .DAT-based index.

                        qheader qHead;
                        fseek(infile, temp - 128, SEEK_SET);
                        if (!qHead.init(infile) || qHead.netblock) {
                            hasNdx = false;
                            break;
                        }

                        body[x][y].pointer = temp;
                    }
                    fclose(idxFile);
                }

                if (hasNdx) {
                    areas[x].nummsgs = cMsgNum;
                    numMsgs += cMsgNum;
                }
            }
            p = hasNdx ? mm.workList->getNext(".ndx") : 0;
        }

        // Clean up after aborting

        if (!hasNdx) {
            if (!hasPers) {
                areas--;
                maxConf++;
            }
            for (int x = 0; x < maxConf; x++) {
                delete[] body[x];
                body[x] = 0;
                areas[x].nummsgs = 0;
            }
            numMsgs = 0;
        }
    }
    return hasNdx;
}

void qwkpack::readIndices()
{
    int x;

    body = new bodytype *[maxConf];

    for (x = 0; x < maxConf; x++) {
        body[x] = 0;
        areas[x].nummsgs = 0;
    }

    numMsgs = 0;

    // Polls are records with no body, which Synchronet leaves out of the .NDX
    // files (and out of a conference's .NDX entirely, if a poll was all it
    // had). When the packet has any, index from MESSAGES.DAT instead, so they
    // can be indexed alongside everything else.

    if (mm.res.getInt(IgnoreNDX) || votes.getNoOfPolls() || !externalIndex()) {
        ndx_fake base, *tmpndx = &base;

        long counter;
        int personal = 0;

        qheader qHead;

        fseek(infile, 128, SEEK_SET);
        while (qHead.init_short(infile)) {
            counter = ftell(infile);

            // A poll has no body, so init_short() cannot tell it from a bad
            // header and flags it as one. Index it anyway -- its text comes
            // from VOTING.DAT, via getBody().

            bool isPoll = ('V' == qHead.status) && votes.isPoll(counter - 128);

            if (!qHead.netblock || isPoll) {    // skip net-status flags
                x = getXNum(qHead.origArea);

                if (-1 != x) {
                    tmpndx->next = new ndx_fake;
                    tmpndx = tmpndx->next;

                    tmpndx->confnum = x;

                    if (!strcasecmp(qHead.to, LoginName) ||
                        (qwke && !strcasecmp(qHead.to, AliasName))) {

                        tmpndx->pers = true;
                        personal++;
                    } else
                        tmpndx->pers = false;

                    tmpndx->pointer = counter;
                    tmpndx->length = 0;    // temp

                    areas[x].nummsgs++;
                    numMsgs++;
                }

                fseek(infile, qHead.msglen, SEEK_CUR);
            }
        }

        initBody(base.next, personal);
    }
}

// Read a whole packet file into a null-terminated buffer for the .ini-style
// parsers. Returns 0 if it's missing or empty; caller deletes[] the result.
static char *slurpWorkFile(const char *name)
{
    FILE *f = mm.workList->ftryopen(name);
    if (!f)
        return 0;

    char *buf = 0;
    long size = mm.workList->getSize();
    if (size > 0) {
        buf = new char[size + 1];
        size_t got = fread(buf, 1, size, f);
        buf[got] = '\0';
    }
    fclose(f);

    return buf;
}

void qwkpack::loadHeaders()
{
    char *buf = slurpWorkFile("headers.dat");

    if (buf) {
        headers.parse(buf);
        delete[] buf;
    }
}

void qwkpack::loadVoting()
{
    char *buf = slurpWorkFile("voting.dat");

    if (buf) {
        votes.parse(buf);
        delete[] buf;
    }
}

// Render a raw "MM-DD-YY HH:MM" QWK date with the user's DateFormat, for
// display only. The raw form stays in qHead.date so replies round-trip
// byte-for-byte through qheader::output(); see the note in qheader::init().
static void qwkDispDate(char *dest, size_t destlen, const char *raw)
{
    struct tm t;
    memset(&t, 0, sizeof t);
    int mo = 0, dy = 0, yr = 0, hh = 0, mn = 0;
    sscanf(raw, "%d-%d-%d %d:%d", &mo, &dy, &yr, &hh, &mn);
    t.tm_mon = mo - 1;
    t.tm_mday = dy;
    t.tm_year = yr + ((yr < 70) ? 100 : 0);
    t.tm_hour = hh;
    t.tm_min = mn;
    formatDate(dest, destlen, &t, mm.res.get(dateFormat));
}

letter_header *qwkpack::getNextLetter()
{
    static net_address nullNet;
    qheader q;
    unsigned long pos, rawpos;
    int areaID, letterID;

    rawpos = body[currentArea][currentLetter].pointer;
    pos = rawpos - 128;

    fseek(infile, pos, SEEK_SET);
    if (q.init(infile)) {
        if (areas[currentArea].num == -1) {
            areaID = getXNum(q.origArea);
            letterID = getYNum(areaID, rawpos);
        } else {
            areaID = currentArea;
            letterID = currentLetter;
        }
    } else
        areaID = letterID = -1;

    if ((-1 == areaID) || (-1 == letterID))
        return new letter_header("MESSAGES.DAT", "READING", "ERROR",
                                 "ERROR", 0, 0, 0, 0, 0, false, 0, this,
                                 nullNet, false);

    body[areaID][letterID].msgLength = q.msglen;

    currentLetter++;

    // Synchronet HEADERS.DAT: when this message's header offset has an
    // extended-header section, use the full-length Sender/Recipient/Subject
    // in place of the 25-char MESSAGES.DAT fields.
    const char *lsubj = q.subject, *lto = q.to, *lfrom = q.from;
    char *csubj = 0, *cto = 0, *cfrom = 0;
    int upVotes = 0, downVotes = 0;

    if (headers.has(pos)) {
        const char *u = headers.get(pos, "Utf8");
        bool utf8 = u && !strcasecmp(u, "true");

        const char *hsender = headers.get(pos, "Sender");
        const char *hrecip = headers.get(pos, "To");
        const char *hsubj = headers.get(pos, "Subject");

        if (hsender)
            lfrom = cfrom = hdrField(hsender, utf8);
        if (hrecip)
            lto = cto = hdrField(hrecip, utf8);
        if (hsubj)
            lsubj = csubj = hdrField(hsubj, utf8);

        // VOTING.DAT ballots name their target by message-ID, so a tally can
        // only be attached to a message whose HEADERS.DAT section has one.
        votes.get(headers.get(pos, "Message-ID"), upVotes, downVotes);
    }

    // A poll is not written to HEADERS.DAT, so its record carries only the
    // 25-char subject; VOTING.DAT has the whole question.
    const char *psubj = votes.getPollSubject(pos);
    if (psubj)
        lsubj = psubj;

    char ddate[40];
    qwkDispDate(ddate, sizeof ddate, q.date);

    letter_header *newLetter = new letter_header(lsubj, lto, lfrom, ddate,
        headers.get(pos, "Message-ID"), q.refnum, letterID, q.msgnum,
        areaID, q.privat, q.msglen, this,
        nullNet, !(!(areas[areaID].attr & LATINCHAR)));

    newLetter->setVotes(upVotes, downVotes);

    delete[] cfrom;
    delete[] cto;
    delete[] csubj;

    return newLetter;
}

// A poll's record in MESSAGES.DAT is a header with no body; its question,
// answers and vote counts live in VOTING.DAT. Render those instead.

letter_body *qwkpack::getBody(letter_header &mhead)
{
    bodytype *areaP = body[mhead.getAreaID() - 1];

    if (areaP) {
        char *text = votes.getPollText(areaP[mhead.getLetterID()].pointer -
                                       128);
        if (text) {
            delete bodyString;
            bodyString = new letter_body(text, strlen(text));
            return bodyString;
        }
    }

    return pktbase::getBody(mhead);
}

void qwkpack::getblk(int, long &offset, long blklen,
                     unsigned char *&p, unsigned char *&begin)
{
    int linebreak = greekqwk ? 12 : 227;

    for (long count = 0; count < blklen; count++) {
        int kar = fgetc(infile);

        if (!kar)
            kar = ' ';

        *p++ = (kar == linebreak) ? '\n' : kar;

        if (kar == linebreak) {
            begin = p;
            offset = ftell(infile);
        }
    }
}

void qwkpack::postfirstblk(unsigned char *&p, letter_header &mhead)
{
    // Get extended (QWKE-type) info, if available:

    char extsubj[72]; // extended subject or other line
    bool anyfound;
    unsigned char *q;

    do {
        anyfound = false;

        q = onecomp(p, extsubj, "subject:");

        if (!q) {
            q = onecomp(p + 1, extsubj, "@subject:");
            if (q) {
                extsubj[strlen(extsubj) - 1] = '\0';
                cropesp(extsubj);
            }
        }

        // For WWIV QWK door:
        if (!q)
            q = onecomp(p, extsubj, "title:");
        if (q) {
            p = q;
            mhead.changeSubject(extsubj);
            anyfound = true;
        }

        // To and From are now checked only in QWKE packets:

        if (qwke) {
            q = onecomp(p, extsubj, "to:");
            if (q) {
                p = q;
                mhead.changeTo(extsubj);
                anyfound = true;
            }

            q = onecomp(p, extsubj, "from:");
            if (q) {
                p = q;
                mhead.changeFrom(extsubj);
                anyfound = true;
            }
        }

        // Skip Synchronet kludge lines:

        static const char *skippers[] = {"@VIA:", "@TZ:", "@MSGID:",
            "@REPLY:", "@REPLYTO:"};

        for (int x = 0; x < 5; x++) {
            q = onecomp(p, extsubj, skippers[x]);
            if (q) {
                p = q;
                anyfound = true;
                break;
            }
        }

    } while (anyfound);
}

void qwkpack::endproc(letter_header &mhead)
{
    // Change to Latin character set, if necessary:
    checkLatin(mhead);
}

void qwkpack::readControlDat()
{
    char *p, *q;

    infile = mm.workList->ftryopen("control.dat");
    if (!infile)
        fatalError("Could not open CONTROL.DAT");

    BBSName = strdupplus(nextLine());           // 1: BBS name
    nextLine();                                 // 2: city/state
    nextLine();                                 // 3: phone#

    q = nextLine();                             // 4: sysop's name
    size_t slen = strlen(q);
    if (slen > 6) {
        p = q + slen - 5;
        if (!strcasecmp(p, "Sysop")) {
            if (*--p == ' ')
                p--;
            if (*p == ',')
                *p = '\0';
        }
    }
    SysOpName = strdupplus(q);

    q = nextLine();                             // 5: doorserno, BBSid
    strtok(q, ",");
    p = strtok(0, " ");
    strnzcpy(packetBaseName, p, 8);

    nextLine();                                 // 6: time & date

    p = nextLine();                             // 7: user's name
    cropesp(p);
    LoginName = strdupplus(p);
    AliasName = strdupplus(p);

    nextLine();                                 // 8: blank/any
    nextLine();                                 // 9: anything
    nextLine();                                 // 10: # messages (unreliable)
    maxConf = atoi(nextLine()) + 2;             // 11: Max conf #

    areas = new AREAs[maxConf];

    areas[0].num = -1;
    strcpy(areas[0].numA, "PERS");
    areas[0].name = strdupplus("PERSONAL");
    areas[0].attr = PUBLIC | PRIVATE | COLLECTION;

    int c;
    for (c = 1; c < maxConf; c++) {
        areas[c].num = atoi(nextLine());              // conf #
        sprintf(areas[c].numA, "%d", areas[c].num);
        areas[c].name = strdupplus(nextLine());       // conf name
        areas[c].attr = PUBLIC | PRIVATE;
    }

    hello = strdupplus(nextLine());
    strncpy(newsfile[0], nextLine(), 12);
    goodbye = strdupplus(nextLine());

    fclose(infile);
}

/* DOOR.ID lines read "KEY = value". Skipping the "= " with strchr(s, '=') + 2
   walks past the terminator when a line has no '=', or nothing after it, and a
   packet can contain either. */

static const char *idValue(const char *s)
{
    const char *p = strchr(s, '=');

    if (!p)
        return "";

    p++;
    while (' ' == *p)
        p++;

    return p;
}

void qwkpack::readDoorId()
{
    bool hasAdd = false, hasDrop = false;

    if (!qwke)
        strcpy(controlname, "QMAIL");

    greekqwk = false;

    infile = mm.workList->ftryopen("door.id");
    if (infile) {
        const char *s, *door, *ver;
        char *t;

        t = strdupplus(nextLine());     // DOOR =
        s = nextLine();                 // VERSION =
        door = idValue(t);
        ver = idValue(s);
        mm.synchro = !strcmp(door, "Synchronet");

        // Both halves are whole lines out of the packet, so size the
        // result rather than writing into a fixed buffer.
        DoorProg = new char[strlen(door) + strlen(ver) + 2];
        sprintf(DoorProg, "%s %s", door, ver);
        delete[] t;

        s = nextLine();                 // SYSTEM =
        BBSProg = strdupplus(idValue(s));

        while (!feof(infile)) {
            s = nextLine();
            if (!strcasecmp(s, "CONTROLTYPE = ADD"))
                hasAdd = true;
            else
                if (!strcasecmp(s, "CONTROLTYPE = DROP"))
                    hasDrop = true;
                else
                    if (!strncasecmp(s, "CONTROLNAME", 11))
                        strnzcpy(controlname, s + 14, 25);
                    else
                        if (!strcasecmp(s, "GREEKQWK"))
                            greekqwk = true;
        }
        fclose(infile);
    }
    if (!qwke)
        hasOffConfig = (hasAdd && hasDrop) ? OFFCONFIG : 0;
}

// Read the QWKE file TOREADER.EXT
void qwkpack::readToReader()
{
    char *s;
    int cnum;
    unsigned long attr;

    hasOffConfig = OFFCONFIG;

    infile = mm.workList->ftryopen("toreader.ext");
    if (infile) {
        while (!feof(infile)) {
            s = nextLine();

            if (!strncasecmp(s, "area ", 5)) {
                if (sscanf(s + 5, "%d %s", &cnum, s) == 2) {

                    attr = SUBKNOWN;

                    // If a group is marked subscribed:
                    if (strchr(s, 'a'))
                        attr |= ACTIVE;
                    else
                        if (strchr(s, 'p'))
                            attr |= (ACTIVE | PERSONLY);
                        else
                            if (strchr(s, 'g'))
                                attr |= (ACTIVE | PERSALL);

                    // "Handles" or "Anonymous":
                    if (strchr(s, 'H') || strchr(s, 'A'))
                        attr |= ALIAS;

                    // Public-only/Private-only:
                    if (strchr(s, 'P'))
                        attr |= PRIVATE;
                    else
                        if (strchr(s, 'O'))
                            attr |= PUBLIC;
                        else
                            attr |= (PUBLIC | PRIVATE);

                    // Read-only:
                    if (strchr(s, 'R'))
                        attr |= READONLY;

                    /* Set character set to Latin-1 for Internet or
                       Usenet areas -- but is this the right thing here? */

                    if (strchr(s, 'U') || strchr(s, 'I'))
                        attr |= LATINCHAR;

                    if (strchr(s, 'E'))
                        attr |= ECHOAREA;

                    areas[getXNum(cnum)].attr = attr;
                }
            } else
                if (!strncasecmp(s, "alias ", 6)) {
                    cropesp(s);
                    delete[] AliasName;
                    AliasName = strdupplus(s + 6);
                }
        }
        fclose(infile);
    }
}

const char *qwkpack::ctrlName()
{
    return controlname;
}

// -----------------------------------------------------------------
// The QWK reply methods
// -----------------------------------------------------------------

qwkreply::upl_qwk::upl_qwk(const char *name) : pktreply::upl_base(name)
{
    memset(&qHead, 0, sizeof(qHead));
    recpos = 0;
    voteDir = 0;
    voteTarget = voteID = 0;
}

qwkreply::upl_qwk::~upl_qwk()
{
    delete[] voteTarget;
    delete[] voteID;
}

qwkreply::qwkreply() : pktreply()
{
    qwke = ((qwkpack *) mm.packet)->isQWKE();
    greekqwk = ((qwkpack *) mm.packet)->isGreekQWK();
    hdrFile = votFile = 0;
    hdrsWritten = votesWritten = false;
}

qwkreply::~qwkreply()
{
}

bool qwkreply::getRep1(FILE *rep, upl_qwk *l)
{
    FILE *replyFile;
    char *p, *q, blk[1280];

    if (!l->qHead.init(rep))
        return false;

    replyFile = fopen(l->fname, "wt");
    if (!replyFile)
        return false;

    long count, length = 0, chunks = l->qHead.msglen >> 7;
    char linebreak = greekqwk ? ((char) 12) : ((char) 227);

    bool firstblk = true;
    while (chunks) {
        count = (chunks > 10) ? 10 : chunks;
        chunks -= count;
        count <<= 7;

        if (!fread(blk, 1, count, rep))
            fatalError("Error reading reply file");

        for (p = blk; p < (blk + count); p++)
            if (*p == linebreak)
                *p = '\n';         // PI-softcr

        p = blk;

        // Get extended (QWKE-type) info, if available:

        if (firstblk) {
            firstblk = false;

            bool anyfound;

            do {
                anyfound = false;

                q = (char *) onecomp((unsigned char *) p,
                                     l->qHead.subject, "subject:");
                if (q) {
                    p = q;
                    anyfound = true;
                }

                if (qwke) {
                    q = (char *) onecomp((unsigned char *) p,
                                         l->qHead.to, "to:");
                    if (q) {
                        p = q;
                        anyfound = true;
                    }

                    q = (char *) onecomp((unsigned char *) p,
                                         l->qHead.from, "from:");
                    if (q) {
                        p = q;
                        anyfound = true;
                    }
                }

            } while (anyfound);
        }

        q = blk + count - 1;
        if (!chunks)
            for (; ((*q == ' ') || (*q == '\n')) && (q > blk); q--);

        length += (long) fwrite(p, 1, q - p + 1, replyFile);
    }
    fclose(replyFile);

    l->qHead.msglen = l->msglen = length;

    return true;
}

void qwkreply::getReplies(FILE *repFile)
{
    fseek(repFile, 128, SEEK_SET);

    noOfLetters = 0;

    upl_qwk baseUplList, *currUplList = &baseUplList;

    while (!feof(repFile)) {
        // Link the node only once its record parses, so the last node's
        // nextRecord can't be left dangling at the deleted EOF allocation
        // (castVote() and readbackVotes() walk the chain by pointer, not
        // by count).
        long pos = ftell(repFile);
        upl_qwk *n = new upl_qwk;
        n->recpos = pos;
        if (!getRep1(repFile, n)) {
            delete n;
            break;
        }
        currUplList->nextRecord = n;
        currUplList = n;
        noOfLetters++;
    }
    uplListHead = baseUplList.nextRecord;

    readbackVotes();
}

// Re-attach vote metadata to the 'V' records just read back from the .MSG,
// by matching their offsets against the marker sections of the packet's own
// VOTING.DAT. A 'V' record with no section stays voteDir 0; it is repacked
// as-is and the BBS ignores it.
void qwkreply::readbackVotes()
{
    FILE *votes = upWorkList->ftryopen("voting.dat");
    if (!votes)
        return;

    char *buf = 0;
    long size = upWorkList->getSize();
    if (size > 0) {
        buf = new char[size + 1];
        size_t got = fread(buf, 1, size, votes);
        buf[got] = '\0';
    }
    fclose(votes);
    if (!buf)
        return;

    unsigned long marker = 0;
    upl_qwk *cur = 0;       // entry the current [vote:...] section belongs to
    char *line = buf;

    while (line) {
        char *nl = strchr(line, '\n');
        if (nl)
            *nl = '\0';

        char *t = iniTrim(line);

        if (*t == '[') {
            char *close = strchr(t, ']');
            if (close) {
                *close = '\0';
                char *sep = strchr(t + 1, ':');
                if (!sep) {
                    marker = strtoul(t + 1, 0, 16);
                    cur = 0;
                } else {
                    *sep = '\0';
                    cur = 0;
                    if (iniKeyEq(t + 1, "vote"))
                        for (upl_qwk *l = (upl_qwk *) uplListHead; l;
                             l = (upl_qwk *) l->nextRecord)
                            if (('V' == l->qHead.status) &&
                                ((unsigned long) l->recpos == marker)) {
                                cur = l;
                                delete[] cur->voteID;
                                cur->voteID = strdupplus(sep + 1);
                                break;
                            }
                }
            }
        } else if (*t && cur) {
            char *key, *value;
            if (iniSplit(t, key, value)) {
                if (iniKeyEq(key, "In-Reply-To")) {
                    delete[] cur->voteTarget;
                    cur->voteTarget = strdupplus(value);
                } else if (iniKeyEq(key, "UpVote")) {
                    if (iniKeyEq(value, "true"))
                        cur->voteDir = 1;
                } else if (iniKeyEq(key, "DownVote")) {
                    if (iniKeyEq(value, "true"))
                        cur->voteDir = -1;
                }
            }
        }

        line = nl ? (nl + 1) : 0;
    }

    delete[] buf;
}

area_header *qwkreply::getNextArea()
{
    int hdrlen = (qwke || ((qwkpack *) mm.packet)->hasHeaders()) ? 71 : 25;

    return new area_header(0, "REPLY", "REPLIES",
        "Letters written by you", (greekqwk ? "GreekQWK replies" :
        (qwke ? "QWKE replies" : "QWK replies")),
        (COLLECTION | REPLYAREA | ACTIVE | PUBLIC | PRIVATE),
        noOfLetters, 0, hdrlen, hdrlen);
}

letter_header *qwkreply::getNextLetter()
{
    static net_address nullNet;
    upl_qwk *current = (upl_qwk *) uplListCurrent;

    int area = ((qwkpack *) mm.packet)->
        getXNum((int) current->qHead.msgnum) + 1;

    char ddate[40];
    qwkDispDate(ddate, sizeof ddate, current->qHead.date);

    letter_header *newLetter = new letter_header(
        current->qHead.subject, current->qHead.to, current->qHead.from,
        ddate, 0, current->qHead.refnum, currentLetter,
        currentLetter, area, current->qHead.privat,
        current->qHead.msglen, this, nullNet, mm.areaList->isLatin(area));

    currentLetter++;
    uplListCurrent = uplListCurrent->nextRecord;
    return newLetter;
}

void qwkreply::enterLetter(letter_header &newLetter,
                           const char *newLetterFileName, long length)
{
    // Specify the format separately from strftime() to suppress
    // GCC's Y2K warning:
    const char *datefmt_qwk = "%m-%d-%y %H:%M";

    upl_qwk *newList = new upl_qwk(newLetterFileName);

    strncpy(newList->qHead.subject, newLetter.getSubject(),
            sizeof(newList->qHead.subject) - 1);
    strncpy(newList->qHead.from, newLetter.getFrom(),
            sizeof(newList->qHead.from) - 1);
    strncpy(newList->qHead.to, newLetter.getTo(),
            sizeof(newList->qHead.to) - 1);

    newList->qHead.msgnum = atol(mm.areaList->getShortName());
    newList->qHead.privat = newLetter.getPrivate();
    newList->qHead.refnum = newLetter.getReplyTo();

    time_t now = time(0);
    strftime(newList->qHead.date, 15, datefmt_qwk, localtime(&now));

    newList->qHead.msglen = newList->msglen = length;

    addUpl(newList);
}

// Cast a vote: a REPLY-area entry that packs as a bodyless status-'V'
// record plus a ballot section in the REP's VOTING.DAT (the mirror image
// of what we read; see docs/synchronet-qwk-extensions.md). Synchronet
// discards senderless ballots and rejects a Conference mismatch, so both
// fields matter. One ballot per message per packet.
bool qwkreply::castVote(const char *msgid, int conf, const char *sender,
                        const char *subject, bool up)
{
    if (!msgid || !*msgid || !sender || !*sender)
        return false;

    for (upl_qwk *l = (upl_qwk *) uplListHead; l;
         l = (upl_qwk *) l->nextRecord)
        if (l->voteDir && !strcasecmp(l->voteTarget, msgid))
            return false;       // already voted on it in this packet

    const char *datefmt_qwk = "%m-%d-%y %H:%M";

    upl_qwk *n = new upl_qwk;   // no body: the file is never created

    sprintf(n->qHead.subject, "%s: %.60s", up ? "Upvote" : "Downvote",
            subject ? subject : "");
    strnzcpy(n->qHead.from, sender, sizeof(n->qHead.from) - 1);

    // No recipient: Synchronet's own ballot records leave To empty, and
    // nothing reads the field on import (un_rep.cpp uses only the record's
    // offset, status byte and block count).

    n->qHead.msgnum = conf;
    n->qHead.status = 'V';

    time_t now = time(0);
    strftime(n->qHead.date, 15, datefmt_qwk, localtime(&now));

    n->voteDir = up ? 1 : -1;
    n->voteTarget = strdupplus(msgid);

    char id[80];
    sprintf(id, "<%lx.%d.ncmail@%.40s>", (unsigned long) now,
            noOfLetters + 1, getBaseName());
    n->voteID = strdupplus(id);

    addUpl(n);
    return true;
}

bool qwkreply::isVote(int letterNo)
{
    upl_qwk *l = (upl_qwk *) uplListHead;

    for (int c = 1; l && (c < letterNo); c++)
        l = (upl_qwk *) l->nextRecord;

    return l && l->voteDir;
}

void qwkreply::addRep1(FILE *rep, upl_base *node, int c)
{
    FILE *replyFile;
    upl_qwk *l = (upl_qwk *) node;
    long count = 0;
    char linebreak = greekqwk ? ((char) 12) : ((char) 227);

    if (c == 0) {
        hdrFile = votFile = 0;
        hdrsWritten = votesWritten = false;
    }

    long headerpos = ftell(rep);
    l->qHead.output(rep);

    // A vote packs as just the header record (chunks = 1, no body) and a
    // ballot section in VOTING.DAT. The key order and separators mirror
    // Synchronet's own writer (msgtoqwk.cpp).
    if (l->voteDir) {
        if (!votFile)
            votFile = fopen("VOTING.DAT", "wb");
        if (votFile) {
            char when[24];
            qwkVoteDate(when, time(0));
            fprintf(votFile, "[%lx]\r\n", (unsigned long) headerpos);
            fprintf(votFile, "[vote:%s]\r\n", l->voteID);
            fprintf(votFile, "%sVote = true\r\n",
                    (l->voteDir > 0) ? "Up" : "Down");
            fprintf(votFile, "In-Reply-To: %s\r\n", l->voteTarget);
            fprintf(votFile, "WhenWritten:  %s\r\n", when);
            fprintf(votFile, "Sender: %s\r\n", l->qHead.from);
            fprintf(votFile, "Conference: %ld\r\n", l->qHead.msgnum);
            fprintf(votFile, "\r\n");
            votesWritten = true;
        }
        l->qHead.set_length(rep, headerpos, ftell(rep));
        return;
    }

    bool longfrom = strlen(l->qHead.from) > 25;
    bool longto = strlen(l->qHead.to) > 25;
    bool longsubj = strlen(l->qHead.subject) > 25;

    if (longfrom)
        fprintf(rep, "From: %s%c", l->qHead.from, linebreak);
    if (longto)
        fprintf(rep, "To: %s%c", l->qHead.to, linebreak);
    if (longsubj)
        fprintf(rep, "Subject: %s%c", l->qHead.subject, linebreak);
    if (longfrom || longto || longsubj)
        fprintf(rep, "%c", linebreak);

    if (longfrom || longto || longsubj) {
        if (!hdrFile)
            hdrFile = fopen("HEADERS.DAT", "wb");
        if (hdrFile) {
            fprintf(hdrFile, "[%lx]\r\n", (unsigned long) headerpos);
            fprintf(hdrFile, "Utf8: false\r\n");
            fprintf(hdrFile, "Sender: %s\r\n", l->qHead.from);
            fprintf(hdrFile, "To: %s\r\n", l->qHead.to);
            fprintf(hdrFile, "Subject: %s\r\n", l->qHead.subject);
            fprintf(hdrFile, "\r\n");
            hdrsWritten = true;
        }
    }

    replyFile = fopen(l->fname, "rt");
    if (replyFile) {

        int c, lastsp = 0;
        while ((c = fgetc(replyFile)) != EOF) {
            count++;
            if ((count > 80) && lastsp) {
                fseek(replyFile, lastsp - count, SEEK_CUR);
                fseek(rep, lastsp - count, SEEK_CUR);
                c = '\n';
            }
            if ('\n' == c) {
                fputc(linebreak, rep);
                count = lastsp = 0;
            } else {
                fputc(c, rep);
                if (' ' == c)
                    lastsp = count;
            }
        }
        fclose(replyFile);
    }

    fputc(linebreak, rep);

    long curpos = ftell(rep);
    l->qHead.set_length(rep, headerpos, curpos);
}

void qwkreply::repFinish()
{
    if (hdrFile) {
        fclose(hdrFile);
        hdrFile = 0;
    }
    if (votFile) {
        fclose(votFile);
        votFile = 0;
    }
}

void qwkreply::addHeader(FILE *repFile)
{
    char tmp[129];
    sprintf(tmp, "%-128s", getBaseName());
    fwrite(tmp, 128, 1, repFile);
}

void qwkreply::repFileName()
{
    int x;
    const char *basename = getBaseName();

    for (x = 0; basename[x]; x++) {
        replyPacketName[x] = tolower(basename[x]);
        replyInnerName[x] = toupper(basename[x]);
    }
    strcpy(replyPacketName + x, ".rep");
    strcpy(replyInnerName + x, ".MSG");
}

const char *qwkreply::repTemplate(bool offres)
{
    static char buff[112];

    // The inner name comes from the packet header, and this string is handed
    // to the shell with the file names alongside it, so quote it.
    char *qname = quotespace(replyInnerName);
    sprintf(buff, (offres && qwke) ? "%s TODOOR.EXT" : "%s", qname);
    delete[] qname;

    if (hdrsWritten)
        sprintf(buff + strlen(buff), " HEADERS.DAT");
    if (votesWritten)
        sprintf(buff + strlen(buff), " VOTING.DAT");

    return buff;
}

bool qwkreply::getOffConfig()
{
    bool status = false;

    if (qwke) {
        FILE *olc;

        upWorkList = new file_list(mm.res.get(UpWorkDir));

        olc = upWorkList->ftryopen("todoor.ext");
        if (olc) {
            char line[128], mode;
            int areaQWK, areaNo;

            while (!feof(olc)) {
                myfgets(line, sizeof line, olc);
                if (sscanf(line, "AREA %d %c", &areaQWK, &mode) == 2) {
                    areaNo = ((qwkpack *) mm.packet)->getXNum(areaQWK) + 1;
                    mm.areaList->gotoArea(areaNo);
                    if (mode == 'D')
                        mm.areaList->Drop();
                    else
                        mm.areaList->Add();
                }
            }
            fclose(olc);
            upWorkList->kill();

            status = true;
        }
        delete upWorkList;
    }
    return status;
}

bool qwkreply::makeOffConfig()
{
    FILE *todoor = 0;    // warning suppression
    net_address bogus;
    letter_header *ctrlMsg;
    const char *myname = 0, *ctrlName = 0;

    if (qwke) {
        todoor = fopen("TODOOR.EXT", "wb");
        if (!todoor)
            return false;
    } else {
        myname = mm.packet->getLoginName();
        ctrlName = ((qwkpack *) mm.packet)->ctrlName();
    }

    int oldarea = mm.areaList->getAreaNo();

    int maxareas = mm.areaList->noOfAreas();
    for (int areaNo = 0; areaNo < maxareas; areaNo++) {
        mm.areaList->gotoArea(areaNo);
        unsigned long attrib = mm.areaList->getType();

        if (attrib & (ADDED | DROPPED)) {
            if (qwke)
                fprintf(todoor, "AREA %s %c\r\n",
                        mm.areaList->getShortName(), (attrib & ADDED) ?
                        ((attrib & PERSONLY) ? 'p' : ((attrib & PERSALL)
                        ? 'g' : 'a')) : 'D');
            else {
                ctrlMsg = new letter_header((attrib & ADDED) ?
                          "ADD" : "DROP", ctrlName, myname, "", 0, 0, 0,
                          0, areaNo, false, 0, this, bogus, false);

                enterLetter(*ctrlMsg, "", 0);

                delete ctrlMsg;

                if (attrib & ADDED)
                    mm.areaList->Drop();
                else
                    mm.areaList->Add();
            }
        }
    }
    mm.areaList->gotoArea(oldarea);

    if (qwke)
        fclose(todoor);
    else
        mm.areaList->refreshArea();

    return true;
}
