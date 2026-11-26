package com.sf2.lan;

import java.io.ByteArrayOutputStream;
import java.io.DataOutputStream;
import java.nio.ByteBuffer;
import java.nio.ByteOrder;
import java.nio.charset.StandardCharsets;
import java.security.MessageDigest;
import java.util.Arrays;
import java.util.Locale;

/** Tier e presets escolhidos uma unica vez pela autoridade da sala. */
final class LanMatchConfig {
    private static final int VERSION = 1;
    private static final int HEADER_SIZE = 12;
    private static final int MAX_PRESET_BYTES = 32;

    final int matchLevel;
    final long seed;
    final String playerOnePreset;
    final String playerTwoPreset;

    LanMatchConfig(int matchLevel, long seed, String playerOnePreset, String playerTwoPreset) {
        if (matchLevel < 1 || matchLevel > 52 || seed == 0) {
            throw new IllegalArgumentException("configuracao de partida fora do intervalo");
        }
        LanEquipmentPreset.get(playerOnePreset);
        LanEquipmentPreset.get(playerTwoPreset);
        this.matchLevel = matchLevel;
        this.seed = seed;
        this.playerOnePreset = playerOnePreset;
        this.playerTwoPreset = playerTwoPreset;
    }

    byte[] encode() throws Exception {
        byte[] playerOne = encodePreset(playerOnePreset);
        byte[] playerTwo = encodePreset(playerTwoPreset);
        ByteArrayOutputStream bytes = new ByteArrayOutputStream(HEADER_SIZE + playerOne.length + playerTwo.length);
        try (DataOutputStream output = new DataOutputStream(bytes)) {
            output.writeByte(VERSION);
            output.writeByte(matchLevel);
            output.writeLong(seed);
            output.writeByte(playerOne.length);
            output.writeByte(playerTwo.length);
            output.write(playerOne);
            output.write(playerTwo);
        }
        return bytes.toByteArray();
    }

    static LanMatchConfig decode(byte[] payload) {
        ByteBuffer input = ByteBuffer.wrap(payload).order(ByteOrder.BIG_ENDIAN);
        if (input.remaining() < HEADER_SIZE || (input.get() & 0xff) != VERSION) {
            throw new IllegalArgumentException("versao de configuracao de partida invalida");
        }
        int level = input.get() & 0xff;
        long seed = input.getLong();
        int playerOneSize = input.get() & 0xff;
        int playerTwoSize = input.get() & 0xff;
        if (playerOneSize < 1 || playerTwoSize < 1
            || playerOneSize > MAX_PRESET_BYTES || playerTwoSize > MAX_PRESET_BYTES
            || input.remaining() != playerOneSize + playerTwoSize) {
            throw new IllegalArgumentException("tamanho dos presets da partida invalido");
        }
        String playerOne = readPreset(input, playerOneSize);
        String playerTwo = readPreset(input, playerTwoSize);
        return new LanMatchConfig(level, seed, playerOne, playerTwo);
    }

    long fingerprint() {
        try {
            byte[] digest = MessageDigest.getInstance("SHA-256").digest(encode());
            long value = ByteBuffer.wrap(Arrays.copyOf(digest, 8)).order(ByteOrder.BIG_ENDIAN).getLong();
            return value == 0 ? 1 : value;
        } catch (Exception error) {
            throw new IllegalStateException("SHA-256 indisponivel", error);
        }
    }

    String presetForRole(int role) {
        return role == 1 ? playerOnePreset : playerTwoPreset;
    }

    String remotePresetForRole(int role) {
        return role == 1 ? playerTwoPreset : playerOnePreset;
    }

    String toJson() {
        return String.format(Locale.ROOT,
            "{\"level\":%d,\"seed\":\"%s\",\"fingerprint\":\"%016x\","
                + "\"playerOnePreset\":\"%s\",\"playerTwoPreset\":\"%s\"}",
            matchLevel, Long.toUnsignedString(seed), fingerprint(), playerOnePreset, playerTwoPreset);
    }

    private static byte[] encodePreset(String value) {
        byte[] encoded = value.getBytes(StandardCharsets.UTF_8);
        if (encoded.length < 1 || encoded.length > MAX_PRESET_BYTES) {
            throw new IllegalArgumentException("nome de preset invalido");
        }
        return encoded;
    }

    private static String readPreset(ByteBuffer input, int size) {
        byte[] encoded = new byte[size];
        input.get(encoded);
        return new String(encoded, StandardCharsets.UTF_8);
    }
}
