"""Host tests of display and network setup in the complete production main file.

Run with: python -m unittest discover -s esp-base/tests -p test_display_state.py -v
No ESP-IDF installation, hardware, network access, or additional packages are required.
"""

import os
from pathlib import Path
import shlex
import shutil
import subprocess
import tempfile
import unittest


TEST_DIR = Path(__file__).resolve().parent


class DisplayStateTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        compiler = shlex.split(os.environ.get("CC", "cc"))
        if not compiler or shutil.which(compiler[0]) is None:
            raise unittest.SkipTest("A host C compiler is required")
        cls.temp_dir = tempfile.TemporaryDirectory(prefix="display-state-tests-")
        cls.addClassCleanup(cls.temp_dir.cleanup)
        cls.binaries = {}
        for enabled in (0, 1):
            for wifi_configured in (0, 1):
                binary = Path(cls.temp_dir.name) / f"display_state_{enabled}_{wifi_configured}"
                command = compiler + [
                    "-std=c11", "-Wall", "-Wextra", "-Werror",
                    f"-DCONFIG_BABY_HEALTH_DISPLAY_ENABLED={enabled}",
                    f"-DTEST_WIFI_CONFIGURED={wifi_configured}",
                    "-I", str(TEST_DIR / "display_state_mocks"),
                    str(TEST_DIR / "display_state_host_test.c"),
                    "-o", str(binary),
                ]
                result = subprocess.run(command, capture_output=True, text=True, timeout=30)
                if result.returncode:
                    # Do not echo diagnostics that could quote locally edited
                    # production source or configuration containing credentials.
                    raise RuntimeError(
                        f"Host harness compilation failed for display={enabled}, "
                        f"wifi_configured={wifi_configured}; "
                        "compiler output withheld to avoid exposing credentials."
                    )
                cls.binaries[enabled, wifi_configured] = binary

    def run_scenario(self, scenario, enabled=1, wifi_configured=1):
        result = subprocess.run(
            [str(self.binaries[enabled, wifi_configured]), scenario],
            capture_output=True, text=True, timeout=5,
        )
        self.assertEqual(result.returncode, 0, result.stdout + result.stderr)

    def test_build_option_off_skips_all_display_work(self):
        self.run_scenario("disabled", enabled=0)

    def test_missing_display_disables_output(self):
        self.run_scenario("missing")

    def test_initial_bus_error_disables_output(self):
        self.run_scenario("bus_error")

    def test_initial_status_and_reading_render_successfully(self):
        self.run_scenario("normal")

    def test_initial_show_failure_cleans_up_once(self):
        self.run_scenario("initial_show_error")

    def test_reading_show_failure_cleans_up_once(self):
        self.run_scenario("reading_show_error")

    def test_status_show_failure_cleans_up_once(self):
        self.run_scenario("status_show_error")

    def test_initial_cleanup_failure_keeps_pointer_but_disables_output(self):
        self.run_scenario("initial_cleanup_error")

    def test_reading_cleanup_failure_keeps_pointer_but_disables_output(self):
        self.run_scenario("reading_cleanup_error")

    def test_status_cleanup_failure_keeps_pointer_but_disables_output(self):
        self.run_scenario("status_cleanup_error")

    def test_empty_wifi_ssid_skips_wifi_and_http_startup(self):
        for enabled in (0, 1):
            with self.subTest(display_enabled=enabled):
                self.run_scenario("network_unconfigured", enabled, wifi_configured=0)

    def test_configured_wifi_uses_config_values_before_http_startup(self):
        for enabled in (0, 1):
            with self.subTest(display_enabled=enabled):
                self.run_scenario("network_normal", enabled)

    def test_wifi_initialization_error_skips_http_startup(self):
        for enabled in (0, 1):
            with self.subTest(display_enabled=enabled):
                self.run_scenario("network_wifi_error", enabled)

    def test_http_startup_error_returns_without_aborting(self):
        for enabled in (0, 1):
            with self.subTest(display_enabled=enabled):
                self.run_scenario("network_http_error", enabled)


if __name__ == "__main__":
    unittest.main()
