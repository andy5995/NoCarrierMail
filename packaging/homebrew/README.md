# Homebrew formula

`ncmail.rb` builds NoCarrierMail from the source tarball GitHub generates for a
tag. It is kept here so it travels with the project; Homebrew itself installs it
from a *tap* repository, which is a separate repo named `homebrew-tap`:

    homebrew-tap/
      Formula/
        ncmail.rb

With that in place, users run:

    brew install andy5995/tap/ncmail

Homebrew maps `andy5995/tap` to `github.com/andy5995/homebrew-tap` — the
`homebrew-` prefix on the repo name is required and is not typed by the user.

## There is no way to install this without a tap

Homebrew 6 rejects a formula that is not in a tap. Both of these fail, verified
against 6.0.12:

    brew install ./packaging/homebrew/ncmail.rb
    # Error: Homebrew requires formulae to be in a tap, rejecting: ...

    brew install https://raw.githubusercontent.com/.../ncmail.rb
    # Warning: No available formula or cask with the name "https://..."
    # (it reads the URL as a formula name)

So the tap repo is not optional. `brew test` also wants an *installed* formula,
and `brew audit` by path is disabled outright ("Use `brew audit [name ...]`").
Everything goes through a tap and a formula name.

Homebrew installs meson, ninja and pkgconf itself, and needs the Xcode command
line tools for the compiler (it offers to install them).

## Checking a change locally

Make a local tap once, then work by name:

    brew tap-new andy5995/tap
    cp packaging/homebrew/ncmail.rb "$(brew --repository andy5995/tap)/Formula/"
    brew install --build-from-source andy5995/tap/ncmail
    brew test andy5995/tap/ncmail
    brew audit --strict andy5995/tap/ncmail

`brew style` is the exception that does take a path — but **only inside a tap
layout**. Run against a loose file it applies generic Ruby cops instead of the
formula ones and invents six offences (Sorbet sigils, frozen-string comment,
class documentation, argument alignment). In a `*/Formula/*.rb` path this
formula reports no offences:

    brew style "$(brew --repository andy5995/tap)/Formula/ncmail.rb"

Once the real tap exists on GitHub, use `brew tap andy5995/tap` instead of
`tap-new`, and edit the formula in that clone.

## At each release

Two lines change:

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
