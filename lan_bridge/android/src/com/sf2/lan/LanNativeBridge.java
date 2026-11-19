package com.sf2.lan;

import android.util.Log;

import java.util.concurrent.atomic.AtomicBoolean;

/** Liga o snapshot Java aos leitores de GamePad 1 existentes no IL2CPP. */
public final class LanNativeBridge {
    private static final String TAG = "SF2LAN";
    private static final AtomicBoolean STARTED = new AtomicBoolean(false);
    private static volatile boolean installed;

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

    public static void update(LanInputState state) {
        if (installed) {
            nativeUpdateInput(state.horizontal, state.vertical, state.buttons, state.sequence, state.receivedAtMs);
        }
    }

    private static native boolean nativeInstall();

    private static native void nativeUpdateInput(
        float horizontal,
        float vertical,
        int buttons,
        long sequence,
        long receivedAtMs
    );
}
