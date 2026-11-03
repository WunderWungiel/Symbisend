# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## What this is

Symbisend is a [LocalSend](https://localsend.org) (protocol v2) client for Symbian^3/Anna/Belle phones, written in Qt 4 C++ with a Qt Quick 1.1 / Symbian Qt Quick Components UI. A MeeGo Harmattan port is planned (`qtc_packaging/debian_harmattan/`, `Symbisend_harmattan.desktop`), so C++ should stay platform-neutral and keep Symbian-specific code behind `#ifdef Q_OS_SYMBIAN`. Currently implemented: device discovery, choosing what to send, sending (`Sender`), receiving (`Receiver`), and "Receive via link" (`WebShare`: browsers upload through the official LocalSend page in `web/`). Not yet: "Share via link".

## Build

```bat
build_symbian.bat            :: release (default)
build_symbian.bat debug
```

- Uses the Qt SDK at `C:\QtSDK\Symbian\SDKs\Symbian3Qt474` (Qt **4.7.4**, GCCE, `symbian-sbsv2` spec) and produces an **unsigned** `Symbisend.sis` in the repo root. The app needs developer-certificate capabilities (`ReadDeviceData`, … for the gallery picker) that self-signing can't grant, so the script passes `QT_SIS_OPTIONS=-o` (unsigned only, no capability patching) and renames `Symbisend_unsigned.sis`; the phone accepts unsigned packages.
- **In-source build only** — Qt 4.7 + sbsv2 does not support shadow builds. qmake/sbsv2 output (`Makefile`, `bld.inf`, `*.mmp`, `*.pkg`, `*.rss`, `moc_*.cpp`, `*.sis`, …) lands in the repo root and is gitignored.
- When adding source files, add them to `SOURCES`/`HEADERS` in `Symbisend.pro`. Each entry goes on its own real line, and every line but the last ends with ` \` (line continuation). Never write a literal `\n` into the file: a scripted edit once produced `src/tlsrandom.cpp \n    src/tlsserver.cpp` on one line. After editing the `.pro`, check it with `grep -nF '\n' Symbisend.pro` (should find nothing).
- There are no tests or linters. Verification happens on a real device (Nokia 808, Belle, Qt 4.8 runtime): hand over the `.sis` and say what to look for on the in-app Log page / log file (`E:/Symbisend/`, falling back to `F:` or `C:/Data`).

## Platform constraints (compile against Qt 4.7.4, run on 4.8)

- No `QJsonDocument` — use `src/json.h` (`Json::parseObject` / `Json::serialize`), which goes through `QScriptEngine`'s `JSON` object (hence `QT += script`).
- No `QUdpSocket::joinMulticastGroup()` — `MulticastReceiver` uses a raw Open C / POSIX BSD socket polled by a timer (disabled on Windows via `SYMBISEND_NO_RAW_MULTICAST`). `NetworkManager::setDefaultInterfaceForOpenC()` calls `setdefaultif()` so that Open C uses the same access point as the Qt `QNetworkSession`.
- Old C++ (pre-C++11): no `auto`, lambdas, `nullptr`, or new-style `connect`; use `SIGNAL()/SLOT()`.
- TLS, client side (sending, discovery requests): Qt/OpenSSL with a system-level patch on the phone that provides TLS 1.2/1.3. Do not add TLS protocol code there; only `ignoreSslErrors` for LocalSend's self-signed certificates.
- TLS, server side: our server speaks `https` through mbedTLS (`src/tlsserver.{h,cpp}`: TLS 1.2, ECDHE-RSA with AES-GCM/ChaCha20, our `ClientIdentity` certificate, no client certificate asked; `HttpServer` feeds socket bytes through a `TlsSession` per connection) or plain `http`. `Discovery` picks it from `settings.encryption` (default on), or from `webShare.encrypted` (default off, not persisted) while "Receive via link" is active, and restarts the server when that changes — like the official app. Without a usable certificate `HttpServer` falls back to http.
- mbedTLS 3.4.1 (Symbian port) is compiled into the app: the sources are the git submodule `external/mbedtls/upstream` (`git submodule update --init` after cloning), our glue is `external/mbedtls/mbedtls.pri` + config `symbisend_config.h` (see its README). It needs no system-wide mbedTLS. Use the shared RNG in `src/tlsrandom.h` (`TlsRandom::random` / `context()`) for every mbedTLS call that takes `f_rng`. Include mbedTLS headers last, inside `extern "C"`: on Symbian they undefine `EXPORT_C`/`IMPORT_C`.
- The official LocalSend server requires **mutual TLS** and ignores a `/register` whose fingerprint isn't the SHA-256 of the client certificate. `src/clientidentity.cpp` creates our certificate once with libcrypto (Open C, SDK headers 0.9.8g — use only functions exported there) and installs it as the default `QSslConfiguration`; `DeviceInfo`'s fingerprint is that certificate's hash. A handshake failing with OpenSSL `reason(1116)` = alert `certificate_required`.

## Architecture

`main.cpp` builds the object graph and exposes it to QML as context properties: `logger`, `myDevice`, `peerModel`, `network`, `discovery`, `settings`, `picker`, `selection`, `sender`, `receiver`, `webShare`. `Picker` (native file/gallery/folder pickers) feeds `SelectionModel` (what will be sent) through signals connected in `main.cpp`. **`docs/qml-api.md` is the contract between C++ and QML** — update it whenever a context property, Q_PROPERTY, role or Q_INVOKABLE changes. The QML in `qml/Symbisend/` is a deliberately bare functional placeholder; the user designs the real UI.

Flow:
1. `NetworkManager` opens a `QNetworkSession` (Symbian access point), finds the local IPv4 address/prefix, and emits `onlineChanged`.
2. On going online, `Discovery` starts `HttpServer` on port 53317, joins multicast group `224.0.0.167:53317` via `MulticastReceiver`, and runs `refresh()` (= `announce()` + `scan()`).
3. Peers are found three ways, all ending in `PeerModel::upsert()` (keyed by fingerprint, `source` records how it was found):
   - incoming multicast announcements → reply by POSTing `/register` to the sender;
   - incoming `/register` and `/info` requests to our server (`Discovery` implements `HttpHandler::handleRequest`);
   - HTTP(S) subnet scan fallback via `QNetworkAccessManager` (bounded concurrency, per-request timeout, driven by `onScanTick`).
4. `DeviceInfo` holds our identity (alias, fingerprint persisted in `QSettings`) and builds register/announce JSON bodies.
5. `Logger` installs a Qt message handler: every `qDebug`/`qWarning` goes to the log file and the last ~300 lines are shown on the Log page. Log liberally with a `Component:` prefix — it is the main debugging channel on device.

`HttpServer` is a minimal HTTP/1.1 server: one request per connection, `Content-Length` or chunked bodies. It speaks http or https (see TLS above). Several `HttpHandler`s share it (`Discovery`: `/register`, `/info`; `Receiver`: `/prepare-upload`, `/upload`, `/cancel`; `WebShare`: `/`, `/i18n.json`). The browser pages in `web/` are the official LocalSend assets, unchanged, compiled in via `web.qrc` — keep them identical to upstream (see `web/README.md`). A handler may answer later (return status 0, then `HttpServer::respond()`; prepare-upload waits for the user) and may take the body as a stream (`streamsBody()`/`bodyData()`; uploads go straight to disk). Buffered bodies are capped at 1 MB.

### Icons

Use the stock Symbian theme icons whenever one fits: pass the name as `iconSource` (`toolbar-back`, `toolbar-refresh`, `toolbar-settings`, `toolbar-share`, `toolbar-home`, `toolbar-add`, `toolbar-delete`, `toolbar-search`, `toolbar-menu`, `toolbar-list`, `toolbar-next`, `toolbar-previous`, `toolbar-dialer`, `toolbar-mediacontrol-{play,pause,stop,forward,backwards}`). These follow `platformInverted` automatically. Only add your own SVG in `qml/Symbisend/images/` when no stock icon fits (e.g. device types, info). Use Material Design icons, as LocalSend does, in a white `name.svg` and a black `name-inverted.svg` (SVG Tiny 1.2), and pick between them with `platformInverted`.

`qmlapplicationviewer/` is Qt Creator–generated boilerplate; don't modify it or the `qtcAddDeployment()` lines in the `.pro`.
