# VITURE udev regression

Run from the repository root on Linux with Python 3, bubblewrap, systemd's
`udevadm` and unprivileged user namespaces enabled:

```sh
python3 tests/test_viture_udev.py -v
udevadm verify udev/70-viture-xr.rules
```

The test borrows a USB root hub's read-only sysfs structure and overlays only
its vendor ID in a private mount namespace. It does not require VITURE glasses.
Only the repository's VITURE rule is visible to the native udev parser. Device
nodes, host rules, host udev database and the system udev daemon are not exposed.
No events are triggered on the host and no device permissions are changed.

Add and non-remove actions must retain `uaccess`; removal and unrelated vendors
must not receive it from this rule. This verifies rule matching, not logind's
ACL application or USB hardware behavior.
