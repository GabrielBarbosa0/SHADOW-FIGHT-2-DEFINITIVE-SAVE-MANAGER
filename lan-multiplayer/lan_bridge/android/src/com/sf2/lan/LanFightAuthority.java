package com.sf2.lan;

import java.util.Locale;

/** Vida e resultado decididos pela simulacao do jogador 1. */
final class LanFightAuthority {
    static final LanFightAuthority EMPTY = new LanFightAuthority(
        0, 0, 0.0f, 0.0f, 0, 0,
        0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0
    );

    final long roundId;
    final long tick;
    final float playerDamage;
    final float opponentDamage;
    final int playerRoundsLost;
    final int opponentRoundsLost;
    final float playerX;
    final float playerY;
    final float playerZ;
    final float opponentX;
    final float opponentY;
    final float opponentZ;
    final long capturedAtMs;

    LanFightAuthority(
        long roundId,
        long tick,
        float playerDamage,
        float opponentDamage,
        int playerRoundsLost,
        int opponentRoundsLost,
        float playerX,
        float playerY,
        float playerZ,
        float opponentX,
        float opponentY,
        float opponentZ
    ) {
        this(
            roundId, tick, playerDamage, opponentDamage,
            playerRoundsLost, opponentRoundsLost,
            playerX, playerY, playerZ, opponentX, opponentY, opponentZ,
            System.currentTimeMillis()
        );
    }

    LanFightAuthority(
        long roundId,
        long tick,
        float playerDamage,
        float opponentDamage,
        int playerRoundsLost,
        int opponentRoundsLost,
        float playerX,
        float playerY,
        float playerZ,
        float opponentX,
        float opponentY,
        float opponentZ,
        long capturedAtMs
    ) {
        this.roundId = roundId;
        this.tick = tick;
        this.playerDamage = playerDamage;
        this.opponentDamage = opponentDamage;
        this.playerRoundsLost = playerRoundsLost;
        this.opponentRoundsLost = opponentRoundsLost;
        this.playerX = playerX;
        this.playerY = playerY;
        this.playerZ = playerZ;
        this.opponentX = opponentX;
        this.opponentY = opponentY;
        this.opponentZ = opponentZ;
        this.capturedAtMs = capturedAtMs;
    }

    boolean isAvailable() {
        return roundId > 0 && tick > 0;
    }

    String toJson() {
        return String.format(
            Locale.ROOT,
            "{\"roundId\":%d,\"tick\":%d,\"capturedAt\":%d,"
                + "\"player\":{\"damage\":%.6f,\"roundsLost\":%d,"
                + "\"position\":[%.6f,%.6f,%.6f]},"
                + "\"opponent\":{\"damage\":%.6f,\"roundsLost\":%d,"
                + "\"position\":[%.6f,%.6f,%.6f]}}",
            roundId, tick, capturedAtMs,
            playerDamage, playerRoundsLost, playerX, playerY, playerZ,
            opponentDamage, opponentRoundsLost, opponentX, opponentY, opponentZ
        );
    }
}
