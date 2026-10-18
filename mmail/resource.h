/*
 * NoCarrierMail offline mail reader
 * resource class

 Copyright 1996-1997 Toth Istvan <stoty@vma.bme.hu>
 Copyright 1997-2017 William McBrine <wmcbrine@gmail.com>
 Distributed under the GNU General Public License, version 3 or later. */

#ifndef RESOURCE_H
#define RESOURCE_H

enum {
    UserName, InetAddr, QuoteHead, InetQuote, outCharset, noOfRaw
};

enum {
    homeDir = noOfRaw, mmHomeDir, PacketDir, TempDir, BaseDir, WorkDir,
    UncompressCommand, PacketName, ReplyDir, CompressCommand, UpWorkDir,
    editor, viewer, dateFormat, SaveDir, AddressFile, TaglineFile,
    arjUncompressCommand,
    zipUncompressCommand, lhaUncompressCommand, rarUncompressCommand,
    tarUncompressCommand, unknownUncompressCommand, arjCompressCommand,
    zipCompressCommand, lhaCompressCommand, rarCompressCommand,
    tarCompressCommand, unknownCompressCommand, sigFile, ColorFile,
    oldPacketName, noOfStrings
};

enum {
    PacketSort = noOfStrings, AreaMode, LetterSort, LetterMode,
    Charset, UseTaglines, AutoSaveReplies, StripSoftCR, BeepOnPers,
    UseLynxNav, ReOnReplies, QuoteWrapCols, MaxLines, UseQPMailHead,
    UseQPNewsHead, UseQPMail, UseQPNews, ExpertMode, IgnoreNDX, Mouse,
#ifdef USE_SPAWNO
    swapOut,
#endif
    UseColors, Transparency, BackFill, ClockMode, noOfResources
};

class baseconfig
{
 protected:
    const char **names, **comments, **intro;
    int configItemNum;

    bool *seen;             //keyword was present in the file we parsed
    bool unknownKeyword;    //file had a keyword this version does not know
    bool fileFound;         //the last parseConfig() could open its file
    bool quiet;             //don't print: curses has the screen
    const baseconfig *old;  //unknown keywords this one had are not new
    char unknownName[32];   //the first new unknown keyword
    char unknownSeen[256];  //every unknown keyword, each followed by \n

    bool hadUnknown(const char *) const;

    int findKeyword(const char *, int) const;
    bool parseConfig(const char *);
    bool updateConfig(const char *);
    void newConfig(const char *);
    virtual void processOne(int, const char *) = 0;
    virtual const char *configLineOut(int) = 0;
 public:
    baseconfig();
    void processOneByName(const char *, const char *);
    virtual ~baseconfig();
};

// The settings file, in the home directory (homeDir).
#ifdef DOSNAMES
# define RCNAME "ncmail.rc"
#else
# define RCNAME ".ncmailrc"
#endif

class resource : public baseconfig
{
    static const char *rc_names[], *rc_intro[], *rc_comments[];
    static const int startUp[], defInt[];

    char *resourceData[noOfStrings];
    int resourceInt[(int)noOfResources - (int)noOfStrings];
    bool pathsReady;
    bool edited[noOfResources];     // changed by the last editConfig()

    void homeInit();
    void mmEachInit(int, const char *);
    void subPath(int, const char *);
    void initinit();
    void mmHomeInit();
    void processOne(int, const char *);
    const char *configLineOut(int);
    bool checkPath(const char *, bool);
    bool verifyPaths();
    void runEditor(const char *);
    void askEditor();
    bool sameAs(const resource &, int) const;
 public:
    resource();
    ~resource();
    void init();
    int editConfig(const char *&);
    bool wasEdited(int) const;
    const char *editUnknown() const;
    char *configName() const;
    const char *get(int) const;
    int getInt(int) const;
    void set(int, const char *);
    void set_noalloc(int, char *);
    void set(int, int);
};

#endif
