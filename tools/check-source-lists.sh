#!/bin/sh
# Check that every source file is listed everywhere it has to be listed.
#
# meson.build is the source of truth. The legacy makefiles each carry their own
# copy of the source list, as do the Turbo C++ link response file (tclist) and
# the shared `depend` file. Only Makefile.wcc is built in CI -- nothing builds
# Makefile.bcc or Makefile.vc, because no runner has Borland/Turbo C++ and the
# MSVC job uses meson -- so a forgotten entry in those goes unnoticed until
# someone runs that toolchain by hand. This catches the common case without
# needing any of those compilers.
#
# Run from the repository root:  sh tools/check-source-lists.sh

set -e

status=0

# Object names from a makefile variable, e.g. "MOBJS = a.$(O) b.$(O) \" over
# however many continued lines. Watcom's wmake continues with '&', the others
# with '\'.
mk_objs() {
    awk -v var="$2" '
        index($0, var " =") == 1 { inblk = 1 }
        inblk {
            line = $0
            sub(/[\\&]$/, "", line)
            printf "%s ", line
            if ($0 !~ /[\\&]$/)
                inblk = 0
        }
    ' "$1" | tr ' \t' '\n\n' | sed -n 's/^\([A-Za-z0-9_]*\)\.\$(O)$/\1/p' | sort
}

# Source basenames from a meson files() list, e.g. "'mmail/area.cc',".
meson_srcs() {
    awk -v var="$1" '
        index($0, var " = files(") == 1 { inblk = 1; next }
        inblk && index($0, ")") == 1 { inblk = 0 }
        inblk { print }
    ' meson.build | sed -n "s|.*'$2/\([A-Za-z0-9_]*\)\.cc'.*|\1|p" | sort
}

compare() {
    # $1 = what is being checked, $2 = expected list, $3 = actual list
    if [ "$2" = "$3" ]; then
        echo "  ok    $1"
        return
    fi
    echo "  FAIL  $1"
    printf '%s\n' "$2" > /tmp/csl-want.$$
    printf '%s\n' "$3" > /tmp/csl-have.$$
    missing=$(comm -23 /tmp/csl-want.$$ /tmp/csl-have.$$ | tr '\n' ' ')
    extra=$(comm -13 /tmp/csl-want.$$ /tmp/csl-have.$$ | tr '\n' ' ')
    [ -n "$missing" ] && echo "          missing: $missing"
    [ -n "$extra" ] && echo "          not a source: $extra"
    rm -f /tmp/csl-want.$$ /tmp/csl-have.$$
    status=1
}

backend=$(meson_srcs ncmail_src mmail)
frontend=$(meson_srcs interfac_src interfac)
everything=$(printf '%s\n%s\n' "$backend" "$frontend" | sort)

if [ -z "$backend" ] || [ -z "$frontend" ]; then
    echo "could not read the source lists out of meson.build" >&2
    exit 2
fi

echo "meson.build lists $(printf '%s\n' "$backend" | wc -l | tr -d ' ') backend" \
     "and $(printf '%s\n' "$frontend" | wc -l | tr -d ' ') interface sources"

for mk in Makefile.bcc Makefile.vc Makefile.wcc; do
    compare "$mk MOBJS" "$backend" "$(mk_objs $mk MOBJS)"
    compare "$mk IOBJS" "$frontend" "$(mk_objs $mk IOBJS)"
done

compare "tclist" "$everything" \
    "$(grep -oE '[A-Za-z0-9_]+\.obj' tclist | sed 's/\.obj$//' | sort)"

compare "depend" "$everything" \
    "$(sed -n 's/^\([A-Za-z0-9_]*\)\.\$(O):.*/\1/p' depend | sort)"

if [ "$status" -ne 0 ]; then
    echo
    echo "A source file is listed in meson.build but not everywhere else (or"
    echo "the reverse). Adding or removing a .cc means updating meson.build,"
    echo "all three legacy makefiles, tclist and depend. See CLAUDE.md."
fi

exit $status
