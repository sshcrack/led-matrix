"""Exercise the production CMake fingerprint generator in isolated source trees."""
import pathlib
import re
import shutil
import subprocess
import tempfile
import unittest

SCRIPT = pathlib.Path(__file__).resolve().parents[1] / "cmake/SceneCodeDigest.cmake"


class SceneCodeDigestTest(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory()
        self.addCleanup(self.temp.cleanup)
        self.root = pathlib.Path(self.temp.name) / "source"
        self.root.mkdir()
        self.output = pathlib.Path(self.temp.name) / "digest.h"
        self.write("plugins/Probe/scenes/Probe.cpp", b"int scene = 1;\n")
        self.write("shared/matrix/render.h", b"int render();\n")
        self.git("init", "-q")
        self.commit()

    def write(self, path, data):
        target = self.root / path
        target.parent.mkdir(parents=True, exist_ok=True)
        target.write_bytes(data)

    def git(self, *args):
        return subprocess.check_output(["git", *args], cwd=self.root, stderr=subprocess.PIPE)

    def digest(self):
        subprocess.run(["cmake", f"-DSOURCE_DIR={self.root}", f"-DOUTPUT={self.output}",
                        "-P", str(SCRIPT)], check=True, capture_output=True)
        return re.search(r'DIGEST "([^"]+)"', self.output.read_text())[1]

    def commit(self):
        self.git("add", ".")
        self.git("-c", "user.name=Test", "-c", "user.email=test@example.invalid",
                 "commit", "-qm", "change")

    def test_source_archive_matches_checkout(self):
        expected = self.digest()
        shutil.rmtree(self.root / ".git")
        self.assertEqual(expected, self.digest())
        self.assertRegex(expected, r"^v2-sha256-[0-9a-f]{64}$")

    def test_staging_does_not_change_digest(self):
        self.write("plugins/Probe/scenes/Probe.cpp", b"int scene = 42;\n")
        expected = self.digest()
        self.git("add", ".")
        self.assertEqual(expected, self.digest())

    def test_windows_text_checkout_matches_lf(self):
        for path in ("plugins/Probe/scenes/Probe.cpp", "shared/matrix/render.h",
                     "plugins/Probe/shader.frag", "shared/fonts/font.bdf",
                     "vcpkg.json", "cmake/overlay-ports/probe/fix.patch",
                     "plugins/Probe/thirdparty/vendor/configure",
                     "plugins/Probe/thirdparty/vendor/config.guess",
                     "plugins/Probe/thirdparty/vendor/project.vcxproj"):
            self.write(path, b"first line\nsecond line\n")
        expected = self.digest()
        for path in self.root.rglob("*"):
            if path.is_file() and ".git" not in path.parts:
                path.write_bytes(path.read_bytes().replace(b"\n", b"\r\n"))
        self.assertEqual(expected, self.digest())

    def test_unrelated_files_do_not_change_digest(self):
        expected = self.digest()
        for path in ("plugins/Probe/README.md", "plugins/Probe/web/index.tsx",
                     "plugins/Probe/desktop/Probe.cpp", "shared/desktop/config.cpp",
                     "plugins/Probe/docs/example.cpp", "plugins/Probe/img/screenshot.png",
                     "plugins/Probe/build/generated.cpp", "plugins/Probe/.git/config",
                     "plugins/Probe/scenes/Probe.cpp.swp", "react-web/src/App.tsx"):
            self.write(path, b"unrelated change\n")
            self.assertEqual(expected, self.digest(), path)

    def test_render_inputs_change_digest(self):
        for path in ("plugins/Probe/scenes/Probe.cpp", "shared/matrix/render.h",
                     "shared/common/protocol.h", "shared/fonts/font.bdf",
                     "src_scene_worker/main.cpp", "CMakeLists.txt", "shared/CMakeLists.txt",
                     "plugins/Probe/CMakeLists.txt", "vcpkg.json", "vcpkg-configuration.json",
                     "cmake/overlay-ports/probe/fix.patch",
                     "plugins/Probe/thirdparty/vendor/source.c", "plugins/Probe/shader.frag"):
            before = self.digest()
            self.write(path, b"changed render input\n")
            self.assertNotEqual(before, self.digest(), path)

    def test_deletion_and_rename_change_digest(self):
        before = self.digest()
        path = self.root / "plugins/Probe/scenes/Probe.cpp"
        path.rename(path.with_name("Renamed.cpp"))
        renamed = self.digest()
        self.assertNotEqual(before, renamed)
        path.with_name("Renamed.cpp").unlink()
        self.assertNotEqual(renamed, self.digest())

    def test_untracked_and_ignored_sources_are_included(self):
        self.write(".gitignore", b"ignored.cpp\n")
        for path in ("plugins/Probe/scenes/new.cpp", "plugins/Probe/scenes/ignored.cpp"):
            before = self.digest()
            self.write(path, b"new render input\n")
            self.assertNotEqual(before, self.digest())

    def test_submodule_worktree_edits_are_included(self):
        vendor = self.root / "plugins/Probe/thirdparty/vendor"
        vendor.mkdir(parents=True)
        subprocess.run(["git", "init", "-q", str(vendor)], check=True)
        self.write("plugins/Probe/thirdparty/vendor/render.c", b"int vendor = 1;\n")
        subprocess.run(["git", "-C", str(vendor), "add", "."], check=True)
        subprocess.run(["git", "-C", str(vendor), "-c", "user.name=Test",
                        "-c", "user.email=test@example.invalid", "commit", "-qm", "vendor"],
                       check=True)
        self.commit()
        before = self.digest()
        self.write("plugins/Probe/thirdparty/vendor/render.c", b"int vendor = 2;\n")
        self.assertNotEqual(before, self.digest())

    def test_output_is_not_rewritten_when_unchanged(self):
        self.digest()
        before = self.output.stat().st_mtime_ns
        self.digest()
        self.assertEqual(before, self.output.stat().st_mtime_ns)

    def test_checkout_location_and_creation_order_do_not_matter(self):
        expected = self.digest()
        original = self.root
        self.root = original.parent / "other checkout"
        self.root.mkdir()
        for path in sorted(original.rglob("*"), reverse=True):
            if path.is_file() and ".git" not in path.parts:
                self.write(str(path.relative_to(original)), path.read_bytes())
        self.assertEqual(expected, self.digest())

    def test_binary_crlf_is_not_normalized(self):
        for path, prefix in (("plugins/Probe/texture.png", b"binary"),
                             ("plugins/Probe/resource.custom", b"\x00binary")):
            self.write(path, prefix + b"\r\n")
            before = self.digest()
            self.write(path, prefix + b"\n")
            self.assertNotEqual(before, self.digest(), path)

    def test_empty_source_tree_fails_closed(self):
        shutil.rmtree(self.root)
        self.root.mkdir()
        with self.assertRaises(subprocess.CalledProcessError):
            self.digest()

    def test_committing_identical_content_does_not_change_digest(self):
        self.write("plugins/Probe/scenes/Probe.cpp", b"int scene = 2;\n")
        dirty = self.digest()
        self.commit()
        self.assertEqual(dirty, self.digest())

    def test_binary_edits_are_distinguished(self):
        self.write("plugins/Probe/texture.png", b"\x00texture-one\xff")
        self.commit()
        self.write("plugins/Probe/texture.png", b"\x00texture-two\xff")
        first = self.digest()
        self.write("plugins/Probe/texture.png", b"\x00texture-six\xff")
        self.assertNotEqual(first, self.digest())

    def test_worker_changes_are_detected(self):
        before = self.digest()
        self.write("src_scene_worker/main.cpp", b"int worker = 2;\n")
        self.assertNotEqual(before, self.digest())


if __name__ == "__main__":
    unittest.main()
