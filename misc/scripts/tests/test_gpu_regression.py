"""Check that the GPU regression report rejects missing or erroneous evidence."""
import sys
from pathlib import Path
import unittest

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
from phase2_gpu_regression import cases, inspect_log


class GpuRegressionTest(unittest.TestCase):
    def test_planned_cases_include_all_scenes_and_lifetime_checks(self):
        labels = {name for name, _ in cases(Path("/tmp/captures"))}
        self.assertTrue({"primitives", "hierarchy", "depth", "orthographic", "courtyard", "frame-graph", "resize", "inflight", "subresources"} <= labels)

    def test_validation_errors_and_missing_validation_fail(self):
        self.assertTrue(inspect_log("triangle", 0, ""))
        self.assertTrue(inspect_log("triangle", 1, "validation=true"))
        self.assertTrue(inspect_log("triangle", 0, "validation=true\nSYNC-HAZARD"))
        self.assertTrue(inspect_log("triangle", 0, "validation=true\nVUID-example"))
        self.assertFalse(inspect_log("triangle", 0, "validation=true"))

    def test_scene_requires_all_expected_readbacks(self):
        partial = "validation=true\n" + "Scene readback verified:\n" * 7
        self.assertTrue(inspect_log("primitives", 0, partial))
        self.assertFalse(inspect_log("primitives", 0, partial + "Scene readback verified:\n"))

    def test_pool_counts_and_completion_are_verified(self):
        log = "validation=true\nRendered 120 frames\nreleasing 6 textures, 2 buffers"
        self.assertFalse(inspect_log("inflight", 0, log))
        self.assertTrue(inspect_log("inflight", 0, log.replace("6 textures", "9 textures")))
        self.assertTrue(inspect_log("inflight", 0, log.replace("Rendered 120 frames", "Rendered 3 frames")))

    def test_subresources_require_gpu_readback_evidence(self):
        log = "validation=true\n"
        self.assertTrue(inspect_log("subresources", 0, log))
        self.assertFalse(inspect_log(
            "subresources", 0, log + "Subresources GPU verified: 8 frames, 3 mips x 2 layers"))


if __name__ == "__main__":
    unittest.main()
