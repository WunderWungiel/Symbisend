# Parity with the official LocalSend apps

Goal: Symbisend's behaviour (logic) should be nearly identical to the official Android/desktop apps.

Audit of 2026-11-04: Symbisend `fd4ea6f` against official `main@92e133a` (app 1.18.2+64, protocol 2.2). Both codebases were read; nothing was run, so the scenarios below come from the code, not from tests. Line numbers are from `fd4ea6f` and will drift. Before fixing an item, check that it still applies.

The official source isn't in this repo:

```bat
git clone --depth 1 https://github.com/localsend/localsend.git
git clone --depth 1 https://github.com/localsend/protocol.git
```

## Bugs and wrong outcomes

### 1. Receiver cancel doesn't work

- [x] Fixed

- **Ours:** `Receiver::cancel()` (`src/receiver.cpp:579`) never tells the sender. An incoming `/cancel` for a session we're sending is ignored (`src/receiver.cpp:380`). If an official receiver cancels our only file, its server lets the upload finish, so we show "Sent" and clear the selection.
- **Official:** the receiver POSTs a pinned `/cancel`. The sender matches session ID, IP and sending state, then shows "cancelled by receiver".
- **Fix:** keep the sender's port and protocol in `Receiver` (today only `m_peerHttps`), POST `/cancel` pinned to `m_peerFingerprint`, and forward unknown sessions to a new `Sender` hook.
- **Official source:** `receive_controller.dart` (`cancelSession`, `onCancelReceived`), `send_provider.dart` (`cancelSessionByReceiver`), `packages/core/src/http/server/v2.rs` (`cancel`), `mod.rs` (`cancel_v2_session`).

### 2. Quick save shows nothing

- [x] Fixed

- **Ours:** the dialog only opens on state `request` (`qml/Symbisend/main.qml:67`). Quick save, favourites and link auto-accept skip that state, so there's no progress, Cancel or result.
- **Official:** the progress page opens at once.
- **Fix:** also open the dialog on `receiving`, and on `failed` for when the download folder can't be created.

### 3. One failed file ends everything

- [x] Fixed

- **Ours:** the sender cancels the whole session (`src/sender.cpp:446`). The receiver also ends it on a write error (`src/receiver.cpp:431`, `:461`).
- **Official:** the file is marked failed, the other files continue, and the session ends as `finishedWithErrors` with no `/cancel`.
- **Fix:** add per-file status, a `finishedWithErrors` state, and treat failed files as final in `checkDone()`. Do this together with #1, because they share the same session states.
- **Official source:** `upload_isolate.dart`, `send_provider.dart` (`_finish`), `v2.rs` (`finalize_file`), `session.rs` (`is_complete`).

### 4. PIN rules differ

- [x] Fixed

- **Ours:** the check runs after body parsing and the busy check (`src/receiver.cpp:199`). It allows 5 attempts (`kMaxPinAttempts`), and the right PIN still works after lockout.
- **Official:** PIN is checked first, then the body, then busy. After 3 wrong attempts it answers 429 even for the right PIN, until the server restarts. An empty `pin=` counts as a wrong attempt.
- **Fix:** reorder the checks and copy the limits. The official server restarts for every PIN or web share change and ours doesn't, so those changes clear the counter too.
- **Official source:** `packages/core/src/http/server/common/pin.rs`, `v2.rs` (`prepare_upload`).

### 5. The announce burst is wasted

- [x] Fixed

- **Ours:** `m_recentReplies` drops the later messages even if the first register failed (`src/discovery.cpp:405`). The burst exists to cover peers that aren't ready yet.
- **Official:** every message is answered.
- **Fix:** remove the entry on failure, or skip only while a request is in flight or after success.

## Logic differences

### Discovery

- [x] Done

- **Ours:** refresh is always a full /24 scan, at startup and every 60 s on the Send page (`src/discovery.cpp:300`). Devices expire after two missed refreshes (`src/discovery.cpp:747`).
- **Official:**
  - It announces and probes favourites, then scans only if nothing was confirmed within 1 s.
  - There is no periodic refresh and no expiry.
  - The Send tab scans only when the list is empty, and manual refresh clears the list.
- **Official source:** `packages/core/src/discovery/mod.rs` (`discover_staged`), `scan_facade.dart`, `send_tab_vm.dart` (`SendTabInitAction`), `send_tab.dart` (`_ScanButton`).

This is the biggest logic change, and it should also save battery and CPU on the phone.

### Checksums (the only protocol change in 2.2)

- [x] Verify on receive, answer 422, allow retries
- [x] Retry on 422 when sending
- [x] Send `sha256` (with `settings.createChecksums`, off by default)
- [x] Bump version to 2.2

- **Ours:** verifies while the file streams in and answers 422. A mismatched file is deleted and may be uploaded again, 3 uploads in all. The sender retries a 422 the same way, and sends `sha256` only with the setting on.
- **Official:** sends `sha256`, verifies on receive, answers 422 on a mismatch, and retries up to 3 times into the same target.
- **Note:** the official app uses the version only for display.
- **Official source:** `v2.rs` (`finalize_file`, `MAX_UPLOAD_ATTEMPTS`), `common/save.rs`, `upload_isolate.dart`, `server_isolate.dart` (`_ReceiveSession.targets`).

### Accepting files

- [x] Done, except quick actions (random names, counters)

- **Ours:** the Options page of the offer selects and renames files, and changes the folder and gallery saving for that session.
- **Official:** the same.
- **Official source:** `receive_options_page.dart`, `selected_receiving_files_provider.dart`.

### Favourites

- [x] Send from Favourites
- [x] Add a favourite by its address
- [x] Favourite's name and check mark in the device list
- [x] Favourite's name for incoming transfers and history
- [x] Probe favourites during discovery

- **Ours:** tapping a favourite registers at its stored address and sends to whichever device answers. The Add button asks the address who it is. The name replaces the device's own in the device list, the send and receive dialogs, and history.
- **Official:** the same.
- **Note:** this list also had a favourite button on the receive dialog, but the official receive page has none (checked at `92e133a`), so we don't have one. The official marker in the device list is a check circle, not a heart.
- **Official source:** `favorite_dialog.dart`, `favorite_edit_dialog.dart`, `send_tab.dart` (`nameOverride`), `device_list_tile.dart`, `send_page.dart`, `receive_controller.dart` (`senderAlias`), `receive_page.dart`.

### Messages

- [x] Done

- **Ours:** saved to history when it arrives, with Verify, Copy, Open (for links) and Close. It stays on screen if the sender withdraws it.
- **Official:** the same.
- **Note:** neither app writes a lone message to a file. Only a text sent with other items, or one without a preview, is uploaded and saved, under the name the sender gave it (`<uuid>.txt`).
- **Official source:** `receive_controller.dart` (`onPrepareUpload`), `receive_page.dart`, `receive_session_state.dart` (`message`), `selected_sending_files_provider.dart` (`AddMessageAction`).

### Smaller items

- [x] **Quick save:** official is one setting (off, favourites, on) that defaults to favourites. Ours stays two independent switches, both off, by choice (see Keep as is).
- [x] **Auto finish:** applies to sending too, in both apps. Only a fully sent transfer counts down.
- [x] **After quick save:** after a fully successful quick save, the session closes by itself and a dialog offers to open the last file for 5 s, in both apps. Not for link auto accept, messages or errors.
- [x] **File timestamps:** a saved file gets the sender's `modified` and `accessed` times, in both apps.
- [x] **Text preview:** both send `preview` only for a lone text.
- [x] **History size:** both keep 30 entries.

### Connections

- [ ] Done

We send `Connection: close` on every response (`src/httpserver.cpp:517`), and mbedTLS has no session tickets. Every file therefore costs a full RSA handshake on the phone. Official keeps connections alive, which matters most for folders of small files.

## Missing features

- [ ] **Share via link:** `prepare-download`, `download`, the download page, `download:true`, accept prompts (`packages/core/src/http/server/web.rs`, `send_controller.dart`)
- [ ] **Multiple send mode:** several parallel sessions
- [ ] **Per-file progress:** file list, speed, retrying failed files
- [ ] **Address dialog:** `#123` suffix mode and recent devices
- [ ] **Settings:** port, multicast group, discovery timeout, device type and model, turning off checksum verification
- [ ] **Translations:** the language setting is stored but does nothing

## Robustness

- [x] **Connection limits:** 64 in total and 8 per IP, in both apps. A connection from this device only counts towards the total.
- [x] **Self-healing:** official restarts a listener that reports an error, and connects to its own port when the app is resumed on Android and iOS. It also rebinds multicast on iOS, and gives up on a multicast socket after 10 failed reads. Ours does the same checks on resume and after 10 failed reads, and retries whatever isn't running (see Keep as is).
- [x] **JSON depth guard:** nesting over 32 levels is refused before `JSON.parse`, which recurses. Official's serde_json stops at 128, but our stack is 80 KB, and LocalSend messages have about 4 levels.

## Keep as is

These differences are deliberate or better for the phone:

- Trying https and then falling back to http.
- 3 s scan timeout and 24 parallel requests (official uses 500 ms and 50), because TLS is slower on the phone.
- Uploading one file at a time (official does 2).
- Deleting incomplete files.
- Stall and sender-gone timeouts.
- Skipping repeats of an announce burst while a register is running or after it succeeded (official answers every message), to save TLS handshakes.
- The encryption toggle applying immediately.
- The `/v1/info` route, which official keeps for old clients.
- Announcing when we go online. Official announces nothing until its Send tab opens, but a phone can drop off the network and come back, and other devices wouldn't notice. It's only UDP; going online doesn't scan.
- No automatic discovery during a transfer, and the Send tab's check also runs when the connection comes up, since a phone can be offline when the tab opens.
- Creating checksums when sending is a setting that's off by default (official has it on), because hashing large files first is slow on a phone. Verifying on receive is always on.
- A file with a checksum mismatch is deleted and the retry saves it again under the same name. Official overwrites the same target.
- Favourites are probed with https, then http, like the subnet scan (official uses only the protocol from its own settings). Each probe has our 3 s scan timeout, so the 1 s grace can start up to 3 s later (6 s with the http retry) when a favourite is away.

- Tapping a favourite with nothing selected says so and stays on the page. Official registers first and then opens its add-file dialog, which we can't show on top of a pushed page.
- The Favourites page keeps a delete button on each row, next to the edit button. Official has Delete inside its edit dialog.
- A favourite added by address is custom-named when the typed name differs from the device's own, so discovery doesn't overwrite it. Official stores it as automatic, and the next discovery replaces the typed name. Adding a device that is a favourite already updates it. Official adds a duplicate.
- Quick save is two independent switches, both off by default. Official is one setting that defaults to favourites, so turning one on there turns the other off.
- After a quick save we stay on the current page. Official goes to the Receive tab, because its progress is a page that has to be left. Ours is a dialog.
- The Open file dialog always has Folder. Official hides "Show in folder" for files saved to the gallery, but our gallery is an ordinary folder.
- Symbian files have only a modification time, so `accessed` isn't applied there.
- Renaming a file from a sent folder changes only its name, and it stays in its folder. Official's dialog refuses the `/` on Android and Windows, so there the file can only leave its folder.
- An answered connection doesn't count towards the 8 per IP. We close after every answer, so a sender opens one connection per file, and a slow close must not fail the next file.
- A server or multicast listener that won't start or dies is tried again after 5 s, doubling up to 5 min, while online. Official tries a dead server once and doesn't restart multicast after failed reads.
- The foreground check connects to the WLAN address (official: loopback), because a Symbian socket belongs to its access point. It waits 3 s (official: 1 s) and is skipped while clients are connected, because it shares a thread with the rest of the app. Qt 4 has no signal for a dead listener, so this is the only way to notice one.

`/v1/register` and the UDP reply are harmless extras that official no longer has, and can be removed whenever convenient.

## Suggested order

1. **Quick fixes:** #2, #4, #5, auto finish on send, preview.
2. **Transfer failures:** #1 and #3 together.
3. **Discovery and favourites:** staged discovery, then favourites.
4. **Protocol and receive:** receiver checksums and version 2.2, then selective accept and the message flow.
5. **Performance and protection:** keep-alive, connection limits and the JSON guard.
6. **Features:** Share via link, then multi-send and per-file progress.

## Open decisions

1. **Is multi-send worth it on a Nokia 808?** It's a large change to `Sender` for little use on one phone. Recommendation: skip it unless exact parity matters more.
