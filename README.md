# Symbisend

Symbisend is a [LocalSend](https://localsend.org) client for Nokia phones running
Symbian^3, Anna and Belle. It sends and receives files and text to and from any device
running LocalSend (Android, iOS, Windows, macOS, Linux) over your local Wi-Fi network:
no internet connection, no cables, no accounts.

It is written in Qt 4 C++ with a Qt Quick 1.1 / Symbian Qt Quick Components UI and speaks
LocalSend protocol v2, including its encrypted (HTTPS, mutual TLS) mode, so it works with
the official apps out of the box. A MeeGo Harmattan (Nokia N9) port is planned.

## Features

- **Discovery** – finds LocalSend devices on the network automatically (multicast, with a
  subnet scan as fallback) and lets them find you.
- **Send** – files, photos and videos from the gallery, whole folders, or a piece of text.
- **Receive** – accept or decline incoming transfers; received items are kept in a history.
- **Favourites** – remember devices you send to often; devices that weren't found
  automatically can be reached by entering their IP address.
- **Receive via link** – anyone with a web browser can upload files to your phone through
  the official LocalSend web page, opened from a link or QR code.
- **Encryption** – HTTPS with self-signed certificates and fingerprint pinning, like the
  official app (can be turned off in Settings).

Not yet implemented: *Share via link* (sending to a browser).

## Requirements

- A Symbian^3, Anna or Belle phone with the Qt 4.8 runtime and Qt Quick Components
  (tested on the Nokia 808 PureView, Belle).
- The phone and the other device on the same Wi-Fi network.

## Building

Symbisend is built on Windows with the old Nokia **Qt SDK** (Symbian target, Qt 4.7.4,
GCCE). It is expected at `C:\QtSDK\Symbian\SDKs\Symbian3Qt474`; edit the path in
`build_symbian.bat` if yours is elsewhere.

```bat
git clone --recurse-submodules https://github.com/WunderWungiel/Symbisend.git
cd Symbisend
build_symbian.bat            :: release build
build_symbian.bat debug      :: debug build
```

If you cloned without `--recurse-submodules`, run `git submodule update --init` first
(mbedTLS lives in a submodule).

The result is an unsigned **`Symbisend.sis`** in the repository root. Copy it to the phone
and install it. Notes:

- The build is in-source only (Qt 4.7 with sbsv2 doesn't support shadow builds), so
  generated files (`Makefile`, `*.mmp`, `moc_*.cpp`, …) appear in the repository root;
  they are gitignored.
- The package is deliberately left unsigned: the app uses capabilities that a self-signed
  certificate cannot grant. The phone has to allow installing unsigned packages.

## Troubleshooting

Symbisend logs heavily. Check the in-app **Log** page, or the log file in `E:/Symbisend/`
(falling back to `F:` or `C:/Data`).

## For developers

- `src/` – C++ core (networking, discovery, HTTP/TLS server, sender, receiver, models).
- `qml/Symbisend/` – the UI, compiled into the binary through `qml.qrc`.
- `docs/qml-api.md` – the interface between C++ and QML.
- `CLAUDE.md` – architecture and platform constraints in more detail.

## Third-party code

- [mbedTLS](https://github.com/Mbed-TLS/mbedtls) 3.6 LTS (Apache-2.0) – server-side TLS,
  `external/mbedtls/`.
- [QR Code generator](https://github.com/nayuki/QR-Code-generator) by Project Nayuki
  (MIT) – `external/qrcodegen/`.
- LocalSend web pages (Apache-2.0, © the LocalSend authors) – `web/`.

Symbisend is an independent project and is not affiliated with LocalSend.
