<div align="center">
  <h1>baguette</h1>

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

## Download

Prebuilt `veil` binaries are published on the
[Releases](https://github.com/IdanKoblik/veil/releases) page.

```sh
# Grab the latest release binary
curl -L -o baguette https://github.com/IdanKoblik/veil/releases/latest/download/veil
chmod +x veil-{}
./veil-{}
```

## Build from source

**Clone the repository:**
```sh
git clone https://github.com/IdanKoblik/veil.git
cd veil
```

** Configure and build:**
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

| Carrier | Technique | Status |
|---------|-----------|--------|
| PNG     |   LSB     |    ✅    |
| JPEG     |   DCT coefficients     |    ✅    |
| MP4     |   Video transform coefficients     |    🚧    |

Support is actively evolving as the carrier abstraction is developed.

## Encryption

When a passphrase is supplied, the payload can be encrypted before being
embedded into the carrier.

The passphrase is also used for deterministic pseudo-random placement when
supported by the carrier. This prevents the encoded payload from necessarily
being stored sequentially throughout the carrier.
