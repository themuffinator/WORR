# Server Quickstart

This is the practical setup for hosting a WORR dedicated server without digging through every cvar first.

## Start a Local Dedicated Server

From `.install/`:

- Windows:

  ```powershell
  .\worr.ded.exe +set basedir . +set deathmatch 1 +map q2dm1
  ```

- Linux/macOS:

  ```bash
  ./worr.ded +set basedir . +set deathmatch 1 +map q2dm1
  ```

## Recommended Baseline Cvars

```text
set hostname "My WORR Server"
set maxclients 12
set timelimit 20
set fraglimit 50
set allow_download 1
```

## Common Admin Flow

1. Keep a server config in `baseq2/server.cfg`.
2. Launch with `+exec server.cfg`.
3. Test locally, then open your firewall/router port for WAN play.
4. Watch console logs for bad configs, missing maps, or denied file loads.

## Troubleshooting

- Server starts then exits: check map name and data path (`basedir`).
- Players cannot join: confirm network port forwarding/firewall rules.
- Wrong gameplay rules: check load order of `+set` arguments and `+exec` configs.

## Need Full Command Coverage?

Use `docs-user/server.asciidoc` for the complete low-level command and cvar reference.
