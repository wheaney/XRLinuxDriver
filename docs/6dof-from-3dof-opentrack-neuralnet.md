# Getting 6DoF from 3DoF glasses (OpenTrack + NeuralNet)

This guide describes how to get **6DoF positional tracking** from any supported **3DoF glasses** by combining:

- **XR Linux Driver** (for the glasses’ 3DoF IMU + runtime)
- **OpenTrack** (for filtering/mapping and UDP output)
- **NeuralNet Tracker** (optional OpenTrack input, for camera-based position tracking)

The high-level idea:

1. Have XR Linux Driver consume a pose orientation from a set of 3DoF glasses acting as a "primary" device.
2. Use a webcam + NeuralNet in OpenTrack to estimate a 6DoF pose, including **position** (XYZ).
3. Have XR Linux Driver’s **OpenTrack listener** ingest that pose as a synthetic "supplemental" device.
4. Merge the glasses' 3DoF orientation + OpenTrack's 6DoF position for use with Breezy Desktop, XR Gaming, or other external applications.

## Prereqs

- A working XR Linux Driver install
- A webcam (for NeuralNet position tracking)

## Install OpenTrack (+ NeuralNet input)

OpenTrack won't typically come with NeuralNet out of the box. You'll need to make sure the appropriate `onnxruntime` is installed.

### Arch Linux installation example

```bash
sudo pacman -S onnxruntime
yay -S opentrack
```

## Apply the recommended OpenTrack profile (one-liner)

This script configures an OpenTrack profile tuned for the XR Linux Driver listener. Before running this script, you will need to launch OpenTrack at least once to create the default profile.

```bash
curl -fsSL https://github.com/wheaney/XRLinuxDriver/releases/latest/download/xr_driver_ot_profile_setup | bash
```

Note: piping a remote script to `bash` trades convenience for auditability. If you prefer to review it first, download it and inspect before running.

## Enable the XR Linux Driver OpenTrack listener

Enable the listener (input):

```bash
xr_driver_cli --opentrack-listener
```

If you changed the OpenTrack UDP output port, update the listener to match:

```bash
xr_driver_cli --opentrack-listen-port 4242
```

## Run it

1. Plug in your glasses first (confirm normal **3DoF** tracking is working).
2. Launch OpenTrack and select the Profile named "xr-driver.ini". Hit the settings icon next to the NeuralNet input and make sure the appropriate camera is selected, set the `Diagonal FOV` and `Resolution` values to match your camera.
3. Click **Start** in OpenTrack.
4. Launch Breezy Desktop and enable the effect. You should now be able to lean in to see a closer view of your screens.

**Note** - This will also be available in a future update of XR Gaming, if you've set up your deck with a webcam or you're sending the UDP data over the network.

## Troubleshooting

- View XR Linux Driver logs:

  ```bash
  xr_driver_cli --view-log
  ```

- Confirm the listener is bound:

  ```bash
  ss -u -lpn | grep 4242
  ```

- If OpenTrack is running but the listener never “connects”, double-check:
  - OpenTrack output is **UDP over network**
  - remote IP/port match the listener’s bind port
  - you didn’t accidentally enable XR Linux Driver’s OpenTrack **app/output** mode at the same time (see the feedback-loop guard notes in the listener page)
