package com.sf2.lan;

import android.util.Log;

import java.util.concurrent.atomic.AtomicBoolean;

/** Liga o snapshot Java aos leitores de GamePad 1 existentes no IL2CPP. */
public final class LanNativeBridge {
    private static final String TAG = "SF2LAN";
    private static final AtomicBoolean STARTED = new AtomicBoolean(false);
    private static volatile boolean installed;
    private static volatile boolean sessionReady;
    private static volatile int playerRole;
    private static volatile LanFighterProfile remoteProfile;

    private LanNativeBridge() {
    }

    public static void start() {
        if (!STARTED.compareAndSet(false, true)) {
            return;
        }
        try {
            System.loadLibrary("sf2lanhook");
        } catch (Throwable error) {
            Log.e(TAG, "Nao foi possivel carregar o hook nativo LAN.", error);
            return;
        }
        Thread installer = new Thread(() -> {
            for (int attempt = 0; attempt < 120 && !installed; attempt++) {
                try {
                    installed = nativeInstall();
                } catch (Throwable error) {
                    Log.e(TAG, "Falha ao instalar o hook nativo LAN.", error);
                    return;
                }
                if (!installed) {
                    try {
                        Thread.sleep(250L);
                    } catch (InterruptedException ignored) {
                        Thread.currentThread().interrupt();
                        return;
                    }
                }
            }
            if (installed) {
                pushRemoteProfile();
                nativeSetSessionReady(sessionReady);
                nativeSetPlayerRole(playerRole);
                update(LanTransport.getRemoteInput());
            }
            Log.i(TAG, installed ? "Hook do GamePad 1 instalado." : "libil2cpp nao ficou disponivel para o hook.");
        }, "sf2-lan-hook");
        installer.setDaemon(true);
        installer.start();
    }

    public static boolean isInstalled() {
        return installed;
    }

    public static void setSessionReady(boolean ready) {
        sessionReady = ready;
        if (installed) {
            nativeSetSessionReady(ready);
        }
    }

    public static void setPlayerRole(int role) {
        playerRole = role;
        if (installed) {
            nativeSetPlayerRole(role);
        }
    }

    public static void setRemoteProfile(LanFighterProfile profile) {
        remoteProfile = profile;
        if (installed) {
            pushRemoteProfile();
        }
    }

    private static void pushRemoteProfile() {
        LanFighterProfile profile = remoteProfile;
        if (profile == null) {
            nativeClearRemoteProfile();
            return;
        }
        nativeSetRemoteProfile(
            profile.level,
            profile.armor,
            profile.helm,
            profile.weapon,
            profile.ranged,
            profile.magic
        );
    }

    public static void update(LanInputState state) {
        if (installed) {
            nativeUpdateInput(state.horizontal, state.vertical, state.buttons, state.sequence, state.receivedAtMs);
        }
    }

    public static int getFightGeneration() {
        return installed ? nativeGetFightGeneration() : 0;
    }

    public static LanFightConfig readFightConfig() {
        if (!installed) {
            return null;
        }
        int mode = nativeGetFightConfigMode();
        long fingerprint = nativeGetFightConfigFingerprint();
        String identity = nativeGetFightConfigIdentity();
        if (mode < 0 || mode > 0xff || fingerprint == 0 || identity == null || identity.isEmpty()) {
            return null;
        }
        try {
            return new LanFightConfig(mode, fingerprint, identity);
        } catch (IllegalArgumentException error) {
            Log.w(TAG, "Descritor nativo da luta rejeitado.", error);
            return null;
        }
    }

    public static void scheduleFightStart(long startAtMs) {
        if (installed) {
            nativeScheduleFightStart(startAtMs);
        }
    }

    public static LanInputState readLocalInput() {
        if (!installed) {
            return LanInputState.NEUTRAL;
        }
        float[] values = nativeReadLocalInput();
        if (values == null || values.length != 3
            || !Float.isFinite(values[0]) || !Float.isFinite(values[1])) {
            return LanInputState.NEUTRAL;
        }
        return new LanInputState(values[0], values[1], (int) values[2], -1, System.currentTimeMillis());
    }

    public static LanFightState readLocalFightState(long roundId) {
        if (!installed || roundId <= 0) {
            return LanFightState.EMPTY;
        }
        float[] raw = nativeReadLocalFightState();
        if (raw == null || raw.length != 9 || !Float.isFinite(raw[0]) || raw[0] <= 0) {
            return LanFightState.EMPTY;
        }
        float[] values = new float[8];
        for (int index = 0; index < values.length; index++) {
            float value = raw[index + 1];
            if (!Float.isFinite(value) || Math.abs(value) > 1_000_000.0f) {
                return LanFightState.EMPTY;
            }
            values[index] = value;
        }
        return new LanFightState(roundId, (long) raw[0], values);
    }

    public static void updateRemoteFightClock(float framesPassed, long receivedAtMs) {
        if (installed) {
            nativeUpdateRemoteFightClock(framesPassed, receivedAtMs);
        }
    }

    public static LanFightAuthority readLocalFightAuthority(long roundId) {
        if (!installed || roundId <= 0) {
            return LanFightAuthority.EMPTY;
        }
        float[] raw = nativeReadLocalFightAuthority();
        if (raw == null || raw.length != 11 || !Float.isFinite(raw[0]) || raw[0] <= 0
            || !Float.isFinite(raw[1]) || !Float.isFinite(raw[2])) {
            return LanFightAuthority.EMPTY;
        }
        int playerRoundsLost = (int) raw[3];
        int opponentRoundsLost = (int) raw[4];
        for (int index = 5; index < raw.length; index++) {
            if (!Float.isFinite(raw[index]) || Math.abs(raw[index]) > 1_000_000.0f) {
                return LanFightAuthority.EMPTY;
            }
        }
        if (raw[1] < 0.0f || raw[1] > 1.0f || raw[2] < 0.0f || raw[2] > 1.0f
            || playerRoundsLost < 0 || playerRoundsLost > 2
            || opponentRoundsLost < 0 || opponentRoundsLost > 2) {
            return LanFightAuthority.EMPTY;
        }
        return new LanFightAuthority(
            roundId, (long) raw[0], raw[1], raw[2],
            playerRoundsLost, opponentRoundsLost,
            raw[5], raw[6], raw[7], raw[8], raw[9], raw[10]
        );
    }

    public static void updateAuthoritativeFightState(LanFightAuthority authority) {
        if (installed && authority != null && authority.isAvailable()) {
            nativeUpdateAuthoritativeFightState(
                authority.playerDamage,
                authority.opponentDamage,
                authority.playerRoundsLost,
                authority.opponentRoundsLost,
                authority.playerX,
                authority.playerY,
                authority.playerZ,
                authority.opponentX,
                authority.opponentY,
                authority.opponentZ,
                authority.capturedAtMs
            );
        }
    }

    public static void clearAuthoritativeFightState() {
        if (installed) {
            nativeUpdateAuthoritativeFightState(
                0.0f, 0.0f, 0, 0,
                0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f,
                0
            );
        }
    }

    private static native boolean nativeInstall();

    private static native void nativeSetSessionReady(boolean ready);

    private static native void nativeSetPlayerRole(int role);

    private static native void nativeSetRemoteProfile(
        int level,
        String armor,
        String helm,
        String weapon,
        String ranged,
        String magic
    );

    private static native void nativeClearRemoteProfile();

    private static native void nativeUpdateInput(
        float horizontal,
        float vertical,
        int buttons,
        long sequence,
        long receivedAtMs
    );

    private static native float[] nativeReadLocalInput();

    private static native float[] nativeReadLocalFightState();

    private static native float[] nativeReadLocalFightAuthority();

    private static native void nativeUpdateRemoteFightClock(float framesPassed, long receivedAtMs);

    private static native void nativeUpdateAuthoritativeFightState(
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
        long receivedAtMs
    );

    private static native int nativeGetFightGeneration();

    private static native int nativeGetFightConfigMode();

    private static native long nativeGetFightConfigFingerprint();

    private static native String nativeGetFightConfigIdentity();

    private static native void nativeScheduleFightStart(long startAtMs);
}
