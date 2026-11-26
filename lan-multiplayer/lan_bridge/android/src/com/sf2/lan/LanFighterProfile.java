package com.sf2.lan;

import android.content.Context;
import android.util.Xml;

import org.xmlpull.v1.XmlPullParser;

import java.io.ByteArrayOutputStream;
import java.io.DataOutputStream;
import java.io.File;
import java.io.FileInputStream;
import java.nio.ByteBuffer;
import java.nio.ByteOrder;
import java.nio.charset.StandardCharsets;

final class LanFighterProfile {
    private static final int VERSION = 1;
    private static final int MAX_FIELD_BYTES = 96;

    final int level;
    final String name;
    final String avatar;
    final String voice;
    final String skeleton;
    final String armor;
    final String helm;
    final String weapon;
    final String ranged;
    final String magic;

    LanFighterProfile(int level, String name, String avatar, String voice, String skeleton,
        String armor, String helm, String weapon, String ranged, String magic) {
        this.level = level;
        this.name = name;
        this.avatar = avatar;
        this.voice = voice;
        this.skeleton = skeleton;
        this.armor = armor;
        this.helm = helm;
        this.weapon = weapon;
        this.ranged = ranged;
        this.magic = magic;
    }

    static LanFighterProfile read(Context context) throws Exception {
        File save = new File(context.getExternalFilesDir(null), "userdata/users.xml");
        XmlPullParser parser = Xml.newPullParser();
        try (FileInputStream input = new FileInputStream(save)) {
            parser.setInput(input, "UTF-8");
            while (parser.getEventType() != XmlPullParser.END_DOCUMENT) {
                if (parser.getEventType() == XmlPullParser.START_TAG
                    && "Warrior".equals(parser.getName())) {
                    return new LanFighterProfile(
                        integerAttribute(parser, "Level"),
                        attribute(parser, "FirstName"),
                        attribute(parser, "Avatar"),
                        attribute(parser, "Voice"),
                        attribute(parser, "Skeleton"),
                        attribute(parser, "Armor"),
                        attribute(parser, "Helm"),
                        attribute(parser, "Weapon"),
                        attribute(parser, "Ranged"),
                        attribute(parser, "Magic")
                    );
                }
                parser.next();
            }
        }
        throw new IllegalArgumentException("Warrior ausente em users.xml");
    }

    byte[] encode() throws Exception {
        if (level < 1 || level > 255) {
            throw new IllegalArgumentException("nivel fora do protocolo");
        }
        ByteArrayOutputStream bytes = new ByteArrayOutputStream();
        try (DataOutputStream output = new DataOutputStream(bytes)) {
            output.writeByte(VERSION);
            output.writeByte(level);
            writeString(output, name);
            writeString(output, avatar);
            writeString(output, voice);
            writeString(output, skeleton);
            writeString(output, armor);
            writeString(output, helm);
            writeString(output, weapon);
            writeString(output, ranged);
            writeString(output, magic);
        }
        return bytes.toByteArray();
    }

    static LanFighterProfile decode(byte[] payload) {
        ByteBuffer input = ByteBuffer.wrap(payload).order(ByteOrder.BIG_ENDIAN);
        if (input.remaining() < 2 || (input.get() & 0xff) != VERSION) {
            throw new IllegalArgumentException("versao de perfil invalida");
        }
        int level = input.get() & 0xff;
        LanFighterProfile profile = new LanFighterProfile(level,
            readString(input), readString(input), readString(input), readString(input),
            readString(input), readString(input), readString(input), readString(input), readString(input));
        if (level < 1 || input.hasRemaining()) {
            throw new IllegalArgumentException("perfil de lutador invalido");
        }
        return profile;
    }

    LanFighterProfile withPreset(LanEquipmentPreset preset, int matchLevel) {
        return new LanFighterProfile(
            matchLevel,
            name,
            preset.avatar,
            voice,
            preset.skeleton,
            preset.armor,
            preset.helm,
            preset.weapon,
            preset.ranged,
            preset.magic
        );
    }

    String toJson() {
        return String.format(java.util.Locale.ROOT,
            "{\"level\":%d,\"name\":\"%s\",\"avatar\":\"%s\",\"voice\":\"%s\","
                + "\"skeleton\":\"%s\",\"armor\":\"%s\",\"helm\":\"%s\","
                + "\"weapon\":\"%s\",\"ranged\":\"%s\",\"magic\":\"%s\"}",
            level, escape(name), escape(avatar), escape(voice), escape(skeleton), escape(armor),
            escape(helm), escape(weapon), escape(ranged), escape(magic));
    }

    private static String attribute(XmlPullParser parser, String name) {
        String value = parser.getAttributeValue(null, name);
        return value == null ? "" : value;
    }

    private static int integerAttribute(XmlPullParser parser, String name) {
        return Integer.parseInt(attribute(parser, name));
    }

    private static void writeString(DataOutputStream output, String value) throws Exception {
        byte[] encoded = value.getBytes(StandardCharsets.UTF_8);
        if (encoded.length > MAX_FIELD_BYTES) {
            throw new IllegalArgumentException("campo de perfil excede 96 bytes");
        }
        output.writeByte(encoded.length);
        output.write(encoded);
    }

    private static String readString(ByteBuffer input) {
        if (!input.hasRemaining()) {
            throw new IllegalArgumentException("perfil truncado");
        }
        int size = input.get() & 0xff;
        if (size > MAX_FIELD_BYTES || input.remaining() < size) {
            throw new IllegalArgumentException("campo de perfil invalido");
        }
        byte[] encoded = new byte[size];
        input.get(encoded);
        return new String(encoded, StandardCharsets.UTF_8);
    }

    private static String escape(String value) {
        return value.replace("\\", "\\\\").replace("\"", "\\\"").replace("\n", "\\n");
    }
}
