# QR Code generator (Project Nayuki)

`qrcodegen.c` and `qrcodegen.h` are the C version of
<https://github.com/nayuki/QR-Code-generator>, tag `v1.8.0`, unmodified, under the MIT
license (see the top of either file).

`src/qrcode.cpp` uses them for `image://qrcode`. They are C99, like mbedTLS, and rely on the
`-std=gnu99` that `external/mbedtls/mbedtls.pri` sets.

To update them, copy both files from another tag's `c/` folder.
