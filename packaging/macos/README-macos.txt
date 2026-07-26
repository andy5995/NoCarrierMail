NoCarrierMail for macOS
=======================

This is a command-line program. Run it from Terminal.

This build is for Apple Silicon (arm64) Macs. On an Intel Mac, build from
source instead. See INSTALL.md in the source tree.


First: remove the download flag
-------------------------------

macOS marks every downloaded file as unsafe. If you do not clear that mark,
the program will not start. In Terminal, in the folder you unpacked:

    xattr -d com.apple.quarantine ncmail

You only need to do this once.


Running it
----------

    ./ncmail

To run it from anywhere, copy it into a folder in your PATH:

    cp ncmail /usr/local/bin/

The first run writes a settings file, ~/.ncmailrc, and asks if you want to
edit it. It also makes a folder, ~/ncmail, and puts your mail packets in
~/ncmail/down.

macOS already has the "zip" and "unzip" programs, which is what QWK and Blue
Wave packets need. Other packet types may need another program, such as
"arj" or "lha". You can install those with Homebrew.


The manual
----------

    man ./ncmail.1

To install it:

    cp ncmail.1 /usr/local/share/man/man1/

Then "man ncmail" works.


Colors
------

The "colors" folder holds example color schemes. Pick one with the
ColorFile setting in ~/.ncmailrc, for example:

    ColorFile: /Users/yourname/ncmail-colors/ansi.col

Copy the folder somewhere of your own first, so an upgrade does not
replace your choice.
