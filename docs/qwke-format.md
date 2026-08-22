# QWKE: the extended QWK format

Developer reference, summarised from **QWKE Specifications 1.02** (Peter Rocca,
1994-1997), available at <http://vert.synchro.net/files/main/BBS/qwke.txt> and
linked from <https://wiki.synchro.net/ref:qwk>. Cross-checked against real
packets in `~/mmail/down` (`shsbbs.*`, from an OLMS 2000 door on RemoteAccess
2.x) and against our own reader in `mmail/qwk.cc`.

QWKE is not a separate format. Everything matches plain QWK 1.0 except four
files: `CONTROL.DAT` (conference names up to 255 characters instead of 13),
`DOOR.ID` (extra `CONTROLTYPE` names), and the two `.EXT` files below. A packet
is QWKE if it contains `TOREADER.EXT`; that is exactly the test `qwkpack` uses.

Unknown identifiers and unknown flag letters must be **ignored, not rejected**.
That is what makes an unhandled flag silent rather than an error, on both sides.

## Kludge lines

The first lines of a message body may carry header fields too long for the
25-character fixed fields in the `MESSAGES.DAT` header block:

```
To:
From:
Subject:
```

They sit at the top of the body, each terminated by CR or the 0xE3 QWK
newline, with a blank line after the last one (a reader should cope if it is
missing). A door importing a reply puts `To:` and `Subject:` back into the
message fields; `From:` may be honoured but has to be security-checked against
what the user is allowed to post as.

These are for **length, not addressing**. See the next section for the
distinction, which is easy to get backwards.

## Netmail and internet addressing

The spec calls this the "Subject line method": the destination address is an
`@` followed by the full netmail address, placed **in the subject field**.

```
To: PETE ROCCA
Fm: JOE USER
Sb: @1:2401/305.200
```

Internet mail has two shapes:

- **Out through a FidoNet gateway** — a netmail message as above, plus
  `To: <address>` as the first line of the body. The door is expected *not* to
  strip that line as a kludge, since UUCP software wants it left in place.
- **A native internet area** — an ordinary message whose `To:` kludge line
  holds the address, used only because it exceeds 25 characters.

The address therefore occupies the subject field, and a message's real subject
has to travel as a `Subject:` kludge line if it is needed at all.

We do not implement any of this yet; see issues #44 and #45.

## TOREADER.EXT (door to reader)

CRLF text. Each line is an identifier and its arguments:

```
ALIAS   <alias>
AREA    <conference#> <flags>
BULL    <filename> <description>
ATTACH  <filename> <conference#> <message#>
FILE    <filename> [description]
KEYWORD <keyword>
FILTER  <filter>
TWIT    <twit>
```

`qwkpack::readToReader()` reads `ALIAS` and `AREA` and ignores the rest.

### AREA flags

Case sensitive. Set by the user's subscription:

| Flag | Meaning |
| --- | --- |
| `a` | all messages |
| `p` | personal messages only |
| `g` | general: personal plus those addressed to ALL |

Set by the mail door, overriding BBS settings:

| Flag | Meaning |
| --- | --- |
| `w` | include mail the user wrote themselves |
| `k` | include in keyword searches |
| `f` | include in filter-exclude searches |
| `F` | forced-read area |
| `B` | blocked from getting replies |

Taken from the BBS's own records:

| Flag | Meaning |
| --- | --- |
| `P` | private mail only |
| `O` | public mail only |
| `X` | either private or public allowed |
| `R` | read-only, no posting at all |
| `Z` | no replies (no continuation of a thread) |
| `L` | local message area |
| `N` | netmail area |
| `E` | echomail area |
| `I` | internet area |
| `U` | newsgroup area |
| `H` | handles-only area |
| `A` | pick-an-alias: any name allowed in From |
| `&` | area allows file attaches |

Of these, `qwkpack::readToReader()` acts on `a`/`p`/`g`, `H`, `A`, `P`, `O`,
`R` and `E`; it treats `U` and `I` as nothing more than a hint to set
`LATINCHAR`, and ignores the rest. So **a QWK conference is never given an
area type** — not `NETMAIL`, not `INTERNET`, not `USENET` — even when the
packet says so plainly (issue #44).

[Synchronet](https://www.synchro.net/) writes a subset of these and adds `Q`
for a QWKnet area, which is not in the spec; see `src/sbbs3/pack_qwk.cpp`.

## TODOOR.EXT (reader to door)

The mirror image, sent up in the `.REP`. `qwkreply::makeOffConfig()` writes
only `AREA <conference#> <flag>` lines, using `a`, `p`, `g` for a subscription
and `D` to drop one.

## See also

- `docs/synchronet-qwk-extensions.md` — `HEADERS.DAT` and `VOTING.DAT`, which
  are Synchronet's extensions rather than QWKE's, and solve the same
  field-length problem a different way.
