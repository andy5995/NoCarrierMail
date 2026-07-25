# Driving the UI headlessly (for testing)

Developer reference. The curses UI can be driven and scraped from a script,
which is how display fixes get verified without a person watching a terminal.
Everything below was confirmed first-hand against a real Synchronet QWK packet;
where a claim comes from reading the source instead, the function is named so it
can be rechecked.

## Harness

`tmux` supplies the pty; `send-keys` types; `capture-pane` scrapes.

```
tmux -u new-session -d -s t -x 120 -y 32 /tmp/t/inner.sh
tmux send-keys -t t Down          # one key per call
tmux capture-pane -p -t t         # rendered screen, plain text
tmux capture-pane -ep -t t        # same, keeping SGR escapes (colors)
```

Put the program plus its environment in a wrapper script rather than an
env-prefixed command, and end the script with a `sleep` so the pane outlives the
program and its last screen stays readable. Set the pane size explicitly:
column widths come from `COLS`, so a narrow pane changes what you are testing.

Point `NCMAIL` at a scratch directory holding a copy of an `.ncmailrc`. Because
the paths in that file are absolute, the program still reads and writes the real
mail directories unless you edit them — decide which you want before running.

## Facts about the UI worth knowing before scripting it

- **`R` (reply) works in the letter window, not the letter list.** From the list,
  `Enter` opens the message first. The list's own keys are `E` (enter a new
  letter), `L`, `S`, `M`, `U`, `$`.
- **Replying asks for the target area first.** A picker listing every area opens
  before the header window; the current area is preselected.
- **The header window opens with the cursor on `Subj`,** not on `From`. Field
  order is `From`, `To`, `Subj` (`LetterWindow::EnterHeader`). `Up`/`Down` move
  between fields, `Enter` accepts and advances, and `Enter` on the last field
  accepts the whole header. `ESC` cancels the letter.
- **Typing a printable character as the first key replaces a field; any
  navigation key instead makes it editable in place** (`Win::getstring`, via its
  `first_key` handling). Pressing `Enter` on a field you never typed into leaves
  it unchanged, so you can pass through fields safely.
- **`From` does not come from the `UserName` rc setting.** For QWK it starts as
  the packet's alias or login name from CONTROL.DAT (`LetterWindow::setToFrom`).
  It is editable in the header window, which is the only way to change it for a
  single message.
- **The tagline window** (shown after editing, when `UseTaglines` is on) uses `A`
  to add, `E` to edit, `R` for random, `K` to delete, `S` to sort, `Enter` to
  apply, `Q` to apply none. A tagline added with `A` is appended to the tagline
  file, so `End` reaches it.
- **`AutoSaveReplies: Yes` writes the reply packet on exit** with no prompt, as
  `<packet>.rep` in `ReplyDir`. Quit all the way out; the packet is not written
  while the program is still open.
- **Existing replies are edited and deleted from the REPLY area** at the top of
  the area list, with `E` and `K`; in a normal area those keys start a new letter
  and do nothing. Re-editing moves that reply to the end of the reply list, so
  its position changes under you. `MANUAL.md` documents both.
- **`ESC` is sensed only after a delay under ncurses,** which `MANUAL.md` warns
  about; a scripted `Escape` may appear to be ignored entirely. Send the raw byte
  (`tmux send-keys -H 1b`) instead of relying on it.

For a small change to an already-written reply, editing the `.REP` directly can
beat driving the UI: it is a zip holding one `<BBSID>.MSG` file in plain QWK
layout — a 128-byte header per message, body lines terminated by `0xE3` and
padded with spaces to a 128-byte boundary, with the block count (header included)
as left-aligned ASCII in header bytes 116-121. Rewrite the body, fix that count,
rezip, then open the packet in the reader: if the area list shows the `R` flag,
the packet parsed.

## Two traps for scripted runs

- **A scripted editor must change the file's mtime by at least a second.** After
  the editor returns, `LetterWindow::EnterLetter` compares the reply file's
  mtime with what it was before, and an unchanged value means "the user did not
  edit anything", which raises *Cancel this letter?*. `mystat::fdate()` is
  whole seconds, so an editor stand-in that copies a prepared file finishes
  inside the same second and looks like a cancel. Sleeping about a second inside
  the stand-in avoids it.
- **An empty value on the command line is ignored, so a string setting cannot be
  cleared that way.** `resource::processOne` returns early on an empty value, so
  `-signature ""` leaves the configured signature in place — which will append
  the wrong person's signature to a message whose `From` you just changed. Point
  the option at an empty file instead.

## Colors

`capture-pane -p` drops attributes. To show that a drawing change did not alter
colors, compare a histogram of the SGR codes on the same screen before and
after:

```
grep -o $'\x1b\[[0-9;]*m' cap.txt | sort | uniq -c | sort -rn
```

## Character set behaviour to keep in mind when reading a screen

The conversion direction depends on the area, not only on the `Charset` setting:
`isLatin` comes from the area's `LATINCHAR` flag, which the QWK driver sets for
areas the door marks Internet or Usenet (`qwk.cc`, in the area-flag parsing).
The same byte therefore displays differently in different areas, and the code
that sets the flag carries an upstream comment questioning whether it is right.
When a character looks wrong, check the byte in the packet before assuming the
display is at fault: a value may already have been folded to something lossy by
the BBS before the packet was built.
