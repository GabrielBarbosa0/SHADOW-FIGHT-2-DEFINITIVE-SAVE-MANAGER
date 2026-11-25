from __future__ import annotations

import tempfile
import unittest
import xml.etree.ElementTree as ET
from pathlib import Path

from save_manager.save_model import CHEAT_GEMS, CHEAT_MONEY, CHEAT_ORBS, SaveConfig, generate_save


ROOT = Path(__file__).resolve().parents[1]
FRESH = ROOT / "saves" / "templates" / "fresh-default.xml"
TITAN = ROOT / "saves" / "titan-complete" / "users.xml"
PROGRESS = ROOT / "saves" / "templates" / "character-progress.xml"


class SaveGenerationTests(unittest.TestCase):
    def generate(self, config: SaveConfig):
        temp = tempfile.TemporaryDirectory()
        self.addCleanup(temp.cleanup)
        output = Path(temp.name) / "users.xml"
        summary = generate_save(config, FRESH, TITAN, PROGRESS, output)
        ET.parse(output)
        return output, summary

    def test_normal_campaign_removes_all_cheats(self) -> None:
        _output, summary = self.generate(SaveConfig())
        self.assertEqual(summary.level, 1)
        self.assertEqual(summary.experience, 0)
        self.assertEqual(summary.money, 0)
        self.assertEqual(summary.gems, 50)
        self.assertEqual(summary.orbs, (0, 0, 0))
        self.assertFalse(summary.unlimited_energy)
        self.assertEqual(summary.weapon, "Fists")
        self.assertEqual(summary.tutorial, "MOVE")
        self.assertEqual(summary.battle_count, 1)

    def test_maximum_campaign_has_requested_resources(self) -> None:
        config = SaveConfig(
            level=52,
            rich_money=True,
            rich_gems=True,
            unlimited_energy=True,
            rich_orbs=True,
        )
        _output, summary = self.generate(config)
        self.assertEqual(summary.level, 52)
        self.assertEqual(summary.experience, 88_000_002)
        self.assertEqual(summary.money, CHEAT_MONEY)
        self.assertEqual(summary.gems, CHEAT_GEMS)
        self.assertEqual(summary.orbs, (CHEAT_ORBS, CHEAT_ORBS, CHEAT_ORBS))
        self.assertTrue(summary.unlimited_energy)
        self.assertEqual(summary.armor, "Body")

    def test_titan_equipment_is_copied_into_fresh_campaign(self) -> None:
        _output, summary = self.generate(SaveConfig(level=52, equipment="titan"))
        self.assertEqual(summary.tutorial, "MOVE")
        self.assertEqual(summary.zone, "ZONE_1")
        self.assertEqual(summary.armor, "BODY_TITAN")
        self.assertEqual(summary.helm, "HEAD_TITAN")
        self.assertEqual(summary.weapon, "WEAPON_TITAN_GIANT_SWORD")
        self.assertEqual(summary.ranged, "RANGED_TITANS_HARPOON")
        self.assertEqual(summary.magic, "MAGIC_MIND_THROW")

    def test_all_main_boss_equipment_sets_work_in_a_fresh_campaign(self) -> None:
        expected = {
            "lynx": ("boss_lynx", "BODY_LYNX", "HEAD_LYNX", "WEAPON_LYNX_CLAWS"),
            "hermit": ("boss_hermit", "ARMOR_ROBE", "HEAD_HERMIT", "WEAPON_HERMIT_SWORDS"),
            "butcher": ("boss_butcher", "BODY_BUTCHER", "HEAD_BUTCHER", "WEAPON_BUTCHER_KNIVES"),
            "wasp": ("boss_wasp", "ARMOR_WASP", "HEAD_WASP", "WEAPON_WASP_NAGINATA"),
            "widow": ("boss_huntress", "ARMOR_HUNTRESS", "HEAD_HUNTRESS", "WEAPON_WIDOW_FANS"),
            "shogun": ("boss_shogun", "ARMOR_SHOGUN", "HELM_CRESCENT", "WEAPON_SHOGUN_KATANA"),
        }
        for preset, fields in expected.items():
            with self.subTest(preset=preset):
                output, summary = self.generate(SaveConfig(level=52, equipment=preset))
                self.assertEqual((summary.avatar, summary.armor, summary.helm, summary.weapon), fields)
                self.assertEqual(summary.tutorial, "MOVE")
                self.assertEqual(summary.zone, "ZONE_1")
                warrior = ET.parse(output).getroot().find("./Warriors/Warrior")
                self.assertIsNotNone(warrior)
                item = warrior.find(f"./Items/Item[@Name='{fields[3]}']")
                self.assertIsNotNone(item)
                self.assertEqual(item.get("Equipped"), "1")

    def test_butcher_uses_heavy_skeleton_and_shogun_has_full_loadout(self) -> None:
        butcher_output, _summary = self.generate(SaveConfig(level=52, equipment="butcher"))
        butcher = ET.parse(butcher_output).getroot().find("./Warriors/Warrior")
        self.assertEqual(butcher.get("Skeleton"), "SkeletonHeavy")

        shogun_output, shogun_summary = self.generate(SaveConfig(level=52, equipment="shogun"))
        self.assertEqual(shogun_summary.ranged, "RANGED_CHAKRAM_OF_MASTER")
        self.assertEqual(shogun_summary.magic, "MAGIC_ASTEROID")
        shogun = ET.parse(shogun_output).getroot().find("./Warriors/Warrior")
        katana = shogun.find("./Items/Item[@Name='WEAPON_SHOGUN_KATANA']")
        self.assertEqual(katana.get("UpgradeLevel"), "5200")

    def test_custom_values_are_preserved(self) -> None:
        config = SaveConfig(
            level=17,
            rich_money=True,
            money=123456,
            rich_gems=True,
            gems=789,
            rich_orbs=True,
            orbs=42,
        )
        _output, summary = self.generate(config)
        self.assertEqual(summary.level, 17)
        self.assertEqual(summary.money, 123456)
        self.assertEqual(summary.gems, 789)
        self.assertEqual(summary.orbs, (42, 42, 42))


if __name__ == "__main__":
    unittest.main()
