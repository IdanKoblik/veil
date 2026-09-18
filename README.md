<div align="center">
  <h1>Veil</h1>

  <img src="assets/logo.png" alt="veil" width="300">

  <p>General Purpose Steganography Toolkit.</p>
</div>

---

## About

veil is a general-purpose steganography toolkit for hiding data inside
different types of carriers.

The project is designed around a modular core that separates the steganography
logic from the carrier format. This makes it possible to support different
media formats and hiding techniques without coupling them to the CLI.

> ⚠️ GUI is marked as deprecated and would not get any updates!
>

## Install

Prebuilt `veil` binaries for `x86_64` and `aarch64` Linux are published on the
[Releases](https://github.com/IdanKoblik/veil/releases) page.

**Install script:**

Downloads the release binary and man page, verifies them against
`checksums.txt` and installs them under `PREFIX` (default `/usr/local`):
```sh
curl -fsSL https://raw.githubusercontent.com/IdanKoblik/veil/main/devtools/install.sh | bash
```

Pin a version or change the install location with `VERSION` and `PREFIX`:
```sh
curl -fsSL https://raw.githubusercontent.com/IdanKoblik/veil/main/devtools/install.sh | VERSION=1.0.1 PREFIX=~/.local bash
```

To uninstall (use the same `PREFIX` you installed with):
```sh
curl -fsSL https://raw.githubusercontent.com/IdanKoblik/veil/main/devtools/uninstall.sh | bash
```

**Arch Linux:**

A `PKGBUILD` is provided under `packaging/arch`:
```sh
git clone https://github.com/IdanKoblik/veil.git
cd veil/packaging/arch
makepkg -si
```

**AppImage (GUI):**

`veil-gui` can be packaged as an AppImage. The script requires `cmake`, `curl`
and ImageMagick (`magick`), and downloads `linuxdeploy` on first run:
```sh
./packaging/build_appimage.sh
./Veil-*.AppImage
```

## Build from source

**Clone the repository:**
```sh
git clone https://github.com/IdanKoblik/veil.git
cd veil
```

**Configure and build:**
```sh
cmake --preset dev
cmake --build --preset dev
```

The exact available presets can be inspected with:
```sh
cmake --list-presets
```

## Supported carriers

Veil is being developed around a carrier abstraction so that different media
types can implement their own storage mechanisms.

Current and planned carrier types include:

| Carrier | Technique                    | Status |
|---------|------------------------------|--------|
| PNG     | LSB                          | ✅     |
| JPEG    | DCT coefficients             | ✅     |
| MP4     | Video transform coefficients | ✅     |

Support is actively evolving as the carrier abstraction is developed.

## Encryption

When a passphrase is supplied, the payload is encrypted and authenticated
with XChaCha20-Poly1305 before being embedded into the carrier, under a key
derived from the passphrase with Argon2. A wrong passphrase or a tampered
carrier is refused instead of producing garbage.

The passphrase is also used for deterministic pseudo-random placement when
supported by the carrier. This prevents the encoded payload from necessarily
being stored sequentially throughout the carrier.
