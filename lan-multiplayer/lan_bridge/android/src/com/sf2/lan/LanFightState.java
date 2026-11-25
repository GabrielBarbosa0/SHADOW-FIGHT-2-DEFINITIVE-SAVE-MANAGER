package com.sf2.lan;

import java.util.Locale;

/** Snapshot read-only do HUD da luta, produzido no thread nativo da luta. */
final class LanFightState {
    static final LanFightState EMPTY = new LanFightState(0, 0, new float[8], 0);

    final long roundId;
    final long tick;
    final float[] values;
    final long capturedAtMs;

    LanFightState(long roundId, long tick, float[] values) {
        this(roundId, tick, values, System.currentTimeMillis());
    }

    LanFightState(long roundId, long tick, float[] values, long capturedAtMs) {
        if (values == null || values.length != 8) {
            throw new IllegalArgumentException("FightState exige oito valores float");
        }
        this.roundId = roundId;
        this.tick = tick;
        this.values = values.clone();
        this.capturedAtMs = capturedAtMs;
    }

    boolean isAvailable() {
        return roundId > 0 && tick > 0;
    }

    String toJson() {
        return String.format(
            Locale.ROOT,
            "{\"roundId\":%d,\"tick\":%d,\"capturedAt\":%d,"
                + "\"timer\":{\"secondsLeft\":%.0f,\"framesLeft\":%.0f,"
                + "\"framesPassed\":%.0f,\"framesTotal\":%.0f},"
                + "\"life\":{\"left\":%.6f,\"leftHit\":%.6f,"
                + "\"right\":%.6f,\"rightHit\":%.6f}}",
            roundId, tick, capturedAtMs, values[0], values[1], values[6], values[7],
            values[2], values[3], values[4], values[5]
        );
    }
}
