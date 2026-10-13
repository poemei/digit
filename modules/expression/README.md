# Independent Expression Model

The existing accepted expression fixture recognizes `purrs` as `cat vocalization`. This standalone module packages that capability for Digit's default console.

It exports the frozen ABI 1.5 descriptor and registers the `purrs` command on activation; the generic Router binds the command without module-specific changes. On stop the module unregisters its command.

Build: `make -C modules/expression`. Install: `sudo make -C modules/expression install`. Launch with `sudo -u digit /opt/digit/run`, then enter `purrs`. An unknown command such as `puuurs` remains unknown; no meaning is invented.

This is a bounded fixture, not general natural-language recognition or a replacement for future language interpretation.
