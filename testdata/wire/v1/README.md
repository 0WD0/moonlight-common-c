# Moonlight protocol version 1 golden wire corpus

Each `.hex` file is one complete artifact encoded as hexadecimal octets. Its
scope is identified below: a DATAGRAM, lane preface, message envelope with
payload, TLV container, or initiating stream half. Only ASCII whitespace
separates octets; comments, prefixes, and non-hexadecimal characters are
invalid. The SHA-256 values cover decoded bytes, not the textual `.hex`
representation.

| Artifact | Scope | Bytes | Decoded SHA-256 |
| --- | --- | ---: | --- |
| `app-list-success-response.hex` | envelope and payload | 173 | `94d795a2e4dce12e6bf6db06151ce540cbd2efb0854bb916af3c51cbecca8189` |
| `audio-data.hex` | DATAGRAM | 52 | `27d871c6068a816024a923373f84399ad5517075d550cf91bbecd0548d67b1b3` |
| `audio-parity.hex` | DATAGRAM | 52 | `c7d7d64ad1703f7511f503f6a74c4e744571ff99962f341a1a8e69b83246e4fd` |
| `control-coalesced.hex` | initiating stream half | 73 | `07fa4b2cfa8673eb57e2345936107dcae2c177f05cc6a6b10253a90d1bde30fd` |
| `control-early-hello-max.hex` | initiating stream half | 138 | `065d17241af9da8f98a00b1dbcdac8fee4a06addba4c8681c234c6122f1873cf` |
| `goaway-notification.hex` | envelope and payload | 48 | `c87ac84c6f08b18c2386cc63987af9537201f793793d7ce768d7338bb2130e78` |
| `host-info-success-response.hex` | envelope and payload | 177 | `98d11ca451c1f1a391e41fb35b615cbea0d6635d4cd49051005df1b8a7dafc0e` |
| `pair-request-max.hex` | envelope and payload | 184 | `a9d6cadd6e4b24094963efab6124d78a1478cbb253f8a1c368cc9c4d9b9f2e3d` |
| `pairing-proof-transcript.hex` | Client Proof signature transcript | 190 | `9cbe9648e2b917ff5e4f3850f84ab4fa942d5bea663d1bdf1bc79e7afb38d9f0` |
| `ping-success-response.hex` | envelope and payload | 35 | `cf325e5eb19d9daab5c2011e97762bb9ad86bde75713da712fa70bcee0bb9eaf` |
| `rate-limited-error-response.hex` | envelope and payload | 48 | `ac964da8fe2359f7b813d88ca022ec2abe0661ce020c98ad93d42cb07b9833e2` |
| `reliable-input-preface.hex` | lane preface | 16 | `12d6a7ab4f2132b0c46f156b3fec40c9d955f0672dd9fdba5c55cd112ec5db03` |
| `streaming-proof-transcript.hex` | Client Proof signature transcript | 226 | `c6ae10b145465f543e74c7849ef9e9ff2b33dbd9a090132f87767c735b6413f6` |
| `tlv-repeated-nested.hex` | TLV container | 52 | `b8703ca26ee8ed735e34a00051bad8633f5c43f1f5dac5f6660690ab7248e71c` |
| `video-data.hex` | DATAGRAM | 552 | `6f876d51680df4a99d48f8004e691ce1ebb8a8761db467070a2865aaefde2027` |
| `video-parity.hex` | DATAGRAM | 552 | `0597eadbe31816d595a1b4ac667597b0cc1d67f270578995ae9cd36e81a45ea0` |

The Audio parity packet is canonical Reed-Solomon row 4 for four 16-byte
source shards. Its coefficients are `8e f4 47 a7`, producing
`008c0ae8b52200000000000000000000`. The Video parity packet uses the
canonical one-data/one-parity profile, so its coding shard equals the Video
data coding shard.

`control-early-hello-max.hex` is the exact 138-byte 0-RTT ceiling: a Control
preface, a protocol-minor-1 `CLIENT_HELLO` request envelope, and 98 bytes of
canonical TLVs advertising every defined capability, including microphone
DATAGRAM, physical-keyboard input, high-resolution pointer scroll, and
positioned pointer input.
The reverse direction of a bidirectional lane never repeats the preface;
response and notification artifacts therefore begin directly with `SQM1`.

`app-list-success-response.hex` uses post-READY correlation
`0x0102030405060708`. Its sorted records are `desktop` without an icon and
`game.moon` with icon digest bytes `00..1f`; the latter display name is
`Moon Game 🌙`. The opaque next-page cursor is `a0..af`.

`host-info-success-response.hex` uses post-READY correlation
`0x0102030405060708`, Host ID `00..0f`, display name `Sunshine 🌞`,
software version `2026.7.30`, QUIC port 47989, every defined Host capability
and ACL bit, eight maximum and three available Stream Session slots,
authorization generation `0x1112131415161718`, and shared visibility.

`pairing-proof-transcript.hex` uses proof format 1, exporter bytes `00..1f`,
Host ID `20..2f`, Host Identity `30..4f`, P-256 scheme 1, Credential digest
`50..6f`, and admission hash `70..8f`.

`pair-request-max.hex` is the exact maximum 184-byte `PAIR_REQUEST` envelope
and payload, excluding the lane preface as required by `admission_hash`. It
uses correlation ID `0x0102030405060708`, Host ID `00..0f`, token ID
`10..1f`, invitation secret `20..3f`, and a 64-byte ASCII `A` Client name.
Its decoded SHA-256 above is therefore also the request's canonical
`admission_hash`.

`streaming-proof-transcript.hex` uses proof format 1, exporter bytes `90..af`,
Host ID `b0..bf`, Host Identity `c0..df`, Principal ID `e0..ef`, Credential
epoch `0x0102030405060708`, observed authorization generation
`0x1112131415161718`, RSA compatibility scheme 2, Credential digest `00..1f`,
and admission hash `20..3f`. Both transcript artifacts are the exact bytes
passed once to the platform's SHA-256-with-signature operation; no terminating
NUL is present and callers must not pre-hash before using a `SHA256with...`
signing API.
