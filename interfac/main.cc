/*
 * NoCarrierMail offline mail reader
 * main, error

 Copyright 1996-1997 Kolossvary Tamas <thomas@vma.bme.hu>
 Copyright 1997-2021 William McBrine <wmcbrine@gmail.com>
 Distributed under the GNU General Public License, version 3 or later. */

#include "error.h"
#include "interfac.h"

#include <locale.h>
#ifdef HAS_LANGINFO
# include <langinfo.h>
# include <string.h>
#endif

const chtype *ColorArray = 0;
time_t starttime;
ErrorType error;
mmail mm;
Interface ui;

#ifdef USE_MOUSE
MEVENT mm_mouse_event;
#endif

ErrorType::ErrorType()
{
    starttime = time(0);
    srand((unsigned) starttime);

    origdir = mygetcwd();
}

ErrorType::~ErrorType()
{
    mychdir(origdir);
    delete[] origdir;
}

const char *ErrorType::getOrigDir()
{
    return origdir;
}

#if defined(SIGWINCH) && !defined(PDCURSES) && !defined(NCURSES_SIGWINCH)
extern "C" void sigwinchHandler(int sig)
{
    if (sig == SIGWINCH)
        ungetch(KEY_RESIZE);
    signal(SIGWINCH, sigwinchHandler);
}
#endif

void fatalError(const char *description)
{
    if (ui.on && !isendwin())
        ui.close();
    fprintf(stderr, "\n\n%s\n\n", description);
    exit(EXIT_FAILURE);
}

void pauseError(const char *description)
{
    fprintf(stderr, "\n\n%s\n\n", description);
    napms(2000);
}

#ifdef USE_MOUSE
void mm_mouse_get()
{
# ifdef NCURSES_MOUSE_VERSION
    getmouse(&mm_mouse_event);
# else
    nc_getmouse(&mm_mouse_event);
# endif
}
#endif

/* Handle an option given on its own, which the loop in main() cannot: that one
   reads "-keyword value" pairs, so a flag with no value after it would be taken
   for a packet name. Plain strcmp rather than getopt, which the DOS and Watcom
   toolchains do not have -- and the option set here is "any keyword from the
   configuration file", which no fixed option table can describe anyway.

   Returns true if the program should stop after this. */

static bool loneOption(const char *arg)
{
    const char *opt = arg + 1;

    if ('-' == *opt)
        opt++;

    if (!strcasecmp(opt, "version") || !strcasecmp(opt, "v")) {
        printf(MM_NAME " " MM_VERNUM "\n");
#if defined(NCURSES_VERSION) || defined(PDCURSES)
        printf("curses: %s\n", curses_version());
#endif
#ifdef MM_UTF8_OUT
        printf("wide character output: available");
#else
        printf("wide character output: not available");
#endif
        printf(", this terminal is %s\n",
               utf8Console ? "UTF-8" : "single byte");

        return true;
    }

    if (!strcasecmp(opt, "help") || !strcasecmp(opt, "h") ||
        !strcmp(opt, "?")) {
        printf("Usage: ncmail [-keyword value ...] [packet or directory ...]\n"
               "\n"
               "Any keyword from the configuration file may be given as an\n"
               "option, for example: -PacketDir /path/to/packets\n"
               "\n"
               "  -h, --help     show this message and exit\n"
               "  -V, --version  show version information and exit\n"
               "\n"
               "See the ncmail(1) man page for the keywords and for the keys\n"
               "used while reading mail.\n");

        return true;
    }

    return false;
}

int main(int argc, char **argv)
{
    setlocale(LC_ALL, "");

#ifdef HAS_LANGINFO
    utf8Console = !strcmp(nl_langinfo(CODESET), "UTF-8");
#else
    utf8Console = false;
#endif

    if ((2 == argc) && ('-' == argv[1][0]) && loneOption(argv[1]))
        return EXIT_SUCCESS;

    while ((argc > 2) && ('-' == argv[1][0])) {
        char *resName = argv[1] + 1;
        char *resValue = argv[2];

        if ('-' == *resName)
            resName++;

        mm.res.processOneByName(resName, resValue);

        argv += 2;
        argc -= 2;
    }

    ui.init();
    if (argc > 1)
        for (int i = 1; (i < argc) &&
            ui.fromCommandLine(argv[i]); i++);
    else
        ui.main();
    ui.close();

    return EXIT_SUCCESS;
}
