"""Conjuntos usados somente pelos diagnósticos visuais do módulo LAN."""

import random


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

EQUIPMENT_PRESET_NAMES = tuple(EQUIPMENT_PRESETS)


def select_match_presets(seed: int) -> tuple[str, str]:
    """Sorteia P1/P2 de forma reproduzivel; repeticoes sao permitidas."""

    if not 1 <= seed <= 0xFFFFFFFFFFFFFFFF:
        raise ValueError("seed deve ficar no intervalo uint64 sem zero")
    generator = random.Random(seed)
    return (
        generator.choice(EQUIPMENT_PRESET_NAMES),
        generator.choice(EQUIPMENT_PRESET_NAMES),
    )
