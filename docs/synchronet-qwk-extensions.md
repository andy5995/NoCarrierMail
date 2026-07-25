# Synchronet QWK extensions: HEADERS.DAT and VOTING.DAT

Developer reference. Source: <https://wiki.synchro.net/ref:qwk>, cross-checked
against the Synchronet source (`src/sbbs3/msgtoqwk.cpp`, `pack_qwk.cpp`,
`qwk.cpp`, `un_rep.cpp`, `postmsg.cpp`, `src/smblib/`) and against real packets
in `~/mmail/down` (`vert.*`, `kd3net.000`).

Both files are optional additions to a QWK packet. Both are INI-style text,
CRLF- or LF-terminated. Both key their sections to the **hexadecimal byte
offset of a message's 128-byte header block in MESSAGES.DAT**.

## HEADERS.DAT

One section per message. Section name is the hex offset. Field lines use
`Key: Value` or `Key = Value`; the colon form is preferred upstream because it
preserves trailing whitespace.

```
[c80]
Message-ID: <5D4AFDF1.40645.dove_dove-gen@somebbs.com>
WhenWritten: 20190807093601-0700  c1e0
```

Values are up to 1024 characters, CP437 unless the section sets `Utf8: true`,
which marks both the header fields and that message's body as UTF-8. Values
here override the equivalent fields in the MESSAGES.DAT header block, which is
how a packet carries names and subjects longer than the 25-character fixed
fields without QWKE.

Fields we use: `Utf8`, `Sender`, `To`, `Subject`, `Message-ID`. Others the
format defines: `Format` (fixed/flowed), `In-Reply-To`, `WhenWritten`,
`WhenImported`, `WhenExported`, `ExportedFrom`, `SenderNetAddr`,
`SenderIpAddr`, `SenderHostName`, `SenderProtocol`, `Organization`,
`Reply-To`, `ToNetAddr`, `X-FTN-*`, `Editor`, `Columns`, `Tags`, `Path`,
`Newsgroups`, `Conference`.

## VOTING.DAT

Carries poll and vote messages. A voting message is flagged in MESSAGES.DAT by
the **status byte `V`** (ASCII 86) at offset 0 of its header block. Such a
message **has no body block** — it occupies one 128-byte header record and
nothing more. Synchronet also omits it from the `.NDX` files
(`pack_qwk.cpp`: the index write is guarded by `size > 0`), so a reader that
indexes from `.NDX` never sees these stubs. Nor does the `.DAT`-scanning path
by default: a stub's chunk count is 1, and `qheader::init_short()` rejects
anything below 2 as an unparseable header.

That is fine for ballots, which have nothing to show, but a **poll** is a
message the user should be able to read. So `qwkpack::readIndices()` forces
the `.DAT`-scanning path whenever the packet has any polls, and admits a
one-chunk record when its status byte is `V` and VOTING.DAT has a poll at that
offset. Forcing the scan is what makes a poll reachable at all when it was the
only new item in its conference: Synchronet then writes a zero-length `.NDX`
and deletes it, so there is no index file for that conference to extend.

A poll's body is synthesized from VOTING.DAT by `qwkpack::getBody()`, and its
subject comes from the poll section's `Subject` — a poll is *not* written to
HEADERS.DAT, so its record carries only the 25-character subject field.

Each entry is **two sections**: an empty `[<hex offset>]` marker followed
immediately by the section that holds the data.

```
[180]
[vote:<6A6266FF.205.debate@vert.synchro.net>]
UpVote = true
In-Reply-To: <6A6226AD.204.debate@vert.synchro.net>
WhenWritten:  20260723120951-0700  c1e0
Sender: Digital Man
Conference: 1003
```

Section kinds, named by the voting message's own Message-ID:

- `[poll:<id>]` — a posted poll.
- `[vote:<id>]` — a ballot. Two distinct kinds, see below.
- `[close:<id>]` — closure of a previously posted poll. The poll it closes is
  named by `In-Reply-To`, and we mark that poll's results closed. **Untested
  against real data:** none of the packets here contain a single `close:`
  section, so this is read off `msgtoqwk.cpp` alone.

Poll sections carry `Subject` (the question), `Sender`, `Conference`,
`MaxVotes` (1–16, how many answers one ballot may select), `Results`
(visibility: 0 = voters, 1 = open, 2 = closed, 3 = secret), `Comment<N>`
(optional lines shown before the answers) and `PollAnswer<N>` (the options).
`Comment` and `PollAnswer` are numbered from 0, so no duplicate-key handling
is needed.

Ballots come in two kinds, distinguished by which keys they carry:

- **Message ballot** — `UpVote = true` or `DownVote = true`. Votes on an
  ordinary message. `In-Reply-To` is that message's Message-ID.
- **Poll ballot** — `Votes = 0x<mask>`. A bit-field over the poll's
  `PollAnswer<N>` (bit 0 = answer 0). `In-Reply-To` is the poll's Message-ID.
  Real packets contain multi-bit masks such as `0x1c` (answers 2, 3 and 4).

### Things that bit us, or would have

- **The offset marker means different things for ballots and polls.** For a
  ballot it is useless: it points at the ballot's own bodyless stub, not at the
  message being voted on, so to attach a tally you must match `In-Reply-To`
  against that message's `Message-ID` **from HEADERS.DAT**. Vote-count support
  therefore depends on HEADERS.DAT support; there is no other way to identify
  the target. For a poll the marker is essential — it is how the poll's record
  is located and put into the letter index.
- **`In-Reply-To` is not always an RFC-style message-ID.** Real packets
  contain values like `ANETBBS_f2c2ce96_6a5e96df`. Treat it as an opaque
  string; never parse it.
- **Ballots vastly outnumber polls.** In `kd3net.000`: 2581 `[vote:]`, 6
  `[poll:]`, 0 `[close:]`. Up/down votes on ordinary messages are what this
  file is mostly used for in practice.
- **Tallies are necessarily partial.** A packet contains only the ballots that
  were exported into it, so a count computed by an offline reader is a floor,
  not the BBS's authoritative total.
- **Synchronet's own ballot records carry an empty `To` *and* an empty
  `Subject`** in MESSAGES.DAT — only `From` (the voter) is set. We match the
  empty `To` but write a descriptive subject ("Upvote: <subject>") anyway, so
  the entry is identifiable in our REPLY area. Nothing reads either field on
  import, so the divergence is safe.
- Voting data also travels in the other direction, in a `.REP`, and we write
  it: `qwkreply::castVote()` packs each vote as a bodyless status-`V` record
  (chunks = 1, msgnum field = ASCII conference number) plus a ballot section
  in the REP's VOTING.DAT. Synchronet reads it back in `un_rep.cpp`, which
  takes that path only when the record's status is `V`, its block count is 1,
  **and** the REP actually contains a VOTING.DAT; otherwise the record is
  logged as a short message and skipped. A body would make the block count 2
  or more and the record would be imported as an ordinary post, so the
  no-body/one-chunk shape is the invariant to protect.

  Requirements learned from `qwk.cpp:qwk_vote()` and `postmsg.cpp:votemsg()`:
  `Sender` is mandatory (senderless ballots are discarded), `Conference` must
  match the record's conference number, `In-Reply-To` must resolve to a message
  in that sub via `smb_getmsgidx_by_msgid()` (if it does not, `thread_back`
  stays 0 and the vote is dropped *silently* — the caller deliberately ignores
  the error "for old messages"), and `WhenWritten` needs to be present and
  recent where the BBS sets `max_qwkmsgage`, since a missing date reads as 1970
  and gets age-filtered.

  Duplicate detection is **not** by the ballot's message-ID: `votemsg()` calls
  `smb_voted_already(smb, thread_back, from, net_type, net_addr)`, keyed on the
  voted-on message plus the voter's identity. So a second vote on the same
  message is rejected even from a different packet, and our own one-vote-per-
  packet guard is the narrower check of the two. The `[vote:<id>]` section name
  is just the ballot's own RFC822 Message-ID; we still generate a unique one
  per ballot.

### Round-trip, confirmed 2026-07-25

A REP containing two ncmail ballots was uploaded to Vertrauen, and the next
packet (`vert.013`) returned both of them — same generated IDs
(`<6a643f4b.4.ncmail@VERT>`), same targets, same `Sender` and `Conference`.
Two things that proves:

- The records import as ballots, not as posts. The only messages of ours in
  that packet are the four we actually wrote.
- **Omitting the trailing hex SMB zone on `WhenWritten` is fine.** We write
  only the ISO form (`20260724234933-0500`); Synchronet parsed it and
  re-exported it as `20260724234933-0500  fed4`, where `0xfed4` is -300 as a
  signed 16-bit, i.e. exactly the -0500 we sent. Its parser falls back to the
  ISO offset when the `sscanf` for the hex field fails.
