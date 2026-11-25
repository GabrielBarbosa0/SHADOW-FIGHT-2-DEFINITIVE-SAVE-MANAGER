from __future__ import annotations

import copy
import hashlib
import xml.etree.ElementTree as ET
from dataclasses import dataclass
from pathlib import Path


MAX_LEVEL = 52
DEFAULT_MONEY = 0
DEFAULT_GEMS = 50
CHEAT_MONEY = 6_144_458_734_554
CHEAT_GEMS = 10_000_000_000_035
CHEAT_ORBS = 2_147_000_000

EQUIPMENT_PRESETS = {
    "initial": {"Avatar": "avatar_hero", "Skeleton": "Skeleton", "Armor": "Body", "Helm": "Head", "Weapon": "Fists", "Ranged": "NoRanged", "Magic": "NoMagic"},
    "lynx": {"Avatar": "boss_lynx", "Skeleton": "Skeleton", "Armor": "BODY_LYNX", "Helm": "HEAD_LYNX", "Weapon": "WEAPON_LYNX_CLAWS", "Ranged": "NoRanged", "Magic": "NoMagic"},
    "hermit": {"Avatar": "boss_hermit", "Skeleton": "Skeleton", "Armor": "ARMOR_ROBE", "Helm": "HEAD_HERMIT", "Weapon": "WEAPON_HERMIT_SWORDS", "Ranged": "NoRanged", "Magic": "NoMagic"},
    "butcher": {"Avatar": "boss_butcher", "Skeleton": "SkeletonHeavy", "Armor": "BODY_BUTCHER", "Helm": "HEAD_BUTCHER", "Weapon": "WEAPON_BUTCHER_KNIVES", "Ranged": "NoRanged", "Magic": "NoMagic"},
    "wasp": {"Avatar": "boss_wasp", "Skeleton": "Skeleton", "Armor": "ARMOR_WASP", "Helm": "HEAD_WASP", "Weapon": "WEAPON_WASP_NAGINATA", "Ranged": "NoRanged", "Magic": "NoMagic"},
    "widow": {"Avatar": "boss_huntress", "Skeleton": "Skeleton", "Armor": "ARMOR_HUNTRESS", "Helm": "HEAD_HUNTRESS", "Weapon": "WEAPON_WIDOW_FANS", "Ranged": "NoRanged", "Magic": "NoMagic"},
    "shogun": {"Avatar": "boss_shogun", "Skeleton": "Skeleton", "Armor": "ARMOR_SHOGUN", "Helm": "HELM_CRESCENT", "Weapon": "WEAPON_SHOGUN_KATANA", "Ranged": "RANGED_CHAKRAM_OF_MASTER", "Magic": "MAGIC_ASTEROID"},
    "titan": {"Avatar": "avatar_human", "Skeleton": "Skeleton", "Armor": "BODY_TITAN", "Helm": "HEAD_TITAN", "Weapon": "WEAPON_TITAN_GIANT_SWORD", "Ranged": "RANGED_TITANS_HARPOON", "Magic": "MAGIC_MIND_THROW"},
}

EQUIPMENT_FIELDS = ("Armor", "Helm", "Weapon", "Ranged", "Magic")
ORB_NAMES = ("ForgeMaterial1", "ForgeMaterial2", "ForgeMaterial3")


@dataclass(frozen=True)
class SaveConfig:
    base_profile: str = "fresh"
    level: int = 1
    rich_money: bool = False
    money: int = CHEAT_MONEY
    rich_gems: bool = False
    gems: int = CHEAT_GEMS
    unlimited_energy: bool = False
    rich_orbs: bool = False
    orbs: int = CHEAT_ORBS
    equipment: str = "initial"

    def validate(self) -> None:
        if self.base_profile not in {"fresh", "titan_complete"}:
            raise ValueError(f"Perfil-base inválido: {self.base_profile}")
        if not 1 <= self.level <= MAX_LEVEL:
            raise ValueError(f"O nível deve estar entre 1 e {MAX_LEVEL}.")
        if self.equipment not in EQUIPMENT_PRESETS:
            raise ValueError(f"Equipamento inválido: {self.equipment}")
        for label, value in (("moedas", self.money), ("gemas", self.gems), ("orbes", self.orbs)):
            if value < 0:
                raise ValueError(f"O valor de {label} não pode ser negativo.")


@dataclass(frozen=True)
class SaveSummary:
    path: Path
    sha256: str
    level: int
    experience: int
    money: int
    gems: int
    unlimited_energy: bool
    orbs: tuple[int, int, int]
    avatar: str
    armor: str
    helm: str
    weapon: str
    ranged: str
    magic: str
    tutorial: str
    zone: str
    item_count: int
    battle_count: int

    def as_text(self) -> str:
        energy = "infinita" if self.unlimited_energy else "normal"
        return (
            f"Nível: {self.level} (EXP {self.experience:,})\n"
            f"Moedas: {self.money:,}\n"
            f"Gemas: {self.gems:,}\n"
            f"Energia: {energy}\n"
            f"Orbes: verde {self.orbs[0]:,} | vermelho {self.orbs[1]:,} | roxo {self.orbs[2]:,}\n"
            f"Campanha: tutorial={self.tutorial}, zona={self.zone}, batalhas={self.battle_count}\n"
            f"Visual: avatar={self.avatar}, armadura={self.armor}, elmo={self.helm}\n"
            f"Equipamento: {self.weapon}, {self.ranged}, {self.magic}\n"
            f"Itens no XML: {self.item_count}\n"
            f"SHA-256: {self.sha256}"
        )


def _warrior(root: ET.Element) -> ET.Element:
    warrior = root.find("./Warriors/Warrior")
    if warrior is None:
        raise ValueError("XML inválido: nó Warriors/Warrior não encontrado.")
    return warrior


def _experience_for_level(level: int, progress_path: Path) -> int:
    if level <= 1:
        return 0
    progress_root = ET.parse(progress_path).getroot()
    previous = progress_root.find(f"./Thresholds/Threshold[@Level='{level - 1}']")
    if previous is None:
        raise ValueError(f"Não foi possível calcular a experiência para o nível {level}.")
    # O save completo confirma este formato: nível 52 = limite do nível 51 + 2.
    return int(previous.attrib["Exp"]) + 2


def _find_item(items: ET.Element, name: str) -> ET.Element | None:
    return next((item for item in items.findall("Item") if item.get("Name") == name), None)


def _ensure_item(items: ET.Element, donor_items: ET.Element, name: str, level: int = 1) -> ET.Element:
    existing = _find_item(items, name)
    if existing is not None:
        return existing
    donor = _find_item(donor_items, name)
    if donor is None:
        copied = ET.Element("Item", {
            "Name": name,
            "Equipped": "0",
            "Count": "1",
            "UpgradeLevel": str(level * 100),
            "DeliveryTime": "-1",
            "DeliveryUpgradeLevel": "-1",
            "AcquireType": "Item",
        })
    else:
        copied = copy.deepcopy(donor)
    items.append(copied)
    return copied


def _set_equipment(warrior: ET.Element, donor_warrior: ET.Element, preset: str) -> None:
    selected = EQUIPMENT_PRESETS[preset]
    level = int(warrior.get("Level", "1"))
    items = warrior.find("Items")
    donor_items = donor_warrior.find("Items")
    if items is None or donor_items is None:
        raise ValueError("XML inválido: lista de itens não encontrada.")

    for item in items.findall("Item"):
        if item.get("Equipped") == "1":
            item.set("Equipped", "0")

    for field, item_name in selected.items():
        warrior.set(field, item_name)
        if field in EQUIPMENT_FIELDS:
            _ensure_item(items, donor_items, item_name, level).set("Equipped", "1")


def _set_energy(warrior: ET.Element, donor_warrior: ET.Element, enabled: bool) -> None:
    items = warrior.find("Items")
    donor_items = donor_warrior.find("Items")
    if items is None or donor_items is None:
        raise ValueError("XML inválido: lista de itens não encontrada.")
    current = _find_item(items, "Unlimited_Energy")
    if enabled and current is None:
        _ensure_item(items, donor_items, "Unlimited_Energy")
    elif not enabled and current is not None:
        items.remove(current)


def _set_orbs(warrior: ET.Element, enabled: bool, amount: int) -> None:
    currencies = warrior.find("Currencies")
    if currencies is None:
        currencies = ET.SubElement(warrior, "Currencies")
    for name in ORB_NAMES:
        if enabled:
            currencies.set(name, str(amount))
        else:
            currencies.attrib.pop(name, None)


def generate_save(
    config: SaveConfig,
    fresh_template: Path,
    titan_template: Path,
    progress_template: Path,
    output_path: Path,
) -> SaveSummary:
    config.validate()
    source = fresh_template if config.base_profile == "fresh" else titan_template
    tree = ET.parse(source)
    root = tree.getroot()
    warrior = _warrior(root)
    donor_warrior = _warrior(ET.parse(titan_template).getroot())

    warrior.set("Level", str(config.level))
    warrior.set("Experience", str(_experience_for_level(config.level, progress_template)))
    warrior.set("Money", str(config.money if config.rich_money else DEFAULT_MONEY))
    warrior.set("Bonus", str(config.gems if config.rich_gems else DEFAULT_GEMS))

    _set_energy(warrior, donor_warrior, config.unlimited_energy)
    _set_orbs(warrior, config.rich_orbs, config.orbs)
    _set_equipment(warrior, donor_warrior, config.equipment)

    output_path.parent.mkdir(parents=True, exist_ok=True)
    ET.indent(tree, space="  ")
    tree.write(output_path, encoding="utf-8", xml_declaration=True)
    return inspect_save(output_path)


def inspect_save(path: Path) -> SaveSummary:
    root = ET.parse(path).getroot()
    warrior = _warrior(root)
    items = warrior.find("Items")
    battles = warrior.find("Battles")
    currencies = warrior.find("Currencies")
    item_nodes = [] if items is None else items.findall("Item")
    battle_nodes = [] if battles is None else battles.findall("Battle")
    orbs = tuple(int((currencies.get(name, "0") if currencies is not None else "0")) for name in ORB_NAMES)
    digest = hashlib.sha256(path.read_bytes()).hexdigest().upper()
    return SaveSummary(
        path=path,
        sha256=digest,
        level=int(warrior.get("Level", "0")),
        experience=int(warrior.get("Experience", "0")),
        money=int(warrior.get("Money", "0")),
        gems=int(warrior.get("Bonus", "0")),
        unlimited_energy=any(item.get("Name") == "Unlimited_Energy" for item in item_nodes),
        orbs=(orbs[0], orbs[1], orbs[2]),
        avatar=warrior.get("Avatar", ""),
        armor=warrior.get("Armor", ""),
        helm=warrior.get("Helm", ""),
        weapon=warrior.get("Weapon", ""),
        ranged=warrior.get("Ranged", ""),
        magic=warrior.get("Magic", ""),
        tutorial=warrior.get("Tutorial", ""),
        zone=warrior.get("CurrentZone", ""),
        item_count=len(item_nodes),
        battle_count=len(battle_nodes),
    )
