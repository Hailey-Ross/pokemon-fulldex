import re
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
GUIDES = ROOT / "frontend" / "companion_guides.h"

BASE = {"BICYCLE": 450, "OLD ROD": 445, "GOOD ROD": 446, "SUPER ROD": 447}
HGSS = {**BASE, "DOWSING MCHN": 471, "SQUIRTBOTTLE": 477}
EXPECTED = {
    "IPKE": {**HGSS, "COIN CASE": 444, "RAINBOW WING": 483,
             "CLEAR BELL": 474, "S.S. TICKET": 456, "PASS": 480},
    "IPGE": {**HGSS, "COIN CASE": 444, "SILVER WING": 482, "TIDAL BELL": 503,
             "S.S. TICKET": 456, "PASS": 480},
    "CPUE": {**BASE, "VS SEEKER": 443, "EXPLORER KIT": 428, "POKe RADAR": 431,
             "PSYDUCK block ROUTE 210": 464, "The VALLEY WINDWORKS is locked": 438,
             "Can't get into the GALACTIC WAREHOUSE": 463,
             "Locked doors in the GALACTIC HQ": 440},
}
# The HM moves differ by game; find them by the title prefix.
HMS = {
    "IPKE": ["HM01 CUT", "HM02 FLY", "HM03 SURF", "HM04 STRENGTH", "HM05 WHIRLPOOL",
             "HM06 ROCK SMASH", "HM07 WATERFALL", "HM08 ROCK CLIMB"],
    "IPGE": ["HM01 CUT", "HM02 FLY", "HM03 SURF", "HM04 STRENGTH", "HM05 WHIRLPOOL",
             "HM06 ROCK SMASH", "HM07 WATERFALL", "HM08 ROCK CLIMB"],
    "CPUE": ["HM01 CUT", "HM02 FLY", "HM03 SURF", "HM04 STRENGTH", "HM05 DEFOG",
             "HM06 ROCK SMASH", "HM07 WATERFALL", "HM08 ROCK CLIMB"],
}


class GuideItemIdTests(unittest.TestCase):
    def test_gen4_where_is_and_key_items_have_ids(self):
        source = GUIDES.read_text()
        starts = [(m.start(), m.group(1)) for m in re.finditer(r'if\(code=="([A-Z0-9]+)"\)', source)]
        starts.append((len(source), "END"))
        blocks = {code: source[a:b] for (a, code), (b, _) in zip(starts, starts[1:])}
        for code in ("IPKE", "IPGE", "CPUE"):
            expected = dict(EXPECTED[code])
            for n, title in enumerate(HMS[code], 1):
                expected[title] = 420 + n - 1
            for title, item in expected.items():
                m = re.search(r'\{"' + re.escape(title) + r'",.*,"(?:WHERE IS|STUCK\?)",(\d+)\}', blocks[code])
                self.assertIsNotNone(m, f"{code}: {title} has no item id")
                self.assertEqual(int(m.group(1)), item, f"{code}: {title} id")


if __name__ == "__main__":
    unittest.main()
