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
| `source`      | string   | `scan`, `register`, `multicast`, `announce`, `address` (debug info) |
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
- `serverHttps` (bool) — the running server speaks https (false while stopped). It
  follows `settings.encryption`, or `webShare.encrypted` while `webShare.receiveActive`;
  a change restarts a running server at once (requests in progress are aborted) and
  re-announces us. Without a usable certificate (e.g. desktop builds) it stays http.
- `multicastListening` (bool) — we receive other devices' announcements
- `refresh()` — announce + rescan the subnet (also connects if offline)
- `autoRefresh` (bool, read/write, default false) — while true, `refresh()` runs every
  60 s, and once right away if the last refresh is older than 15 s. Never connects by
  itself: does nothing while offline. Devices missing from two refreshes in a row are
  dropped, so set it while the device list is visible (the Send page binds it to
  `visible && Qt.application.active && !sender.busy && !receiver.busy`); without it the list is
  only pruned on manual refreshes.
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
- `verification(peerFingerprint)` → `{ text, icons }` — LocalSend's Verify page: `text` is
  both fingerprints sorted and joined, `icons` 16 icon names (SHA-256 of `text`, first 128
  bits in base 260). Show each as `image://verify/<name>/<rrggbb>` (the icon in that colour,
  e.g. `ffffff`; set `sourceSize`). Empty if a fingerprint is unknown. `VerifyDialog.qml`
  shows it (`openFor(fingerprint)`).
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
- `quickSave` (bool, default false) — offers of files are accepted at once, without the
  receive dialog (messages are still shown)
- `quickSaveFromFavorites` (bool, default false) — the same, but only for senders in
  `favorites` (matched by fingerprint: over https the sender's certificate hash, over
  http the fingerprint it claims). The official app's "Quick save for favorites".
- `requirePin` (bool, default false) — senders must send `pin`; can only be turned on while `pin`
  is non-empty (otherwise the write is ignored); turning it off also clears `pin`
- `pin` (string, default `""`) — any text, not only digits. Setting it
  to `""` is ignored while `requirePin` is on. To enable: set `pin`, then `requirePin = true`.
- `downloadFolder` (string, read-only, native separators, e.g. `F:\Symbisend`) — where
  received files go. Default: `F:\Symbisend` if F: exists, else `E:\Symbisend`, else
  `C:\Data\Symbisend`; recomputed each launch. If the chosen folder is unavailable
  (card removed) the default is used, but the choice is kept.
- `downloadFolderIsDefault` (bool) — false once the user picked a folder
- `pickDownloadFolder()` — opens the native folder picker; returns immediately,
  `downloadFolder` changes when the user confirms (nothing happens on cancel)
- `resetDownloadFolder()` — back to the default
- `saveMediaToGallery` (bool, default true) — received images and videos go to the
  phone's media folders instead of `downloadFolder`, like the official app: images to
  `Images\Symbisend`, videos to `Videos\Symbisend` on the download folder's drive
  (`F:\Images\Symbisend`, `C:\Data\Videos\Symbisend`, ...), where the Photos app finds
  them; on desktop the system pictures/movies folder + `Symbisend`. A file counts as media
  by its announced `fileType` (`image/*`, `video/*`; a missing or
  `application/octet-stream` type is guessed from the extension). Not applied when the
  transfer contains folders (any `fileName` with `/`): then everything stays together in
  `downloadFolder`. If a media folder can't be created, the file goes to `downloadFolder`.
- `autoFinish` (bool, default false) — applied in QML (`ReceiveDialog.qml`): after files
  were received (`receiver.state` `finished`, not a message) the Close button counts down
  3 s and the dialog closes by itself, like the official app. Failed/cancelled transfers
  and messages stay open.
- `saveToHistory` (bool, default true) — when off, `receiver` adds nothing to `history`
  (received files and messages are not recorded; existing entries stay), like the official app.
- `receiveViaLinkAutoAccept` (bool, default false) — while `webShare.receiveActive`, every
  file offer is accepted at once (like `quickSave`; messages are still shown). The
  official app's "Automatically accept requests" on its Receive via link page.

## `picker` — what to send (Send page buttons)
Native pickers. Their results go straight into `selection` (connected in `main.cpp`).
- `pickFile()` — native file picker (memory, then browse), one file
- `pickMedia(kind)` — the gallery (Photos) picker, several items; `kind` is `"image"` or
  `"video"` (the gallery picker shows one type at a time)
- `pickFolder()` — native folder picker
- The three `pick*()` methods return immediately; on confirm the result arrives as signal
  `filesPicked(paths)` (list of native paths, e.g. `E:\Images\a.jpg`) or
  `folderPicked(path)`, and is added to `selection`. Nothing is emitted on cancel.
- `clipboardText()` → string — the clipboard's text (`""` if empty); also logged. Not
  added to the selection by itself: call `selection.addText()`.
- `clipboardHasText` (bool) — whether the clipboard holds text (enable/disable Paste).
  Updates by itself when this app changes the clipboard; a copy made in another app is
  **not** signalled (Symbian), so call `checkClipboard()` when the app becomes active
  or the Paste button comes into view.
- `checkClipboard()` — re-reads the clipboard and updates `clipboardHasText`
- `showInfo(text)` — a native information note (Avkon `CAknInformationNote`, e.g.
  "Clipboard is empty"); returns at once, the note hides by itself

## `selection` — what will be sent (ListView model)
Everything picked on the Send page, in the order added; `sender.send()` sends it.

| role           | type   | example                                         |
|----------------|--------|-------------------------------------------------|
| `kind`         | string | `file`, `image`, `video` (by extension) or `text` |
| `name`         | string | `a.jpg`; for text, the text itself              |
| `path`         | string | native path `E:\Images\a.jpg`; `""` for text    |
| `relativePath` | string | `a.jpg`, or `Trip/day1/a.jpg` for folder contents |
| `text`         | string | the message (text items only)                   |
| `size`         | number | bytes (UTF-8 length for text)                   |
| `sizeText`     | string | `1.2 MB`                                        |

Properties: `count` (int), `totalSize` (number, bytes), `totalSizeText` (string, `3.4 MB`).
Methods:
- `addFiles(paths)` — native paths; already selected paths and non-files are skipped
- `addFolder(path)` — every file inside, recursively (`relativePath` starts with the
  folder's name). Hidden files and hidden folders are skipped. At most 1000 new files
  are added per folder; if there are more, the rest is left out and signal
  `folderTruncated(folder, maxFiles)` is emitted (`folder` is the native path; the Send
  page shows a note)
- `addText(text)` — a text message; `""` is ignored
- `setText(index, text)` — edit a text item; `""` removes it
- `remove(index)`, `clear()`
- `formatSize(bytes)` → string, e.g. `formatSize(1300000)` = `1.2 MB`

## `sender` — sending the selection to a device
One transfer at a time (LocalSend v2: `prepare-upload`, then the accepted files one by
one, streamed from disk). The selection is copied when sending starts; it is cleared when
the transfer ends in `finished` and kept after `declined`, `failed` or `cancelled` (to retry).
- `send(fingerprint)` → bool — starts sending `selection` to that peer. Returns false
  (state `failed`, `errorText` says why) when already busy, nothing is selected, offline,
  the peer is unknown, or it speaks protocol v1 (not supported yet).
- `sendToAddress(ip)` → bool — the same for a device typed in by hand, on the default port:
  state `connecting` while it asks who is there (`/register`, https then http; the device
  also joins `peerModel`, `source` = `address`), then as after `send()`. Returns false only
  when already busy or offline; no device at that address ends in state `failed`.
- `cancel()` — stops a running transfer and tells the receiver.
- `submitPin(pin)` — in state `pin`: asks again with that PIN.
- `state` (string):
  - `idle` — nothing sent yet
  - `connecting` — `sendToAddress()` is finding the device; `peerAlias` is the IP until then
  - `waiting` — the receiver's user hasn't accepted yet (no timeout; with quick save this
    passes at once)
  - `pin` — the receiver wants a PIN; `errorText` is `Wrong PIN` after a wrong one
  - `sending` — uploading
  - `finished` — everything accepted was sent (also: a text message was shown, or the
    receiver accepted nothing)
  - `declined` — the receiver said no
  - `failed` — see `errorText` (busy receiver, connection lost, no progress for 60 s, ...)
  - `cancelled` — by us
- `busy` (bool) — `connecting`, `waiting`, `pin` or `sending`
- `peerAlias`, `errorText` (string)
- `fileCount` (int, accepted files), `fileIndex` (int, 1-based, the file being uploaded),
  `fileName` (string, its relative path; `Text` for a text message)
- `bytesSent`, `bytesTotal` (number, bytes of the accepted files), `progress` (0–1)
- `remainingSeconds` (int) — estimated time left from the average speed so far; -1 while
  unknown (before 2 s of sending). Progress properties update every 0.5 s.
- Signals `stateChanged()`, `progressChanged()`.

The Send page calls `send()` when a device is tapped, `sendToAddress()` after
`AddressDialog.qml`, and opens `SendDialog.qml`.

## `receiver` — receiving from other devices
Our server answers LocalSend v2 `prepare-upload`, `upload` and `cancel` (plain HTTP, port
53317, while `discovery.serverRunning`). One session at a time; while busy, other senders
are told we're busy. Files go to `settings.downloadFolder` (images/videos to the media
folders with `settings.saveMediaToGallery`), folders inside it are created
as sent (`Trip/day1/a.jpg`); an existing file is not overwritten, the new one becomes
`a (1).jpg`. Unfinished files are deleted.
- `state` (string):
  - `idle` — nothing received yet, or the last offer was declined
  - `request` — an offer waits for `accept()` / `decline()` (no timeout; the sender may
    withdraw it: `cancelled`). With `settings.quickSave` files skip this state.
  - `receiving` — files are coming in
  - `finished` — everything was saved (or the message was accepted)
  - `failed` — see `errorText` (connection lost, the sender stopped sending for 30 s,
    memory full, ...)
  - `cancelled` — by the sender (`errorText` says so) or by `cancel()`
- `busy` (bool) — `request` or `receiving`
- `errorText` (string)
- The sender: `peerAlias`, `peerDeviceModel` (system, e.g. `Windows`), `peerDeviceType`
  (`mobile`, `desktop`, ...), `peerFingerprint` (over https the SHA-256 of the sender's
  client certificate, which TLS has proven; the fingerprint it claims only over http or
  from a browser), `peerIp`, `peerVersion` (strings),
  `peerHttps` (bool: the sender announced https; the official app offers Verify only then)
- The offer: `fileCount` (int), `totalSize` (number, bytes), `files` (list of
  `{ fileName, size, fileType }`; `fileName` may contain folders)
- `isMessage` (bool), `message` (string) — the offer is a single text message (like the
  official app: one `text/*` file with a preview). It is shown, not saved: accepting it
  goes straight to `finished`.
- `folder` (string, native separators) — where the files go / went: the download folder,
  or the media folder if every saved file went to the same one
- `folders` (string list, native separators) — every folder files were saved to (e.g.
  `F:\Images\Symbisend` and `F:\Symbisend` for photos plus a PDF); final once `finished`
- `filesDone` (int, files saved), `fileName` (string, the file coming in now)
- `bytesReceived`, `bytesTotal` (number), `progress` (0–1), `remainingSeconds` (int, -1
  while unknown). Progress properties update every 0.5 s.
- `accept()`, `decline()` — in state `request`
- `cancel()` — stops receiving (in `request`: same as `decline()`)
- Signals `stateChanged()`, `progressChanged()`.

Network:
- `encryption` (bool, default true) — our server speaks https (LocalSend's "Encryption").
  Applied at once, see `discovery.serverHttps`. Sending to others always uses the protocol
  each peer announced, whatever this says.

`main.qml` opens `ReceiveDialog.qml` whenever `state` becomes `request`.

## `history` — received files and messages (ListView model)
Newest first, at most 200 entries (older ones are dropped), persisted as JSON in its own
file (`history.json` in the app's data folder; older versions' `QSettings` key
`history/entries` is moved there once). `receiver` adds every file once it is saved and
every accepted text message, unless `settings.saveToHistory` is off. Messages longer than
10 000 characters are kept shortened (ending in `…`).
- `count` (int)
- Roles:
  - `kind` (string) — `image`, `video`, `file` (from the extension) or `text`
  - `fileName` (string) — name of the saved file (`a (1).jpg` if it was renamed); empty
    for text
  - `path` (string, native separators) — the saved file; empty for text
  - `text` (string) — text messages only, in full (up to the 10 000 characters kept).
    Don't bind list rows to it: use `preview`
  - `preview` (string) — one line for the list: a message's first non-empty line, cut
    to 150 characters (`…`); the file name for files
  - `size` (number, bytes; UTF-8 length of the whole message for text), `sizeText`
    (string, `1.2 MB`)
  - `time` (date), `timeText` (string, the phone's short date + time format)
  - `senderAlias` (string) — the device that sent it
- `openFile(int row)` → bool — opens that entry's file in the system's app for it
  (`QDesktopServices::openUrl`). False for text entries, a missing file, or when nothing
  could open it.
- `openFolder(string folder)` → bool — shows that folder (pass `settings.downloadFolder`)
  in the phone's File manager (Symbian; started with the folder as its document, or the
  running one switched to it) or the desktop's file browser. Creates the folder if
  missing. False if nothing could show it.
- `clear()` — forgets every entry (saved at once). The received files stay on disk.
- Signal `countChanged()`.

## `favorites` — favourite devices (ListView model)
Like the official app's favourites, keyed by fingerprint, persisted in `QSettings`
(`favorites` array). Used by `settings.quickSaveFromFavorites`.

| role          | type   | example                                        |
|---------------|--------|------------------------------------------------|
| `fingerprint` | string | the device's fingerprint                        |
| `alias`       | string | the name shown                                  |
| `customAlias` | bool   | the user typed the name; false = it follows the device's alias whenever the device is discovered again |
| `ip`          | string | `192.168.1.20`                                  |
| `port`        | int    | `53317`                                         |

- `count` (int), signal `countChanged()`
- `contains(fingerprint)` → bool
- `favorite(fingerprint)` → `{ fingerprint, alias, customAlias, ip, port }`, or `{}`
- `add(fingerprint, alias, deviceAlias, ip, port)` → bool — adds the device, or updates
  it if it is a favourite already. `alias` is what the user typed: `""` or the same as
  `deviceAlias` (the device's own alias) means automatic. False (nothing stored) for an
  empty fingerprint, an IP that isn't IPv4 (`a.b.c.d`) or a port outside 1–65535.
- `remove(fingerprint)`
- Signal `favoritesChanged(fingerprint)` — after `add`, `remove` or an automatic rename;
  call `contains()` / `favorite()` again (they don't notify by themselves).
- `FavoriteDialog.qml` is the "Add to favourites" dialog (`openFor(peerModel.peer(...))`)
  and the "Edit favourite" one (`openEdit(favorites.favorite(...))`). An edit with an
  empty name while the device's own alias is unknown keeps the stored name (automatic,
  updated when the device is discovered again).
- `FavoritesPage.qml` lists them (heart button next to Refresh on the Send page).

## `webShare` — "Receive via link" (browsers send files to us)
Like the official app: while active, opening `url` in any browser on the network shows
LocalSend's own upload page (`web/upload.html`, copied unchanged from the official app),
which sends the chosen files through the normal `prepare-upload`/`upload` endpoints, so
they arrive through `receiver` as a sender with `peerAlias` `Web Browser`,
`peerDeviceType` `web`. While inactive, `/` answers with LocalSend's 403 page.
- `receiveActive` (bool, read-only)
- `url` (string) — `http://<our IP>:53317` (`https://` while `discovery.serverHttps`), or
  `""` while offline.
- `encrypted` (bool, read/write) — while active, the server speaks https instead of
  following `settings.encryption` (the server restarts, `url` changes). Like the official
  app it is not persisted: `startReceive()` and `stop()` set it to false. Browsers warn
  about our self-signed certificate; show the official hint "LocalSend uses a self-signed
  certificate. You need to accept it in your browser." while it is on.
- `pin` (string, read/write, `""` = none) — while active, required from every sender
  (browsers ask for it), replacing `settings.pin`. `startReceive()` sets it to
  `settings.pin` if `settings.requirePin`, else `""`; changing it doesn't touch the
  settings. `stop()` clears it.
- `startReceive()`, `stop()` — `ReceiveLinkPage.qml` calls them when it opens / closes.
- Signals `receiveActiveChanged()`, `urlChanged()`, `pinChanged()`, `encryptedChanged()`.
- Also see `settings.receiveViaLinkAutoAccept`. Not implemented yet: "Share via link"
  (browsers downloading from us).

## `logger` — debug log
- `model` — list model of the last ~300 lines, oldest first, role `line` (string). Use it
  as a ListView model: lines arrive in batches (every ~200 ms at most), and only new lines
  are added, so the view doesn't re-layout the whole log.
- `logText` (string, the same lines joined with newlines; rebuilt on every read, so
  prefer `model` for display), `logFilePath` (string), `clear()`
- Signal `logChanged()` after every batch and after `clear()`.
