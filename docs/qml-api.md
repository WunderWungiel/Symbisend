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

## `discovery`
- `scanning` (bool), `scanProgress` (0–100)
- `serverRunning` (bool) — our HTTP server on port 53317 is up
- `multicastListening` (bool) — we receive other devices' announcements
- `refresh()` — announce + rescan the subnet (also connects if offline)
- `announce()`, `scan()` — the two halves separately

## `network`
- `online` (bool), `localAddress` (string), `connectionName` (string, e.g. the WLAN name)
- `connectNetwork()`

## `myDevice` — our identity as other devices see it
- `alias` (string, read/write, persisted), `fingerprint`, `deviceModel`, `port`

## `logger` — debug log
- `logText` (string, last ~300 lines), `logFilePath` (string), `clear()`
