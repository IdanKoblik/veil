# Changelog

All notable changes to Veil are documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.1.0/),
and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

## [Unreleased]

## [2.0.0]

### Added

- Add support for MP4 video files.
- Add data checksum to container header. 
- `veil(1)` man page, installed with the CLI and published with each release.
- `devtools/install.sh` and `devtools/uninstall.sh`, which fetch the release binary and check it against `checksums.txt`.
- An Arch `PKGBUILD` and `packaging/build_appimage.sh` for a `veil-gui` AppImage.

### Changed

- Add Data piping to the cli
- LSB & DCT replacement is now **deprecated**, and cannot be selected as a codec.

### Fixed

- **Security:** a passphrase now encrypts the payload itself.

## [1.0.0]

### Added

- `veil-gui`, a desktop inspector. 

### Changed

- Release builds are cut per architecture instead of per Ubuntu release: one
  `x86_64` and one `aarch64` set of assets, both built on 22.04 so they link
  against the oldest glibc available. The Ubuntu version is gone from every
  asset name, leaving the loose `veil-<version>-linux-<arch>`,
  `veil-gui-<version>-linux-<arch>` and `libveil-<version>-linux-<arch>.a`.
  The `veil-<version>-linux-<arch>.tar.gz` drop is gone: every asset is now
  a single file to download, and the headers are no longer published.
- `inspect` command has been deleted. `veil analyse` replaces the end to end test
  that covered it.

### Fixed

- LSB inspection read the alpha channel, which the codec never writes to, so
  the recovered stream was wrong for any RGBA carrier. It now skips alpha the
  way `codecs/lsb.c` does.
- `veil inspect` leaked its rows and the decoded image on every run, and its
  pixel buffer again on the failure path.

## [0.1.1]

### Added

- JPEG carriers. `encode` and `decode` now accept a JPEG target and hide the
  payload in the low bits of its quantised DCT coefficients, with the same
  container, passphrase derived keys and scattered payload as the PNG path.
  The image is rewritten straight from its coefficients, so nothing is
  requantised.

### Changed

- The container format (magic, sealed header, scatter walk, encryption) moved
  out of the LSB codec into `codecs/container.c`, which works against a
  `Carrier` of abstract bit slots. `codecs/lsb.c` supplies the pixel carrier and
  `codecs/dct.c` the coefficient one.
