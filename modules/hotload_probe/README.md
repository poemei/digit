# Digit Hot-Load Probe

Temporary, non-operational Linux ABI module used solely to check Digit Core's
module discovery, SHA-256/version enforcement, and live replacement. No routes,
commands, services, network access, or persistent data. The module logs START
and STOP through Core's host callback.

## Test sequence

1. Build and install the 0.1.0 baseline:
   `make -C modules/hotload_probe clean all test PATCH=0`
   `sudo make -C modules/hotload_probe install PATCH=0`
2. **Restart Digit once** to discover and activate this newly installed test
   module. The current Core replacement mechanism does not automatically
   admit modules which were not active at the start of the process.
3. While Digit remains running, build and install the 0.1.1 candidate:
   `make -C modules/hotload_probe clean all test PATCH=1`
   `sudo make -C modules/hotload_probe install PATCH=1`
4. Check `/opt/digit/logs/modules.log` for HOTLOAD_DETECTED and HOTLOAD_ACTIVE
   with module=hotload_probe and versions 0.1.0 -> 0.1.1. Verify START 0.1.1
   and STOP from the lifecycle logs. No Digit restart should occur in step 3.
5. **Negative version test:** build and install 0.1.0 again while 0.1.1 is active:
   `make -C modules/hotload_probe clean all test PATCH=0`
   `sudo make -C modules/hotload_probe install PATCH=0`
   Expect HOTLOAD_DENIED, incumbent version 0.1.1 retained.

Each change of PATCH **requires** a clean rebuild; changing Makefile variables
alone does not make GNU Make consider existing binaries out of date. Every
versioned artifact has different source constants and therefore a distinct
SHA-256. The qualification callback follows the repository ABI's minimum
qualification report; independent unit tests run via the `test` target.

This test is not proof of live activation until the operator confirms the
observed module log and runtime behavior.
