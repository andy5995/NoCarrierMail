---
name: drive-ui
description: >-
  Drive and scrape NoCarrierMail's curses interface headlessly to check a
  change — send keystrokes, read the rendered screen as text, compare its
  colors, or inspect the bytes it writes to the terminal. Use when verifying a
  display or interface change, reproducing a screen-rendering bug, entering or
  editing a reply from a script, or whenever a task needs the program run and
  looked at with nobody at the keyboard. The unit tests cover no interface code,
  so this is the only way to check what reaches the screen.
---

# Driving the interface headlessly

**Read `docs/driving-the-ui-headlessly.md` before driving the interface.** It
has the harness, the keystroke flow for entering a reply, the traps that break
scripted runs, and the reply packet layout.

That file is the single copy of this knowledge. It lives in `docs/` so it is
also useful to contributors who are not working through an agent, and so there
is only one version to keep correct. **Add what you learn to that document, not
to this file.**
