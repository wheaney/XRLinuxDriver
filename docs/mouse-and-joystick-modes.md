# Mouse / joystick modes

XR Linux Driver turns head movement into input that games and apps can use.

Most commonly that’s **mouse movement**, but you can also switch to a **virtual joystick** mode.

## Mouse mode (default)

Since device movements are converted to mouse movements, they should be recognized by any PC game that supports keyboard/mouse input. This will work most naturally for games where mouse movements is used to control "look"/camera movements. For point-and-click style games, you may want to disable the driver so your glasses act as just a simple display.

To adjust sensitivity, use the Decky UI on Steam Deck, or:

```bash
xr_driver_cli --mouse-sensitivity 20
```

If you're using keyboard and mouse to control your games, then the mouse movements from this driver will simply add to your own mouse movements and they should work naturally together.

If you're using a game controller, Valve pushes pretty heavily for PC games support mouse input *in addition to* controller input, so you should find that most modern games will just work with this driver straight out of the box.

## Controller mappings (Steam)

1. Open your game's controller configuration in Steam
2. Open the Layouts view
3. Choose a keyboard/mouse template (e.g. "Keyboard (WASD) and Mouse"). Be sure to edit the configuration and set "Gyro Behavior" to "As Mouse" for any games where you want to use gyro.

## Controller mappings (non-Steam)

You'll probably want to use a utility that does what Steam's controller layouts are doing behind the scenes: mapping controller buttons, joystick, and gyro inputs to keyboard/mouse inputs. One popular tool is [JoyShockMapper](https://github.com/Electronicks/JoyShockMapper).

## Joystick mode

If mouse input just won't work for a specific game, you can enable joystick mode using the Decky UI on Steam Deck, or:

```bash
xr_driver_cli --use-joystick
```

Revert back to mouse mode with:

```bash
xr_driver_cli --use-mouse
```

Joystick mode creates a virtual gamepad whose right joystick is driven by movements from the glasses.

Notes:

- Joystick movement is capped (you can only move a joystick so far).
- This creates a *second* controller on your PC.
- If the game interprets another controller as a second player, its movements won't get combined with your real controller's movements.
