# Web pages served to browsers

Copied unchanged from the official LocalSend app
(`packages/core/assets/web/` in github.com/localsend/localsend, commit
`d84fcb2` of 2026-10-25), Apache License 2.0, © the LocalSend authors.
Keep them identical to upstream so browsers get exactly the official page;
to update, copy the new versions over these files.

- `upload.html` — "Receive via link": served at `/` while `webShare.receiveActive`. It
  reads its strings from `/i18n.json` and uploads through the normal v2
  `prepare-upload` / `upload` endpoints (as device type `web`).
- `error-403.html` — served at `/` while no link is active.

They are compiled into the app as Qt resources (`web.qrc`) and served by
`src/webshare.cpp`.
