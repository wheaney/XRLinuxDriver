# Development (Docker Buildx)

This repo includes a Docker-based build pipeline that produces release artifacts for multiple architectures using Docker **Buildx**.

The main scripts are:

- `docker-build/init.sh`: sets up a Buildx builder and builds the per-arch build images
- `docker-build/run-build.sh`: runs the build inside those images and writes artifacts to `out/`
- *Needs work* `docker-build/run-fpm.sh`: runs the build and produces `.deb` / `.rpm` packages (written to `out/`)

## Requirements

- Docker
- Docker Buildx (`docker buildx version` should work)

For cross-arch builds (e.g. building `linux/arm64` on an x86_64 machine), you also need binfmt/qemu emulation enabled. The init script will attempt to install this via privileged containers.

## 1) Initialize Buildx + build the build images

Run from the repo root:

```bash
./docker-build/init.sh --init
```

This will:

- install binfmt/qemu handlers (for cross-arch builds)
- create or recreate a Buildx builder named `xrdriverbuilder`
- build and `--load` two images locally:
	- `xr-driver:amd64`
	- `xr-driver:arm64`

If you already have the builder and just want to reuse it, you can run:

```bash
./docker-build/init.sh
```

## 2) Build artifacts (tarball output)

After the images exist, run:

```bash
./docker-build/run-build.sh
```

To build only one architecture:

```bash
./docker-build/run-build.sh x86_64
./docker-build/run-build.sh aarch64
```

Build outputs are written under `out/`.

Notes:

- The build runs in containers, so intermediate files under `build/` may be owned by root; the script cleans `build/` afterwards.
- The build passes through `UA_API_SECRET` (and `UA_API_SECRET_INTENTIONALLY_EMPTY`) as environment variables when running containers.

## 3) Build distro packages (.deb / .rpm)

If you want Debian/RPM packages, run:

```bash
./docker-build/run-fpm.sh
```

Or per-arch:

```bash
./docker-build/run-fpm.sh x86_64
./docker-build/run-fpm.sh aarch64
```

The resulting packages are moved to `out/`.

## Troubleshooting

- If `linux/arm64` builds fail on x86_64, rerun init:

	```bash
	./docker-build/init.sh --init
	```

- Confirm Buildx sees the builder:

	```bash
	docker buildx ls
	docker buildx inspect xrdriverbuilder
	```
