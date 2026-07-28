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

## Sending keys

- One key per `send-keys` call, roughly a quarter of a second apart. A burst can
  overflow the input buffer. Send `Enter` as its own call.
- Named keys work: `Down`, `Up`, `Enter`, `Home`, `End`, `F2`. Literal text needs
  `-l`, as in `tmux send-keys -l -t t "Some subject"`.
- A key that appears to do nothing may simply not have arrived. Confirm with a
  key whose effect is lasting and visible before deciding the program is at
  fault.

## Navigate in two passes, never one

Do not compose a long keystroke sequence in advance; it will drift out of step.
Capture the pane, read where the cursor actually is, then send the next key.

- **Confirm the highlighted row before pressing `Enter`.** The highlight shows up
  only in `capture-pane -ep` output, as a reverse-video attribute; the plain text
  capture drops it. Landing on the neighbouring item gives a screen that looks
  plausible and is wrong.
- **First-run prompts fire once.** The "a new .ncmailrc has been written, edit it
  now?" prompt appears on the first run and not the second, so a fixed sequence
  shifts by one key and every later step lands somewhere unintended.
- **A stray key can persist.** The area and letter list modes are written back to
  the rc file, so an accidental `L` leaves the next run in a different view,
  sometimes an empty one.
- **An empty list is usually a filtered view**, not missing data. Check the
  window title, which names the current mode, before concluding the packet is
  wrong.
- A stray keystroke can also open a confirmation prompt, including one to delete
  a packet. Read the pane before answering anything you did not mean to open.

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
- **Editing `From` changes the local packet, not necessarily the posted
  message.** A board may ignore the name in an uploaded reply and attribute the
  message to the account it came from. That is what happened here: three messages
  written with `From` set to another name came back in the next packet under the
  account name. Whether the field is honoured is the board's decision, so do not
  treat the name shown in the reply area as proof of how a message will be
  attributed. If a message needs to be credited to someone else, say so in the
  message text.
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
- **A key that does nothing under `send-keys` is not evidence of a bug.** The
  charset toggle `C` produced no visible change for me on any screen and I was
  ready to call it broken; it works every time by hand. Check a suspect key
  interactively before concluding anything. Its popup now clears on the next
  keypress, so capture the pane before sending anything else; a popup that is
  already gone may mean you sent one key too many rather than that the toggle
  failed. Other popups are not transient — nothing erases them until another
  window draws over them.

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

## Inspecting the bytes the program writes

When the question is encoding or escape sequences rather than layout, wrap the
program in `script` to log the pty stream:

```
script -q -c "./builddir/ncmail /path/to/packet" /tmp/out.raw
```

- `script` block-buffers and flushes when the child exits cleanly, so quit the
  program through its own exit path instead of killing it, or the tail is lost.
- The log is not a screen. It is every write concatenated with cursor movement in
  between, so stripping the escapes gives overlapping text rather than a layout.
  Use it only for byte-level questions, such as whether a character went out as
  UTF-8 or as one raw high byte.

## Comparing against an earlier build

To show that a display change did what it claims, build the previous commit in a
temporary worktree and drive both the same way:

```
git worktree add /tmp/pre <commit>
meson setup /tmp/pre/bd /tmp/pre && meson compile -C /tmp/pre/bd
```

Compare the captured screens, and the SGR histograms for colours. If the fault
does not appear in the older build either, say so instead of reporting the change
as a fix: rendering faults often depend on the terminal, the curses version and
timing.

## Testing the DOS build

CI compiles the 16-bit DOS build but never runs it, so nothing catches a fault
that only appears at runtime there. Run it by hand from time to time, and before
a release.

`dosemu2 -t` uses a text-mode terminal, so the tmux harness above works on it
unchanged. DOSBox and its forks draw into an SDL window instead and cannot be
scraped this way.

The quickest check needs no packet and no keystrokes:

```
dosemu -t -ks -E "NCMAIL --version"
```

It prints the program version and the PDCurses version, which is enough to prove
the binary loads and runs.

For a full pass, copy `NCMAIL.EXE` and a packet into a directory under
`~/.dosemu/drive_c` and drive it with tmux. Two things to know first:

- **The DOS build needs an external unzip program.** The default command is
  `pkunzip -# -o`. Without one, opening a packet fails with "check archiver
  config" and nothing else in the program can be reached. PKZIP 2.04g is a
  self-extracting DOS executable; extract it into its own directory and add that
  directory to the DOS `PATH`.
- **On DOS the settings file is written next to the program**, not in a home
  directory, so each test directory gets its own `ncmail.rc`.

A pass is complete when the packet list, the area list, the letter list, a
message body and the ANSI viewer have all been seen, and quitting removes the
`work*` directory it created.

## Afterwards

Kill every tmux session you started, and remove temporary worktrees with
`git worktree remove --force`. Check `git worktree list` when you are done, and
leave alone any worktree you did not create.

## Character set behaviour to keep in mind when reading a screen

The conversion direction depends on the area, not only on the `Charset` setting:
`isLatin` comes from the area's `LATINCHAR` flag, which the QWK driver sets for
areas the door marks Internet or Usenet (`qwk.cc`, in the area-flag parsing).
The same byte therefore displays differently in different areas, and the code
that sets the flag carries an upstream comment questioning whether it is right.
When a character looks wrong, check the byte in the packet before assuming the
display is at fault: a value may already have been folded to something lossy by
the BBS before the packet was built.

On a UTF-8 terminal the ANSI viewer ignores `Charset` entirely — it maps the
message's own set straight to Unicode — so toggling `C` there changes nothing
but the popup. Two captures of the art that differ in no way but the popup is
the correct result, not a keystroke that failed to arrive.
