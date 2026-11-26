package com.sf2.lan;

/** Conjunto fechado de equipamentos suportados pelo prototipo LAN. */
final class LanEquipmentPreset {
    static final String[] NAMES = {
        "initial", "lynx", "hermit", "butcher", "wasp", "widow", "shogun", "titan"
    };

    final String name;
    final String avatar;
    final String skeleton;
    final String armor;
    final String helm;
    final String weapon;
    final String ranged;
    final String magic;

    private LanEquipmentPreset(String name, String avatar, String skeleton, String armor,
        String helm, String weapon, String ranged, String magic) {
        this.name = name;
        this.avatar = avatar;
        this.skeleton = skeleton;
        this.armor = armor;
        this.helm = helm;
        this.weapon = weapon;
        this.ranged = ranged;
        this.magic = magic;
    }

    static LanEquipmentPreset get(String name) {
        switch (name) {
            case "initial":
                return new LanEquipmentPreset(name, "avatar_hero", "Skeleton", "Body", "Head",
                    "Fists", "NoRanged", "NoMagic");
            case "lynx":
                return new LanEquipmentPreset(name, "boss_lynx", "Skeleton", "BODY_LYNX", "HEAD_LYNX",
                    "WEAPON_LYNX_CLAWS", "NoRanged", "NoMagic");
            case "hermit":
                return new LanEquipmentPreset(name, "boss_hermit", "Skeleton", "ARMOR_ROBE", "HEAD_HERMIT",
                    "WEAPON_HERMIT_SWORDS", "NoRanged", "NoMagic");
            case "butcher":
                return new LanEquipmentPreset(name, "boss_butcher", "SkeletonHeavy", "BODY_BUTCHER", "HEAD_BUTCHER",
                    "WEAPON_BUTCHER_KNIVES", "NoRanged", "NoMagic");
            case "wasp":
                return new LanEquipmentPreset(name, "boss_wasp", "Skeleton", "ARMOR_WASP", "HEAD_WASP",
                    "WEAPON_WASP_NAGINATA", "NoRanged", "NoMagic");
            case "widow":
                return new LanEquipmentPreset(name, "boss_huntress", "Skeleton", "ARMOR_HUNTRESS", "HEAD_HUNTRESS",
                    "WEAPON_WIDOW_FANS", "NoRanged", "NoMagic");
            case "shogun":
                return new LanEquipmentPreset(name, "boss_shogun", "Skeleton", "ARMOR_SHOGUN", "HELM_CRESCENT",
                    "WEAPON_SHOGUN_KATANA", "RANGED_CHAKRAM_OF_MASTER", "MAGIC_ASTEROID");
            case "titan":
                return new LanEquipmentPreset(name, "avatar_human", "Skeleton", "BODY_TITAN", "HEAD_TITAN",
                    "WEAPON_TITAN_GIANT_SWORD", "RANGED_TITANS_HARPOON", "MAGIC_MIND_THROW");
            default:
                throw new IllegalArgumentException("preset LAN desconhecido: " + name);
        }
    }
}
