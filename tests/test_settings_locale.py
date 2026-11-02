"""Check host UI translation coverage without game files or external packages."""

import ast
from collections import Counter
from pathlib import Path
import re
import unittest

ROOT = Path(__file__).resolve().parents[1]
LITERAL = r'"(?:\\.|[^"\\])*"'


def literals(text):
    return [ast.literal_eval(value) for value in re.findall(LITERAL, text)]


class SettingsLocaleTest(unittest.TestCase):
    def test_catalog_and_visible_labels(self):
        catalog = (ROOT / "runtime/settings_locale.h").read_text(encoding="utf-8")
        rows = [
            tuple(literals(value))
            for value in re.findall(
                r"\{\s*" + LITERAL + (r"\s*,\s*" + LITERAL) * 6 + r"\s*\}", catalog
            )
        ]
        keys = [row[0] for row in rows]
        self.assertTrue(rows)
        self.assertEqual(
            keys,
            [
                ast.literal_eval(value)
                for value in re.findall(r"^\s*\{\s*(" + LITERAL + ")", catalog, re.M)
            ],
            "Every catalog entry must provide all seven languages",
        )
        self.assertFalse([key for key, count in Counter(keys).items() if count > 1])
        self.assertTrue(all(all(value.strip() for value in row) for row in rows))
        self.assertNotIn(chr(0xFFFD), catalog)
        translations = {row[0]: row[1] for row in rows}
        self.assertEqual(translations["Misc"], "Varios")
        self.assertEqual(translations["Hunter"], "Cazador")
        self.assertEqual(translations["Reset this tab"], "Restablecer esta pestaña")
        formats = re.compile(
            r"%(?:[-+#0 ]*(?:\d+|\*)?(?:\.(?:\d+|\*))?"
            r"(?:hh|h|ll|l|z|t|j|L)?[diuoxXfFeEgGaAcspn]|%)"
        )
        for row in rows:
            expected = formats.findall(row[0])
            for language, translated in enumerate(row[1:], 1):
                self.assertEqual(
                    formats.findall(translated),
                    expected,
                    f"Format arguments changed in language {language}: {row[0]}",
                )

        source = (ROOT / "runtime/settings_panel.cpp").read_text(encoding="utf-8")
        source = re.sub(r"//[^\n]*|/\*.*?\*/", "", source, flags=re.S)
        # INI keys looked up in the settings table (ini("Mode"), lo("FPS")...) are not shown
        source = re.sub(r"\b(?:ini|cold|lo|hi)\(" + LITERAL + r"\)", "", source)
        unchanged = {
            "English",
            "Español",
            "Français",
            "Deutsch",
            "Italiano",
            "简体中文",
            "繁體中文",
            "Xbox",
            "Switch",
            "PlayStation",
            "FSR",
            "FSR 3.1",
            "FSR 4",
            "MSAA",
            "FXAA",
            "Settings##main",
            "Settings##dropdown",
        }
        visible = {
            text
            for text in literals(source)
            if text
            and (text[0].isupper() or " " in text)
            and any(c.isalpha() for c in text)
        }
        self.assertFalse(
            visible - set(keys) - unchanged,
            f"Untranslated UI labels: {sorted(visible - set(keys) - unchanged)}",
        )

        # notices on screen (runtime/net/ldn.c: UI_TEXT("..."))
        notices = literals(
            " ".join(
                re.findall(
                    r"UI_TEXT\((" + LITERAL + r")\)",
                    (ROOT / "runtime/net/ldn.c").read_text(encoding="utf-8"),
                )
            )
        )
        self.assertTrue(notices)
        self.assertFalse(
            set(notices) - set(keys),
            f"Untranslated notices: {sorted(set(notices) - set(keys))}",
        )

        errors = (ROOT / "runtime/config_store.c").read_text(encoding="utf-8")
        messages = [
            literals(value)[-1]
            for value in re.findall(r"fail\([^;]*?" + LITERAL + r"\)", errors)
        ]
        # the settings table's messages (runtime/config.c g_settings)
        table = (ROOT / "runtime/config.c").read_text(encoding="utf-8")
        messages += [
            literals(value)[0]
            for value in re.findall(r"#define ERR_\w+ +(" + LITERAL + ")", table)
        ]
        # Item-box readiness and validation errors are fixed localization keys.
        for path in ("runtime/item_box.c", "runtime/item_box_model.h"):
            item_box = (ROOT / path).read_text(encoding="utf-8")
            messages += [
                ast.literal_eval(value)
                for value in re.findall(
                    r"(?:return\s+|(?:\.reason|\breason)\s*=\s*)(" + LITERAL + ")",
                    item_box,
                )
                if ast.literal_eval(value)
            ]
        self.assertTrue(messages)
        self.assertFalse(
            set(messages) - set(keys),
            f"Untranslated errors: {sorted(set(messages) - set(keys))}",
        )


if __name__ == "__main__":
    unittest.main()
