# UI translations

`*.json` here are the official LocalSend translations, copied unchanged from
`app/assets/i18n/` in github.com/localsend/localsend (commit `58955fd` of
2026-11-07), Apache License 2.0, © the LocalSend authors and translators
(translated on LocalSend's Weblate). The `_missing_*` / `_unused_*` files are
not copied. Keep them identical to upstream; to update, copy the new versions
over these files, and list new languages in `i18n.qrc` and
`symbisend/languages.json`.

`symbisend/` is ours:

- `en.json` — strings the official app has no key for (Log page, Symbian
  pickers, error texts, ...), looked up as `symbisend.<key>`. Translations of
  it go in `symbisend/<code>.json` (same code as the upstream file, e.g.
  `pt-BR.json`) and into `i18n.qrc`; missing keys fall back to English.
  The translations for every LocalSend language were machine-made (and
  machine-reviewed) and have not been checked by native speakers yet:
  corrections are welcome. Fixes to the official strings in `i18n/*.json`
  belong on LocalSend's Weblate instead.
- `languages.json` — every language with the name the official app shows for
  it (`getLocaleName()` in `app/lib/util/i18n.dart`), in that order.

Everything is compiled in through `i18n.qrc` and read by `src/i18n.cpp`
(QML: `i18n`, see `docs/qml-api.md`). Only the language in use and English
are loaded.

The format is upstream's (slang): nested keys, key modifiers such as
`minutesUnit(plural, param=m)` (ignored), links `@:other.key`, placeholders
`{name}`, plural forms (`zero`, `one`, `two`, `few`, `many`, `other`, chosen
like the official app: 0 → zero, 1 → one, 2 → two in Hebrew only, else other)
and lists (shown as paragraphs).
