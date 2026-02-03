# XR Linux Driver Docs

This site documents selected XR Linux Driver features and integrations.

## OpenTrack integrations

There are two separate OpenTrack-related components:

- **OpenTrack app (output/source):** the driver **sends** pose data over UDP to an OpenTrack instance.
- **OpenTrack listener (input):** the driver **receives** pose data over UDP (in the OpenTrack payload format) and exposes it as a synthetic IMU device.

### Quick links

- [OpenTrack app (output/source)](opentrack-app.md)
- [OpenTrack listener (input)](opentrack-listener.md)
- [6DoF with any supported 3DoF glasses + a webcam (OpenTrack + NeuralNet)](6dof-from-3dof-opentrack-neuralnet.md)

## Where settings live

Most configuration is written by `xr_driver_cli` into:

- `$XDG_CONFIG_HOME/xr_driver/config.ini` (usually `~/.config/xr_driver/config.ini`)

Logs are written to:

- `$XDG_STATE_HOME/xr_driver/driver.log` (usually `~/.local/state/xr_driver/driver.log`)

Tip: `xr_driver_cli --view-log` tails the log in `less`.
