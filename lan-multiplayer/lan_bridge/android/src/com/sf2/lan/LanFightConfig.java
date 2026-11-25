package com.sf2.lan;

import java.nio.ByteBuffer;
import java.nio.ByteOrder;
import java.nio.charset.StandardCharsets;
import java.util.Locale;

/** Identidade da luta escolhida pelo jogo antes de a ponte LAN alterar o FightConfig. */
final class LanFightConfig {
    static final int VERSION = 1;
    static final int HEADER_SIZE = 12;
    static final int MAX_IDENTITY_BYTES = 512;

    final int mode;
    final long fingerprint;
    final String identity;

    LanFightConfig(int mode, long fingerprint, String identity) {
        if (mode < 0 || mode > 0xff || fingerprint == 0 || identity == null) {
            throw new IllegalArgumentException("Configuracao de luta invalida.");
        }
        byte[] encoded = identity.getBytes(StandardCharsets.UTF_8);
        if (encoded.length == 0 || encoded.length > MAX_IDENTITY_BYTES) {
            throw new IllegalArgumentException("Identidade de luta invalida.");
        }
        this.mode = mode;
        this.fingerprint = fingerprint;
        this.identity = identity;
    }

    byte[] encode() {
        byte[] encoded = identity.getBytes(StandardCharsets.UTF_8);
        return ByteBuffer.allocate(HEADER_SIZE + encoded.length).order(ByteOrder.BIG_ENDIAN)
            .put((byte) VERSION)
            .put((byte) mode)
            .putLong(fingerprint)
            .putShort((short) encoded.length)
            .put(encoded)
            .array();
    }

    static LanFightConfig decode(byte[] payload) {
        if (payload == null || payload.length < HEADER_SIZE) {
            throw new IllegalArgumentException("Configuracao de luta incompleta.");
        }
        ByteBuffer input = ByteBuffer.wrap(payload).order(ByteOrder.BIG_ENDIAN);
        int version = input.get() & 0xff;
        int mode = input.get() & 0xff;
        long fingerprint = input.getLong();
        int identitySize = input.getShort() & 0xffff;
        if (version != VERSION || fingerprint == 0 || identitySize == 0
            || identitySize > MAX_IDENTITY_BYTES || input.remaining() != identitySize) {
            throw new IllegalArgumentException("Configuracao de luta fora do protocolo.");
        }
        byte[] identity = new byte[identitySize];
        input.get(identity);
        return new LanFightConfig(mode, fingerprint, new String(identity, StandardCharsets.UTF_8));
    }

    boolean sameFight(LanFightConfig other) {
        return other != null && mode == other.mode && fingerprint == other.fingerprint
            && identity.equals(other.identity);
    }

    String toJson() {
        return String.format(
            Locale.ROOT,
            "{\"mode\":%d,\"fingerprint\":\"%016x\",\"identity\":\"%s\"}",
            mode, fingerprint, escape(identity)
        );
    }

    private static String escape(String value) {
        return value.replace("\\", "\\\\").replace("\"", "\\\"").replace("\n", "\\n");
    }
}
