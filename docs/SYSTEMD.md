# Digit and systemd

Digit Core runs in the foreground and handles SIGTERM for a clean shutdown.
The reference unit at `systemd/digit.service` targets `/opt/digit/bin/digit`
and has no dependency on IRC or any specific transport module.

## Existing systemd service

**Do not overwrite the installed unit without reviewing it.** The repository
does not contain a copy of the unit currently installed on the operator's host.

Inspect it first:

```sh
sudo systemctl cat digit.service
sudo systemctl show digit.service -p FragmentPath -p User -p Group -p WorkingDirectory -p ExecStart
```

The existing unit should have `Type=simple`,
`WorkingDirectory=/opt/digit`, and
`ExecStart=/opt/digit/bin/digit`. Keep its existing service user and group
if they differ from the reference, provided the account has the necessary
file permissions. Ensure that user can read module binaries and write
`/opt/digit/logs/digit.log` and `/opt/digit/logs/modules.log`.

`sudo make install` installs Digit and its modules; it **does not** overwrite,
enable, or restart the installed systemd unit. To install the reference unit
only on a host without one, use `sudo make install-systemd`.
The target refuses to replace an existing `digit.service`.

After explicitly checking or updating the unit, the operator can run:

```sh
sudo systemctl daemon-reload
sudo systemctl restart digit.service
systemctl status digit.service
journalctl -u digit.service -n 50 --no-pager
```

Systemd's journal is supplementary; Core owns the authoritative on-disk
Digit and module logs. The IRC module remains unqualified and cannot
activate merely by starting the service.
