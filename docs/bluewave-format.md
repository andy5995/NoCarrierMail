# The Blue Wave packet format

Developer reference for packet level 3. Three sources, all from Cutting Edge
Computing:

- **The Blue Wave Offline Mail System Packet Structures, version 3** (George
  Hatchew, 30 November 1995) — the C header, which lives in this tree as
  `mmail/bluewave.h`.
- **The Blue Wave Developer's Kit 3.00 documentation** (documentation by Martin
  Pollard, 30 November 1995), at
  <https://wmcbrine.com/mmail/specs/bwdev300.html#section-33--the-blue-wave-mail-packet>,
  converted to markdown by William McBrine. It is the only source for the
  `*.OLC` grammar and for the packet-naming and reader-behaviour rules below;
  the header points at it and stops.
- **Mail Packet File Structures, Revision Level 2** (created by George Hatchew,
  documentation by Martin Pollard and George Hatchew, revision 2.01, 18 January
  1994), at
  <https://www.moon-soft.com/program/FORMAT/internet/bluewave.htm>. This is the
  level 2 document, so it predates `*.OLC`, `uses_upl_file` and the extensible
  structure lengths, but it is the fullest description of the older reply files
  and it carries the two appendices on building a packet and a reply packet.

Cross-checked against a real packet (`shsbbs.011` in `~/mmail/down`, packet
level 3) and against our own driver in `mmail/bw.cc`.

A packet is Blue Wave if it contains a `*.INF` file. That is the whole test
`mmail::detect_and_open()` uses, and it runs after the QWK test, so a Blue Wave
packet that also carries `CONTROL.DAT` and `MESSAGES.DAT` is read as QWK.

## Files

All files share one 1-8 character root name, given in `INF_HEADER.packet_id`.
Renaming the archive does not rename them, so `bluewave::findInfBaseName()`
takes the root from whichever `.INF` it finds and derives every other name from
that.

| File | Direction | Contents |
| --- | --- | --- |
| `*.INF` | door → reader | BBS details, user details, one record per area |
| `*.MIX` | door → reader | One record per *scanned* area; index into `*.FTI` |
| `*.FTI` | door → reader | One header record per message |
| `*.DAT` | door → reader | Message text, unstructured |
| `*.XTI` | reader-local | Read/replied/marked status, one record per message |
| `*.UPL` | reader → door | Reply headers (packet level 3) |
| `*.UPI`, `*.NET` | reader → door | Reply headers (packet level 2 and earlier) |
| `*.OLC` | reader → door | Offline configuration, text (level 3) |
| `*.PDQ` | reader → door | Offline configuration, binary (level 2 and earlier) |
| `*.REQ` | reader → door | Filenames to request, one 13-byte record each |

The packet extension is either a day-of-week form — the first two letters of
the weekday plus a digit, `MO4` — or, when `INF_HEADER.uflags` has
`INF_NUMERIC_EXT`, a number of one to three digits. A reply packet is the
packet ID plus `.NEW`.

Bulletins and other display files are separate files named in
`INF_HEADER.readerfiles`, five slots of 13 bytes: `WELCOME`, `GOODBYE` and
`NEWS` in the sample packet. The kit also allows `BLT*.*` and `*.TXT` bulletins
and a `NEWFILES.*` file listing downloads, and leaves their display entirely to
the reader. `INF_HEADER.mashtype` names the compression the archive used;
the door fills it with 0 and the Blue Wave reader writes it there itself.

## Integers and structure packing

Every multi-byte field is little-endian, and every structure is packed with no
alignment padding. `bluewave.h` offers two ways to get that: the plain typedefs
(`tWORD` as `unsigned short`, `tLONG` as `signed long`), or, with `BIG_ENDIAN`
defined before the include, byte arrays plus your own conversion.

**We always take the byte-array path.** `mmail/bw.h` defines `BIG_ENDIAN`
unconditionally before including the header, and reads every field through
`getshort()`/`getlong()` from `mmail/misc.h`. This is not about byte order — it
is about layout. `tLONG` is `signed long`, which is 8 bytes on LP64, so the
plain typedefs give the wrong offsets for `MIX_REC.msghptr` and
`FTI_REC.msgptr` on any 64-bit host. With byte arrays every member has
alignment 1, the offsets are exact everywhere, and `sizeof` is safe to use.

The exception is `PDQ_REC`, a bare `tBYTE echotag[21]`. Compilers that pad
structures to an even size make `sizeof` 22, so `bw.h` defines
`PDQ_REC_SIZE 21` and `getPDQ()`/`makeOffConfig()` use that. (`UPI_HEAD_SIZE`
is defined next to it and used nowhere — we never write a `*.UPI`.)

## `*.INF` — BBS and area information

One `INF_HEADER`, then one `INF_AREA_INFO` per area available to the user.
Fields we read: `loginname`, `aliasname`, `sysop`, `systemname`, `ctrl_flags`
(only `INF_NO_CONFIG`), `readerfiles`, `ver`, `from_to_len`, `subject_len`, the
four structure-length fields, and the offline-config blocks copied into a
`*.PDQ`. The rest — netmail credits and debits, keywords, filters, macros,
`maxfreqs`, `is_QWK`, `passtype`, `mashtype` — we ignore.

`password` and the reply files' `vernum` are stored as ASCII plus 10, so that
`TYPE *.INF` does not print them. That is the whole of the obfuscation, and the
header says as much.

**The structure lengths are the extensibility mechanism, and 0 means "use the
original".** `inf_header_len`, `inf_areainfo_len`, `mix_structlen` and
`fti_structlen` give the on-disk record sizes; a reader seeks by those, not by
`sizeof`, and reads only the `ORIGINAL_*_LEN` prefix it understands. Any value
below the original is treated as the original. `initInf()` does exactly that.
Our sample packet fills all four with the original values (1230, 80, 14, 186).

`from_to_len` and `subject_len` are the host's field limits, clamped to 35 and
71 — the maxima the reply structures can carry. Zero means "use the maximum".
For an internet area we pass 511 as the subject limit instead, since the real
subject travels as a kludge line (below) rather than in `UPL_REC.subj`.

Area flags worth naming: `INF_SCANNING` (user is active in the area),
`INF_ALIAS_NAME`/`INF_ANY_NAME` (which From: name is allowed),
`INF_ECHO`/`INF_NETMAIL` (the pair that distinguishes local, echo, newsgroup
and netmail — see the chart under `INF_NET_FIDONET` in the header),
`INF_NO_PUBLIC`/`INF_NO_PRIVATE`, `INF_POST` (off means read-only), and
`INF_PERSONAL`/`INF_TO_ALL` (personal-only and to-all-plus-personal scanning,
level 3 and later).

`network_type` is `INF_NET_FIDONET` or `INF_NET_INTERNET`. It decides how the
same flag pair is read, and in our driver it also turns on `LATINCHAR` and
switches the body parser into internet mode.

## `*.MIX` — the index

One 14-byte record per area that was *scanned*, not per area that exists. Each
holds `areanum` (six bytes of ASCII, NUL-padded), `totmsgs`, `numpers`, and
`msghptr`, a byte offset into the `*.FTI`.

Matching `MIX_REC`s to `INF_AREA_INFO`s is by that ASCII area number, and
`initMixID()` walks both lists forward together rather than searching from the
start each time. This relies on both files being in the same area order, which
the format does not state anywhere. Areas with no `MIX_REC` get `mixID` -1 and
show as empty.

## `*.FTI` — message headers

Fixed 186-byte records, always in area order, contiguous per area: `msghptr`
plus *n* × `fti_structlen` reaches message *n* of that area. Fields are `from`,
`to` and `subject` (36/36/72, space-padded — `cropesp()` trims), `date` as free
text of up to 19 bytes, `msgnum`, `replyto`, `replyat`, `msgptr` and
`msglength` into the `*.DAT`, `flags`, and a Fido origin address.

**The date field is text in whatever format the host BBS uses**, and the header
warns it may take 19 bytes or 10. Our sample carries `07-19-26 20:54`. It is
also not guaranteed to be NUL-terminated, so `getNextLetter()` writes
`ftiRec.date[19] = '\0'` before using it.

Of `flags` we read only bit 0, private. The rest (crash, file attach, kill/sent,
hold, immediate, file request, direct) we neither display nor preserve.

## `*.DAT` — message text

Unstructured. A message begins with an ASCII space that is **not part of the
text**, then `msglength` bytes read from `msgptr`. Lines end with a bare CR.

`bluewave::getblk()` skips that leading space, converts CR to LF, discards LF,
and treats a NUL or a premature EOF as a space — the comment there calls those
"some buggy packets". With `StripSoftCR` set it also discards 0x8D (the soft
carriage return some Fido editors emit), but not in internet areas.

### Internet kludge lines

In an area whose `network_type` is `INF_NET_INTERNET`, the text may open with
header lines introduced by SOH (0x01):

```
\001From: user@example.com
\001Message-ID: <...>
\001References: <...>
\001Newsgroups: comp.foo
\001Followup-To: comp.bar
\001Subject: a subject longer than 71 characters
\001Date: Mon, 7 Sep 2026 12:00:00 -0500
```

`bluewave::endproc()` pulls each out and overrides the corresponding `FTI_REC`
field. `Date:` always wins over the `FTI_REC.date`, because the Usenet header
carries more and because the two disagreed in the packets the driver was
written against. Non-internet areas go to `fidocheck()` instead, which handles
the usual Fido kludges.

We write the same shape back out: `addRep1()` emits `\001X-Mailreader:` or
`\001X-Newsreader:` and then `References`, `Newsgroups` and `Subject` as needed.

## `*.XTI` — read status

Not part of the official format; it is what the Blue Wave reader itself uses,
documented in the header for third parties. One two-byte record per message,
in the same order as the `*.FTI`: a `flags` byte (read, replied, personal,
tagged, saved, printed) and a `marks` byte (save, reply, print, delete).

The header asks that a reader which stores status some other way not name its
file `*.XTI`. We use the real thing, mapping `XTI_HAS_READ`, `XTI_HAS_REPLIED`,
`XTI_IS_PERSONAL` and `XTI_HAS_SAVED` to our own status bits, and collapsing all
four `marks` bits to a single "marked" flag that is written back as
`XTI_MARK_SAVE`.

## The reply packet

Named `<basename>.new` on disk, with the inner header file `<BASENAME>.UPL`;
each message body is a separate `NNNNN.MSG`, numbered from `00000` by
`freeFileName()`.

`UPL_HEADER` then one `UPL_REC` per message. The record carries `from`, `to`,
`subj`, a Fido destination address, `msg_attr`, `netmail_attr`, a Unix
timestamp, `replyto`, the body's `filename`, and `echotag`.

**Messages are routed by echo tag, not by area number**, and the door is
required to compare case-insensitively — the header's reasoning is that sysops
renumber areas. `bwreply::getAreaFromTag()` does the same on the way back in,
though with `strcmp`, not `strcasecmp`.

`net_dest` is a 100-byte field the format reserves for a non-Fido destination
address. We use it for that on internet messages — and, on Fido messages, we
also park the message ID being replied to there as `REPLY: <msgid>`, which is
ours, not the format's. A door will read it as a destination address; nothing in
the spec says it should not, but nothing says it should either.

`area_flags` and `user_area` in `UPL_REC` are reader-private. The header tells
door authors to ignore both.

**The two documents disagree on when to write the old files, and the newer one
wins.** Revision 2.01 says a reader should create all three — `*.UPL`, `*.UPI`
and `*.NET` — and leave the door to prefer `*.UPL`. The level 3 kit narrows
that to writing `*.UPI` only when `uses_upl_file` is clear *and* the packet is
level 2 or earlier, which is what `bwreply` does. Revision 2.01 predates
`uses_upl_file` entirely, so at the time it was written there was no way for a
reader to know what the door could read; the field is the answer to that
question, and a level 3 packet therefore always gets a `*.UPL`.

Appendix B adds one rule that is easy to miss: **delete the reply archive
before repacking it**, or messages the user deleted survive as orphans inside
it. `pktreply::makeReply()` calls `deleteReplies()` for this reason.

The serial-number fields in `INF_HEADER`, `UPI_HEADER` and `UPL_HEADER` use a
proprietary Cutting Edge algorithm and mean nothing to anyone else; the Blue
Wave doors read `UPL_HEADER.reader_name` first and only consult the serial when
the packet came from the Blue Wave reader itself, to decide whether to append
`[NR]` to the tear line. We fill neither.

### Offline configuration

`bwreply::makeOffConfig()` picks the format from `INF_HEADER.ver`: below 3 it
writes a binary `*.PDQ`, copying the keywords, filters, macros and password
straight out of the `INF_HEADER` and setting `PDQ_AREA_CHANGES`; otherwise a
text `*.OLC`:

```
[Global Mail Host Information]
AreaChanges = ON

[ECHOTAG]
Scan = ALL

```

CRLF-terminated. The kit calls this Windows INI format: one required global
section, then one optional section per area, keyed by echo tag. Its global
keywords are `MenuHotKeys`, `ExpertMenus`, `SkipUserMsgs`, `ExtendedInfo`,
`NumericExtensions`, `NewFileList`, `DoorGraphics`, `Password`,
`MaxPacketSize`, `Filter`, `Keyword`, `Macro` and `AreaChanges`; the only area
keyword is `Scan`, whose value we set to `ALL`, `PERSONLY` or `PERSALL`. We
write `AreaChanges` and `Scan` and nothing else.

Reading back (`getOffConfig()`) prefers a `*.PDQ` if one is present, and
recognises only the `[section]` lines of an `*.OLC`, ignoring `Scan`.

Both sides mean "turn off every area, then turn on the ones listed".

## What our driver adds

**A synthetic personal area.** Blue Wave has no personal-mail area; `MIX_REC`
only counts personal messages per area in `numpers`. `initMixID()` scans every
`*.FTI` record whose area reports any, matches `to` case-insensitively against
`loginname` or `aliasname`, and builds an index that becomes area 0.

That area is a real slot in the `areas` and `mixID` arrays, allocated as
`maxConf + 1`. When the packet has no personal mail, `initMixID()` increments
both pointers by one so index 1 becomes index 0, and the destructor decrements
them again before `delete[]`. Anything that reallocates or reorders those arrays
has to keep that pairing intact.

**`getTear()` returns 0.** We add no tear line to a Blue Wave reply, so
`UPL_HEADER.reader_tear` is filled but unused by us; the door decides. The kit
asks a reader to append FTS-0004 tear and origin lines itself on FidoNet echo
areas.

## Known gaps

- **`INF_HEADER.uses_upl_file` is never read.** We always write a `*.UPL`. A
  version-2 door that requires `*.UPI` plus `*.NET` cannot import our replies.
  Nothing in the tree reads or writes either structure.
- **No `*.REQ`.** File requesting is not implemented. The kit allows MS-DOS
  wildcards in the field, while warning that Blue Wave's own doors reject them
  and read only the first ten records.
- Netmail attribute flags (`crash`, `hold`, `file attach`, `file request`) are
  carried by both `FTI_REC.flags` and `UPL_REC.netmail_attr`, and we set none of
  them.
- `INF_HEADER.max_packet_size`, `file_list_type` and `auto_macro` are read past
  and ignored; they exist for offline configuration.
