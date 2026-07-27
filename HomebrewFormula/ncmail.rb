# frozen_string_literal: true

# The comment above, and the argument alignment in install, are here because this
# file lives in HomebrewFormula/ rather than Formula/ -- Homebrew's formula
# rubocop rules only apply to the latter, so the generic Ruby cops run and
# brew audit --strict fails without them. No formula in homebrew-core carries
# either (0 of 8521 on 2026-07-26); do not "tidy" them away.

class Ncmail < Formula
  desc "Offline mail packet reader for Blue Wave, QWK, OMEN, SOUP and OPX"
  homepage "https://github.com/andy5995/NoCarrierMail"
  url "https://github.com/andy5995/NoCarrierMail/archive/refs/tags/v0.53.tar.gz"
  sha256 "114d28cce07b6860f18fe96edfd0669e2d1f27e2c0f1c63dd9ef6a17db851082"
  license "GPL-3.0-or-later"
  head "https://github.com/andy5995/NoCarrierMail.git", branch: "master"

  depends_on "meson" => :build
  depends_on "ninja" => :build
  depends_on "pkgconf" => :build

  # No ncurses dependency on purpose. macOS ships ncurses 6.0 in /usr/lib and it
  # does have the wide-character API this needs, so the build is self-contained.
  # Homebrew's ncurses is keg-only, and linking it without declaring it here
  # would leave an undeclared dependency that breaks if the user removes it.
  # Emptying pkg_config_path cancels the Homebrew hint that meson.build sets for
  # people building by hand.
  def install
    system "meson", "setup", "build", *std_meson_args,
           "-Dtests=false",
           "-Dpkg_config_path="
    system "meson", "compile", "-C", "build", "--verbose"
    system "meson", "install", "-C", "build"
  end

  def caveats
    <<~EOS
      On first run ncmail writes ~/.ncmailrc and creates ~/ncmail, and asks
      whether you want to edit the settings. Put your mail packets in
      ~/ncmail/down.

      Example colour schemes were installed to:
        #{doc}/colors
      Copy the folder somewhere of your own and point the ColorFile setting at
      one of the files in it, so an upgrade cannot replace your choice.

      QWK and Blue Wave packets need zip and unzip, which macOS already has.
      Other packet types may want arj or lha.
    EOS
  end

  # ncmail is a full-screen curses program with no batch mode, so there is little
  # to run without a terminal. 0.54 adds --version; when the url above is bumped
  # to it, this becomes:
  #
  #   output = shell_output("#{bin}/ncmail --version")
  #   assert_match "NoCarrierMail", output
  #   assert_match version.to_s, output unless build.head?
  #
  # HOME has to be set either way: the settings file is read by a global object
  # before main() runs, so even --version writes ~/.ncmailrc on a first run.
  test do
    ENV["HOME"] = testpath

    assert_path_exists bin/"ncmail"
    assert_match "offline mail reader", (man1/"ncmail.1").read
  end
end
