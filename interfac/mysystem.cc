/*
 * NoCarrierMail offline mail reader
 * some low-level routines common to both sides

 Copyright 1997-2021 William McBrine <wmcbrine@gmail.com>
 Distributed under the GNU General Public License, version 3 or later. */

/* Most non-ANSI, non-curses stuff is here. */

#include "interfac.h"
#include "error.h"

extern "C" {
#if defined(__WATCOMC__) || defined(_MSC_VER)
# include <direct.h>
#else
# ifndef USE_FINDFIRST
#  include <dirent.h>
# endif
#endif

#ifdef USE_SPAWNO
# include <spawno.h>
#endif

#ifdef LIMIT_MEM
# ifdef __WATCOMC__
#  include <malloc.h>
#  include <dos.h>
# else
#  include <alloc.h>
# endif
#endif

#ifdef HAS_UNISTD
# include <unistd.h>
#endif

#ifdef USE_DIRH
# include <dir.h>
#endif

#include <sys/stat.h>

#ifdef HAS_UNAME
# include <sys/utsname.h>
#endif

#ifdef USE_SETFTIME
# include <fcntl.h>
#else
# if defined(__WATCOMC__) || defined(_MSC_VER)
#  include <sys/utime.h>
# else
#  include <utime.h>
# endif
#endif

#if defined(USE_IOH) || defined(USE_SETFTIME)
# include <io.h>
#endif

#if defined(__WATCOMC__) || defined(TURBO16)
# include <dos.h>
#endif

#if defined(USE_FINDFIRST) && defined(USE_IOH)
# include <stdint.h>
#endif
}

// Outside the extern "C" block: the Windows headers carry C++ of their own.
#ifdef HAS_CONSOLE_CP
# define WIN32_LEAN_AND_MEAN
# include <windows.h>
#endif

#ifndef S_IREAD
# define S_IREAD S_IRUSR
#endif

#ifndef S_IWRITE
# define S_IWRITE S_IWUSR
#endif

#ifndef USE_FINDFIRST
static DIR *Dir;
#endif

void fatalError(const char *description);

char *myfgets(char *s, int size, FILE *stream)
{
    char *end = 0;

    if (!feof(stream) && fgets(s, size, stream)) {

        end = s + strlen(s) - 1;

        // Skip any leftovers:
        if (*end != '\n')
            while (!feof(stream) && (fgetc(stream) != '\n'));

    }
    return end;
}

int mysystem(const char *cmd)
{
    if (ui.on && !isendwin())
        endwin();

#ifdef USE_SPAWNO
    int result = mm.res.getInt(swapOut) ?
                 systemo(mm.res.get(BaseDir), cmd) : -1;

    if (-1 == result)
        result = system(cmd);
#else
    int result = system(cmd);
#endif

    // Non-zero result = error; pause so it can (maybe) be read
    if (result)
        napms(2000);

    if (ui.on) {
#ifdef PDCURSES
        PDC_set_title(MM_NAME);
#endif
        keypad(stdscr, TRUE);
    }

    return result;
}

int mysystem2(const char *cmd, const char *args)
{
    size_t lencmd = strlen(cmd);
    char *qargs = canonize(quotespace(args));
    char *cmdline = new char[lencmd + strlen(qargs) + 2];

    sprintf(cmdline, "%s %s", cmd, qargs);

    int result = mysystem(cmdline);

    delete[] cmdline;
    delete[] qargs;
    return result;
}

char *mytmpdir(const char *home)
{
    mystat st;
    char name[9];

    if (mychdir(home))
        fatalError("Could not change to temp dir");

    do
        sprintf(name, "work%04x", (rand() & 0xffff));
    while (st.init(name));

    return canonize(fullpath(home, name));
}

char *mytmpnam()
{
    static long tcount = 1;
    char name[13];

    if (tcount > 99999L)
        fatalError("Out of temporary filenames");

    sprintf(name, "tmp%05ld.txt", tcount++);

    return canonize(fullpath(mm.res.get(BaseDir), name));
}

void edit(const char *reply_filename)
{
    mysystem2(mm.res.get(editor), reply_filename);
}

#ifdef __WATCOMC__
void setdisk(int drive)
{
    unsigned total;  /* We don't care, but we have to feed it. */

    _dos_setdrive((unsigned) ++drive, &total);
}
#endif

int mychdir(const char *pathname)
{
#ifdef USE_SETDISK
    if (':' == pathname[1])
        setdisk(toupper(pathname[0]) - 'A');
#endif
    return chdir(pathname);
}

int mymkdir(const char *pathname)
{
#ifdef HAS_UNISTD
    return mkdir(pathname, S_IRWXU);
#else
    return mkdir(pathname);
#endif
}

void myrmdir(const char *pathname)
{
    rmdir(pathname);
}

char *mygetcwd()
{
    char pathname[256], *result;
    result = getcwd(pathname, 255);
    return strdupplus(result ? pathname : ".");
}

#ifdef HAS_APPDATA
/* Where per-user files go on Windows, which sets no HOME: %APPDATA%\\ncmail,
   created if it is not there yet. Returns 0 if APPDATA is unset.

   The shell folder API would answer this too -- canfigger uses
   SHGetFolderPath(CSIDL_APPDATA) -- but that needs shell32 adding to three of
   this project's build files, including two Win32 targets nothing builds, and
   the shell sets APPDATA from the same place for a program a user launched. */

const char *appDataHome()
{
    static char path[512];

    if (!path[0]) {
        const char *base = getenv("APPDATA");

        if (!base || !*base)
            return 0;

        size_t len = strlen(base);

        if ((len + sizeof "\\ncmail") > sizeof path)
            return 0;

        strcpy(path, base);

        if (path[len - 1] != '\\')
            path[len++] = '\\';

        strcpy(path + len, "ncmail");

        /* Created here rather than in verifyPaths(): the configuration file is
           written before that runs, so on a first run there would be no
           directory to write it into. */

        mymkdir(path);
    }

    return path;
}
#endif

#ifdef HAS_CONSOLE_CP
// The user's own code pages, and the one we set; 0 until we change anything.
static UINT userOutCP, userInCP, ourCP;

/* The console draws our single-byte output through its own code page, so one
   left at 1252 shows CP437 box drawing as accented letters. */

bool winConsoleIsUTF8()
{
    return 65001 == GetConsoleOutputCP();
}

static BOOL WINAPI winConsoleBreak(DWORD)
{
    winConsoleRestore();
    return FALSE;       // let the default handling end the process
}

/* Match the console to what we send it and read from it: CP437, or Latin-1
   when the Charset setting (or the C key) says the console is Latin-1. A
   UTF-8 console is left alone, since the wide drawing path needs no code
   page. Input is set too, so typed characters arrive in the page we draw
   them in.

   1252 stands in for Latin-1. Every console has it, and the two differ only
   at 0x80-0x9F: our CP437-to-Latin-1 table never produces those, while a
   letter already marked Latin-1 passes through untouched -- and text with
   bytes there is usually 1252 in practice, which 28591 would show as
   control codes. */

void winConsoleSetCP(bool latin1)
{
    if (utf8Console)
        return;

    if (!ourCP) {
        userOutCP = GetConsoleOutputCP();
        userInCP = GetConsoleCP();
        SetConsoleCtrlHandler(winConsoleBreak, TRUE);
    }

    ourCP = latin1 ? 1252 : 437;
    winConsoleResume();
}

// Put the user's pages back: on exit, and around the command shell.
void winConsoleRestore()
{
    if (ourCP) {
        SetConsoleOutputCP(userOutCP);
        SetConsoleCP(userInCP);
    }
}

void winConsoleResume()
{
    if (ourCP) {
        SetConsoleOutputCP(ourCP);
        SetConsoleCP(ourCP);
    }
}
#endif

// system name -- results of uname()
const char *sysname()
{
#if defined(__WIN32__)
    return "Win";
#elif defined(__APPLE__)
    return "Mac";
#elif defined(__MSDOS__)
# ifdef SIXTEENBIT
    return "XT";
# else
    return "DOS";
# endif
#elif defined(__OS2__)
    return "OS2";
#elif defined(HAS_UNAME)
    static struct utsname buf;

    if (!buf.sysname[0])
        uname(&buf);

    return buf.sysname;
#else
    return "?";
#endif
}

bool myopendir(const char *dirname)
{
#ifdef USE_FINDFIRST
    return !mychdir(dirname);
#else
    return ((Dir = opendir((char *) dirname)) != 0) ?
           !mychdir(dirname) : false;
#endif
}

const char *myreaddir(mystat &st)
{
#ifdef USE_FINDFIRST
# ifdef USE_IOH                         // Windows
    static intptr_t handle;
    static bool first = true;
    static struct _finddata_t blk;
    int result;

    if (first) {
        char wildcard[] = "*";
        handle = _findfirst(wildcard, &blk);
        result = (handle == -1) ? -1 : 0;
        first = false;
    } else
        result = _findnext(handle, &blk);

    if (-1 == result) {
        if (-1 != handle)
            _findclose(handle);

        first = true;
        return 0;
    } else {
        st.init((long) blk.size, blk.time_write, blk.attrib);
        return blk.name;
    }
# else                                  // DOS(ish)
    static struct ffblk blk;
    static bool first = true;
    int result;

    if (first) {
        result = findfirst("*.*", &blk, FA_DIREC);
        first = false;
    } else
        result = findnext(&blk);

    if (result) {
#  ifndef __MSDOS__
        findclose(&blk);
#  endif
        first = true;
        return 0;
    } else {
        st.init(blk.ff_fsize, ((long) blk.ff_ftime << 16) +
                (long) blk.ff_fdate, blk.ff_attrib);
        return blk.ff_name;
    }
# endif
#else                                   // POSIX
    static dirent *entry;
    const char *result = 0;

    entry = readdir(Dir);
    if (entry)
        result = entry->d_name;
    else
        closedir(Dir);

    if (result)
        st.init(result);
    return result;
#endif
}

void clearDirectory(const char *DirName)
{
    mystat st;
    const char *fname;

    if (myopendir(DirName)) {
        while ((fname = myreaddir(st)) != 0)
            if (!st.isdir())
                remove(fname);
    } else {
        char tmp[512];
        sprintf(tmp, "Could not change to %.491s", DirName);
        fatalError(tmp);
    }
}

#ifdef USE_SETFTIME
void myutime(const char *fname, time_t now)
{
    struct ftime ut;
    struct tm tmnow = *localtime(&now);

    ut.ft_tsec = tmnow.tm_sec >> 1;
    ut.ft_min = tmnow.tm_min;
    ut.ft_hour = tmnow.tm_hour;
    ut.ft_day = tmnow.tm_mday;
    ut.ft_month = tmnow.tm_mon + 1;
    ut.ft_year = tmnow.tm_year - 80;

    int f = open(fname, O_RDWR | O_BINARY);
    if (f != -1) {
        setftime(f, &ut);
        close(f);
    }
}
#endif

time_t touchFile(const char *fname)
{
    time_t now = time(0);
#ifdef USE_SETFTIME
    myutime(fname, now);
#else
    struct utimbuf ut;
    ut.actime = ut.modtime = now;
    utime((char *) fname, &ut);
#endif
    return now;
}

#ifdef LIMIT_MEM

/* Constrain memory allocation according to maximum block size and free
   memory remaining. Currently used only in the 16-bit MS-DOS port.
*/

long maxfreemem()
{
# ifdef __WATCOMC__
    /* In the large model new takes from the far heap, which grows out of
       DOS memory. _memmax() only measures the near heap, so take the
       largest free DOS block (asking for 1 MB fails and reports it) or the
       largest free far heap block, whichever is bigger. Callers want the
       biggest single allocation, not the total. */
    struct _heapinfo h;
    unsigned paras = 0;

    _dos_allocmem(0xFFFF, &paras);
    long largest = (long) paras << 4;

    h._pentry = 0;
    while (_HEAPOK == _heapwalk(&h))
        if ((_FREEENTRY == h._useflag) && ((long) h._size > largest))
            largest = h._size;

    return largest;
# else  // Turbo C++
    return (long) coreleft();
# endif
}

long limitmem(long wanted)
{
    long maxavail = maxfreemem();

    // Give it a 25% margin
    maxavail -= (wanted >> 2);

    if (wanted > maxavail)
        wanted = maxavail;

    return wanted;
}

#endif

/* Convert pathnames to "canonical" form (change slashes to backslashes).
   The "nospace" stuff leaves any parameters unconverted.
*/

char *canonize(char *sinner)
{
#ifdef DOSNAMES
    int i;
    bool nospace = true;
    bool inquotes = false;

    for (i = 0; sinner[i] && nospace; i++) {
        if ('\"' == sinner[i])
            inquotes = !inquotes;
        else
            if ('/' == sinner[i])
                sinner[i] = '\\';
            else
                if ((' ' == sinner[i]) && !inquotes)
                    nospace = false;
    }
#endif
    return sinner;
}

/* The same for a path that is shown rather than run, so every slash is
   converted, spaces or not.
*/

char *backslashes(char *path)
{
#ifdef DOSNAMES
    for (char *p = path; *p; p++)
        if ('/' == *p)
            *p = '\\';
#endif
    return path;
}

#ifdef HAS_HOME

/* Recognize '~' as a substitute for the home directory path, on Unix-like
   systems.
*/

const char *homify(const char *raw)
{
    static const char *home = getenv("HOME");

    if (home && raw && (raw[0] == '~') &&
        ((raw[1] == '/') || (raw[1] == '\0'))) {

        static char expanded[512];

        sprintf(expanded, "%.255s/%.255s", home, raw + 1);
        return expanded;
    } else
        return raw;
}

#endif

#ifdef USE_SHELL

/* Command shell routine -- currently only used in the DOSish ports */

Shell::Shell()
{
    const char *oldprompt = getenv("PROMPT");
    if (!oldprompt)
        oldprompt = "$p$g";

    size_t len = strlen(oldprompt) + 13;
    prompt = new char[len];

    sprintf(prompt, "PROMPT=%s[MM] ", oldprompt);
    putenv(prompt);
}

Shell::~Shell()
{
    delete[] prompt;
}

void Shell::out()
{
    mychdir(error.getOrigDir());
    touchwin(stdscr);
    refresh();

#ifdef HAS_CONSOLE_CP
    /* The shell gets the console as the user had it. An editor or viewer
       keeps ours, since the file it opens was written in our code page. */
    winConsoleRestore();
#endif
    mysystem(getenv("COMSPEC"));
#ifdef HAS_CONSOLE_CP
    winConsoleResume();
#endif

    ui.redraw();
}

#endif

#ifdef EXTRAPATH

/* Add the starting directory and the MMAIL directory to the PATH,
   mainly for the benefit of Windows, where InfoZip is not standard.
   (But currently this is enabled for all the DOSish ports.)
*/
ExtraPath::ExtraPath()
{
    const char *oldpath = getenv("PATH");
    if (!oldpath)
        fatalError("No PATH defined!");

    const char *orig = error.getOrigDir();
    const char *home = mm.res.get(homeDir);

    size_t len = strlen(oldpath) + strlen(orig) + strlen(home) + 8;
    newpath = new char[len];

    sprintf(newpath, "PATH=%s;%s;%s", oldpath, orig, home);
    putenv(newpath);
}

ExtraPath::~ExtraPath()
{
    delete[] newpath;
}

#endif

mystat::mystat(const char *fname)
{
    init(fname);
}

mystat::mystat()
{
    init();
}

bool mystat::init(const char *fname)
{
#ifdef USE_FINDFIRST
# ifdef USE_IOH                         // Windows
    struct _finddata_t blk;
    long result = _findfirst((char *) fname, &blk);
    bool retval = (-1 != result);

    if (retval) {
        init((long) blk.size, blk.time_write, blk.attrib);
        _findclose(result);
    } else
        init();

# else                                  // DOS(ish)
    struct ffblk blk;
    bool retval = !findfirst(fname, &blk, FA_DIREC);

    if (retval)
        init(blk.ff_fsize, ((long) blk.ff_ftime << 16) +
             (long) blk.ff_fdate, blk.ff_attrib);
    else
        init();

#  ifndef __MSDOS__
    findclose(&blk);
#  endif
# endif
#else                                   // POSIX
    struct stat fileStat;
    bool retval = !stat((char *) fname, &fileStat);

    if (retval) {
        size = fileStat.st_size;
        date = fileStat.st_mtime;
        mode = fileStat.st_mode;
    } else
        init();
#endif
    return retval;
}

#ifdef USE_FINDFIRST
# ifdef USE_IOH

void mystat::init(long sizeA, time_t dateA, unsigned attrib)
{
    size = sizeA;
    date = dateA;
    mode = S_IREAD | ((attrib & _A_RDONLY) ? 0 : S_IWRITE) |
           ((attrib & _A_SUBDIR) ? S_IFDIR : 0);
}

# else

void mystat::init(long sizeA, long dateA, char ff_attrib)
{
    size = sizeA;
    date = mktime(getdostime(dateA));
    mode = S_IREAD | ((ff_attrib & FA_RDONLY) ? 0 : S_IWRITE) |
           ((ff_attrib & FA_DIREC) ? S_IFDIR : 0);
}

# endif
#endif

void mystat::init()
{
    size = -1;
    date = (time_t) -1;
    mode = 0;
}

bool mystat::isdir()
{
    return !(!(mode & S_IFDIR));
}

bool mystat::readable()
{
    return !(!(mode & S_IREAD));
}

bool mystat::writeable()
{
    return !(!(mode & S_IWRITE));
}

off_t mystat::fsize()
{
    return size;
}

time_t mystat::fdate()
{
    return date;
}

void mystat::reset_date(const char *fname)
{
    if (date != (time_t) -1)
#ifdef USE_SETFTIME
        myutime(fname, date);
#else
    {
        struct utimbuf ut;
        ut.actime = date;     // Should be current time
        ut.modtime = date;
        utime((char *) fname, &ut);
    }
#endif
}
