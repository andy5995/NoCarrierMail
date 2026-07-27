# NoCarrierMail

[![Linux](https://github.com/andy5995/NoCarrierMail/actions/workflows/linux.yml/badge.svg)](https://github.com/andy5995/NoCarrierMail/actions/workflows/linux.yml)
[![macOS](https://github.com/andy5995/NoCarrierMail/actions/workflows/macos.yml/badge.svg)](https://github.com/andy5995/NoCarrierMail/actions/workflows/macos.yml)
[![BSD](https://github.com/andy5995/NoCarrierMail/actions/workflows/bsd.yml/badge.svg)](https://github.com/andy5995/NoCarrierMail/actions/workflows/bsd.yml)
[![MSVC](https://github.com/andy5995/NoCarrierMail/actions/workflows/msvc.yml/badge.svg)](https://github.com/andy5995/NoCarrierMail/actions/workflows/msvc.yml)
[![MSYS2](https://github.com/andy5995/NoCarrierMail/actions/workflows/msys2.yml/badge.svg)](https://github.com/andy5995/NoCarrierMail/actions/workflows/msys2.yml)
[![DOS](https://github.com/andy5995/NoCarrierMail/actions/workflows/dos.yml/badge.svg)](https://github.com/andy5995/NoCarrierMail/actions/workflows/dos.yml)

NoCarrierMail is an offline mail packet reader with a curses-based interface,
for Unix / Linux, macOS, Windows, and other systems. It reads and replies
to messages in the Blue Wave, QWK, OMEN, SOUP, and OPX formats.

> NoCarrierMail is a fork of MultiMail, maintained by [andy5995][maintainer].
> MultiMail was written by Kolossvary Tamas and Toth Istvan and maintained for
> many years by [William McBrine][wmcbrine]; see [Credits](#credits). This fork
> adds a Meson build, a unit-test suite, and a number of fixes and features on
> top of the 0.52 base.

NoCarrierMail is free software, distributed under the [GNU General Public
License][gpl], version 3 or later.

Downloads, help, and where to take part:

- [Source code][github] -- the project on GitHub
- [Latest release][releases] -- prebuilt binaries for the newest version
- [Snapshot builds][snapshot] -- built from the latest code, for testing
- [Discussions][discussions] -- questions and general talk
- [Issue tracker][issues] -- bugs and feature requests


## Features

Additions in this fork (see [HISTORY.md] for the full list):

* **Meson build system** for Unix/Linux, macOS, and Windows (MSYS2 with
  ncurses, MSVC with PDCurses), with per-platform CI.
* **Full-length QWK headers** via Synchronet's HEADERS.DAT -- both read
  (To/From/Subject beyond the 25-character MESSAGES.DAT limit) and written
  back into reply packets.
* **Message voting** via Synchronet's VOTING.DAT -- see each message's
  up/down vote count, read polls and their results as messages, and cast
  your own vote with `W`.
* **UTF-8 terminals** are drawn through the wide curses API, so the IBM
  line and block characters in ANSI art and message text appear as
  themselves instead of as raw bytes.
* **Configurable date format** in the reading view (the `DateFormat`
  keyword, a strftime string; defaults to your locale).
* **External viewer** -- the `L` key opens the current message in a pager
  or editor of your choice (the `Viewer` keyword), so links can be clicked
  and text selected.
* A **unit-test suite** (`meson test`), also run under
  AddressSanitizer/UBSan, plus several memory-safety fixes.


## Building

You'll need a C++ compiler, Meson, Ninja, and a curses library (ncurses,
SysV curses, or PDCurses 3.1+, 3.6+ recommended). From the top of the
source tree:

    meson setup builddir
    meson compile -C builddir

The binary is `builddir/ncmail`. To run the tests:

    meson test -C builddir

To install under the prefix (default `/usr/local`):

    meson install -C builddir

See [INSTALL.md] for build options, the 16-bit DOS/OS2 makefiles, and
PDCurses/XCurses notes. At run time you'll also need an archiver such as
InfoZip to unpack packets and pack replies.


## Installing on macOS

This repository is also a Homebrew tap:

    brew tap andy5995/nocarriermail https://github.com/andy5995/NoCarrierMail
    brew install ncmail

Releases also carry a prebuilt tarball for Apple Silicon; see
`HomebrewFormula/README.md` for the details of the formula.


## Usage

See the [man page][MANUAL] for usage, and edit `~/.ncmailrc` (or `ncmail.rc`
on Windows) to configure the reader. Example color schemes are in the
`colors` directory; select one with the `ColorFile` keyword.

If you used MultiMail before, you can keep your old settings. Copy your
`~/.mmailrc` file to `~/.ncmailrc` (on Windows, copy `mmail.rc` to
`ncmail.rc`). New users do not need to do this.


## Contributing

Bug reports, suggestions, and pull requests are welcome. See the links at
the top of this file.


## Credits

MultiMail was originally developed under Linux by Kolossvary Tamas and
Toth Istvan. John Zero maintained versions 0.2 through 0.6; from version
0.7, [William McBrine][wmcbrine] was the maintainer, and his releases are
the basis for this fork.

Additional code has been contributed by Peter Krefting, Mark D. Rejhon,
Ingo Brueckl, Robert Vukovic, and Frederic Cambus. The QWK HEADERS.DAT
work in this fork was done with input from Rob Swindell (Synchronet).

Earlier bug reports and suggestions are noted in the [HISTORY.md] file.


[gpl]: LEGAL.md
[HISTORY.md]: HISTORY.md
[INSTALL.md]: INSTALL.md
[MANUAL]: MANUAL.md

[maintainer]: https://github.com/andy5995
[github]: https://github.com/andy5995/NoCarrierMail
[issues]: https://github.com/andy5995/NoCarrierMail/issues
[discussions]: https://github.com/andy5995/NoCarrierMail/discussions
[releases]: https://github.com/andy5995/NoCarrierMail/releases/latest
[snapshot]: https://github.com/andy5995/NoCarrierMail/releases/tag/snapshot
[wmcbrine]: https://wmcbrine.com/MultiMail/
