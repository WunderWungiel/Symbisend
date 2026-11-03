# Fingerprint icons

The 260 icons of LocalSend's Verify page (`app/lib/util/fingerprint_alphabet.dart` in
localsend/localsend, `iconAlphabet`), one SVG per icon, named like the Flutter `Icons.*`
constant. `src/verification.cpp` holds the same names **in the same order**: the order is
the alphabet, so it must match the official app or both devices show different icons.

Source: Material Icons (filled), Apache License 2.0 — `@material-design-icons/svg@0.14.15`
(`filled/<name>.svg`); `apple`, `conveyor_belt`, `eco`, `forklift`, `motorcycle`,
`shelves` and `door_front_door` (= `door_front`) from
`fonts.gstatic.com/s/i/materialicons/<name>/v<n>/24px.svg`. Cleaned up (no width/height,
no empty `<rect fill="none">`), otherwise unchanged.

They are black; `VerifyIconProvider` (`image://verify/<name>/<rrggbb>`) colours them for
the theme. Compiled in through `fingerprint-icons.qrc`.
