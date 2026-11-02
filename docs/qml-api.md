# Symbisend QML API

C++ objects exposed as context properties (set in `main.cpp`). The QML in
`qml/Symbisend/` is a plain functional placeholder — restyle or replace freely,
as long as it uses only what is listed here.

## `peerModel` — discovered LocalSend devices (ListView model)
Roles (use as `model.<role>` in delegates; `alias` clashes with a QML keyword):

| role          | type     | example                         |
|---------------|----------|---------------------------------|
| `alias`       | string   | `Adorable Mango`                |
| `ip`          | string   | `192.168.1.20`                  |
| `port`        | int      | `53317`                         |
| `protocol`    | string   | `https` / `http`                |
| `version`     | string   | `2.1`                           |
| `deviceModel` | string   | `Windows`, `Samsung`, ...       |
| `deviceType`  | string   | `mobile`, `desktop`, `web`, `headless`, `server` |
| `fingerprint` | string   | unique device id                |
| `source`      | string   | `scan`, `register`, `multicast`, `announce` (debug info) |
| `lastSeen`    | datetime |                                 |

Properties: `count` (int).  Methods: `clear()`.

- `peer(fingerprint)` → object with all the roles above plus `logs`, or `{}` if the
  fingerprint is unknown. `logs` is a list of `{ time, text }`, oldest first, at most 100:
  one entry for every time the device was confirmed (announcement, `/register`, scan
  answer), like LocalSend's device details page. `time` is the local short time
  (`7:27 PM`), `text` is `Discovered via HTTPS (192.168.1.79)` for the first entry and
  `Updated via HTTPS (192.168.1.79)` for later ones. Logs are cleared along with the peer
  (expiry, `clear()`, going offline).
- Signal `peerChanged(fingerprint)` — a peer was added or re-confirmed; call `peer()`
  again to refresh.

## `discovery`
- `scanning` (bool), `scanProgress` (0–100)
- `serverRunning` (bool) — our HTTP server on port 53317 is up
- `multicastListening` (bool) — we receive other devices' announcements
- `refresh()` — announce + rescan the subnet (also connects if offline)
- `announce()`, `scan()` — the two halves separately
- `startServer()`, `stopServer()`, `restartServer()` — the HTTP server and the multicast
  listener together (whether others can find us / send to us); `serverRunning` reflects
  the result. While stopped we don't announce ourselves, but `scan()` still works.
  Stopping is not persisted. `startServer()` while offline connects first.

## `network`
- `online` (bool), `localAddress` (string), `connectionName` (string, the access point
  actually in use, e.g. the WLAN name)
- `connectNetwork()`
- When the connection is lost (out of range, AP off) `online` turns false, `peerModel` is
  cleared and discovery stops. There is no automatic reconnect: `connectNetwork()` or
  `discovery.refresh()` reconnects, and discovery restarts by itself once online again.

## `myDevice` — our identity as other devices see it
- `alias` (string, read/write, persisted), `fingerprint`, `deviceModel`, `port`
- `randomizeAlias()` — picks a new random "Adjective Fruit" name
- Changing the alias re-announces us (if the server is running).

## `settings` — app settings (persisted)
All read/write unless noted. Settings marked *(stored only)* are not acted on yet.

General:
- `theme` (string) — `"dark"` (default) or `"light"`; other values are ignored.
  Applied in QML: light = `platformInverted` on the window and every component
  (each page has `property bool platformInverted: settings.theme == "light"`).
- `language` (string) — `"system"` (default) or a language code (`"en"`). No
  translations exist yet *(stored only)*

Receive:
- `quickSave` (bool, default false) *(stored only)*
- `requirePin` (bool, default false) *(stored only)*
- `downloadFolder` (string, read-only, native separators, e.g. `F:\Symbisend`) — where
  received files go. Default: `F:\Symbisend` if F: exists, else `E:\Symbisend`, else
  `C:\Data\Symbisend`; recomputed each launch. If the chosen folder is unavailable
  (card removed) the default is used, but the choice is kept.
- `downloadFolderIsDefault` (bool) — false once the user picked a folder
- `pickDownloadFolder()` — opens the native folder picker; returns immediately,
  `downloadFolder` changes when the user confirms (nothing happens on cancel)
- `resetDownloadFolder()` — back to the default
- `saveMediaToGallery` (bool, default true) *(stored only)*
- `autoFinish` (bool, default false) *(stored only)*
- `saveToHistory` (bool, default true) *(stored only)*

## `logger` — debug log
- `logText` (string, last ~300 lines), `logFilePath` (string), `clear()`
