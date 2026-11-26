package com.sf2.lan;

import java.util.Locale;

/** Divergencia entre peers na composicao canonica P1-esquerda/P2-direita. */
final class LanFightStateComparison {
    private static final float FRAME_TOLERANCE = 12.0f;
    private static final float LIFE_TOLERANCE = 0.01f;
    private static final long FRESHNESS_TOLERANCE_MS = 500L;

    static final LanFightStateComparison UNAVAILABLE = new LanFightStateComparison(
        0, 0, 0, 0, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, false
    );

    final long roundId;
    final long localTick;
    final long remoteTick;
    final long remoteAgeMs;
    final float timerFramesDelta;
    final float elapsedFramesDelta;
    final float localFighterLifeDelta;
    final float localFighterHitDelta;
    final float remoteFighterLifeDelta;
    final float remoteFighterHitDelta;
    final float maxLifeDelta;
    final boolean withinTolerance;

    private LanFightStateComparison(
        long roundId,
        long localTick,
        long remoteTick,
        long remoteAgeMs,
        float timerFramesDelta,
        float elapsedFramesDelta,
        float localFighterLifeDelta,
        float localFighterHitDelta,
        float remoteFighterLifeDelta,
        float remoteFighterHitDelta,
        float maxLifeDelta,
        boolean withinTolerance
    ) {
        this.roundId = roundId;
        this.localTick = localTick;
        this.remoteTick = remoteTick;
        this.remoteAgeMs = remoteAgeMs;
        this.timerFramesDelta = timerFramesDelta;
        this.elapsedFramesDelta = elapsedFramesDelta;
        this.localFighterLifeDelta = localFighterLifeDelta;
        this.localFighterHitDelta = localFighterHitDelta;
        this.remoteFighterLifeDelta = remoteFighterLifeDelta;
        this.remoteFighterHitDelta = remoteFighterHitDelta;
        this.maxLifeDelta = maxLifeDelta;
        this.withinTolerance = withinTolerance;
    }

    static LanFightStateComparison compare(LanFightState local, LanFightState remote, long nowMs) {
        if (!local.isAvailable() || !remote.isAvailable() || local.roundId != remote.roundId) {
            return UNAVAILABLE;
        }
        long remoteAgeMs = Math.max(0L, nowMs - remote.capturedAtMs);
        float timerFramesDelta = local.values[1] - remote.values[1];
        float elapsedFramesDelta = local.values[6] - remote.values[6];
        float localFighterLifeDelta = local.values[2] - remote.values[2];
        float localFighterHitDelta = local.values[3] - remote.values[3];
        float remoteFighterLifeDelta = local.values[4] - remote.values[4];
        float remoteFighterHitDelta = local.values[5] - remote.values[5];
        float maxLifeDelta = Math.max(
            Math.max(Math.abs(localFighterLifeDelta), Math.abs(localFighterHitDelta)),
            Math.max(Math.abs(remoteFighterLifeDelta), Math.abs(remoteFighterHitDelta))
        );
        boolean withinTolerance = remoteAgeMs <= FRESHNESS_TOLERANCE_MS
            && Math.abs(timerFramesDelta) <= FRAME_TOLERANCE
            && Math.abs(elapsedFramesDelta) <= FRAME_TOLERANCE
            && maxLifeDelta <= LIFE_TOLERANCE;
        return new LanFightStateComparison(
            local.roundId, local.tick, remote.tick, remoteAgeMs,
            timerFramesDelta, elapsedFramesDelta,
            localFighterLifeDelta, localFighterHitDelta,
            remoteFighterLifeDelta, remoteFighterHitDelta,
            maxLifeDelta, withinTolerance
        );
    }

    boolean isAvailable() {
        return roundId > 0;
    }

    String toJson() {
        return String.format(
            Locale.ROOT,
            "{\"roundId\":%d,\"localTick\":%d,\"remoteTick\":%d,\"remoteAgeMs\":%d,"
                + "\"timerFramesDelta\":%.3f,\"elapsedFramesDelta\":%.3f,"
                + "\"localFighter\":{\"lifeDelta\":%.6f,\"hitDelta\":%.6f},"
                + "\"remoteFighter\":{\"lifeDelta\":%.6f,\"hitDelta\":%.6f},"
                + "\"maxLifeDelta\":%.6f,\"withinTolerance\":%s}",
            roundId, localTick, remoteTick, remoteAgeMs,
            timerFramesDelta, elapsedFramesDelta,
            localFighterLifeDelta, localFighterHitDelta,
            remoteFighterLifeDelta, remoteFighterHitDelta,
            maxLifeDelta, withinTolerance
        );
    }
}
