"""Regression checks for fixed-preset text. No copyrighted game files required."""

from pathlib import Path
import re
import sys
import unittest

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / "tools"))
from keyboard_mouse_text import LANGUAGES, pc_text, tip_override


class KeyboardMouseTextTest(unittest.TestCase):
    def test_languages_and_one_pass_chords(self):
        samples = {
            "chS": ("R键和A键", "E键和V键"),
            "chT": ("R鍵和A鍵", "E鍵和V鍵"),
            "eng": ("R Button or A Button", "E or V"),
            "fre": ("bouton R ou bouton A", "E ou V"),
            "ger": ("R- oder A-Knopf", "E oder V"),
            "ita": ("pulsante R o pulsante A", "E o V"),
            "spa": ("botón R o botón A", "E o V"),
        }
        self.assertEqual(set(samples), set(LANGUAGES))
        for lang, (before, after) in samples.items():
            with self.subTest(lang=lang):
                self.assertEqual(pc_text(before, lang), after)
                chord = pc_text("R+A+B", lang)
                self.assertEqual(chord.count("+"), 2)
                self.assertNotIn("Esc", chord)
                self.assertEqual(pc_text("R+R", lang).count("E"), 2)
                self.assertNotIn("ZL", pc_text("ZL, ZR", lang))
                self.assertNotIn("ZR", pc_text("ZL, ZR", lang))
        self.assertEqual(pc_text("ZL-/ZR-Taste", "ger"), "Tab/T")
        self.assertEqual(pc_text("- Button", "eng"), "Z")

    def test_tags_and_non_button_letters(self):
        for lang, before in {
            "chS": "R系列，A级，X图标；<COL FF0000FF>X键</COL><PAGE>",
            "chT": "R系列，A級，X圖示；<COL FF0000FF>X鍵</COL><PAGE>",
            "eng": 'Barrel Bomb L, rank A, an "X". <COL FF0000FF>X Button</COL><PAGE>',
            "fre": "Aiguisoir X; <COL FF0000FF>bouton X</COL><PAGE>",
            "ger": "R-Serie; <COL FF0000FF>X-Knopf</COL><PAGE>",
            "ita": 'Una "X". <COL FF0000FF>pulsante X</COL><PAGE>',
            "spa": 'Una "X". <COL FF0000FF>botón X</COL><PAGE>',
        }.items():
            after = pc_text(before, lang)
            self.assertEqual(before.split("<")[0], after.split("<")[0])
            self.assertEqual(
                re.findall(r"<[^>]*>", before), re.findall(r"<[^>]*>", after)
            )

    def test_melee_mapping_and_wheel_instructions(self):
        for lang, text in {
            "chS": "X键+A键+R键",
            "chT": "X鍵+A鍵+R鍵",
            "eng": "X Button + A Button + R Button",
            "fre": "bouton X + bouton A + bouton R",
            "ger": "X-Knopf + A-Knopf + R-Taste",
            "ita": "pulsante X + pulsante A + pulsante R",
            "spa": "botón X + botón A + botón R",
        }.items():
            result = pc_text(text, lang, layout="Melee")
            self.assertTrue(all(key in result for key in ("X", "V", "R")))
            self.assertIn("F", tip_override(lang, 234, "L A Y", "Melee"))
        self.assertEqual(
            pc_text("X Button or A Button; R Button", "eng", layout="Melee"),
            "X or V; R",
        )
        self.assertEqual(
            tip_override("eng", 235, "Hold L Button, X Button or B Button", "Melee"),
            "Hold Q, X or Space",
        )

    def test_run_is_separate_from_aim_and_power_run(self):
        self.assertEqual(pc_text("按住R键奔跑"), "按住Shift键奔跑")
        self.assertEqual(pc_text("按住R键瞄准"), "按住E键瞄准")
        self.assertEqual(pc_text("B键：强力冲刺"), "Space键：强力冲刺")
        self.assertEqual(
            pc_text("Hold the R Button to dash.", "eng"), "Hold the Shift to dash."
        )
        self.assertEqual(pc_text("R Button: Power Run", "eng"), "E: Power Run")
        self.assertEqual(
            pc_text("<COL FF0000FF>R键</COL>移动", ordinary_run=True),
            "<COL FF0000FF>Shift键</COL>移动",
        )

    def test_stick_clicks(self):
        self.assertEqual(pc_text("L摇杆键、R摇杆键"), "Ctrl键、Alt键")
        self.assertEqual(pc_text("Press the Right Stick", "eng"), "Alt")
        self.assertEqual(pc_text("Drücke den linken Stick", "ger"), "Ctrl")
        self.assertEqual(
            pc_text("WASD Left Stick / Right Stick", "eng"), "WASD WASD / mouse"
        )

    def test_localized_tips_and_version_guard(self):
        for lang in LANGUAGES:
            self.assertIn("Shift", tip_override(lang, 187, "R"))
            ammo = tip_override(lang, 235, "L X B")
            self.assertTrue(all(key in ammo for key in ("Q", "X", "Space")))
            with self.assertRaises(ValueError):
                tip_override(lang, 187, "unexpected version")


if __name__ == "__main__":
    unittest.main()
