"""Run VITURE rules in native udev, with no host devices or rules exposed."""
from pathlib import Path
import shutil
import subprocess
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[1]

class VitureUdevTests(unittest.TestCase):
    def tags_for(self, action, vendor="35ca"):
        for tool in ("bwrap", "udevadm"):
            if not shutil.which(tool):
                self.fail(f"required test tool missing: {tool}")
        with tempfile.TemporaryDirectory(prefix="xr-udev-test-") as temp:
            base = Path(temp)
            # Borrow only a USB root hub's sysfs structure, never its device node.
            # Overlay the vendor attribute inside the private mount namespace.
            devices = sorted(path.resolve() for path in Path("/sys/bus/usb/devices").glob("usb*")
                             if (path / "idVendor").exists())
            self.assertTrue(devices, "native metadata test requires a USB root hub")
            device = devices[0]
            vendor_file = base / "idVendor"
            vendor_file.write_text(vendor + "\n")
            rules = base / "rules"
            rules.mkdir()
            shutil.copyfile(ROOT / "udev/70-viture-xr.rules", rules / "70-viture-xr.rules")
            command = [
                "bwrap", "--unshare-all", "--die-with-parent", "--clearenv",
                "--setenv", "PATH", "/usr/bin", "--setenv", "SYSTEMD_LOG_LEVEL", "debug",
                "--ro-bind", "/usr", "/usr", "--symlink", "usr/bin", "/bin",
                "--symlink", "usr/lib", "/lib", "--symlink", "usr/lib", "/lib64",
                "--proc", "/proc", "--dev", "/dev", "--tmpfs", "/run",
                "--tmpfs", "/etc", "--tmpfs", "/tmp", "--ro-bind", "/sys", "/sys",
                "--ro-bind", str(vendor_file), str(device / "idVendor"),
                "--ro-bind", str(rules), "/usr/lib/udev/rules.d", "--chdir", "/",
                "/usr/bin/udevadm", "test", "--resolve-names=never",
                "--action=" + action, str(device),
            ]
            result = subprocess.run(command, capture_output=True, text=True, timeout=20)
            self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
            properties = dict(line.strip().split("=", 1) for line in result.stdout.splitlines() if "=" in line)
            self.assertEqual(properties.get("DEVPATH"), str(device)[4:], result.stdout + result.stderr)
            read_rules = [line for line in result.stderr.splitlines() if line.startswith("Reading rules file:")]
            self.assertEqual(read_rules, ["Reading rules file: /usr/lib/udev/rules.d/70-viture-xr.rules"])
            self.assertEqual(properties.get("ACTION"), action, result.stdout + result.stderr)
            return set(properties.get("CURRENT_TAGS", "").strip(":").split(":")) - {""}

    def test_add(self):
        self.assertIn("uaccess", self.tags_for("add"))

    def test_non_remove_events_retain_uaccess(self):
        for action in ("change", "bind", "unbind", "move", "online", "offline"):
            with self.subTest(action=action):
                self.assertIn("uaccess", self.tags_for(action))

    def test_remove(self):
        self.assertNotIn("uaccess", self.tags_for("remove"))

    def test_unrelated_vendor(self):
        for action in ("add", "change"):
            with self.subTest(action=action):
                self.assertNotIn("uaccess", self.tags_for(action, "1234"))

if __name__ == "__main__":
    unittest.main()
