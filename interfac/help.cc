/*
 * NoCarrierMail offline mail reader
 * help windows

 Copyright 1996-1997 Kolossvary Tamas <thomas@vma.bme.hu>
 Copyright 1997-2021 William McBrine <wmcbrine@gmail.com>
 Distributed under the GNU General Public License, version 3 or later. */

#include "interfac.h"

#define ARRAY_SIZE(a) ((int) (sizeof (a) / sizeof (a)[0]))

/* One table per screen, read by both renderers: the summary bar along the
   bottom of the screen and the full list on F1. A {0, 0} entry is a spacer
   holding the bar's three-column layout in step. */

static const helpitem hi_packetlist[] = {
    {"Q", "Quit"},
    {"Enter", "select packet"},
    {"S, $", "change Sort type"},

    {"K", "Kill packet"},
    {"/, .", "search / next"},
    {"G", "Go to directory"},

    {"^R", "reply dir"},
    {"U", "Update list"},
    {"R", "Rename packet"},

    {"A", "Addressbook"},
    {"^T", "Tagline editor"},
    {"^Z", "command shell"},

    {"B", "alias for PgUp"},
    {"Space, F", "aliases for PgDn"},
    {"^X", "eXit now"},

    {"T", "Touch file"},
    {"|, ^", "filter list"}
};

static const helpitem hi_arealist[] = {
    {"Q", "back to packet list"},
    {"Enter", "select area"},
    {"F2, !", "Make reply packet"},

    {"E", "Enter letter in area"},
    {"S, Ins", "Subscribe"},
    {"U, Del", "Unsubscribe"},

    {"L", "all/subscribed/active"},
    {"-", "prev non-empty"},
    {"+", "next non-empty"},

    {"8", "area is Latin-1"}
};

static const helpitem hi_letterlist[] = {
    {"L", "List all/unread/marked"},
    {"Enter", "read letter"},
    {"$", "change sort type"},

    {"E", "Enter letter in area"},
    {"^F", "Forward letter"},
    {"S", "Save (all/marked)"},

    {"U", "Unread/read toggle"},
    {"M", "Mark/unmark"},

    {"A", "Addressbook"},
    {"^T", "Tagline editor"},
    {"F2, !", "make reply packet"},

    {"^E", "Enter from addressbook"},
    {"-", "previous unread"},
    {"+", "next unread"}
};

static const helpitem hi_replylist[] = {
    {"K", "Kill letter"},
    {"Enter", "read letter"},
    {"$", "change sort type"},

    {"E", "Edit letter"},
    {"^F", "Forward letter"},
    {"S", "Save (all/marked)"},

    {0, 0},
    {"^B", "Break into parts"},

    {"A", "Addressbook"},
    {"^T", "Tagline editor"},
    {"F2, !", "Make reply packet"}
};

static const helpitem hi_lettercommon[] = {
    {"S", "Save letter"},
    {"L", "open in externaL viewer"},
    {"A", "Addressbook"},
    {"C", "toggle Character set"},
    {"D", "Decrypt (rot13) toggle"},
    {"X", "eXtra (hidden) lines"},
    {"I", "Ignore soft CRs toggle"},
    {"^T", "Tagline editor"},
    {"^F", "Forward letter"},
    {"F2, !", "Make reply packet"},
    {"V, ^V, ^A", "ANSI viewer"},
    {"/", "start a search"},
    {".", "repeat last search"},
    {"-", "previous letter"},
    {"+, Enter", "next letter"},
    {"Space", "page through area"},
    {"^Z", "command shell"},
    {"Q", "back to letter list"},
    {"^X", "eXit " MM_NAME " now"}
};

static const helpitem hi_letter[] = {
    {"E", "Enter new letter (post)"},
    {"R", "Reply (followup)"},
    {"O", "reply to Original sender"},
    {"N", "Netmail/Email reply"},
    {"T", "Take tagline"},
    {"M", "Mark/unmark letter"},
    {"U", "Unread/read toggle"},
    {"W", "vote up/down on message"}
};

static const helpitem hi_replyletter[] = {
    {"K", "Kill letter"},
    {"E, R", "[Re-]Edit letter"},
    {"^B", "Break reply into parts"}
};

static const helpitem hi_ansi[] = {
    {"S", "Save to file"},
    {"C", "toggle Character set"},
    {"/", "start a search"},
    {".", "repeat last search"},
    {"Space", "page down/next"},
    {"-", "previous"},
    {"+", "next"},
    {"@", "toggle at-code parsing"},
    {"|", "toggle pipe codes"},
    {"1", "toggle Synchronet codes"},
    {"^V", "toggle AVATAR parsing"},
    {"^B", "toggle BSAVE parsing"},
    {"Q", "Quit ANSI viewer"}
};

static const helpitem hi_address[] = {
    {"Q", "Quit addressbook"},
    {"A", "Add new address"},
    {"/, .", "search / next"},

    {"K", "Kill current address"},
    {"E", "Edit address"}
};

// Only when the address book was opened while entering a letter.
static const helpitem hi_addressletter[] = {
    {"L", "address from Letter"}
};

static const helpitem hi_littlearea[] = {
    {"Enter", "send the letter to this area"},
    {"L", "all/subscribed/active"},
    {"/, .", "search / next"},
    {"|, ^", "filter list"},
    {"Q", "cancel"}
};

static const helpitem hi_tagline[] = {
    {"Q", "don't apply tagline"},
    {"R", "Random select tagline"},
    {"Enter", "apply tagline"},

    {"K", "Kill current tagline"},
    {"A", "Add new tagline"},
    {"E", "Edit tagline"}
};

static void helpSet(helplist &h, const helpitem *items, int nitems)
{
    h.items = items;
    h.nitems = nitems;
    h.more = 0;
    h.nmore = 0;
}

static const helpitem *helpAt(const helplist &h, int z)
{
    return (z < h.nitems) ? &h.items[z] : &h.more[z - h.nitems];
}

/* Draw key/description pairs into w as rows of columns, the ": " of each
   column at the given screen position and the key right-aligned against it.
   The help bar at the bottom of a list window and the ones the address book
   and tagline window draw inside their own frames all share this.

   Entries beyond rows * ncol are left off: the bar is a summary, and F1 shows
   the whole list. */

void helpBar(Win *w, int row, int rows, const int *colon, int ncol,
             const helplist &h, coltype keyattr, coltype textattr)
{
    int n = h.nitems + h.nmore;

    if (n > rows * ncol)
        n = rows * ncol;

    for (int z = 0; z < n; z++) {
        const helpitem *it = helpAt(h, z);

        if (!it->key)
            continue;

        int c = z % ncol, y = row + z / ncol, x = colon[c];

        /* Stop a description a space short of the next column's key on this
           row; a left column widened for a long key ("Space, F") would
           otherwise run into it. The last column runs to the window edge. */
        int room = -1;
        if (c < ncol - 1) {
            const helpitem *next = (z + 1 < n) ? helpAt(h, z + 1) : 0;
            int nextx = colon[c + 1] -
                        ((next && next->key) ? (int) strlen(next->key) : 0);

            room = nextx - 1 - (x + 2);
            if (room < 0)
                room = 0;
        }

        w->attrib(textattr);
        w->put(y, x, ": ");
        w->put(y, x + 2, it->desc, room);

        w->attrib(keyattr);
        w->put(y, x - (int) strlen(it->key), it->key);
    }
}

HelpWindow::HelpWindow()
{
    menu = 0;
    baseReset();
}

void HelpWindow::newHelpMenu(const helplist &h)
{
    if (mm.res.getInt(ExpertMode))
        menu = 0;
    else {
        int end;

        items = h.nitems;

        menu = new Win(3, COLS - 2, LINES - 4, C_HELP2);

        midpos = (COLS / 2) - 6;
        endpos = COLS - 21;

        if (items < 10)
            end = items;
        else {
            end = base + 8;
            if (end > items)
                end = items;
        }
        int colon[3];
        colon[0] = 2;
        colon[1] = midpos;
        colon[2] = endpos;

        // Keys right-align against the colon, and the left column's sits as
        // far left as a two-character key allows. A wider key on this page
        // ("|, ^") moves it over, or the key is drawn off the edge.
        for (int z = base; z < end; z += 3)
            if (h.items[z].key && ((int) strlen(h.items[z].key) > colon[0]))
                colon[0] = strlen(h.items[z].key);

        // The list screens have no situational entries, so a page is a
        // plain slice of their table.
        helplist page;
        helpSet(page, h.items + base, end - base);

        helpBar(menu, 0, 3, colon, 3, page, C_HELP2, C_HELP1);
        if (items > 9) {
            menu->put(2, endpos - 1, "O");
            menu->attrib(C_HELP1);
            menu->put(2, endpos, ": Other functions");
        }
        menu->delay_update();
    }
}

/* Lay the given tables out as a list, one entry per line where the screen is
   tall enough and two columns where it is not. Nothing scrolls, so the two
   column fallback is what keeps the letter window's long list reachable on a
   24 line terminal. */

Win *HelpWindow::buildFull(const helplist &h)
{
    enum {maxitems = 64};

    const helpitem *item[maxitems];
    int z, n = 0, len, maxdesc = 0;

    for (z = 0; (z < h.nitems + h.nmore) && (n < maxitems); z++) {
        const helpitem *it = helpAt(h, z);

        if (it->key)
            item[n++] = it;
    }

    // Two more lines than the window itself, for the border and the shadow.
    int cols = ((n + 6) <= LINES) ? 1 : 2;

    /* The keys vary in width, so pad them to the widest; otherwise the
       separator and every description sit at a different column per line.
       Each column is padded on its own, which leaves the right one more room
       for descriptions on a narrow terminal. */
    int maxkey[2] = {0, 0};

    for (z = 0; z < n; z++) {
        int c = (cols > 1) ? (z & 1) : 0;

        len = strlen(item[z]->key);
        if (len > maxkey[c])
            maxkey[c] = len;

        len = strlen(item[z]->desc);
        if (len > maxdesc)
            maxdesc = len;
    }

    int widest = (maxkey[0] > maxkey[1]) ? maxkey[0] : maxkey[1];
    int rows = (n + cols - 1) / cols;
    int height = rows + 4;
    int width = (widest + 3 + maxdesc) * cols + (cols > 1 ? 6 : 4);

    // The footer sets a floor on the width; a short list would clip it.
    if (width < 36)
        width = 36;
    if (width > (COLS - 2))
        width = COLS - 2;
    if (height > LINES)
        height = LINES;

    Win *w = new ShadowedWin(height, width, (LINES - height) >> 1, C_HELP3);
    w->attrib(C_HELP4);

    /* Where each column starts, and the room an entry has: up to one space
       before the next column, or before the border. On a narrow terminal the
       width above has been clamped, so descriptions are cut to fit rather
       than run into each other. */
    int colx[2], room[2];

    colx[0] = 2;
    colx[1] = (width >> 1) + 2;
    room[0] = ((cols > 1) ? colx[1] - 1 : width - 2) - colx[0];
    room[1] = (width - 1) - colx[1];

    char tmp[80];

    for (z = 0; z < n; z++) {
        int c = (cols > 1) ? (z & 1) : 0;
        int line = (cols > 1) ? (z >> 1) + 1 : z + 1;
        int dlen = room[c] - maxkey[c] - 3;

        if (dlen > 50)
            dlen = 50;
        if (dlen < 0)
            dlen = 0;

        sprintf(tmp, "%*.12s - %.*s", maxkey[c], item[z]->key, dlen,
                item[z]->desc);
        w->put(line, colx[c], tmp);
    }

    w->put(rows + 2, 2, "Plus the standard direction keys");

    return w;
}

void HelpWindow::h_letter(bool isAnsi)
{
    bool isReply = mm.areaList->isReplyArea();

    const helpitem *extra = isAnsi ? hi_ansi :
                            (isReply ? hi_replyletter : hi_letter);
    int nextra = isAnsi ? ARRAY_SIZE(hi_ansi) :
                 (isReply ? ARRAY_SIZE(hi_replyletter) :
                            ARRAY_SIZE(hi_letter));

    helplist h;
    helpSet(h, extra, nextra);
    if (!isAnsi) {
        h.more = hi_lettercommon;
        h.nmore = ARRAY_SIZE(hi_lettercommon);
    }

    menu = buildFull(h);
    menu->wtouch();
}

/* The one place that says which table belongs to which screen. Both renderers
   go through it, so a key added to a table shows up in the bar and on F1.
   h.items is 0 for a screen with no table. */

void helpTable(statetype st, bool addrInLetter, helplist &h)
{
    helpSet(h, 0, 0);

    switch (st) {
    case packetlist:
        helpSet(h, hi_packetlist, ARRAY_SIZE(hi_packetlist));
        break;
    case arealist:
        helpSet(h, hi_arealist, ARRAY_SIZE(hi_arealist));
        break;
    case letterlist:
        if (mm.areaList->isReplyArea())
            helpSet(h, hi_replylist, ARRAY_SIZE(hi_replylist));
        else
            helpSet(h, hi_letterlist, ARRAY_SIZE(hi_letterlist));
        break;
    case littlearealist:
        helpSet(h, hi_littlearea, ARRAY_SIZE(hi_littlearea));
        break;
    case address:
        helpSet(h, hi_address, ARRAY_SIZE(hi_address));
        if (addrInLetter) {
            h.more = hi_addressletter;
            h.nmore = ARRAY_SIZE(hi_addressletter);
        }
        break;
    case tagwin:
        helpSet(h, hi_tagline, ARRAY_SIZE(hi_tagline));
        break;
    default:;
    }
}

/* F1 or '?' from a list window. The letter window and the ANSI viewer have
   their own help states; this one is a popup that any key dismisses, so the
   list underneath keeps its place. Returns false for a screen with no table,
   having drawn nothing. */

bool HelpWindow::fullHelp(statetype st, bool addrInLetter)
{
    helplist h;
    int c;

    helpTable(st, addrInLetter, h);
    if (!h.items)
        return false;

    Win *w = buildFull(h);
    w->wtouch();
    w->update();

    do
        c = w->inkey();
    while (ERR == c);

    delete w;

#ifdef KEY_RESIZE
    // Any key closes the popup, but a resize is the main loop's to handle:
    // it rebuilds every window for the new size.
    if (KEY_RESIZE == c)
        ungetch(c);
#endif

    return true;
}

void HelpWindow::MakeActive()
{
    switch (ui.active()) {
    case ansi_help:
        h_letter(true);
        break;
    case letter_help:
        h_letter(false);
        break;
    case letterlist:
    case arealist:
    case packetlist:
        {
            helplist h;
            helpTable(ui.active(), false, h);
            newHelpMenu(h);
        }
    default:;
    }
}

void HelpWindow::Delete()
{
    // menu is a Win or a ShadowedWin; virtual ~Win() deletes either correctly.
    // (The old ui.active()-based switch skipped littlearealist/address/tagwin,
    // leaking the menu allocated for those states.)
    delete menu;
    menu = 0;
}

void HelpWindow::baseNext()
{
    if (items > 9) {
        base += 8;
        if (base > (items - 1))
            base = 0;
    }
    Delete();
    MakeActive();
}

void HelpWindow::baseReset()
{
    base = 0;
}
