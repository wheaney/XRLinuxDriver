# Managing the Nix Flake

This repo is packaged as a Nix flake (`flake.nix` / `flake.lock`). This doc covers the
flake's structure and the day-to-day tasks for maintaining it. For install instructions
as an end user, see the [README's Nix section](../README.md#nix).

## Outputs

- `packages.<system>.xrlinuxdriver` / `packages.<system>.default` — built from
  `.nix/XRLinuxDriver/default.nix`.
- `devShells.<system>.default` — a shell with the package's build inputs available.
- `overlays.default` — exposes `xrlinuxdriver` onto `pkgs`.

Supported systems: `x86_64-linux`, `aarch64-linux`.

## Inputs

- `nixpkgs` — tracks `nixos-unstable`, via the `github:` shorthand.
- `flake-compat` — lets `default.nix`/`shell.nix` work for non-flake Nix users; also via
  `github:`.

Neither input needs submodules, so `github:` is fine for both here.

### `self.submodules`

`flake.nix` sets `self.submodules = true;`, which fetches this repo (as `self`) via git
with submodules recursed — required since this repo vendors `xrealInterfaceLibrary`,
`xrDeviceKit`, `xrealAirDeviceKit`, and `xrealOneDeviceKit` as submodules.

`self.submodules = true` only controls how this repo fetches itself when built directly
(e.g. via `nix build .#xrlinuxdriver` from a checkout). It does **not** propagate to
anyone consuming this repo as a flake input — the `github:` shorthand has no way to
request submodules, so a downstream flake pinning `github:wheaney/XRLinuxDriver` gets a
checkout missing the submodule sources and will fail to build. Consumers must instead
use the `git+` fetcher with `submodules=1`:

```nix
inputs.xrlinuxdriver.url = "git+https://github.com/wheaney/XRLinuxDriver?submodules=1";
```

`git+ssh://...` works the same way if the consumer has SSH access, but `git+https://...`
is the form documented for external users since it doesn't require any credentials.

**Known limitation:** three of those submodules use `git@github.com:...` SSH remotes.
Fetching them during flake evaluation happens inside the Nix sandbox, with no SSH agent
or credentials available, so `nix build`/`nix flake check` will fail for anyone without
GitHub SSH keys configured (this includes fresh CI runners). Switching those remotes to
`https://github.com/...` in `.gitmodules` would fix this, but is out of scope here.

## Common tasks

Build the package:

```bash
nix build .#xrlinuxdriver
```

Validate the flake (evaluates all outputs, runs checks):

```bash
nix flake check
```

Enter a dev shell:

```bash
nix develop
```

Update the inputs:

```bash
nix flake update
```

There's no per-input schedule to worry about here (unlike breezy-desktop, which pins
this repo as an input on its own schedule) — a plain `nix flake update` is safe.

## CI

- `.github/workflows/nix-ci.yml` — on every push/PR: builds the package and runs
  `nix flake check`.
- `.github/workflows/nix-update-inputs.yml` — Sundays at 00:00 UTC, runs
  `nix flake update` and pushes the resulting `flake.lock` straight to `main` if it
  changed.

The update workflow pushes directly to `main` rather than opening a PR, so keep an eye
on it if branch protection rules change.
