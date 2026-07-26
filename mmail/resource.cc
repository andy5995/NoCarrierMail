/*
 * NoCarrierMail offline mail reader
 * resource class

 Copyright 1996-1997 Toth Istvan <stoty@vma.bme.hu>
 Copyright 1997-2018 William McBrine <wmcbrine@gmail.com>
 Distributed under the GNU General Public License, version 3 or later. */

#include "mmail.h"
#include "../interfac/error.h"

/* Default filenames. */

#ifdef __MSDOS__
# define DEFEDIT "edit"
# define DEFVIEWER "more"
# define DEFZIP "pkzip -#"
# define DEFUNZIP "pkunzip -# -o"
# define DEFLHA "lha a /m"
# define DEFUNLHA "lha e"
#else
# ifdef __WIN32__
#  define DEFEDIT "start /w notepad"
#  define DEFVIEWER "more"
# else
#  ifdef __OS2__
#   define DEFEDIT "tedit"
#   define DEFVIEWER "more"
#  else
#   define DEFEDIT "vi"
#   define DEFVIEWER "less"
#  endif
# endif
# define DEFZIP "zip -jkq"
# define DEFUNZIP "unzip -joLq"
# define DEFLHA "lha af"
# define DEFUNLHA "lha efi"
#endif

#define DEFARJ "arj a -e"
#define DEFUNARJ "arj e"
#define DEFRAR "rar u -ep -inul"
#define DEFUNRAR "rar e -cl -o+ -inul"
#define DEFTAR "tar zcf"
#define DEFUNTAR "tar zxf"

#define DEFNONE "xxcompress"
#define DEFUNNONE "xxuncompress"

#ifdef DOSNAMES
# define RCNAME "ncmail.rc"
# define ADDRBOOK "address.bk"
#else
# define RCNAME ".ncmailrc"
# define ADDRBOOK "addressbook"
#endif

// ================
// baseconfig class
// ================

baseconfig::baseconfig()
{
    seen = 0;
    unknownKeyword = false;
}

baseconfig::~baseconfig()
{
    delete[] seen;
}

/* Find the keyword at the start of a config line. Returns 0 for a comment, a
   blank line, or anything without a name and a separator, and otherwise the
   start of the name, with its length in *len. It does not modify the line, so
   updateConfig() can ask about text it is going to copy back out unchanged.

   Both callers must agree on where a name ends, or one of them would comment
   out a line the other had matched. That is why the separator is required
   rather than accepting end-of-line: parseConfig() still has the trailing
   newline on the line, and updateConfig() has already removed it. */

static char *keywordOf(char *line, int *len)
{
    char *p = line;

    while ((' ' == *p) || ('\t' == *p))
        p++;

    if (('#' == *p) || ('\n' == *p) || !*p)
        return 0;

    //skip "bw" -- for backwards compatiblity
    if (('b' == *p) && ('w' == p[1]))
        p += 2;

    char *name = p;

    while (*p && (':' != *p) && ('=' != *p) && (' ' != *p) && ('\t' != *p))
        p++;

    *len = (int) (p - name);

    return (*len && *p) ? name : 0;
}

int baseconfig::findKeyword(const char *name, int len) const
{
    for (int c = 0; c < configItemNum; c++)
        if (!strncasecmp(names[c], name, len) && !names[c][len])
            return c;

    return -1;
}

bool baseconfig::parseConfig(const char *configFileName)
{
    FILE *configFile;
    char buffer[256], *pos, *resName, *resValue;

    delete[] seen;
    seen = new bool[configItemNum];
    for (int c = 0; c < configItemNum; c++)
        seen[c] = false;
    unknownKeyword = false;

    configFile = fopen(configFileName, "rt");
    if (configFile) {
        while (myfgets(buffer, sizeof buffer, configFile)) {
            int namelen;

            resName = keywordOf(buffer, &namelen);

            if (resName) {
                pos = resName + namelen;

                if (*pos)
                    *pos++ = '\0';

                //chars between strings
                while (*pos == ' ' || *pos == '\t' ||
                       *pos == ':' || *pos == '=')
                    pos++;

                //resValue
                resValue = pos;
                while (*pos != '\n' && *pos)
                    pos++;
                *pos = '\0';

                if (strncasecmp("ver", resName, 3))
                    processOneByName(resName, resValue);
            }
        }
        fclose(configFile);
    }

    /* Does the file need updating? The question used to be "is it older than
       this program", which fired on any version bump -- opening a development
       cycle was enough to rewrite every user's file and prompt them about it.
       Ask instead whether its keywords are the ones this version has, which is
       the only thing an update actually changes. */

    for (int c = 0; c < configItemNum; c++)
        if (!seen[c])
            return true;

    return unknownKeyword;
}

/* Update the file in place: append the keywords it does not have, comment out
   the ones this version no longer knows, and copy every other line through
   unchanged. Regenerating the file kept the values of known keywords but threw
   away the user's own comments, their ordering and their formatting.

   Returns true if there was no file and a fresh one was written instead. */

bool baseconfig::updateConfig(const char *configname)
{
    FILE *fd = fopen(configname, "rt");

    if (!fd) {
        newConfig(configname);
        return true;
    }

    // Read it whole: it is about to be written back over itself.

    fseek(fd, 0, SEEK_END);
    long size = ftell(fd);
    rewind(fd);

    size_t room = (size_t) size + 1;

    /* A negative size means it is not a seekable file at all -- fopen("rt")
       succeeds on a directory, and ColorFile is a path a user can point
       anywhere. The second test catches a file too big for size_t, which only
       a 16-bit host would trip over. Either way, leave the file alone. */

    if ((size < 0) || ((long) room != (size + 1))) {
        fclose(fd);
        pauseError("Cannot read config file to update it");
        return false;
    }

    char *old = new char[room];
    size_t len = fread(old, 1, room - 1, fd);

    old[len] = '\0';
    fclose(fd);

    fd = fopen(configname, "wt");
    if (!fd) {
        delete[] old;
        pauseError("Error writing config file");
        return false;
    }

    printf("Updating %s...\n", configname);

    int commented = 0;
    char *line = old;

    while (*line) {
        char *next = strchr(line, '\n');

        if (next)
            *next++ = '\0';
        else
            next = line + strlen(line);

        int namelen;
        char *name = keywordOf(line, &namelen);

        if (name && !strncasecmp("ver", name, 3))
            fprintf(fd, "Version: " MM_VERNUM "\n");
        else
            if (name && (findKeyword(name, namelen) < 0)) {
                fprintf(fd, "# %s\n", line);
                commented++;
            } else
                fprintf(fd, "%s\n", line);

        line = next;
    }

    delete[] old;

    int added = 0;

    for (int x = 0; x < configItemNum; x++)
        if (!seen[x]) {
            if (!added)
                fprintf(fd, "\n# Added by " MM_NAME " v" MM_VERNUM ":\n");
            if (comments[x])
                fprintf(fd, "\n# %s\n", comments[x]);
            fprintf(fd, "%s: %s\n", names[x], configLineOut(x));
            added++;
        }

    fclose(fd);

    printf("%d keyword%s added, %d commented out.\n", added,
           (1 == added) ? "" : "s", commented);

    return false;
}

void baseconfig::newConfig(const char *configname)
{
    FILE *fd;
    const char **p;

    printf("Creating %s...\n", configname);

    fd = fopen(configname, "wt");
    if (fd) {
        for (p = intro; *p; p++)
            fprintf(fd, "# %s\n", *p);

        fprintf(fd, "\n# The version that last changed the keywords here\n"
                    "Version: " MM_VERNUM "\n");

        for (int x = 0; x < configItemNum; x++) {
            if (comments[x])
                fprintf(fd, "\n# %s\n", comments[x]);
            fprintf(fd, "%s: %s\n", names[x], configLineOut(x));
        }
        fclose(fd);
    } else
        pauseError("Error writing config file");
}

void baseconfig::processOneByName(const char *resName, const char *resValue)
{
    int c = findKeyword(resName, (int) strlen(resName));

    if (c < 0) {
        unknownKeyword = true;
        printf("Unrecognized keyword: %s\n", resName);
    } else {
        processOne(c, resValue);
        if (seen)
            seen[c] = true;
    }
}

// ==============
// resource class
// ==============

const int startUpLen =
 53
#ifdef USE_SPAWNO
 + 1
#endif
 ;

const char *resource::rc_names[startUpLen] =
{
    "UserName", "InetAddr", "QuoteHead", "InetQuote",
    "mmHomeDir", "TempDir", "signature", "editor", "Viewer", "DateFormat",
    "PacketDir", "ReplyDir", "SaveDir", "AddressBook", "TaglineFile",
    "ColorFile", "UseColors", "Transparency", "BackFill",
    "arjUncompressCommand", "zipUncompressCommand",
    "lhaUncompressCommand", "rarUncompressCommand",
    "tarUncompressCommand", "unknownUncompressCommand",
    "arjCompressCommand", "zipCompressCommand", "lhaCompressCommand",
    "rarCompressCommand", "tarCompressCommand",
    "unknownCompressCommand", "PacketSort", "AreaMode", "LetterSort",
    "LetterMode", "ClockMode", "Charset", "UseTaglines",
    "AutoSaveReplies", "StripSoftCR", "BeepOnPers", "UseLynxNav",
    "ReOnReplies", "QuoteWrapCols", "MaxLines", "outCharset",
    "UseQPMailHead", "UseQPNewsHead", "UseQPMail", "UseQPNews",
    "ExpertMode", "IgnoreNDX", "Mouse"
#ifdef USE_SPAWNO
    , "swapOut"
#endif
};

const char *resource::rc_intro[] = {
 "-----------------------",
 MM_NAME " configuration",
 "-----------------------",
 "",
 "Any of these keywords may be omitted, in which case the default values",
 "(shown here) will be used.",
 "",
 "If you change either of the base directories, all the subsequent paths",
 "will be changed, unless they're overridden in the individual settings.",
 "",
 "Please see the man page for a more thorough explanation of these options.",
 0
};

const char *resource::rc_comments[startUpLen] = {
 "Your name, as you want it to appear on replies (used mainly in SOUP)",
 "Your Internet email address (used only in SOUP replies)",
 "Quote header for replies (non-Internet)",
 "Quote header for Internet email and Usenet replies",
 "Base directories (derived from $HOME or $NCMAIL)", 0,
 "Signature (file) that should be appended to each message. (Not used\n"
 "# unless specified here.)",
 "Editor for replies = $EDITOR; or if not defined, " DEFEDIT,
 "External viewer for a message (L key) = $PAGER, else " DEFVIEWER,
 "Date/time format in the reading view (strftime; %x is your locale's date)",
 MM_NAME " will look for packets here",
 "Reply packets go here",
 "Saved messages go in this directory, by default",
 "Full paths to the address book, tagline and color specification files",
    0, 0,
 "Color or monochrome? (Mono mode uses the default colors)",
 "Make backgrounds transparent? (Only works on some platforms)",
 "Fill background with checkerboard pattern (ACS_BOARD)?",
 "Decompression commands (must include an option to junk/discard paths!)",
    0, 0, 0, 0, 0,
 "Compression commands (must include an option to junk/discard paths!)",
    0, 0, 0, 0, 0,
 "Default sort for packet list: by Name or Time (most recent first)",
 "Default mode for area list: All, Subscribed, or Active",
 "Default sort for letter list: by Subject, Number, From or To",
 "Default mode for letter list: All or Unread",
 "Clock in letter window: Off, Time (of day), or Elapsed (since startup)",
 "Console character set: CP437 (IBM PC) or Latin-1 (ISO-8859-1)",
 "Prompt to add taglines to replies?",
 "Save replies after editing without prompting?",
 "Strip \"soft carriage returns\" (char 141) from messages?",
 "Beep when a personal message is opened in the letter window?",
 "Use Lynx-like navigation (right arrow selects, left backs out)?",
 "Add \"Re: \" prefix on Subject of replies? (Note that it will be added\n"
 "# in Internet email and Usenet areas regardless of this setting.)",
 "Wrap quoted text at this column width (including quote marks)",
 "Maximum lines per part for reply split (see docs)",
 "8-bit character set for SOUP packets (see docs)",
 "Quoted-printable options for outgoing messages (see docs)",
    0, 0, 0,
 "Suppress help messages (use more of the screen for content)",
 "For QWK only: Generate indexes from MESSAGES.DAT instead of *.NDX",
 "Allow use of the mouse?"
#ifdef USE_SPAWNO
 , "Attempt to swap NoCarrierMail out of conventional memory when shelling"
#endif
};

const int resource::startUp[startUpLen] =
{
    UserName, InetAddr, QuoteHead, InetQuote, mmHomeDir, TempDir,
    sigFile, editor, viewer, dateFormat, PacketDir, ReplyDir, SaveDir,
    AddressFile,
    TaglineFile, ColorFile, UseColors, Transparency, BackFill,
    arjUncompressCommand, zipUncompressCommand, lhaUncompressCommand,
    rarUncompressCommand, tarUncompressCommand, unknownUncompressCommand,
    arjCompressCommand, zipCompressCommand, lhaCompressCommand,
    rarCompressCommand, tarCompressCommand, unknownCompressCommand,
    PacketSort, AreaMode, LetterSort, LetterMode, ClockMode, Charset,
    UseTaglines, AutoSaveReplies, StripSoftCR, BeepOnPers, UseLynxNav,
    ReOnReplies, QuoteWrapCols, MaxLines, outCharset, UseQPMailHead,
    UseQPNewsHead, UseQPMail, UseQPNews, ExpertMode, IgnoreNDX, Mouse
#ifdef USE_SPAWNO
    , swapOut
#endif
};

const int resource::defInt[] =
{
    1,  // PacketSort == by time
    1,  // AreaMode == subscribed
    0,  // LetterSort == by subject
    1,  // LetterMode == unread
#ifdef DOSCHARS
    0,  // Charset == CP437
#else
    1,  // Charset == Latin-1
#endif
    1,  // UseTaglines == Yes
    1,  // AutoSaveReplies == Yes
    0,  // StripSoftCR == No
    0,  // BeepOnPers == No
    1,  // UseLynxNav == Yes
    1,  // ReOnReplies == Yes
    78, // QuoteWrapCols
    0,  // MaxLines == disabled
    1,  // UseQPMailHead == Yes
    1,  // UseQPNewsHead == Yes
    1,  // UseQPMail == Yes
    0,  // UseQPNews == No
    0,  // ExpertMode == No
    0,  // IgnoreNDX = No
    1,  // Mouse = Yes
#ifdef USE_SPAWNO
    1,  // swapOut == Yes
#endif
    1,  // UseColors == Yes
    0,  // Transparency == No
    1,  // BackFill == Yes
    1   // ClockMode == Time
};

resource::resource()
{
    names = rc_names;
    intro = rc_intro;
    comments = rc_comments;
    configItemNum = startUpLen;

    int c;
    for (c = 0; c < noOfStrings; c++)
        resourceData[c] = 0;
    for (c = noOfStrings; c < noOfResources; c++) {
        int d = c - noOfStrings;
        resourceInt[d] = defInt[d];
    }
    set(outCharset, "iso-8859-1");

    initinit();
    homeInit();
    mmHomeInit();

    char *configFileName = fullpath(resourceData[homeDir], RCNAME);

    if (parseConfig(configFileName)) {
        bool created = updateConfig(configFileName);

        printf("\nWelcome to " MM_NAME " v" MM_VERNUM "!\n\n");

        if (created)
            printf("A new " RCNAME " has been written, holding the default "
                   "values. If you continue\nnow, " MM_NAME " will use "
                   "those. To edit it first, say 'Y' at the prompt.\n\n");
        else
            printf("Your " RCNAME " has been updated in place. Keywords it "
                   "did not have were\nadded with their default values, and "
                   "anything this version no longer uses\nwas commented out. "
                   "Everything else was left as you had it. To look it\nover "
                   "first, say 'Y' at the prompt.\n\n");

        printf("Edit " RCNAME " now? (y/n) ");

        char inp = fgetc(stdin);

        if (toupper(inp) == 'Y') {
            mysystem2(resourceData[editor], configFileName);
            parseConfig(configFileName);
        }
    }

    delete[] configFileName;

    if (!verifyPaths())
        fatalError("Unable to access data directories");

    resourceData[BaseDir] = mytmpdir(resourceData[TempDir]);
    bool tmpok = checkPath(resourceData[BaseDir], false);
    if (!tmpok)
        fatalError("Unable to create temp directory");
    subPath(WorkDir, "work");
    subPath(UpWorkDir, "upwork");
}

resource::~resource()
{
    clearDirectory(resourceData[WorkDir]);
    clearDirectory(resourceData[UpWorkDir]);
    mychdir(resourceData[BaseDir]);
    myrmdir(resourceData[WorkDir]);
    myrmdir(resourceData[UpWorkDir]);
    clearDirectory(resourceData[BaseDir]);
    mychdir(resourceData[TempDir]);
    myrmdir(resourceData[BaseDir]);
    for (int c = 0; c < noOfStrings; c++)
        delete[] resourceData[c];
}

bool resource::checkPath(const char *onepath, bool show)
{
    if (mychdir(onepath)) {
        if (show)
            printf("Creating %s...\n", onepath);
        if (mymkdir(onepath))
            return false;
    }
    return true;
}

bool resource::verifyPaths()
{
    if (checkPath(resourceData[mmHomeDir], true))
        if (checkPath(resourceData[PacketDir], true))
            if (checkPath(resourceData[ReplyDir], true))
                if (checkPath(resourceData[SaveDir], true))
                    return true;
    return false;
}

void resource::processOne(int c, const char *resValue)
{
    if (*resValue) {
        c = startUp[c];
        if (c < noOfStrings) {
            // Canonized for the benefit of the Windows version:
            set_noalloc(c, (c >= noOfRaw) ? canonize(fixPath(resValue)) :
                        strdupplus(resValue));
            if (mmHomeDir == c)
                mmHomeInit();
        } else {
            int x = 0;
            char r = toupper(*resValue);

            switch (c) {
            case PacketSort:
                x = (r == 'T');
                break;
            case AreaMode:
                x = (r == 'S');
                if (!x) {
                    r = toupper(resValue[1]);
                    if (r == 'C')
                        x = 2;
                }
                break;
            case LetterSort:
                switch (r) {
                case 'N':
                    x = 1;
                    break;
                case 'F':
                    x = 2;
                    break;
                case 'T':
                    x = 3;
                }
                break;
            case LetterMode:
                x = (r == 'U');
                break;
            case ClockMode:
                switch (r) {
                case 'O':
                    x = 0;
                    break;
                case 'T':
                    x = 1;
                    break;
                case 'E':
                    x = 2;
                }
                break;
            case Charset:
                x = (r == 'L');
                break;
            case QuoteWrapCols:
            case MaxLines:
                sscanf(resValue, "%d", &x);
                break;
            default:
                x = (r == 'Y');
            }

            set(c, x);
        }
    }
}

const char *resource::configLineOut(int x)
{
    static const char *pktopt[] = {"Name", "Time"},
        *areaopt[] = {"All", "Subscribed", "Active"},
        *lttopt1[] = {"Subject", "Number", "From", "To"},
        *lttopt2[] = {"All", "Unread"},
        *clockopt[] = {"Off", "Time", "Elapsed"},
        *charopt[] = {"CP437", "Latin-1"},
        *stdopt[] = {"No", "Yes"};

    x = startUp[x];

    if ((x == MaxLines) || (x == QuoteWrapCols)) {
        static char value[8];
        sprintf(value, "%d", getInt(x));
        return value;
    } else
        return (x < noOfStrings) ? get(x) :
               ((x == PacketSort) ? pktopt :
               ((x == AreaMode) ? areaopt :
               ((x == LetterSort) ? lttopt1 :
               ((x == LetterMode) ? lttopt2 :
               ((x == ClockMode) ? clockopt :
               ((x == Charset) ? charopt :
               stdopt))))))[getInt(x)];
}

const char *resource::get(int ID) const
{
    if (ID >= noOfStrings)
        fatalError("String resource out of range");
    return resourceData[ID];
}

int resource::getInt(int ID) const
{
    if (ID < noOfStrings)
        fatalError("Integer resource out of range");
    ID -= noOfStrings;
    return resourceInt[ID];
}

void resource::set(int ID, const char *newValue)
{
    if (ID >= noOfStrings)
        fatalError("String resource out of range");
    delete[] resourceData[ID];
    resourceData[ID] = strdupplus(newValue);
}

void resource::set_noalloc(int ID, char *newValue)
{
    if (ID >= noOfStrings)
        fatalError("String resource out of range");
    delete[] resourceData[ID];
    resourceData[ID] = newValue;
}

void resource::set(int ID, int newValue)
{
    if (ID < noOfStrings)
        fatalError("Integer resource out of range");
    ID -= noOfStrings;
    resourceInt[ID] = newValue;
}

// --------------------------------------------------------------------
// The resource initializer functions
// --------------------------------------------------------------------

void resource::homeInit()
{
    bool usingHOME = false;

    const char *envhome = getenv("NCMAIL");
    if (!envhome) {
        envhome = getenv("HOME");
        if (envhome)
            usingHOME = true;
        else
            envhome = error.getOrigDir();
    }

    set_noalloc(homeDir, canonize(fixPath(envhome)));

    if (usingHOME)
        set_noalloc(mmHomeDir, canonize(fullpath(resourceData[homeDir],
                    "ncmail")));
    else
        set(mmHomeDir, resourceData[homeDir]);
}

void resource::mmEachInit(int index, const char *dirname)
{
    set_noalloc(index, canonize(fullpath(resourceData[mmHomeDir], dirname)));
}

void resource::subPath(int index, const char *dirname)
{
    char *tmp = fullpath(resourceData[BaseDir], dirname);
    set_noalloc(index, tmp);
    if (!checkPath(tmp, 0))
        fatalError("tmp Dir could not be created");
}

void resource::initinit()
{
    set(arjUncompressCommand, DEFUNARJ);
    set(zipUncompressCommand, DEFUNZIP);
    set(lhaUncompressCommand, DEFUNLHA);
    set(rarUncompressCommand, DEFUNRAR);
    set(tarUncompressCommand, DEFUNTAR);
    set(unknownUncompressCommand, DEFUNNONE);
    set(arjCompressCommand, DEFARJ);
    set(zipCompressCommand, DEFZIP);
    set(lhaCompressCommand, DEFLHA);
    set(rarCompressCommand, DEFRAR);
    set(tarCompressCommand, DEFTAR);
    set(unknownCompressCommand, DEFNONE);

    set(UncompressCommand, DEFUNZIP);
    set(CompressCommand, DEFZIP);

    set(sigFile, "");
    set(UserName, "");
    set(InetAddr, "");
    set(QuoteHead, "-=> %f wrote to %t <=-");
    set(InetQuote, "On %d, %f wrote:");

    char *p = getenv("EDITOR");
    set(editor, (p ? p : DEFEDIT));

    p = getenv("PAGER");
    set(viewer, (p ? p : DEFVIEWER));

    set(dateFormat, "%x %H:%M");
}

void resource::mmHomeInit()
{
    set(TempDir, resourceData[mmHomeDir]);

    mmEachInit(PacketDir, "down");
    mmEachInit(ReplyDir, "up");
    mmEachInit(SaveDir, "save");
    mmEachInit(AddressFile, ADDRBOOK);
    mmEachInit(TaglineFile, "taglines");
    mmEachInit(ColorFile, "colors");
}
