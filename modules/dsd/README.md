# DSD independent module

Build from this directory: `make`.

The host module discovery scans `<directory containing bin/digit>/modules/<module directory>/module.conf`, reads exactly `id=dsd`, then loads `bin/dsd.so` through the ABI 1.5 descriptor export `stnlabz_module_get_descriptor`.

To install independently after building, run `sudo make install` from this directory. It deploys `module.conf` and `bin/dsd.so` to `/opt/digit/modules/dsd/` with `digit:digit` ownership. First install Digit itself with the root repository's `sudo make install` if needed. The source tree `modules/dsd/` is not automatically scanned by Digit's running Core. In the default local Digit console, qualified installed modules are offered to the existing Core activation gate. After authorized activation DSD registers its own `diagnose` command through the existing ABI. The generic Router publishes `/diagnose` automatically; no per-module Router or Core source changes are required. This local console is not an authenticated network interface.

The console command `diagnose` reads the last 16 lines of Digit’s operational log and presents the recorded events. The structured `dsd.report` service also accepts bounded supplied observations. DSD does not infer unrecorded source locations or claim a root cause not contained in evidence.
