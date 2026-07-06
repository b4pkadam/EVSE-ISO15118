# Build Instructions

## Requirements
- mbedTLS built and installed in `./mbedtls/build/`
- OpenV2G codec libraries
- Standard C compiler (gcc/clang)

## Quick Build
```bash
gcc app/main.c \
    core/secc_session.c \
    core/secc_state_machine.c \
    transport/tls_transport.c \
    v2g/v2gtp_response_handler.c \
    v2g/v2gtp_handler.c \
    v2g/v2gtp_parser.c \
    v2g/v2gtp_sdp_handler.c \
    exi/exi_decoder.c \
    exi/OpenV2G/src/codec/*.c \
    exi/OpenV2G/src/iso1/*.c \
    exi/OpenV2G/src/appHandshake/*.c \
    -I./mbedtls/include \
    -Icore -Itransport -Iv2g -Iexi -Iexi/OpenV2G/src \
    -L./mbedtls/build/library \
    -lmbedtls -lmbedx509 -lmbedcrypto \
    -o secc_app
```

## Run with Keylogging
```bash
SSLKEYLOGFILE=/tmp/secc_keys.log ./secc_app
```

## Files Added
- `v2g/v2gtp_sdp_handler.h` - SDP protocol definitions
- `v2g/v2gtp_sdp_handler.c` - SDP parsing implementation
- `IMPLEMENTATION_SUMMARY.md` - Feature documentation
- `BUILD_INSTRUCTIONS.md` - This file

## Files Modified
- `core/secc_context.h` - Added transport_mode field
- `core/secc_session.c` - Added SDP detection logic
- `transport/tls_transport.c` - Added dual mode support + keylogging
- `transport/tls_transport.h` - Added new function declarations
- `app/main.c` - Enhanced with usage information

## Compilation Status: ✅ SUCCESS
Binary: `secc_app` (1.2M)
