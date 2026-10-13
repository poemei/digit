# DSD independent module

Build from this directory: `make`.

The host module discovery scans `<directory containing bin/digit>/modules/<module directory>/module.conf`, reads exactly `id=dsd`, then loads `bin/dsd.so` through the ABI 1.5 descriptor export `stnlabz_module_get_descriptor`.

To install independently after building, run `sudo make install` from this directory. It deploys `module.conf` and `bin/dsd.so` to `/opt/digit/modules/dsd/` with `digit:digit` ownership. First install Digit itself with the root repository's `sudo make install` if needed. The source tree `modules/dsd/` is not automatically scanned by Digit's running Core. Core discovers and qualifies modules without automatically activating them; activation and Router service registration require an authorized operational interface. No Core source change is needed.

DSD only reports observations provided by its caller. It does not decide routes, authorize actions, or independently infer an unrecorded source location.
