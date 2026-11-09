"""Use synthetic atlases and our PC sprites; no original game pixels."""

import json
import struct
from pathlib import Path
import sys
import tempfile
import unittest

from PIL import Image

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / "tools"))
from build_keyboard_mouse_assets import (
    ASSETS,
    HUD_INPUTS,
    auto_gmd_sections,
    gmd_sections,
    patch_gmd,
    patch_texture,
    patch_ui_texture,
    texture_prompt_record,
    tex_runs,
)


class PromptAtlasTest(unittest.TestCase):
    def test_automatic_text_keeps_original_count_and_indices(self):
        sections = ["Press <COL RED>R Button</COL> now.", "A Button confirms."]
        body = b"".join(s.encode() + b"\0" for s in sections)
        header = bytearray(41)
        header[:8] = b"GMD\0\x02\x03\x01\0"
        struct.pack_into("<5I", header, 20, 0, len(sections), 0, len(body), 0)
        original = bytes(header) + body
        automatic, count = patch_gmd(original, "test_eng.gmd", "Auto")
        self.assertEqual(count, 2)
        self.assertEqual(struct.unpack_from("<I", automatic, 24)[0], len(sections))
        ranged, melee = auto_gmd_sections(automatic)
        self.assertEqual(ranged, ["Press <COL RED>E</COL> now.", "V confirms."])
        self.assertEqual(melee, ["Press <COL RED>R</COL> now.", "V confirms."])
        # The ordinary serializer continues rejecting accidental extra entries.
        with self.assertRaises(ValueError):
            gmd_sections(automatic)

    def test_automatic_atlas_record_reconstructs_exact_melee_payload(self):
        header = bytearray.fromhex("54455800a080002001400008011701000000010000000000")
        struct.pack_into("<I", header, 8, 1 | (512 << 6) | (512 << 19))
        struct.pack_into("<I", header, 16, 512 * 512)
        original = bytes(header) + bytes(range(256)) * 1024
        with tempfile.TemporaryDirectory() as directory:
            ranged = patch_ui_texture(
                original, HUD_INPUTS, Path(directory) / "r.png", "Ranged"
            )
            melee = patch_ui_texture(
                original, HUD_INPUTS, Path(directory) / "m.png", "Melee"
            )
        record = texture_prompt_record("HD_hud00_BM_NOMIP", ranged, melee)
        kind, width, height, size, patches = struct.unpack_from("<5I", record)
        self.assertEqual((kind, width, height, size), (2, 512, 512, 512 * 512))
        self.assertGreater(patches, 0)
        baseline = record[20 : 20 + size]
        self.assertEqual(baseline, ranged[24:])
        reconstructed = bytearray(baseline)
        pos = 20 + size
        offsets = []
        for _ in range(patches):
            offset = struct.unpack_from("<I", record, pos)[0]
            self.assertEqual(offset % 16, 0)
            offsets.append(offset)
            reconstructed[offset : offset + 16] = record[pos + 4 : pos + 20]
            pos += 20
        self.assertEqual(offsets, sorted(set(offsets)))
        self.assertEqual(pos, len(record))
        self.assertEqual(bytes(reconstructed), melee[24:])

    def test_hud_clock_background_is_preserved(self):
        header = bytearray.fromhex("54455800a080002001400008011701000000010000000000")
        struct.pack_into("<I", header, 8, 1 | (512 << 6) | (512 << 19))
        struct.pack_into("<I", header, 16, 512 * 512)
        original = bytes(header) + bytes(range(256)) * 1024
        with tempfile.TemporaryDirectory() as directory:
            for layout in ("Melee", "Ranged"):
                result = patch_ui_texture(
                    original, HUD_INPUTS, Path(directory) / (layout + ".png"), layout
                )
                self.assertEqual(result[:24], original[:24])
                buttons_changed = False
                for linear, swizzled in tex_runs(512, 512):
                    x, y = (linear % 2048) // 16 * 4, linear // 2048 * 4
                    before = original[24 + swizzled : 40 + swizzled]
                    after = result[24 + swizzled : 40 + swizzled]
                    # The clock frame and its separately sampled dial background are not inputs.
                    if (x < 96 and y < 96) or (184 <= x < 248 and y < 64):
                        self.assertEqual(before, after, (layout, x, y))
                    if x < 192 and 408 <= y < 440 and before != after:
                        buttons_changed = True
                self.assertTrue(buttons_changed)

    def test_actual_font_glyphs_follow_layout(self):
        original = bytes.fromhex(
            "54455800a080002001400008011701000000010000000000"
        ) + bytes(65536)
        glyphs = {
            g["name"]: g
            for g in json.loads((ASSETS / "icon-map.json").read_text(encoding="utf-8"))[
                "icons"
            ]
        }

        def crop(image, name):
            g = glyphs[name]
            return image.crop(
                (g["x"], g["y"], g["x"] + g["width"], g["y"] + g["height"])
            ).tobytes()

        with tempfile.TemporaryDirectory() as directory:
            previews = {}
            for layout in ("Melee", "Ranged"):
                path = Path(directory) / (layout + ".png")
                result = patch_texture(original, path, layout)
                self.assertEqual(result[:24], original[:24])
                previews[layout] = Image.open(path).convert("RGBA")
            melee, ranged = previews["Melee"], previews["Ranged"]
            shared = (
                "PAD_Y",
                "PAD_SELECT",
                "PAD_ZL",
                "KEY",
                "KEY_U",
                "KEY_R",
                "KEY_D",
                "KEY_L",
                "KEY_UD",
                "KEY_RL",
            )
            for name in shared:
                self.assertEqual(crop(melee, name), crop(ranged, name))
            self.assertEqual(len({crop(ranged, name) for name in shared}), len(shared))
            self.assertEqual(crop(melee, "PAD_X"), crop(ranged, "PAD_A"))
            self.assertEqual(crop(melee, "PAD_A"), crop(ranged, "PAD_R"))
            self.assertNotEqual(crop(melee, "PAD_X"), crop(ranged, "PAD_X"))
            self.assertNotEqual(crop(melee, "PAD_R"), crop(ranged, "PAD_R"))
            self.assertNotEqual(crop(melee, "PAD_R"), crop(melee, "PAD_Y"))


if __name__ == "__main__":
    unittest.main()
