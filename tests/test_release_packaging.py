"""Verify release contents without a ROM or real executables."""

import importlib.util
import json
from pathlib import Path
import subprocess
import sys
import tarfile
import tempfile
import unittest
from unittest.mock import patch
import zipfile

ROOT = Path(__file__).resolve().parents[1]
spec = importlib.util.spec_from_file_location("dist", ROOT / "tools/dist.py")
dist = importlib.util.module_from_spec(spec)
spec.loader.exec_module(dist)


class ReleasePackagingTest(unittest.TestCase):
    def test_archives_and_prompt_allowlist(self):
        scratch = ROOT / ".agents"
        scratch.mkdir(exist_ok=True)
        with tempfile.TemporaryDirectory(dir=scratch) as temporary:
            base = Path(temporary)
            build, output, mod = base / "build", base / "dist", base / "Auto"
            build.mkdir()
            for name in (
                "mhgurecomp.exe",
                "mhgugame.dll",
                "mhgurecomp",
                "mhgugame.so",
                "pipelines.list",
            ):
                (build / name).write_bytes(b"test")
            version = (ROOT / "VERSION").read_text(encoding="utf-8").strip()
            for name in ("mhgurecomp-updater.exe", "mhgurecomp-updater"):
                (build / name).write_bytes(f"MHGURecomp-updater/{version}\0".encode())
            runtimes = tuple(
                name for group in dist.GRAPHICS_RUNTIME_GROUPS for name in group
            )
            for name in runtimes:
                path = build / name
                path.parent.mkdir(parents=True, exist_ok=True)
                path.write_bytes(name.encode())
            (build / "graphics/nvidia/ngx/nr/private-cache").write_bytes(
                b"must never ship"
            )
            (build / "nvngx_dlss.dll").write_bytes(b"stale flat-layout runtime")
            (build / "licenses").mkdir()
            (build / "licenses/test.txt").write_text("license")
            (build / "data").mkdir()
            (build / "data/private-rom").write_text("must never ship")
            (build / "mods").mkdir()
            (build / "mods/private-mod").write_text("must never ship")
            (build / "fonts").mkdir()
            (build / "fonts/private-font.ttf").write_bytes(b"must never ship")
            romfs = mod / "romfs"
            (romfs / "nativeNX").mkdir(parents=True)
            (romfs / "nativeNX/test.gmd").write_bytes(b"synthetic fixture")
            (romfs / "keyboard_mouse_input.flag").write_text("Auto\n")
            (romfs / "keyboard_mouse_prompts.bin").write_bytes(b"MHGUPC01fixture")
            for platform in ("windows", "linux"):
                subprocess.run(
                    [
                        sys.executable,
                        str(ROOT / "tools/dist.py"),
                        str(build),
                        str(output),
                        platform,
                        version,
                        "--keyboard-mouse-mod",
                        str(mod),
                    ],
                    check=True,
                    stdout=subprocess.PIPE,
                )
            for archive in output.iterdir():
                if archive.suffix == ".sha256":
                    continue
                if archive.suffix == ".zip":
                    self.assertEqual(
                        archive.name, "MHGURecomp-Enhancement-win_x86-64.zip"
                    )
                    with zipfile.ZipFile(archive) as stream:
                        names = stream.namelist()
                        metadata = json.loads(
                            stream.read("Windows-MHGURecomp/release-info.json")
                        )
                        self.assertIn("Windows-MHGURecomp/docs/README.zh-CN.md", names)
                        font = "Windows-MHGURecomp/fonts/NotoSansCJKsc-Regular.otf"
                        self.assertEqual(
                            stream.getinfo(font).file_size,
                            (ROOT / "third_party/noto" / dist.MENU_FONT).stat().st_size,
                        )
                        with stream.open(font) as data:
                            self.assertEqual(data.read(4), b"OTTO")
                        for name in runtimes:
                            archived = "Windows-MHGURecomp/" + name
                            self.assertEqual(names.count(archived), 1)
                            self.assertEqual(stream.read(archived), name.encode())
                        self.assertNotIn("Windows-MHGURecomp/nvngx_dlss.dll", names)
                else:
                    with tarfile.open(archive) as stream:
                        names = stream.getnames()
                        self.assertEqual(
                            stream.getmember("ArchLinux-MHGURecomp/mhgurecomp").mode,
                            0o755,
                        )
                        font = "ArchLinux-MHGURecomp/fonts/NotoSansCJKsc-Regular.otf"
                        self.assertEqual(
                            stream.getmember(font).size,
                            (ROOT / "third_party/noto" / dist.MENU_FONT).stat().st_size,
                        )
                        with stream.extractfile(font) as data:
                            self.assertEqual(data.read(4), b"OTTO")
                for suffix in (
                    "/fonts/NotoSansCJKsc-Regular.otf",
                    "/licenses/NotoSansCJK-OFL.txt",
                    "/licenses/NotoSansCJK-NOTICE.txt",
                ):
                    self.assertEqual(sum(n.endswith(suffix) for n in names), 1)
                self.assertTrue(
                    any(
                        n.endswith(
                            "/mods/Keyboard_Mouse_Input_Auto/romfs/nativeNX/test.gmd"
                        )
                        for n in names
                    )
                )
                self.assertFalse(any("private-" in n for n in names))
                self.assertTrue(Path(str(archive) + ".sha256").is_file())
                self.assertTrue(
                    Path(str(archive) + ".sha256")
                    .read_text(encoding="ascii")
                    .endswith(f"  {archive.name}\n")
                )
            self.assertEqual(metadata["version"], version)
            self.assertEqual(
                metadata["release_title"], version + "-" + metadata["upstream_version"]
            )
            (romfs / "unrelated-save").write_bytes(b"private")
            with self.assertRaises(ValueError):
                dist.prompt_files(mod)
            with patch.object(dist, "ROOT", base), self.assertRaises(SystemExit):
                dist.files(str(build), "windows")
            for name in runtimes:
                path = build / name
                content = path.read_bytes()
                path.unlink()
                with self.subTest(missing_runtime=name), self.assertRaises(SystemExit):
                    dist.files(str(build), "windows")
                path.write_bytes(content)


if __name__ == "__main__":
    unittest.main()
