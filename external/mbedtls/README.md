# mbedTLS 3.4.1 (Symbian port)

`upstream/` is a git submodule of <https://github.com/shinovon/mbedtls-symbian>, pinned to
commit `f178b939d27d2eeb6c13088ef4fdb4b8a7a29ad0` (2026-10-21). It contains mbedTLS 3.4.1,
patched to be C89-compatible and to build for Symbian OS 9.1+, under the Apache-2.0
license (`upstream/LICENSE`).

After cloning, fetch it with:

    git submodule update --init

You can also clone the repo with `git clone --recursive`. If the submodule is missing,
qmake stops with an error.

To update it, check out another commit inside `upstream/`, then commit the new submodule
pointer.

Symbisend compiles it into its own executable (`mbedtls.pri`, with `MBEDTLS_STATIC`). The
phone therefore needs no system-wide mbedTLS (`mbedtls.sis` / `mbedtls.dll`). Symbian links
DLLs by ordinal, so our copy cannot clash with a system `mbedtls.dll` loaded into the
process.

## Our files

- `symbisend_config.h` is our configuration, selected with `MBEDTLS_CONFIG_FILE`. Upstream's
  `include/mbedtls/mbedtls_config.h` is not used.
- `mbedtls.pri` is the qmake include. It compiles only the modules our configuration needs.
  The port's own `group/` build files are not used.

## Platform notes

- **Entropy:** the port's `mbedtls_hardware_poll()` (`library/symbian.c`) uses `rand()`, so
  `MBEDTLS_ENTROPY_HARDWARE_ALT` stays off. `src/tlsrandom.cpp` feeds `TRandom` (the
  Symbian system CSPRNG) into mbedTLS instead. On desktop, mbedTLS's own platform source is
  used.
- **`library/symbian.c`** is still compiled on Symbian for its `__aeabi_idiv`/`__aeabi_uidiv`
  shims, which GCCE needs.
- **`PIPS`** is defined on Symbian, because we build against Open C (stdapis). This selects
  the port's variant that uses the real `stdint.h` and `snprintf`.
