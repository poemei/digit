# Ask 0.1.2

Module-owned `/ask` service, routed through Digit Router. Handles bounded greetings and exact identity questions. Identity is obtained from the live, read-only Core-owned `core.identity.read` service, never redefined in Ask.

The Windows GUI `/ask` endpoint is currently unauthenticated. DSD's `diagnose my last request` examines shared operational logs; anonymous requests have no verified session identity. Ask therefore returns an explicit access/scope limitation instead of invoking DSD against another operator's records or inventing a diagnosis. The existing DSD `diagnose my last request` command remains available through the authorized local console. No arbitrary command, remote repair, or privileged DSD service is exposed.

Build/test: `make -C modules/ask clean all test`. Install: `sudo make -C modules/ask install`. Source also requires updated Core `src/mvc_console.c` to be rebuilt/installed to register `core.identity.read`. Reboot/restart service after installation. GUI test: `hi`, `who are you?`, `diagnose my last request`, and unsupported natural-language input. Verify DSD responds only with a scope limitation in public chat; do not claim it performed a diagnosis.

No general intent model, persistent chat history, or authenticated GUI session is present. Tests are source-level only until compiled and operator-tested.
