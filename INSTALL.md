NoCarrierMail compilation and installation procedure
================================================

These instructions assume that you're compiling NoCarrierMail from source. For
precompiled binaries, see the README files that accompany them instead.

On Unix, Linux, macOS, and Windows (via MSYS2 or MSVC), NoCarrierMail builds with
the Meson build system. For the 16-bit MS-DOS and OS/2 ports, see "Other
compilers and platforms" near the end of this file.

1. Make sure the needed packages are installed --
    To build NoCarrierMail you'll need a C++ compiler, Meson and Ninja, and a
    curses library -- either ncurses, SysV curses (e.g., Solaris curses), or
    PDCurses. You can get these from:

        Meson:    <https://mesonbuild.com/>
        ncurses:  <https://invisible-island.net/ncurses/>
        PDCurses: <https://pdcurses.org/>

    If using PDCurses, NoCarrierMail requires version 3.6 or later. (On Linux, you
    most likely already have ncurses, Meson, and Ninja in your package manager.)

    At run time, you'll also need InfoZip or PKZIP (and/or LHA, ARJ, etc.) to
    uncompress the packets and compress the replies. InfoZip is available from:

        <http://infozip.sf.net/>

    (PKZIP is the default for DOS; InfoZip is the default for other platforms.)
    These programs should be in the PATH; otherwise, the full path must be
    specified in ~/.ncmailrc.

2. Configure and build --
    From the top of the source tree:

        meson setup builddir
        meson compile -C builddir

    Meson finds curses automatically (via pkg-config, with a fallback search).
    The resulting binary is builddir/ncmail .

    Build options (pass to "meson setup", or change later with
    "meson configure builddir"):

        -Dtests=false         Don't build the unit tests
        -Dvanity_plate=false  Omit the author info on the packet list
        -Dshadows=false       Don't draw shadowed windows
        -Dtar_kludge=false    Don't rearchive the whole file for tar/gz replies
        --buildtype=release   Optimized build (the default is a debug build)

3. Run it --
    Type: `./builddir/ncmail`
    (For Windows, set the NCMAIL or HOME variable first, then run ncmail.)

4. (Optional:) Run the tests --
    Type: `meson test -C builddir`

5. (Optional:) Install it system-wide --
    Type: `meson install -C builddir`
    to install the binary and man page under the prefix (default /usr/local,
    which requires root). Pick a different location with, e.g.,
    "meson setup builddir --prefix ~/.local".

6. (Optional:) Configure it (for the end user) --
    Edit the ~/.ncmailrc file. (For Windows, ncmail.rc.)

This package includes some example color schemes, in the "colors" directory.
To select one, use the "ColorFile" keyword in .ncmailrc .


Support for XCurses (PDCurses)
------------------------------

When NoCarrierMail is compiled with XCurses, you can use the X resource database
to set certain startup options. Here are some example resources:

    XCurses*normalFont: 9x15
    XCurses*boldFont:   9x15bold
    XCurses*lines:      30
    XCurses*cols:       80

For details, see the PDCurses documentation.

If you're using a non-X text editor with an XCurses version of NoCarrierMail, it
will work better if you set NoCarrierMail's editor variable to "xterm -e $EDITOR"
instead of just "$EDITOR" (the default).


Other compilers and platforms
------------------------------

Meson drives the modern builds: Unix/Linux and macOS with ncurses, and Windows
with either MSYS2 (ncurses) or Microsoft Visual C++ (PDCurses). The continuous-
integration workflows under .github/workflows/ show a working setup for each.

The 16-bit MS-DOS and OS/2 ports are not built with Meson; they keep their own
hand-maintained makefiles, which link against PDCurses:

    Makefile.bcc - Borland/Turbo C++ (MS-DOS, Windows)
    Makefile.wcc - Watcom (use "wmake")

Point these at your PDCurses installation, e.g.:

    make -f Makefile.bcc CURS_DIR=/pdcurs38 SYS=DOS

The 16-bit MS-DOS Turbo C++ port also uses Ralf Brown's SPAWNO library:

    <https://www.cs.cmu.edu/afs/cs.cmu.edu/user/ralf/pub/WWW/files.html>

See the [man page](MANUAL.md) and [README.md] for more information.
