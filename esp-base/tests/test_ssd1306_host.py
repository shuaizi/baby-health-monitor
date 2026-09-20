"""Host regression tests for the actual SSD1306 C driver, without ESP hardware.

Run from the repository root:
    python3 -m unittest discover -s esp-base/tests -p 'test_*.py' -v

Only a host C compiler and Python's standard library are required. I2C calls
are mocked, so these tests cannot establish physical display connectivity.
"""

import os
from pathlib import Path
import shlex
import shutil
import subprocess
import tempfile
import unittest


TESTS = Path(__file__).resolve().parent
MAIN = TESTS.parent / "main"


class SSD1306HostTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        compiler = shlex.split(os.environ.get("CC", "cc"))
        if not compiler or shutil.which(compiler[0]) is None:
            raise RuntimeError("Host C compiler is required; install one or set CC")

        cls.temp_dir = tempfile.TemporaryDirectory(prefix="ssd1306-host-tests-")
        cls.addClassCleanup(cls.temp_dir.cleanup)
        cls.binary = Path(cls.temp_dir.name) / "ssd1306_host_test"
        command = [
            *compiler,
            "-std=c11",
            "-Wall",
            "-Wextra",
            "-Werror",
            "-Dcalloc=test_calloc",
            "-Dfree=test_free",
            "-I", str(TESTS / "mocks"),
            "-I", str(MAIN),
            str(MAIN / "ssd1306.c"),
            str(TESTS / "ssd1306_host_test.c"),
            "-o", str(cls.binary),
        ]
        compiled = subprocess.run(command, capture_output=True, text=True, timeout=30)
        if compiled.returncode:
            raise RuntimeError(f"C test build failed:\n{compiled.stdout}{compiled.stderr}")

    def run_case(self, case):
        result = subprocess.run(
            [str(self.binary), case], capture_output=True, text=True, timeout=5
        )
        self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
        self.assertEqual(result.stdout.strip(), f"PASS {case}")

    def test_invalid_arguments(self):
        self.run_case("invalid_arguments")

    def test_allocation_failure(self):
        self.run_case("allocation_failure")

    def test_bus_failure(self):
        self.run_case("bus_failure")

    def test_missing_display_nack(self):
        self.run_case("missing_display_nack")

    def test_probe_timeout(self):
        self.run_case("probe_timeout")

    def test_probe_other_error(self):
        self.run_case("probe_other_error")

    def test_add_failure(self):
        self.run_case("add_failure")

    def test_initial_write_failure(self):
        self.run_case("initial_write_failure")

    def test_initial_write_cleanup_retry(self):
        self.run_case("initial_write_cleanup_retry")

    def test_probe_cleanup_retry(self):
        self.run_case("probe_cleanup_retry")

    def test_success_and_release(self):
        self.run_case("success_and_release")

    def test_null_release(self):
        self.run_case("null_release")

    def test_release_device_retry(self):
        self.run_case("release_device_retry")

    def test_release_bus_retry(self):
        self.run_case("release_bus_retry")

    def test_show_success(self):
        self.run_case("show_success")

    def test_show_command_failure(self):
        self.run_case("show_command_failure")

    def test_show_data_failure(self):
        self.run_case("show_data_failure")


if __name__ == "__main__":
    unittest.main()
