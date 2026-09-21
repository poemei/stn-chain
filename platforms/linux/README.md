# Linux unattended startup

Build with `make` on Linux x64 using a C17 compiler, POSIX threads and OpenSSL
development headers/libraries. The Linux backend is implemented; runtime
qualification is separate from implementation.

Run `stn-chain` without a mode flag to resume existing history or initialize the
existing deterministic built-in genesis when history is absent. The direct-run
default remains `stn-chain-dev.stns` for path compatibility. Use `--data PATH`
to select an existing history. Its genesis is recovered and the entire history
is normally revalidated before RPC starts. `--genesis BLOCK` still pins an
explicit genesis. Invalid, empty or unreadable history fails without replacement.

The built-in genesis is the existing development-origin genesis, not a newly
defined production network or authority set. Repeated fixture transactions still
require `--dev`; ordinary startup uses the pending/mining path. A lifetime
`.node.lock` excludes another Linux node using the same pathname. The file stays
on disk; process exit releases its kernel lock.

## Boot and reboot

Install and enable once on the Linux host:

```sh
make
sudo make install-service
sudo systemctl daemon-reload
sudo systemctl enable --now stn-chain
```

systemd starts the node at boot, restarts failures and sends SIGTERM for shutdown.
No login, terminal, genesis-generation command or mode flag is required.
The service uses a system-managed unprivileged identity and persistent
`/var/lib/stn-chain/chain.stns`. systemd creates the state directory. Logs are
available with `journalctl -u stn-chain`.

The executable path follows Makefile BINDIR (default `/usr/local/bin`). Service
state uses the fixed path above independently of Makefile DATADIR; use a systemd
override to select another history or explicit genesis. Stop any old node first.
DESTDIR stages files only; enablement is a separate host operation. Stop/disable
the service before uninstalling the binary. Data is never removed by uninstall.
RPC remains loopback-only on port 18473.

Run `python3 tools/test-linux-startup.py build/stn-chain` for focused bootstrap,
restart, exclusion, SIGTERM and corrupt-history checks in a temporary directory.
Linux compilation, runtime and actual systemd boot have not been qualified on
the Windows editing host.
