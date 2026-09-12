# Changelog

All notable changes to Hush Hush are documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.1.0/),
and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

## [Unreleased]

## [1.0.0]

### Added

- `hh-gui`, a desktop inspector. 
- `analysis/stats.h`, four statistical tests over a carrier. None of them
  decodes anything: each scores, as a percentage, how far the carrier is from
  one whose low bits are random, which is what a payload leaves behind. The low
  bit ratio is the codec's own rule read backwards, `n1 / (n0 + n1)` over the
  colour samples. The chi-square test is Westfeld and Pfitzmann's pairs of
  values attack, whose percentage is the probability that the sample histogram
  is the one embedding would have left. The histogram of differences runs the
  same ratio over `s[x + 1] - s[x]`, where correlated neighbours hold a
  photograph well below half. The last is the pairs of values test again over
  the quantised DCT coefficients, and is the only one a PNG cannot answer.
- `hh analyse <target>`, which prints those four percentages and what the
  carrier is, with `-e` to explain what each one measures.
- An **Analysis** tab in `hh-gui`, showing the same four scores next to the
  sample histogram, the balance of every pair of values, and the difference and
  coefficient histograms split by parity, drawn with ImPlot.

### Changed

- Release builds are cut per architecture instead of per Ubuntu release: one
  `x86_64` and one `aarch64` set of assets, both built on 22.04 so they link
  against the oldest glibc available. The Ubuntu version is gone from every
  asset name, leaving the loose `hh-<version>-linux-<arch>`,
  `hh-gui-<version>-linux-<arch>` and `libhushhush-<version>-linux-<arch>.a`.
  The `hush-hush-<version>-linux-<arch>.tar.gz` drop is gone: every asset is now
  a single file to download, and the headers are no longer published.
- `inspect` command has been deleted. `hh analyse` replaces the end to end test
  that covered it.
- `file_type_name` moved into `fs/file.h`, where the CLI and the GUI both reach
  it instead of keeping a copy each.
- The rule for which channels a carrier's low bits live in left `analysis/inspect.c`
  and became `pixel_color_channels` and `pixel_slot_to_sample`. Anything that
  walks a carrier now goes through the same two functions, so a new reader
  cannot quietly disagree with the codec about the alpha channel.
- `analysis/inspect.h` now takes a `PixelBuffer` and returns the recovered
  bytes packed into an `LsbStream`, with `InspectRow` filled in place by the
  caller rather than allocated per byte. A megapixel carrier is millions of
  rows, and three allocations each was more than a viewer could carry.
  `hh inspect` reads the same way and prints the same output.
- Opening a JPEG and walking its coefficients moved into
  `handlers/jpeg.c`. `codecs/dct.c` and `analysis/dct.c` had grown their own
  copies of the walk and of the rule deciding which coefficients carry a bit,
  which is exactly the rule the two must never disagree on.

### Fixed

- LSB inspection read the alpha channel, which the codec never writes to, so
  the recovered stream was wrong for any RGBA carrier. It now skips alpha the
  way `codecs/lsb.c` does.
- `hh inspect` leaked its rows and the decoded image on every run, and its
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
