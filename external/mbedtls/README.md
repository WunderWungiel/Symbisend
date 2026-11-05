# mbedTLS (upstream, 3.6 LTS)

`upstream/` is a git submodule of <https://github.com/Mbed-TLS/mbedtls>, pinned to the tag
`v3.6.7`, unmodified, under the Apache-2.0 license (`upstream/LICENSE`).

After cloning, fetch it with:

    git submodule update --init

You can also clone the repo with `git clone --recursive`. If the submodule is missing,
qmake stops with an error. Upstream's own `framework` submodule isn't needed to build the
library: a release tag already contains the generated sources.

To update it, check out another tag inside `upstream/`, then commit the new submodule
pointer. Inside the 3.6 branch this should need nothing else. Check `mbedtls.pri` if a
module was added or removed.

Symbisend compiles it into its own executable (`mbedtls.pri`). The phone needs no
system-wide mbedTLS. The same sources serve Symbian and Harmattan.

## Our files

- `symbisend_config.h` is our configuration, selected with `MBEDTLS_CONFIG_FILE`. Upstream's
  `include/mbedtls/mbedtls_config.h` is not used.
- `mbedtls.pri` is the qmake include. It compiles only the modules our configuration needs.
  mbedTLS is C99, so it adds `-std=gnu99`; on Symbian that goes through an MMP option, which
  also reaches the C++ sources, where GCC warns that it doesn't apply (harmless).
- `platform_glue.c` holds what upstream lacks on our platforms. It uses upstream's own
  `*_ALT` options, so the sources in `upstream/` stay untouched. Don't patch those.

## Platform notes

Symbian (GCCE 4.4, Open C):

- **Entropy:** there is no platform source (`MBEDTLS_NO_PLATFORM_ENTROPY`).
  `src/tlsrandom.cpp` feeds `TRandom`, the system CSPRNG, into mbedTLS instead.
- **`mbedtls_platform_zeroize`:** upstream's keeps `memset` in a static function pointer,
  and `elf2e32` fails on it with `Import relocation does not refer to code segment`.
- **`mbedtls_ms_time`:** Open C doesn't define `_POSIX_VERSION`, so upstream has none.
- The app is built as Thumb. mbedTLS's Thumb assembly works with this compiler.

Harmattan (GCC 4.4, glibc): nothing but `-lrt` for `clock_gettime`. Random numbers come
from `/dev/urandom`, in upstream's code.

Windows is only used for the desktop test harness. The Qt SDK's MinGW has neither
`bcrypt.h` nor `SecureZeroMemory` and can't compile upstream's x86 assembly, so
`MBEDTLS_HAVE_ASM` is limited to ARM and `platform_glue.c` fills in the rest.
