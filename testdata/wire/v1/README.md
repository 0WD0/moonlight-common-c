# Moonlight protocol version 1 golden packets

Each `.hex` file is the complete DATAGRAM encoded as hexadecimal octets. Only
ASCII whitespace separates octets; comments, prefixes, and non-hexadecimal
characters are invalid. The SHA-256 values below cover the decoded packet
bytes, not the textual `.hex` representation.

| Packet | Bytes | Decoded SHA-256 |
| --- | ---: | --- |
| `audio-data.hex` | 52 | `27d871c6068a816024a923373f84399ad5517075d550cf91bbecd0548d67b1b3` |
| `audio-parity.hex` | 52 | `c7d7d64ad1703f7511f503f6a74c4e744571ff99962f341a1a8e69b83246e4fd` |
| `video-data.hex` | 552 | `6f876d51680df4a99d48f8004e691ce1ebb8a8761db467070a2865aaefde2027` |
| `video-parity.hex` | 552 | `0597eadbe31816d595a1b4ac667597b0cc1d67f270578995ae9cd36e81a45ea0` |

The Audio parity packet is canonical Reed-Solomon row 4 for four 16-byte
source shards. Its coefficients are `8e f4 47 a7`, producing
`008c0ae8b52200000000000000000000`. The Video parity packet uses the
canonical one-data/one-parity profile, so its coding shard equals the Video
data coding shard.
