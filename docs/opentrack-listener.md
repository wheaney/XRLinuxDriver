# OpenTrack listener (input)

The **OpenTrack listener** integration makes the driver listen for OpenTrack UDP packets and expose them as a **synthetic IMU device** or as a **supplemental device** if a primary IMU device is already connected (see [6DoF from 3DoF](6dof-from-3dof-opentrack-neuralnet.md)).

This is primarily useful for:

- Feeding externally-tracked pose data into the XR Linux Driver for use with Breezy Desktop or XR Gaming
- Testing/debugging pose ingestion without relying on a physical glasses IMU

## Enable it (via `xr_driver_cli`)

Turn the listener on:

```bash
xr_driver_cli --opentrack-listener
```

Bind address / port:

```bash
# Listen on all interfaces (default is typically 0.0.0.0)
xr_driver_cli --opentrack-listen-ip 0.0.0.0

# Default port used by the OpenTrack integrations
xr_driver_cli --opentrack-listen-port 4242
```

Disable it:

```bash
xr_driver_cli --no-opentrack-listener
```

### What do these flags change?

`xr_driver_cli` writes these keys into `~/.config/xr_driver/config.ini`:

- `opentrack_listener_enabled=true|false`
- `opentrack_listen_ip=...`
- `opentrack_listen_port=...`

## OpenTrack side setup

On the machine that will **send** tracking:

1. Configure OpenTrack to **output** pose data via UDP.
2. Set the destination IP to the machine running XR Linux Driver.
3. Set the destination port to match `--opentrack-listen-port` (default: `4242`).

If you are sending from another host, ensure UDP traffic on that port is allowed.

## Payload expectations

The listener expects packets containing at least:

- 6 × `double`: `(x, y, z, yaw, pitch, roll)`

It will ignore packets smaller than that. (If your sender includes a trailing frame number, that’s fine — it will be ignored.)

Angles are treated as **degrees**.

## Feedback-loop guard (important)

The driver also has an OpenTrack **app/output** mode that can send UDP packets.

To prevent an accidental “send → receive → send …” feedback loop when both features are enabled on the same machine, the listener will **discard all packets** when:

- the listener is enabled, **and**
- OpenTrack app/output mode is enabled (`external_mode` includes `opentrack`), **and**
- `opentrack_app_ip` is set to a localhost/unspecified address (e.g. `127.0.0.1`, `localhost`, `0.0.0.0`, `::1`, `::`).

If you want to use the listener, typically you should:

- disable OpenTrack app/output mode: `xr_driver_cli --disable-external`

…or run the sender on a different machine.

## Troubleshooting

- Check logs: `xr_driver_cli --view-log`
- Confirm the socket is bound (example):

  ```bash
  ss -u -lpn | grep 4242
  ```

- If it binds but never connects, confirm OpenTrack is actually sending UDP packets to the correct IP/port.
