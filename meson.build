project(
    'nocarriermail',
    'cpp',
    version: '0.55',
    license: 'GPL-3.0-or-later',
    meson_version: '>= 0.59.0',
    default_options: [
        'warning_level=3',
        # macOS ships a curses too old to have the wide API, so the UTF-8
        # drawing is compiled out there. Look in both Homebrew prefixes
        # (arm64 and Intel) for a "brew install ncurses" that does have it.
        # The paths are simply absent elsewhere.
        'pkg_config_path=/opt/homebrew/opt/ncurses/lib/pkgconfig,/usr/local/opt/ncurses/lib/pkgconfig',
    ],
)

cc = meson.get_compiler('cpp')
is_msvc = cc.get_id() == 'msvc'

# Portability guardrail: hold GCC/Clang to c++98 so they reject accidental
# C++11+ (the legacy 16-bit DOS/OS2 toolchains target a pre-modern subset).
# MSVC has no cpp_std below c++14, so leave it at its default there. Applied
# per target via override_options to stay meson-0.59 compatible; meson >= 1.3.0
# could instead do default_options: 'cpp_std=c++98,c++14'.
cpp98 = is_msvc ? [] : ['cpp_std=c++98']

# config.h is hand-maintained (platform/compiler detection); meson only
# flips the three feature toggles via -D guards. See config.h.
toggles = [
    ['vanity_plate', 'NO_VANITY_PLATE'],
    ['shadows', 'NO_USE_SHADOWS'],
    ['tar_kludge', 'NO_TAR_KLUDGE'],
]

foreach t : toggles
    if not get_option(t[0])
        add_project_arguments('-D' + t[1], language: 'cpp')
    endif
endforeach

if is_msvc
    add_project_arguments(
        '-D_CRT_SECURE_NO_WARNINGS',
        # MSVC defines _WIN32, not __WIN32__; config.h keys its Windows paths
        # (findfirst dir API, etc.) off __WIN32__. Matches the legacy Makefile.vc.
        '-D__WIN32__',
        language: 'cpp',
    )
else
    extra_flags = [
        '-fno-common',
        '-fstack-protector-strong',
        '-Wformat-security',
        '-Wstrict-overflow=2',
        '-Werror=odr',
        '-Werror=lto-type-mismatch',
        '-Werror=strict-aliasing',
        '-Wno-deprecated-declarations',
        # Deliberate cascading fall-throughs in the key-dispatch switches.
        '-Wno-implicit-fallthrough',
    ]
    add_project_arguments(
        cc.get_supported_arguments(extra_flags),
        language: 'cpp',
    )
endif

# curses: ncurses/SysV curses on Unix/macOS/MSYS2, PDCurses on MSVC.
curses_dep = dependency('curses', required: false)
if not curses_dep.found()
    foreach name : ['ncursesw', 'ncurses', 'pdcurses', 'curses']
        curses_dep = cc.find_library(name, required: false)
        if curses_dep.found()
            break
        endif
    endforeach
endif
if not curses_dep.found()
    error('No curses library found (need ncurses or PDCurses)')
endif

# The wide curses calls the UTF-8 drawing needs are hidden behind
# _XOPEN_SOURCE_EXTENDED. glibc defines it implicitly, which is why Linux got
# that drawing and macOS and the BSDs quietly did not, whatever curses they
# have. Ask for it where the library really does have the wide functions; if
# the probe fails, nothing is defined and the single-byte fallback stays.
wide_curses_check = '''
#define _XOPEN_SOURCE_EXTENDED
#include <curses.h>
int main(void) {
    cchar_t cc;
    wchar_t w[2];
    w[0] = 'x';
    w[1] = 0;
    setcchar(&cc, w, 0, 0, 0);
    mvwadd_wchnstr(stdscr, 0, 0, &cc, 1);
    return 0;
}
'''

if not is_msvc and cc.links(
    wide_curses_check,
    dependencies: curses_dep,
    name: 'wide curses API',
)
    add_project_arguments('-D_XOPEN_SOURCE_EXTENDED', language: 'cpp')
endif

deps = [curses_dep]

# PDCurses' Win32 console port pulls in these; harmless if unused.
if is_msvc
    foreach winlib : ['user32', 'advapi32', 'gdi32', 'winmm']
        wl = cc.find_library(winlib, required: false)
        if wl.found()
            deps += wl
        endif
    endforeach
endif

ncmail_src = files(
    'mmail/area.cc',
    'mmail/bw.cc',
    'mmail/compress.cc',
    'mmail/filelist.cc',
    'mmail/letter.cc',
    'mmail/misc.cc',
    'mmail/mmail.cc',
    'mmail/netadd.cc',
    'mmail/omen.cc',
    'mmail/opx.cc',
    'mmail/pktbase.cc',
    'mmail/qheader.cc',
    'mmail/qwk.cc',
    'mmail/qwkhdr.cc',
    'mmail/qwkvote.cc',
    'mmail/read.cc',
    'mmail/resource.cc',
    'mmail/soup.cc',
)

interfac_src = files(
    'interfac/addrbook.cc',
    'interfac/ansiview.cc',
    'interfac/arealist.cc',
    'interfac/basic.cc',
    'interfac/help.cc',
    'interfac/interfac.cc',
    'interfac/isoconv.cc',
    'interfac/letterl.cc',
    'interfac/letterw.cc',
    'interfac/lettpost.cc',
    'interfac/main.cc',
    'interfac/mmcolor.cc',
    'interfac/mysystem.cc',
    'interfac/packet.cc',
    'interfac/tagline.cc',
)

executable(
    'ncmail',
    ncmail_src + interfac_src,
    include_directories: include_directories('.'),
    dependencies: deps,
    override_options: cpp98,
    install: true,
)

# ncmail.1 is committed and current; regenerate from MANUAL.md only when the
# (optional) go-md2man tool is present.
md2man = find_program('go-md2man', required: false)
if md2man.found()
    custom_target(
        'ncmail.1',
        input: 'MANUAL.md',
        output: 'ncmail.1',
        command: [md2man, '-in', '@INPUT@', '-out', '@OUTPUT@'],
        install: true,
        install_dir: join_paths(get_option('mandir'), 'man1'),
    )
else
    install_man('ncmail.1')
endif

# Docs and the example color schemes (selectable via the ColorFile rc keyword).
install_data(
    files('HISTORY.md', 'LEGAL.md', 'MANUAL.md', 'README.md'),
    install_dir: get_option('docdir'),
)
install_subdir('colors', install_dir: get_option('docdir'))

# Unit tests. Source files under test are listed here (not in tests/meson.build)
# because meson forbids files() from referencing a parent directory.
if get_option('tests')
    mm_misc_src = files('mmail/misc.cc')
    mm_qwkhdr_src = files('mmail/qwkhdr.cc')
    mm_qwkvote_src = files('mmail/qwkvote.cc')
    mm_qheader_src = files('mmail/qheader.cc')
    subdir('tests')
endif
