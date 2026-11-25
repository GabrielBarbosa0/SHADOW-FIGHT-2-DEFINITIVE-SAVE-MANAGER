package com.sf2.lan;

/** Snapshot imutavel da entrada remota mais recente, pronto para a ponte JNI/IL2CPP. */
public final class LanInputState {
    public static final LanInputState NEUTRAL = new LanInputState(0.0f, 0.0f, 0, -1L, 0L);

    public final float horizontal;
    public final float vertical;
    public final int buttons;
    public final long sequence;
    public final long receivedAtMs;

    LanInputState(float horizontal, float vertical, int buttons, long sequence, long receivedAtMs) {
        this.horizontal = horizontal;
        this.vertical = vertical;
        this.buttons = buttons;
        this.sequence = sequence;
        this.receivedAtMs = receivedAtMs;
    }
}
