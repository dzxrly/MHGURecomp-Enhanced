"""Explicit controller-name substitutions for the seven MHGU text languages.

Match button phrases, not bare letters (item names, armor series and arena ranks
also contain A/B/X/R). Formatting tags are copied verbatim by pc_text.
"""

import re

LANGUAGES = ("chS", "chT", "eng", "fre", "ger", "ita", "spa")
BUTTONS = {
    "A": "V",
    "B": "Space",
    "X": "X",
    "Y": "F",
    "L": "Q",
    "R": "E",
    "ZL": "Tab",
    "ZR": "T",
    "+": "Esc",
    "＋": "Esc",
    "-": "Z",
    "－": "Z",
}
BUTTON = r"(?:ZL|ZR|[ABXYLR+＋\-－])"
LETTERS = re.compile(r"(?<![A-Za-z])(?:ZL|ZR|[ABXYLR])(?![A-Za-z])", re.I)
COMBO = rf"{BUTTON}(?:\s*(?:[+＋/,、]|或|和|与|與|and|or|et|ou|und|oder|e|o|oppure|y)\s*{BUTTON})*"
PATTERNS = {
    "eng": rf"(?<!\w)({COMBO})\s+Buttons?(?!\w)",
    "fre": rf"\bboutons?\s+({COMBO})(?!\w)",
    "ger": rf"(?<!\w)({COMBO})\s*-\s*(?:Knopfs?|Knöpfe|Tasten?)(?!\w)|(?<!\w)((?:ZL|ZR|[ABXYLR]))-(?=\s*(?:oder|und|[,/]))",
    "ita": rf"\bpulsant[ei]\s+({COMBO})(?!\w)",
    "spa": rf"\bbot[oó]n(?:es)?\s+({COMBO})(?!\w)",
    "chS": rf"(?<![A-Za-z])({COMBO})键",
    "chT": rf"(?<![A-Za-z])({COMBO})鍵|(?<=按)([ABXYLR])(?=：)",
}
CHORD = r"(?<![\w])((?:ZL|ZR|[ABXYLR])(?:\+(?:ZL|ZR|[ABXYLR]))+)(?![\w])"
KEY_TOKENS = {
    lang: re.compile(
        p + "|" + CHORD + r"|(?<![A-Za-z])(ZL|ZR)(?![A-Za-z])",
        re.I if lang not in ("chS", "chT") else 0,
    )
    for lang, p in PATTERNS.items()
}
RUN = {
    "chS": r"奔跑|冲刺|跑动",
    "chT": r"奔跑|衝刺|跑動",
    "eng": r"\bdash\b|\brun(?:ning)?\b",
    "fre": r"\bcourir\b|\bcourse\b",
    "ger": r"\brennen\b|\bsprinten\b",
    "ita": r"\bcorrere\b|\bcorsa\b",
    "spa": r"\bcorrer\b|\bcarrera\b",
}
NOT_RUN = r"瞄准|瞄準|装填|裝填|射击|射擊|强力|強力|力量衝刺|Power Run|Power Dash|Sprint puissant|Kraft|corsa potente|carrera potente|\baim|\breload|\bshoot|\bviser|\bvisée|\btir|\bziel|\bnachlad|\bspar|\bmira|\bdispar"
STICKS = {
    "chS": (r"L摇杆", r"R摇杆", "鼠标"),
    "chT": (r"L(?:操控)?[搖遙]桿", r"R(?:操控)?[搖遙]桿", "滑鼠"),
    "eng": (r"(?:Left|L) Stick", r"(?:Right|R) Stick", "mouse"),
    "fre": (r"stick gauche", r"stick droit", "souris"),
    "ger": (
        r"(?:link(?:e[nmrs]?)?|L)[ -]Sticks?",
        r"(?:recht(?:e[nmrs]?)?|R)[ -]Sticks?",
        "Maus",
    ),
    "ita": (r"stick (?:sinistro|L)", r"stick (?:destro|R)", "mouse"),
    "spa": (r"palanca izquierda", r"palanca derecha", "ratón"),
}


def pc_text(text, language="chS", ordinary_run=False, layout="Ranged"):
    left, right, mouse = STICKS[language]
    # Clicks are separate inputs from stick motion. Handle them before replacing motion names.
    clicks = {
        "chS": [(r"L摇杆键", "Ctrl键"), (r"R摇杆键", "Alt键")],
        "chT": [(rf"按下{left}", "按下Ctrl鍵"), (rf"按下{right}", "按下Alt鍵")],
        "eng": [
            (
                rf"(?:press(?:ing)? (?:down )?(?:on )?(?:the )?|click(?:ing)? (?:the )?)({left}|{right})",
                None,
            )
        ],
        "fre": [(rf"appuy(?:er|ant|ez) sur le ({left}|{right})", None)],
        "ger": [
            (rf"auf (?:den )?({left}|{right}) drücken", None),
            (rf"({left}|{right}) drückst", None),
            (rf"drücke (?:den )?({left}|{right})", None),
        ],
        "ita": [
            (rf"premi (?:lo )?({left}|{right})", None),
            (rf"({left}|{right}) premuto", None),
        ],
        "spa": [(rf"pulsa(?:r)? (?:la )?({left}|{right})", None)],
    }

    def part_replace(part):
        for pattern, replacement in clicks[language]:
            part = re.sub(
                pattern,
                replacement
                or (
                    lambda m: "Ctrl" if re.fullmatch(left, m.group(1), re.I) else "Alt"
                ),
                part,
                flags=re.I,
            )

        def button(match):
            value = next(g for g in match.groups() if g is not None)
            mapping = dict(BUTTONS)
            if layout == "Melee":
                mapping["R"] = "R"
            if language in ("chS", "chT"):
                suffix = "键" if language == "chS" else "鍵"
                mapping = {k: v + suffix for k, v in mapping.items()}
            # Use the local sentence (including wrapped lines), never a whole tutorial page.
            before = re.split(r"[.!?。！？]|\r?\n\r?\n", part[: match.start()])[-1]
            after = re.split(r"[.!?。！？]|\r?\n\r?\n", part[match.end() :])[0]
            context = before + match.group() + after
            if value.upper() == "R" and (
                ordinary_run
                or (
                    re.search(RUN[language], context, re.I)
                    and not re.search(NOT_RUN, context, re.I)
                )
            ):
                mapping["R"] = "Shift" + (
                    "键" if language == "chS" else "鍵" if language == "chT" else ""
                )
            if value in mapping:
                return mapping[value]
            return LETTERS.sub(lambda m: mapping[m.group().upper()], value)

        part = KEY_TOKENS[language].sub(button, part)
        part = re.sub(left, "WASD", part, flags=re.I)
        part = re.sub(right, mouse, part, flags=re.I)
        return (
            part.replace("推动WASD", "按WASD")
            .replace("推动鼠标", "移动鼠标")
            .replace("操作WASD", "使用WASD")
        )

    chunks = re.split(r"(<[^>]*>)", text)
    result = "".join(s if i % 2 else part_replace(s) for i, s in enumerate(chunks))
    if re.findall(r"<[^>]*>", result) != re.findall(r"<[^>]*>", text):
        raise AssertionError("A text edit changed game control tags")
    return result


TIPS = {
    "chS": (
        "使用WASD移动。\r\n收起武器后，按住Shift键即可奔跑。",
        "滚动鼠标滚轮可切换弹药与瓶。\r\n也可按住Q键，再按X键或Space键选择，\r\n松开Q键确定。",
    ),
    "chT": (
        "使用WASD移動。\r\n收起武器後，按住Shift鍵即可奔跑。",
        "滾動滑鼠滾輪可切換子彈與瓶。\r\n也可按住Q鍵，再按X鍵或Space鍵選擇，\r\n放開Q鍵確定。",
    ),
    "eng": (
        "Use WASD to move. With your weapon\r\nsheathed, hold Shift to dash.",
        "Use the mouse wheel to select\r\nammo or coatings. You can also hold\r\nQ and press X or Space to select.\r\nRelease Q to confirm.",
    ),
    "fre": (
        "Utilisez WASD pour vous déplacer.\r\nArme rangée, maintenez Shift\r\npour courir.",
        "La molette sélectionne les\r\nmunitions et les fioles. Ou maintenez\r\nQ et appuyez sur X ou Space.\r\nRelâchez Q pour valider.",
    ),
    "ger": (
        "Bewege dich mit WASD. Halte Shift\r\nbei weggesteckter Waffe gedrückt,\r\num zu rennen.",
        "Wähle Munition oder Beschichtungen\r\nmit dem Mausrad. Oder halte Q und\r\ndrücke X oder Space zur Auswahl.\r\nLasse Q zum Bestätigen los.",
    ),
    "ita": (
        "Usa WASD per muoverti. Con l'arma\r\nrinfoderata, tieni premuto Shift\r\nper correre.",
        "Usa la rotella per scegliere\r\nmunizioni o boccette. Oppure tieni\r\npremuto Q e premi X o Space.\r\nRilascia Q per confermare.",
    ),
    "spa": (
        "Usa WASD para moverte. Con el arma\r\nguardada, mantén Shift para correr.",
        "Usa la rueda para elegir munición\r\no revestimientos. También puedes\r\nmantener Q y pulsar X o Space.\r\nSuelta Q para confirmar.",
    ),
}


ITEM_TIPS = {
    "chS": "滚动鼠标滚轮可选择道具。\r\n也可按住Q键，用V键或F键选择。\r\n松开Q键后，按F键使用道具。",
    "chT": "滾動滑鼠滾輪可選擇道具。\r\n也可按住Q鍵，用V鍵或F鍵選擇。\r\n放開Q鍵後，按F鍵使用道具。",
    "eng": "Use the mouse wheel to select items.\r\nOr hold Q and press V or F.\r\nRelease Q, then press F to use an item.",
    "fre": "La molette sélectionne les objets.\r\nOu maintenez Q et appuyez sur V ou F.\r\nRelâchez Q, puis utilisez l'objet avec F.",
    "ger": "Wähle Objekte mit dem Mausrad.\r\nOder halte Q und drücke V oder F.\r\nLasse Q los und nutze das Objekt mit F.",
    "ita": "Usa la rotella per scegliere un oggetto.\r\nOppure tieni Q e premi V o F.\r\nRilascia Q e usa l'oggetto con F.",
    "spa": "Usa la rueda para elegir un objeto.\r\nO mantén Q y pulsa V o F.\r\nSuelta Q y usa el objeto con F.",
}


def tip_override(language, index, original, layout="Ranged"):
    # Same labeled sections in the seven version-0x10302 HunterTips tables.
    needed = (
        ("R",) if index == 187 else ("L", "A", "Y") if index == 234 else ("L", "X", "B")
    )
    if any(b not in original for b in needed) or "<" in original:
        raise ValueError(f"Unexpected HunterTips entry {language}:{index}")
    if index == 234:
        return ITEM_TIPS[language]
    if index == 235 and layout == "Melee":
        return pc_text(original, language, layout=layout)
    return TIPS[language][0 if index == 187 else 1]
