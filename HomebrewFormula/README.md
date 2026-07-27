# Homebrew formula

**This directory makes the NoCarrierMail repository its own Homebrew tap.**
Homebrew looks for formulae in `Formula/`, `HomebrewFormula/` or a repository's
root, and uses the first of those it finds, so no separate `homebrew-tap` repo is
needed and the formula is bumped in the same commit as the version.

`ncmail.rb` builds from the source tarball GitHub generates for a tag.

## Installing

Because this repository is not named `homebrew-something`, the URL is given once
when tapping; after that it behaves like any tap:

    brew tap andy5995/nocarriermail https://github.com/andy5995/NoCarrierMail
    brew install ncmail

`brew update` then keeps it current, and `brew upgrade ncmail` moves to a new
release. Homebrew installs meson, ninja and pkgconf itself, and needs the Xcode
command line tools for the compiler (it offers to install them).

The shorter `brew install user/tap/formula` form, which taps automatically,
only works for a repository whose name starts with `homebrew-`. That is the one
thing given up by keeping the formula here.

## There is no way to install a formula outside a tap

Verified against Homebrew 6.0.12 — all of these fail:

    brew install ./HomebrewFormula/ncmail.rb
    # Error: Homebrew requires formulae to be in a tap, rejecting: ...

    brew install https://raw.githubusercontent.com/.../ncmail.rb
    # Warning: No available formula or cask with the name "https://..."
    # (it reads the URL as a formula name)

`brew test` also wants an *installed* formula rather than a path, and
`brew audit` by path is disabled outright ("Use `brew audit [name ...]`").
Everything goes through a tap and a formula name.

## Checking a change locally

`homebrew.yml` does this in CI on every change to the formula or to
`meson.build`. By hand, put the file in a throwaway tap and work by name:

    TAP="$(brew --repository)/Library/Taps/local/homebrew-test"
    mkdir -p "$TAP/HomebrewFormula"
    cp HomebrewFormula/ncmail.rb "$TAP/HomebrewFormula/"

    brew style "$TAP/HomebrewFormula/ncmail.rb"
    brew audit --strict local/test/ncmail
    brew install --build-from-source local/test/ncmail
    brew test local/test/ncmail

Set `HOMEBREW_NO_AUTO_UPDATE=1` first: even `brew install --dry-run` will
otherwise update Homebrew itself before doing anything.

Two things about style checking, both found the hard way:

- `brew style` takes a path, but Homebrew's formula-specific rubocop rules are
  keyed to a directory named exactly `Formula`. Under `HomebrewFormula/` — or on
  a loose file — the generic Ruby cops apply instead and complain about a missing
  frozen-string comment, missing class documentation and Sorbet sigils.
- This formula is written to pass **both** rule sets: it carries the
  `# frozen_string_literal: true` comment and aligns continuation arguments with
  the first argument. `brew style` and `brew audit --strict` are clean whether the
  directory is `Formula` or `HomebrewFormula`.

## At each release

Two lines change, in this one file:

    url "https://github.com/andy5995/NoCarrierMail/archive/refs/tags/vX.Y.tar.gz"
    sha256 "..."

Compute the checksum from the same file Homebrew will fetch:

    curl -sL https://github.com/andy5995/NoCarrierMail/archive/refs/tags/vX.Y.tar.gz |
        shasum -a 256

## Why no ncurses dependency

macOS ships ncurses 6.0 in `/usr/lib`, and it does have the wide-character API,
so nothing needs to come from Homebrew. Homebrew's own ncurses is keg-only:
linking it without declaring it as a dependency would leave the binary relying
on a formula Homebrew does not know about. `-Dpkg_config_path=` in the install
block cancels the Homebrew hint that `meson.build` sets for people building by
hand.
