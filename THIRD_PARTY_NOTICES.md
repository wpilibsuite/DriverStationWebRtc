# Third-party notices

DriverStationRtcClient builds or redistributes the following third-party
software. The configure step creates an aggregate `LICENSE.txt` containing the
complete text of every license listed here.

| Component | Use | License |
| --- | --- | --- |
| Mbed TLS 3.6.7 | Statically linked TLS and cryptography | Apache-2.0 OR GPL-2.0-or-later |
| mbedtls-framework | Mbed TLS build-time support | Apache-2.0 OR GPL-2.0-or-later |
| p256-m | Statically linked through Mbed TLS | Apache-2.0 OR GPL-2.0-or-later |
| Project Everest | Statically linked through Mbed TLS | Apache-2.0 |
| libdatachannel 0.24.5 | Statically linked WebRTC implementation | MPL-2.0 |
| libjuice 1.7.2 | Statically linked ICE implementation | MPL-2.0; its picohash implementation is public domain |
| libsrtp (`24b3bf8`) | Statically linked SRTP implementation | BSD-3-Clause |
| usrsctp (`fec583d`) | Statically linked SCTP implementation | BSD-3-Clause and BSD-2-Clause |
| plog (`94899e0`) | Header-only logging used by libdatachannel | MIT |
| nlohmann/json | Bundled libdatachannel example dependency; not linked by the current build | MIT |
| OpenH264 2.6.0 API | Downloaded build headers | BSD-2-Clause |
| Cisco OpenH264 2.6.0 binary | Redistributed shared codec runtime | Cisco binary license, BSD-2-Clause, and AVC/H.264 patent terms |

The authoritative license sources included in the aggregate are:

- `LICENSE`
- `mbedtls/LICENSE`
- `libdatachannel/LICENSE` (the identical MPL-2.0 text also applies to libjuice)
- `libdatachannel/deps/libsrtp/LICENSE`
- `libdatachannel/deps/usrsctp/LICENSE.md`
- `libdatachannel/deps/plog/LICENSE`
- `libdatachannel/deps/json/LICENSE.MIT`
- `licenses/LibjuicePicohash.txt`
- `licenses/UsrsctpSctpSsFunctions.txt`
- `licenses/OpenH264Source.txt`
- Cisco's `BINARY_LICENSE.txt`, downloaded with the selected OpenH264 binary

Platform SDK, C/C++ runtime, and operating-system libraries are supplied by
the target toolchain or operating system and are not redistributed as separate
files by this project.

## OpenH264 binary distribution

Cisco's binary license conditions include requirements that the Cisco binary
be downloaded separately to the end user's device, that the end user can
control its use, and that software using it displays the attribution
`OpenH264 Video Codec provided by Cisco Systems, Inc.` The complete controlling
terms are reproduced verbatim in the generated `LICENSE.txt`. Anyone packaging
or distributing the OpenH264 runtime should review those terms for the intended
delivery model.
