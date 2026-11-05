# Harmattan (Nokia N9) port audit

Goal: find everything that is specific to Symbian in Symbisend, and what it needs on MeeGo 1.2 Harmattan.

Audit of 2026-11-05, on `26e468a`. Sources: this repo, the Harmattan Developer Library (`~/Desktop/harmattan-dev-md`), and the Madde target `harmattan_10.2011.34-1_rt1.2` with its sysroot (`C:\QtSDK\Madde\sysroots\harmattan_sysroot_10.2011.34-1_slim`).

Given, and not audited: Qt's own TLS on the N9 (the OpenSSL 1.0.2 patch with its rebuilt QtNetwork). See item 3 for the one place where our code touches OpenSSL directly.

## How each finding was checked

- **Built:** seen in a real Harmattan build of a scratch copy of the repo.
- **Sysroot:** read from the SDK's headers, libraries or `com.nokia.meego` sources.
- **Docs:** stated in the Developer Library.
- **Device:** nothing here ran on an N9. These are open questions, collected in [Test on the N9](#test-on-the-n9).
- **My knowledge:** not in the docs or the SDK. Also listed for checking on the device.

## Summary

The C++ side is close. The unmodified tree **compiles, links and packages for Harmattan with no errors** (Built: `Symbisend` armel PIE, `symbisend_0.0.1_armel.deb`). Every Symbian API is already behind `Q_OS_SYMBIAN`, and the networking, HTTP server and mbedTLS need no changes to build.

That build is not usable yet, for three reasons:

1. **The UI doesn't load.** All QML but one file imports `com.nokia.symbian`, which doesn't exist on Harmattan. The result is a black screen.
2. **The pickers have no native alternative.** Harmattan has no system file, gallery or folder picker. The current non-Symbian fallback is `QFileDialog`, a QWidget, which the docs advise against. This is the largest piece of new C++/QML.
3. **Files go to the wrong place.** The default download folder is on the 2 GB system partition, invisible over USB and not meant for user files.

Item 3 below (our direct `libcrypto` link) is the one to look at first, because it may keep the app from starting at all, depending on how the patch installs OpenSSL.

## Symbian feature map

| Feature | Symbian code | Harmattan today (`#else` branch) | Needs | Item |
|---|---|---|---|---|
| UI components | `com.nokia.symbian 1.1` in 27 QML files | module missing, black screen | `com.nokia.meego 1.0` UI | 1 |
| Light/dark theme | `platformInverted` | property missing | `theme.inverted`, reversed meaning | 1 |
| Theme metrics | global `platformStyle`, `privateStyle` | don't exist as globals | own constants / `UIConstants.js` values | 1 |
| Stock icons | `iconSource: "toolbar-*"` on `ToolButton`, `TabButton` | taken as a relative URL, no icon | `ToolIcon { iconId }`, `image://theme/...` | 1 |
| File picker | Avkon `AknCommonDialogsDynMem` | `QFileDialog` | own QML browser | 2 |
| Folder picker | Avkon, in `Picker` and `Settings::chooseFolder` | `QFileDialog` | own QML browser, made asynchronous | 2 |
| Gallery picker | `MGFetch` | `QFileDialog` with a filter | QtMobility gallery or browser | 2 |
| Information note | `CAknInformationNote` | blocking `QMessageBox` | `InfoBanner` | 4 |
| Open folder | starts File manager (`FileMgr.exe`) | starts File Manager (OpenRepos), optional | done | 5 |
| Drive letters, media folders | `F:`, `E:`, `C:/Data` | `~/Downloads`, `QDesktopServices` | `/home/user/MyDocs/...` | 6 |
| Log folder | `E:/Symbisend` | `~/.symbisend` | `MyDocs` or data location | 6 |
| History file | private folder | `DataLocation`, works | none | - |
| Client certificate | libcrypto (Open C) | `-lcrypto` = 0.9.8 | mbedTLS instead | 3 |
| Access point binding | `setdefaultif()` | not needed | none | - |
| Network session | `QNetworkSession` | same code, ICD bearer | background flag, WLAN check | 7 |
| Raw multicast socket | Open C | plain POSIX | none (one token to confirm) | 10 |
| File times | `RFs::SetModified` | `utime()` | none | - |
| TLS random source | `TRandom` | `/dev/urandom` | none | - |
| Capabilities | `TARGET.CAPABILITY` in `.pro` | Aegis manifest, empty today | see item 8 | 8 |
| Packaging | `.sis`, `build_symbian.bat` | Debian package, unfinished | see item 9 | 9 |
| Build system | sbsv2 in-source | `mad` via Madde | build script | 9 |
| Hardware/orientation | keypad, 640x360 | touch only, 854x480 | see item 1 | 1 |

## Blockers

### 1. The whole QML UI is Symbian-only

- [ ] Fixed

The module tree on Harmattan is `com/nokia/{meego,extras,OMB,OviNotifications}` (Sysroot). QML has no conditional import, so every file that imports a component set needs a Harmattan version.

**Measured use** (a script compared every component and property we use with the `com.nokia.meego` sources in the sysroot):

| Use in `qml/Symbisend/` | Count | On Harmattan |
|---|---|---|
| `import com.nokia.symbian 1.1` | 27 of 28 files | Fix: `import com.nokia.meego 1.0` |
| `platformInverted` | 418 | No such property. Setting it is a load error. Use `theme.inverted`. |
| `platformStyle.*` | 296 | No global. Meego components have their own `platformStyle` *property*, of a different kind. |
| `privateStyle.toolBarHeightLandscape` | 11 | No equivalent. |
| `platformContentMaximumWidth/Height` | 20 | Symbian dialogs only. |
| `CommonDialog` | 9, in 7 files | Not exported by the meego `qmldir` (the file exists, but `SelectionDialog` uses it internally). Replace with `Dialog` or `Sheet`. Also 16 lines of `buttonTexts`, `onButtonClicked` and similar. |
| `ListItem`, `ListItemText` | 2, 3 (`PeerItem`, `SettingsItem`) | Don't exist. `ListDelegate` in `com.nokia.extras`, or a plain `MouseArea`. |
| `TabBar` | 1 (`main.qml`) | Doesn't exist. Tabs go in the toolbar: `ToolBarLayout { ButtonRow { TabButton {} } }`. |
| `Switch.onClicked` | 1 (`SettingsSwitch`) | Meego `Switch` has no `clicked` signal. Use `onCheckedChanged`, guarded against changes from the model. |

These exist on both and carry over: `Page`, `PageStackWindow`, `TabGroup`, `TabButton`, `Label`, `Button`, `ButtonRow`, `TextField`, `TextArea`, `QueryDialog`, `Dialog`, `SelectionDialog` (including `titleText`), `ContextMenu`, `MenuLayout`, `MenuItem`, `ScrollDecorator`, `ProgressBar`, `BusyIndicator`, `CheckBox`, `Switch`, `ToolBarLayout`. Their sizes and defaults differ.

**Behaviour that changes, beyond names:**

- **Theme is reversed.** Symbian: default dark, `platformInverted` = light. Harmattan: default light, `theme.inverted` = dark (`Style.qml`, Sysroot). `settings.theme` stays the same, but the mapping flips, and so does the choice between `name.svg` and `name-inverted.svg`.
- **Dialogs sit on a dark overlay,** even in the light theme. A default `Label` inside one uses the theme's dark text, so dialog content needs explicit light colours (My knowledge, check by eye).
- **Text entry.** Eight files use `TextField`/`TextArea`, several in dialogs. `Sheet` is the Harmattan pattern for this, and `SipAttributes` replaces Symbian's input handling.
- **Icons.** Meego `ToolButton` is a `Button` with `iconSource` as a `url` (Sysroot), so `"toolbar-back"` finds nothing. The toolbar uses `ToolIcon { iconId: "toolbar-back" }` (Docs: Using Bottom Toolbar). The slim sysroot has no theme icons, so each of our names (`toolbar-home`, `-share`, `-settings`, `-refresh`, `-add`, `-delete`, `-mediacontrol-play/stop`) must be checked on the device. `toolbar-view-menu` replaces `toolbar-menu`.
- **Screen.** 854x480 against the 808's 640x360, and fonts are larger (`UIConstants.js`: default 24, large 28, small 20). `ReceiveDialog.qml:30` assumes 360 px; a few other sizes are fixed numbers.
- **Touch only,** no close buttons, and leaving the app by swiping (Docs: Porting). We have no quit button, which suits this. Every page needs a way back in the toolbar.
- **Rotation.** Handled per page with `orientationLock`. The docs say not to use `Qt::WA_*Orientation`, which `main.cpp` sets through `setOrientation(ScreenOrientationAuto)`. On Harmattan, skip that call. `qmlapplicationviewer/` stays untouched.
- **Visibility.** The docs say to use `platformWindow.active`/`visible`. `Qt.application.active` (`SendPage.qml`) and `QEvent::ApplicationActivate` (`Discovery::eventFilter`) probably work too, since they come from Qt, but check it.
- **Text.** `AboutPage.qml:50` says "A LocalSend client for Symbian."

**Decision for you (UI designer): how to organise the QML.**

| Option | For | Against |
|---|---|---|
| **A. Separate tree** `qml/Harmattan/`, `qml_harmattan.qrc`, path chosen in `main.cpp` (recommended) | Symbian UI untouched, no slowdown on the 808, free to follow N9 patterns (toolbar tabs, sheets). The Porting guide itself says reimplementing the UI is often the better choice. | Two UIs to maintain. Logic inside QML gets duplicated; move it to C++ or a shared `.js`. |
| B. One tree with thin wrappers for each component, chosen by `qrc` alias | One copy of the page logic | ~20 wrappers, an extra object per `Label` on the 808. `platformStyle` must be renamed everywhere anyway, since inside a meego component the name means that component's own style. Dialogs still differ. |
| C. Rewrite the QML with a script at build time | Little upfront work | Fragile, and dialogs, tabs and list items can't be scripted. |

I recommend A because `docs/qml-api.md` is already the shared contract, and almost all logic is in C++. The cost is that each new UI feature gets done twice.

### 2. No native pickers

- [ ] Fixed

`Picker::runFilePicker`, `runMediaPicker`, `runFolderPicker` (`src/picker.cpp:122`, `:146`, `:182`) and `Settings::chooseFolder` (`src/settings.cpp:283`).

- **What Harmattan has:** no picker service. `GalleryInterface` (`maemo-meegotouch-interfaces`) can only *show* media, not return a choice (Sysroot). The `#else` branch uses `QFileDialog`, and the docs advise against QWidget UI on Harmattan because it's too small for touch.
- **Files and folders:** a new QML page over `Qt.labs.folderlistmodel` (the import is in the sysroot) or a small C++ `QDir` model, rooted at `/home/user/MyDocs`. Multi-select is possible here, which Symbian's `pickFile()` lacks.
- **Gallery:** two options. `QtMobility.gallery` (`DocumentGalleryModel`, in the sysroot) gives a real grid of everything Tracker indexed, but needs `TrackerReadAccess` and `TrackerWriteAccess` (Docs). Or reuse the browser with an image/video filter, opened on `MyDocs/DCIM` and `MyDocs/Pictures`, which needs no tokens. I'd start with the browser and add the gallery later.
- **API change:** the pick calls can stay, but they must turn into a request to QML (a signal such as `pickRequested(kind)`) and a result coming back (`picker.finishPick(paths)`). `filesPicked` and `folderPicked` stay as they are.
- **Harder part:** `Settings::chooseFolder` is **synchronous**, and `Receiver` (`src/receiver.cpp:442`) relies on that. A QML page can't return a value that way, so both callers must become signal-based. On Symbian the same interface can keep calling Avkon.
- Update `docs/qml-api.md`.

### 3. Our direct `libcrypto` link may clash with the patched OpenSSL

- [ ] Fixed

`Symbisend.pro` links `-lcrypto` for `ClientIdentity::generate()` (`src/clientidentity.cpp`). Built result: the binary has `NEEDED libcrypto.so.0.9.8`.

Stock Harmattan QtNetwork does not link OpenSSL: it has no `NEEDED libssl` and loads `libssl.*` at run time (Sysroot, `readelf`, `strings`). So our binary is what pulls in libcrypto, before Qt loads anything. The SDK's 0.9.8 has versioned symbols (`OPENSSL_0.9.8`). What happens depends on how the patch installs 1.0.2, which I couldn't find out:

| The patch... | Result with today's link |
|---|---|
| keeps 0.9.8 installed, adds 1.0.2 beside it | Two libcryptos in one process. 0.9.8 loads first, so 1.0.2's libssl may bind to its functions and crash or misbehave. |
| replaces the `libcrypto.so.0.9.8` file with 1.0.2 | The loader may refuse with ``version `OPENSSL_0.9.8' not found``, or warn, if 1.0.2 has no such version. |
| ships a compatible library under that name | Works. |

**Recommended fix: don't link libcrypto on Harmattan at all.** `generate()` only makes a key and a self-signed certificate once and hands PEM text to Qt. mbedTLS, already in the binary, can do this: `symbisend_config.h` enables `MBEDTLS_GENPRIME`, `MBEDTLS_X509_CRT_WRITE_C`, `MBEDTLS_PK_WRITE_C` and `MBEDTLS_PEM_WRITE_C`. Keep these from the current code: X.509 v3, SHA-256, RSA 2048, CN "LocalSend User", dates 2000-4096, and a PKCS#1 key PEM. Use `TlsRandom`. Then drop `-lcrypto` for non-Symbian builds. Symbian can stay as it is, or move later so only one version exists.

Quick check first, on the N9: `ldconfig -p | grep -E 'ssl|crypto'`.

## Compiles, but wrong on the N9

### 4. `Picker::showInfo` blocks

- [ ] Fixed

`src/picker.cpp:93`. The contract in `picker.h` says it returns at once and hides by itself, but the `#else` branch is a modal `QMessageBox::information()`, a QWidget. Called from 11 places in QML.

**Fix:** a signal (`infoRequested(text)`) on Harmattan, shown with `InfoBanner` from `com.nokia.extras` in `main.qml`.

### 5. "Open folder" has nowhere to go

- [x] Fixed

`HistoryModel::openFolder` fell back to `QDesktopServices::openUrl` on a folder. The docs list `file:///` for files, opened by the type's handler. The N9 has no stock file manager (My knowledge), so this would likely fail. `openFile` should be fine (Docs).

**Done:** `openFolder` starts [File Manager](https://openrepos.net/sites/default/files/packages/1873/filemanager_0.2.10_armel.deb) (package `filemanager`, 0.2.10) through `invoker`, and the package lists it under `Suggests`. Disassembling its `main()` shows that an argument not starting with `-` becomes the start folder. It has no D-Bus service and no `MimeType`, so a copy that is already running only comes to the front, in the folder it was showing. `history.canOpenFolder` is false without the package; the Harmattan UI must hide the button with it (item 1).

### 6. Paths

- [ ] Fixed

Docs: `/home/user` is a 2 GB partition for app data, `/home/user/MyDocs` (VFAT, 58 GB) holds user files and is what USB exposes. Applications must not store in `HomeLocation`, and filling `/home/user` is an explicit warning.

| Code | Today | Should be |
|---|---|---|
| `Settings::defaultDownloadFolder` (`src/settings.cpp:203`) | `~/Downloads/Symbisend` | `/home/user/MyDocs/Downloads/Symbisend` |
| `Settings::mediaFolder` (`src/settings.cpp:219`) | `QDesktopServices` locations, unverified on the N9 | `/home/user/MyDocs/Pictures/Symbisend`, and the video folder (name to confirm with `ls`) |
| `pickLogDir` (`src/logger.cpp:26`) | `~/.symbisend` | `/home/user/MyDocs/Symbisend`, so the log can be copied over USB |
| `HistoryModel::filePath` (`src/historymodel.cpp:312`) | `DataLocation`, `mkpath` done on save | Fine. Remove the `homePath` fallback at `:314`. |

Tracker should index `MyDocs`, so received media reaches Gallery with no code (My knowledge).

**USB mass storage.** While the phone is in that mode, `MyDocs` isn't available to apps (My knowledge). A `mkpath` then writes into the bare mount point on the small partition, or fails. Symbian's `QDir(path.left(3)).exists()` check has no equivalent, so `defaultDownloadFolder` needs a real test (for example, `/proc/mounts`), and a transfer needs a clear failure.

File names need nothing: `Receiver::safeRelativePath` already removes what VFAT refuses, and `MyDocs` has the same limits as a Symbian drive, including 4 GB per file.

### 7. Network behaviour

- [ ] Fixed

`NetworkManager` compiles unchanged and `libqicdbearer.so` is the bearer (Sysroot). The docs describe some differences:

- **Background connections.** A connection not caused by the user must set `session->setSessionProperty("ConnectInBackground", true)`. `connectNetwork()` runs at start and from `Discovery::startServer()`/`refresh()`. The refresh is the user's, but start-up and automatic retries aren't quite. Add a parameter. Harmless on Symbian.
- **The device moves between networks by itself.** "Will change between networks without asking from applications." Our code assumes a session stays on one access point: `onSessionOpened` reads the address once. After a switch, the address, server and multicast join may be stale. Check whether the session reports it, and if not, watch `QNetworkConfigurationManager`.
- **Cellular.** Nothing checks the bearer. `defaultConfiguration()` may give 3G when no WLAN is near, which would start a server and a subnet scan on an operator's address. Check `bearerName()` and refuse to go online, or show "Connect to WLAN".
- **Power saving.** "Existing connections are monitored with an idle timer that terminates unused connections." A phone waiting for files is idle by that measure. We then go offline and stay there (`goOffline`: reconnecting is left to the user), so receiving silently stops. Possibly a retry on the next activation is enough.
- **USB.** `detectLocalAddress` may pick `usb0` if the session interface is unknown (developer mode, My knowledge). Skip it.

### 8. Aegis and background rules

- [ ] Fixed

- **Manifest.** The current one is fine: no tokens needed today, and the build's scan found nothing (Built: `No findings`, `Aegis-Manifest: empty`). The scan also warned `QtDeclarative used, but could not detect QML content`, because our QML is inside the binary. **So tokens for QML modules won't be found automatically.** If item 2 uses `QtMobility.gallery`, list them by hand, and keep both `<for>` lines (the binary and `applauncherd-launcher::`), as the docs require for boosted apps.
- **Private key and PIN.** `QSettings` writes them in plain text to `~/.config/Symbisend/Symbisend.conf`. On Symbian that is a private folder; on Harmattan other third-party apps can likely read it (My knowledge). The options are the Aegis protected storage (`aegis-crypto` is in the sysroot) or accepting this. It is your call; the key only protects a LAN identity, and I'd accept it for a first version.
- **Minimised state.** The docs forbid background processing unless the user asked for it. Our server is event-driven with no polling, and waiting for files is what the user asked for. The multicast fallback poll (`src/multicastreceiver.cpp`, every 5 s until the first datagram) is only for Open C, so turn it off on Harmattan.
- **Killed in background.** Apps can get SIGKILL when minimised. Settings are written at once and history within 1 s, so that's fine.
- **Backup.** Only Tracker and GConf are backed up by default (Docs), so identity and settings won't survive a restore without a backup config. Low priority.

### 9. Packaging and build

- [ ] Fixed

Building needs no `.pro` work, apart from item 3. Packaging is the Qt Creator wizard's unfinished output (Built):

- `control`: `Maintainer: unknown <>`, and placeholder `Description` and `Homepage`.
- **The package has no `Depends` at all,** because `dh_shlibdeps` is commented out in `rules` and `${shlibs:Depends}` stays empty. Enable it (and the `qmake`/`$(MAKE)` lines) for builds outside Qt Creator, or list the dependencies.
- `Build-Depends` needs `applauncherd-dev`, and the package needs `applauncherd` (Docs).
- `Symbisend_harmattan.desktop` is good (`invoker --type=d -s`). The `X-Osso-*` and `X-HildonDesk-*` lines are Fremantle leftovers.
- Add `debian/` and `*.deb` to `.gitignore`.
- `build_harmattan.bat` is missing. What worked, run in a directory with no Symbian output:

```sh
# C:\QtSDK\Madde\bin\bash.exe --login, MSYSTEM=MSYS
mad -t harmattan_10.2011.34-1_rt1.2 qmake Symbisend.pro
mad -t harmattan_10.2011.34-1_rt1.2 make -j4
cp -r qtc_packaging/debian_harmattan debian
mad -t harmattan_10.2011.34-1_rt1.2 dpkg-buildpackage -nc -uc -us -b
```

I removed the `moc_*`, `Makefile` and other Symbian output from the copy first. They share the repo root, so a Harmattan build needs a copy or a shadow build. Qt 4.7 + sbsv2 can't shadow-build, but Madde should; I didn't try it.

- **The booster:** `QmlApplicationViewer` inherits `QDeclarativeView`, which the QML booster's prerequisites rule out, so the cached view isn't used. It still runs, only starting a bit slower. Leave it (`qmlapplicationviewer/` isn't ours to change). `Q_DECL_EXPORT main` is already right.
- **Message handler:** the docs warn of crashes after `main()` if a custom handler stays installed. `Logger::install` (`src/logger.cpp:115`) never removes it. Add `qInstallMsgHandler(0)` at the end of `main()`, after the objects are gone. This matters more if MeeGo Touch is linked (item 11).

### 10. Small fixes

- [ ] Fixed

- **`deviceModel` is wrong.** `src/deviceinfo.cpp:69` tests `MEEGO_EDITION_HARMATTAN`, but that define comes from `qplatformdefs.h`, which isn't included. Built: `deviceinfo.o` contains `"Qt"`, not `"Nokia"`. The N9 would announce itself as "Qt". Add `#include <qplatformdefs.h>`. Every future Harmattan `#ifdef` has the same trap.
- **Runtime is 4.7.4 here.** Docs and sysroot agree, so "compile against 4.7.4, run on 4.8" in `CLAUDE.md` doesn't hold on the N9. Nothing in our code depends on 4.8, but mention it there.
- **Multicast token.** `CAP::net_broadcast` is described as "broadcasting and listening to multicast" (Docs: Aegis examples). Linux needs no capability for `IP_ADD_MEMBERSHIP`, and the docs say not to request unneeded tokens, so leave it out unless the join fails on the device.
- **SVG.** `VerifyIconProvider` and the QML images need `libqsvg.so` and `libQtSvg` (both in the sysroot). Check on the device or declare the dependency.
- **Comments.** "Open C" notes in `multicastreceiver.cpp` remain true for Symbian only; the code path works on Linux too.

## Optional integration

### 11. Things Harmattan adds

- [ ] Decided

Not needed for parity with the Symbian build:

- **Incoming transfers while minimised.** Swiping away is the normal way out on the N9, and `ReceiveDialog` only opens inside the app, so a request can time out unseen. This is the most valuable item here. Easiest is `viewer.activateWindow()`; better is `MNotification` (`meegotouch` headers in the sysroot, with an event type file under `/usr/share/meegotouch/notifications/eventtypes/`).
- **Share menu.** A Share UI plugin (`ShareUI` headers in the sysroot, installed to `/usr/lib/share-ui/plugins/`) would add "Symbisend" to Gallery's share list. Needs `TrackerReadAccess`.
- **Open with.** `MimeType=` in the `.desktop` file.

## Needs no change

Checked by the build, and by reading the code:

- HTTP/HTTPS server, sender, receiver, discovery logic, models, JSON, verification.
- mbedTLS with `symbisend_config.h` and `-lrt`: no `platform_glue.c` code applies, and it links (Built). ARM tests on QEMU were done earlier.
- `MulticastReceiver`: the POSIX path is the same code, and `QSocketNotifier` is native on Linux.
- `setdefaultif()`, `TRandom`, `RFs`, `EPOCHEAPSIZE`, UIDs, capabilities: Symbian-only, all behind `#ifdef` or `symbian:`.
- `QClipboard`, `checkClipboard()`: the extra call is harmless.
- `QSettings`, `HistoryModel` storage, `utime()`.
- `web/` assets, `.qrc` resources.

## Suggested order

1. On the N9, run the OpenSSL check. Do item 3.
2. Item 10 (`qplatformdefs.h`) and item 6 paths: small, and they make logs reachable.
3. Decide the QML option. Make a minimal Harmattan UI: main page, Receive, Send, Settings, Log. This is the first build worth installing.
4. Test network and multicast against the official app, using the list below. Do item 7 from what the log shows.
5. Items 2 and 4: browser page, asynchronous `chooseFolder`, `InfoBanner`.
6. The rest of the UI, then items 5, 8, 9.
7. Item 11, as you choose.

## Test on the N9

Things only the device can answer:

- `ldconfig -p | grep -E 'ssl|crypto'`: how the patch installed OpenSSL. After a run, `grep -E 'ssl|crypto' /proc/<pid>/maps` must show one libcrypto only.
- `ls /home/user/MyDocs`: exact folder names (Pictures, video, Downloads).
- `ls /usr/share/themes/blanco/meegotouch/icons | grep toolbar`: stock icon names.
- `Multicast: joined` in the log, and datagrams arriving with the screen on and off (WLAN power saving may filter multicast).
- Server reachable from another device (no firewall).
- WLAN switch, leaving range, power saving mode: what the `Network:` log lines show.
- Behaviour on 3G with no WLAN.
- `Qt.application.active` and `ApplicationActivate` on minimise/restore.
- USB mass storage during a transfer, and at app start.
- `QDesktopServices::openUrl` on a received file with no tokens. The docs say libcontentaction needs `TrackerReadAccess`, and Qt may use it underneath.

Sources: the pages named above are in `~/Desktop/harmattan-dev-md/guide/html/`, mainly *Porting applications to Harmattan*, *Data storage*, *Network connectivity*, *Application lifecycle*, *Launching another application*, *Harmattan APIs that require credentials*, *Aegis examples*, and the applauncherd pages `qmlboost`, `limitations`, `aegis`.
