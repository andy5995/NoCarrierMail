# Homebrew formula

`ncmail.rb` builds NoCarrierMail from the source tarball GitHub generates for a
tag. It is kept here so it travels with the project; Homebrew itself installs it
from a *tap* repository, which is a separate repo named `homebrew-tap`:

    homebrew-tap/
      Formula/
        ncmail.rb

With that in place, users run:

    brew install andy5995/tap/ncmail

To test a change before tagging a release, without touching the tap:

    brew install --build-from-source --HEAD ./packaging/homebrew/ncmail.rb
    brew test ncmail
    brew audit --strict --new ncmail     # what homebrew-core would check

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
