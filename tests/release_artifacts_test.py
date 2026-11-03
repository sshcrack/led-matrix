"""Release reuse must reject stale, partial and mismatched CI artifacts."""
import importlib.util
from pathlib import Path
import unittest

spec = importlib.util.spec_from_file_location("release", Path(__file__).parents[1] / "scripts/find_release_artifacts.py")
release = importlib.util.module_from_spec(spec)
spec.loader.exec_module(release)


class ReleaseArtifactTests(unittest.TestCase):
    def setUp(self):
        self.manifest = {"source_tree": "verified-tree", "version": "1.17.0"}
        self.artifacts = [{"name": name, "expired": False} for name in release.REQUIRED]

    def matches(self):
        return release.matching_artifacts(self.manifest, self.artifacts, "verified-tree", "1.17.0")

    def test_exact_source_and_complete_artifacts(self):
        self.assertTrue(self.matches())

    def test_changed_source_is_rejected(self):
        self.manifest["source_tree"] = "different-tree"
        self.assertFalse(self.matches())

    def test_changed_version_is_rejected(self):
        self.manifest["version"] = "1.16.3"
        self.assertFalse(self.matches())

    def test_partial_build_is_rejected(self):
        self.artifacts.pop()
        self.assertFalse(self.matches())

    def test_expired_artifact_is_rejected(self):
        self.artifacts[0]["expired"] = True
        self.assertFalse(self.matches())


if __name__ == "__main__":
    unittest.main()
