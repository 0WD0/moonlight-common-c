# Moonlight protocol version 1 golden wire corpus

Each `.hex` file is one complete artifact encoded as hexadecimal octets. Its
scope is identified below: a DATAGRAM, lane preface, message envelope with
payload, TLV container, or initiating stream half. Only ASCII whitespace
separates octets; comments, prefixes, and non-hexadecimal characters are
invalid. The SHA-256 values cover decoded bytes, not the textual `.hex`
representation.

| Artifact | Scope | Bytes | Decoded SHA-256 |
| --- | --- | ---: | --- |
| `audio-data.hex` | DATAGRAM | 52 | `27d871c6068a816024a923373f84399ad5517075d550cf91bbecd0548d67b1b3` |
| `audio-parity.hex` | DATAGRAM | 52 | `c7d7d64ad1703f7511f503f6a74c4e744571ff99962f341a1a8e69b83246e4fd` |
| `control-coalesced.hex` | initiating stream half | 73 | `07fa4b2cfa8673eb57e2345936107dcae2c177f05cc6a6b10253a90d1bde30fd` |
| `control-early-hello-max.hex` | initiating stream half | 138 | `8d767889a177ba2514683fcd7a4c0e8dcac08bcfe8d24b05b0db21c581f5f56e` |
| `goaway-notification.hex` | envelope and payload | 48 | `c87ac84c6f08b18c2386cc63987af9537201f793793d7ce768d7338bb2130e78` |
| `ping-success-response.hex` | envelope and payload | 35 | `cf325e5eb19d9daab5c2011e97762bb9ad86bde75713da712fa70bcee0bb9eaf` |
| `rate-limited-error-response.hex` | envelope and payload | 48 | `ac964da8fe2359f7b813d88ca022ec2abe0661ce020c98ad93d42cb07b9833e2` |
| `reliable-input-preface.hex` | lane preface | 16 | `12d6a7ab4f2132b0c46f156b3fec40c9d955f0672dd9fdba5c55cd112ec5db03` |
| `tlv-repeated-nested.hex` | TLV container | 52 | `b8703ca26ee8ed735e34a00051bad8633f5c43f1f5dac5f6660690ab7248e71c` |
| `video-data.hex` | DATAGRAM | 552 | `6f876d51680df4a99d48f8004e691ce1ebb8a8761db467070a2865aaefde2027` |
| `video-parity.hex` | DATAGRAM | 552 | `0597eadbe31816d595a1b4ac667597b0cc1d67f270578995ae9cd36e81a45ea0` |

The Audio parity packet is canonical Reed-Solomon row 4 for four 16-byte
source shards. Its coefficients are `8e f4 47 a7`, producing
`008c0ae8b52200000000000000000000`. The Video parity packet uses the
canonical one-data/one-parity profile, so its coding shard equals the Video
data coding shard.

`control-early-hello-max.hex` is the exact 138-byte 0-RTT ceiling: a Control
preface, a `CLIENT_HELLO` request envelope, and 98 bytes of canonical TLVs.
The reverse direction of a bidirectional lane never repeats the preface;
response and notification artifacts therefore begin directly with `SQM1`.
