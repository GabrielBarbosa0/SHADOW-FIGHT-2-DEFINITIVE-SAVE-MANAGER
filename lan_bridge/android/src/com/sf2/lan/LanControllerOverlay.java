package com.sf2.lan;

import android.app.Activity;
import android.graphics.Canvas;
import android.graphics.Color;
import android.graphics.Paint;
import android.graphics.Typeface;
import android.view.MotionEvent;
import android.view.View;
import android.view.ViewGroup;

import java.lang.reflect.Field;
import java.util.concurrent.atomic.AtomicBoolean;

final class LanControllerOverlay extends View {
    private static final AtomicBoolean STARTED = new AtomicBoolean(false);
    private static final int PUNCH = 1;
    private static final int KICK = 2;
    private static final int RANGED = 4;
    private static final int MAGIC = 8;
    private static final int BOSS = 16;
    private static final int PAUSE = 32;

    private final Paint fill = new Paint(Paint.ANTI_ALIAS_FLAG);
    private final Paint text = new Paint(Paint.ANTI_ALIAS_FLAG);
    private int stickPointer = -1;
    private float stickX;
    private float stickY;
    private float horizontal;
    private float vertical;
    private int buttons;

    private LanControllerOverlay(Activity activity) {
        super(activity);
        setBackgroundColor(Color.TRANSPARENT);
        text.setColor(Color.WHITE);
        text.setTextAlign(Paint.Align.CENTER);
        text.setTypeface(Typeface.DEFAULT_BOLD);
        setFocusable(true);
    }

    static void start() {
        if (!STARTED.compareAndSet(false, true)) {
            return;
        }
        Thread thread = new Thread(() -> {
            for (int attempt = 0; attempt < 120; attempt++) {
                try {
                    Activity activity = currentActivity();
                    if (activity != null) {
                        activity.runOnUiThread(() -> {
                            LanControllerOverlay overlay = new LanControllerOverlay(activity);
                            activity.addContentView(overlay, new ViewGroup.LayoutParams(
                                ViewGroup.LayoutParams.MATCH_PARENT,
                                ViewGroup.LayoutParams.MATCH_PARENT));
                        });
                        return;
                    }
                    Thread.sleep(250);
                } catch (Exception ignored) {
                    // A Activity do Unity pode ainda nao existir durante o bootstrap.
                }
            }
            STARTED.set(false);
        }, "sf2-lan-controller-ui");
        thread.setDaemon(true);
        thread.start();
    }

    private static Activity currentActivity() throws Exception {
        Class<?> unityPlayer = Class.forName("com.unity3d.player.UnityPlayer");
        Field field = unityPlayer.getDeclaredField("currentActivity");
        field.setAccessible(true);
        return (Activity) field.get(null);
    }

    @Override
    protected void onDraw(Canvas canvas) {
        super.onDraw(canvas);
        float unit = Math.min(getWidth(), getHeight()) / 9.0f;
        float baseX = stickPointer >= 0 ? stickX : unit * 1.6f;
        float baseY = stickPointer >= 0 ? stickY : getHeight() - unit * 1.7f;

        fill.setColor(Color.argb(75, 255, 255, 255));
        canvas.drawCircle(baseX, baseY, unit, fill);
        fill.setColor(Color.argb(150, 80, 190, 255));
        canvas.drawCircle(baseX + horizontal * unit, baseY - vertical * unit, unit * 0.42f, fill);

        drawButton(canvas, "SOCO", 0.84f, 0.62f, unit * 0.58f, PUNCH);
        drawButton(canvas, "CHUTE", 0.72f, 0.77f, unit * 0.58f, KICK);
        drawButton(canvas, "DIST.", 0.90f, 0.82f, unit * 0.48f, RANGED);
        drawButton(canvas, "MAGIA", 0.61f, 0.61f, unit * 0.48f, MAGIC);
        drawButton(canvas, "CHEFE", 0.52f, 0.82f, unit * 0.42f, BOSS);
        drawButton(canvas, "PAUSA", 0.94f, 0.12f, unit * 0.35f, PAUSE);

        text.setTextSize(unit * 0.25f);
        canvas.drawText("CONTROLE LAN", getWidth() / 2.0f, unit * 0.45f, text);
    }

    private void drawButton(Canvas canvas, String label, float xRatio, float yRatio, float radius, int bit) {
        float x = getWidth() * xRatio;
        float y = getHeight() * yRatio;
        boolean pressed = (buttons & bit) != 0;
        fill.setColor(pressed ? Color.argb(210, 255, 120, 45) : Color.argb(105, 255, 255, 255));
        canvas.drawCircle(x, y, radius, fill);
        text.setTextSize(radius * 0.34f);
        canvas.drawText(label, x, y + radius * 0.12f, text);
    }

    @Override
    public boolean onTouchEvent(MotionEvent event) {
        int action = event.getActionMasked();
        int actionIndex = event.getActionIndex();
        if (action == MotionEvent.ACTION_DOWN || action == MotionEvent.ACTION_POINTER_DOWN) {
            if (event.getX(actionIndex) < getWidth() * 0.48f && stickPointer < 0) {
                stickPointer = event.getPointerId(actionIndex);
                stickX = event.getX(actionIndex);
                stickY = event.getY(actionIndex);
            }
        } else if (action == MotionEvent.ACTION_UP || action == MotionEvent.ACTION_CANCEL) {
            stickPointer = -1;
        } else if (action == MotionEvent.ACTION_POINTER_UP
            && event.getPointerId(actionIndex) == stickPointer) {
            stickPointer = -1;
        }

        int ignoredIndex = action == MotionEvent.ACTION_UP || action == MotionEvent.ACTION_CANCEL
            ? -2 : (action == MotionEvent.ACTION_POINTER_UP ? actionIndex : -1);
        recompute(event, ignoredIndex);
        LanTransport.updateLocalInput(horizontal, vertical, buttons);
        invalidate();
        return true;
    }

    private void recompute(MotionEvent event, int ignoredIndex) {
        float unit = Math.min(getWidth(), getHeight()) / 9.0f;
        horizontal = 0.0f;
        vertical = 0.0f;
        buttons = 0;
        if (ignoredIndex == -2) {
            return;
        }
        for (int index = 0; index < event.getPointerCount(); index++) {
            if (index == ignoredIndex) {
                continue;
            }
            int pointerId = event.getPointerId(index);
            float x = event.getX(index);
            float y = event.getY(index);
            if (pointerId == stickPointer) {
                horizontal = clamp((x - stickX) / unit);
                vertical = clamp((stickY - y) / unit);
            } else {
                buttons |= buttonAt(x, y, unit);
            }
        }
    }

    private int buttonAt(float x, float y, float unit) {
        if (inside(x, y, 0.84f, 0.62f, unit * 0.70f)) return PUNCH;
        if (inside(x, y, 0.72f, 0.77f, unit * 0.70f)) return KICK;
        if (inside(x, y, 0.90f, 0.82f, unit * 0.60f)) return RANGED;
        if (inside(x, y, 0.61f, 0.61f, unit * 0.60f)) return MAGIC;
        if (inside(x, y, 0.52f, 0.82f, unit * 0.54f)) return BOSS;
        if (inside(x, y, 0.94f, 0.12f, unit * 0.47f)) return PAUSE;
        return 0;
    }

    private boolean inside(float x, float y, float xRatio, float yRatio, float radius) {
        float dx = x - getWidth() * xRatio;
        float dy = y - getHeight() * yRatio;
        return dx * dx + dy * dy <= radius * radius;
    }

    private static float clamp(float value) {
        return Math.max(-1.0f, Math.min(1.0f, value));
    }
}
