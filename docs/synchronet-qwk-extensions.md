# Synchronet QWK extensions: HEADERS.DAT and VOTING.DAT

Developer reference. Source: <https://wiki.synchro.net/ref:qwk>, cross-checked
against the Synchronet source (`src/sbbs3/msgtoqwk.cpp`, `pack_qwk.cpp`,
`qwk.cpp`) and against real packets in `~/mmail/down` (`vert.*`, `kd3net.000`).

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
indexes from `.NDX` never sees these stubs. We do not see them on the
`.DAT`-scanning path either, but only by accident: a stub's chunk count is 1,
and `qheader::init_short()` already rejects anything below 2 as an unparseable
header. No explicit `V` check is needed — one was written and then removed
after an A/B against the pre-change binary showed the counts were already
identical.

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
- `[close:<id>]` — closure of a previously posted poll.

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

- **The offset marker is nearly useless for reading.** It points at the
  ballot's own bodyless stub record, not at the message being voted on. To
  attach a tally to a message you must match `In-Reply-To` against that
  message's `Message-ID` **from HEADERS.DAT**. Vote support therefore depends
  on HEADERS.DAT support; there is no other way to identify the target.
- **`In-Reply-To` is not always an RFC-style message-ID.** Real packets
  contain values like `ANETBBS_f2c2ce96_6a5e96df`. Treat it as an opaque
  string; never parse it.
- **Ballots vastly outnumber polls.** In `kd3net.000`: 2581 `[vote:]`, 6
  `[poll:]`, 0 `[close:]`. Up/down votes on ordinary messages are what this
  file is mostly used for in practice.
- **Tallies are necessarily partial.** A packet contains only the ballots that
  were exported into it, so a count computed by an offline reader is a floor,
  not the BBS's authoritative total.
- Voting data can also travel in the other direction, in a `.REP`; Synchronet
  reads it back in `qwk.cpp:qwk_voting()`. We do not write it.
