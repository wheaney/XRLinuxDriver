# OpenTrack app (output/source)

The **OpenTrack app** integration makes the driver send pose data over UDP in an OpenTrack-compatible payload format.

This is useful when you want to consume the driver’s tracking in other applications via OpenTrack’s ecosystem (mappings, output protocols, etc.).

## Enable it (via `xr_driver_cli`)

Enable the OpenTrack external mode (this sets `output_mode=external_only` and `external_mode=opentrack`):

```bash
xr_driver_cli --opentrack-app
```

Configure the UDP target (where OpenTrack is listening):

```bash
xr_driver_cli --opentrack-app-ip 127.0.0.1
xr_driver_cli --opentrack-app-port 4242

## OpenTrack side setup

On the OpenTrack machine:

1. Configure OpenTrack to **receive** UDP pose data (exact UI wording varies by OpenTrack version).
2. Set the listening **port** to match `--opentrack-app-port` (default: `4242`).
3. If OpenTrack is on a different machine, make sure firewalls allow inbound UDP on that port.

## Data format and notes

The driver sends a UDP packet containing:

- 6 × `double`: `(x, y, z, yaw, pitch, roll)`
- 1 × `uint32`: frame number

Angles are in **degrees**.

The implementation converts the driver’s coordinate system into what OpenTrack expects (see the implementation in the OpenTrack source plugin).

## Troubleshooting

- Confirm the driver is running: `systemctl --user status xr-driver`
- Check logs: `xr_driver_cli --view-log`
- Verify packets are being sent (localhost example):

  ```bash
  sudo tcpdump -n -i lo udp port 4242
  ```

- If nothing arrives in OpenTrack, verify:
  - the IP/port in `xr_driver_cli --opentrack-app-ip/--opentrack-app-port`
  - OpenTrack is actually listening on that port
  - you’re not trying to “receive from yourself” through the listener at the same time (see the listener page for the feedback-loop guard)
