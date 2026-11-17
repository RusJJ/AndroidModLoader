package net.rusjj.aml;

import android.animation.Animator;
import android.animation.AnimatorListenerAdapter;
import android.animation.ValueAnimator;
import android.app.Activity;
import android.content.Context;
import android.graphics.PixelFormat;
import android.graphics.Typeface;
import android.graphics.drawable.GradientDrawable;
import android.os.IBinder;
import android.util.DisplayMetrics;
import android.util.TypedValue;
import android.util.Log;
import android.view.Gravity;
import android.view.View;
import android.view.ViewGroup;
import android.view.WindowManager;
import android.view.animation.AccelerateInterpolator;
import android.view.animation.DecelerateInterpolator;
import android.widget.FrameLayout;
import android.widget.TextView;
import java.util.HashMap;
import java.util.Map;

// Views over the game's window, in a window of their own: nothing the game adds can cover them.
// They take no touches, the game gets all of them
public class Overlay
{
    private static final int SLIDE_LEFT = 1, SLIDE_RIGHT = 2, SLIDE_UP = 3, SLIDE_DOWN = 4; // eOverlaySlide

    // Sizes are in 1/360 of the screen's short side, not dp/sp: the same part of any screen, like in games
    private static float unit(Context ctx)
    {
        DisplayMetrics m = ctx.getResources().getDisplayMetrics();
        return Math.min(m.widthPixels, m.heightPixels) / 360.0f;
    }

    // Puts every view by its { x, y } tag, translation stays free for the animations
    private static final class Layer extends FrameLayout
    {
        Layer(Context ctx)
        {
            super(ctx);
            int margin = (int)(12.0f * unit(ctx));
            setPadding(margin, margin, margin, margin);
        }
        @Override protected void onLayout(boolean changed, int l, int t, int r, int b)
        {
            int freeW = getWidth() - getPaddingLeft() - getPaddingRight(), freeH = getHeight() - getPaddingTop() - getPaddingBottom();
            for(int i = 0; i < getChildCount(); ++i)
            {
                View v = getChildAt(i);
                float[] pos = (float[])v.getTag();
                int x = getPaddingLeft() + (int)(pos[0] * (freeW - v.getMeasuredWidth()));
                int y = getPaddingTop() + (int)(pos[1] * (freeH - v.getMeasuredHeight()));
                v.layout(x, y, x + v.getMeasuredWidth(), y + v.getMeasuredHeight());
            }
        }
    }

    private static Layer layer;
    private static Activity owner;
    private static final HashMap<Integer, TextView> views = new HashMap<>();
    private static final HashMap<Integer, ValueAnimator> animations = new HashMap<>();
    private static final HashMap<Integer, Runnable> holds = new HashMap<>(); // Not an animator delay: "remove animations" scales those to 0
    private static final HashMap<Integer, String> pendingTexts = new HashMap<>(); // Only the latest text gets drawn
    private static boolean textsScheduled;

    static void attach(Activity a)
    {
        if(layer == null || a == null || a == owner) return;
        View decor = a.getWindow().peekDecorView();
        if(decor == null) return;
        IBinder token = decor.getWindowToken();
        if(token == null)
        {
            decor.post(() -> attach(a)); // Runs once the game's window is there
            return;
        }
        detach(owner);
        WindowManager.LayoutParams params = new WindowManager.LayoutParams(WindowManager.LayoutParams.TYPE_APPLICATION_SUB_PANEL,
            WindowManager.LayoutParams.FLAG_NOT_FOCUSABLE | WindowManager.LayoutParams.FLAG_NOT_TOUCHABLE |
            WindowManager.LayoutParams.FLAG_LAYOUT_IN_SCREEN | WindowManager.LayoutParams.FLAG_LAYOUT_NO_LIMITS, PixelFormat.TRANSLUCENT);
        params.token = token;
        try
        {
            a.getWindowManager().addView(layer, params);
            owner = a;
        }
        catch(RuntimeException e) { Log.e("AndroidModLoader", "Cannot show overlays on this screen", e); } // A screen that is closing
    }
    // Before the screen's window goes, or Android would report the overlay as leaked
    static void detach(Activity a)
    {
        if(layer == null || a == null || a != owner) return;
        if(layer.isAttachedToWindow()) a.getWindowManager().removeViewImmediate(layer);
        owner = null;
    }

    private static void style(TextView tv, Bridge.Style style)
    {
        float unit = unit(tv.getContext());
        tv.setTextColor((style.text != 0) ? style.text : 0xFFFFFFFF);
        tv.setShadowLayer((style.text == 0) ? 2.0f * unit : 0.0f, 0, 0, 0xFF000000); // Readable over any game
        tv.setTextSize(TypedValue.COMPLEX_UNIT_PX, ((style.textSize > 0) ? style.textSize : 14.0f) * unit);
        tv.setGravity((style.gravity != 0) ? style.gravity : (Gravity.TOP | Gravity.START));
        Typeface font = Bridge.loadFont(tv.getContext(), style.font);
        tv.setTypeface((font != null) ? font : Typeface.DEFAULT);

        GradientDrawable shape = null;
        int pad = 0;
        if(style.background != 0 || style.borderWidth > 0)
        {
            shape = new GradientDrawable();
            shape.setColor(style.background);
            shape.setCornerRadius(Math.max(style.radius, 0.0f) * unit);
            if(style.borderWidth > 0) shape.setStroke((int)(style.borderWidth * unit + 0.5f), (style.border != 0) ? style.border : 0xFF808080);
            pad = (int)(6.0f * unit);
        }
        tv.setBackground(shape);
        tv.setPadding(pad, pad / 2, pad, pad / 2);
    }

    // How far the view has to move to be fully off the screen
    private static float[] offscreen(View v, int slide)
    {
        View parent = (View)v.getParent();
        switch(slide)
        {
            case SLIDE_LEFT:  return new float[] { -v.getRight(), 0 };
            case SLIDE_RIGHT: return new float[] { parent.getWidth() - v.getLeft(), 0 };
            case SLIDE_UP:    return new float[] { 0, -v.getBottom() };
            case SLIDE_DOWN:  return new float[] { 0, parent.getHeight() - v.getTop() };
        }
        return new float[] { 0, 0 };
    }
    private static void whenLaidOut(View v, Runnable r)
    {
        if(v.isLaidOut() && !v.isLayoutRequested())
        {
            r.run();
            return;
        }
        v.addOnLayoutChangeListener(new View.OnLayoutChangeListener()
        {
            @Override public void onLayoutChange(View view, int l, int t, int r2, int b, int ol, int ot, int or, int ob)
            {
                view.removeOnLayoutChangeListener(this);
                r.run();
            }
        });
    }
    // The distance is taken every frame, so a rotation or a resize in the middle only bends the path
    private static void animate(int id, View v, boolean in, int fadeMs, int slide, int delayMs, Runnable end)
    {
        stopAnimation(id);
        if(delayMs > 0)
        {
            Runnable hold = () -> {
                holds.remove(id);
                animate(id, v, in, fadeMs, slide, 0, end);
            };
            holds.put(id, hold);
            Bridge.ui.postDelayed(hold, delayMs);
            return;
        }
        ValueAnimator a = ValueAnimator.ofFloat(0.0f, 1.0f);
        a.setDuration(fadeMs);
        a.setInterpolator(in ? new DecelerateInterpolator() : new AccelerateInterpolator());
        a.addUpdateListener(anim -> {
            float away = in ? 1.0f - (float)anim.getAnimatedValue() : (float)anim.getAnimatedValue();
            float[] off = offscreen(v, slide);
            v.setTranslationX(off[0] * away);
            v.setTranslationY(off[1] * away);
            v.setAlpha(1.0f - away);
        });
        a.addListener(new AnimatorListenerAdapter()
        {
            private boolean cancelled;
            @Override public void onAnimationCancel(Animator anim) { cancelled = true; }
            @Override public void onAnimationEnd(Animator anim)
            {
                if(animations.get(id) == a) animations.remove(id);
                if(!cancelled && end != null) end.run();
            }
        });
        animations.put(id, a);
        a.start();
    }
    private static void stopAnimation(int id)
    {
        Runnable hold = holds.remove(id);
        if(hold != null) Bridge.ui.removeCallbacks(hold);
        ValueAnimator a = animations.remove(id);
        if(a != null) a.cancel();
    }
    private static void removeNow(int id)
    {
        stopAnimation(id);
        TextView v = views.remove(id);
        if(v != null) layer.removeView(v);
    }

    public void add(int id, byte[] text, float x, float y, int[] styleData, byte[] font)
    {
        Bridge.Style style = new Bridge.Style(styleData, font);
        String str = Bridge.str(text);
        Bridge.ui.post(() -> {
            if(layer == null) layer = new Layer(Bridge.appContext);
            TextView v = new TextView(Bridge.appContext);
            v.setTag(new float[] { x, y });
            v.setText(str);
            style(v, style);
            views.put(id, v);
            layer.addView(v, new FrameLayout.LayoutParams(ViewGroup.LayoutParams.WRAP_CONTENT, ViewGroup.LayoutParams.WRAP_CONTENT));
            attach((Bridge.visible != null) ? Bridge.visible : Bridge.findResumed());
        });
    }
    public void setText(int id, byte[] text)
    {
        String str = Bridge.str(text);
        synchronized(pendingTexts)
        {
            pendingTexts.put(id, str);
            if(textsScheduled) return;
            textsScheduled = true;
        }
        Bridge.ui.post(() -> {
            HashMap<Integer, String> texts;
            synchronized(pendingTexts)
            {
                texts = new HashMap<>(pendingTexts);
                pendingTexts.clear();
                textsScheduled = false;
            }
            for(Map.Entry<Integer, String> e : texts.entrySet())
            {
                TextView v = views.get(e.getKey());
                if(v != null) v.setText(e.getValue());
            }
        });
    }
    public void setPosition(int id, float x, float y)
    {
        Bridge.ui.post(() -> {
            TextView v = views.get(id);
            if(v == null) return;
            v.setTag(new float[] { x, y });
            v.requestLayout();
        });
    }
    public void setStyle(int id, int[] styleData, byte[] font)
    {
        Bridge.Style style = new Bridge.Style(styleData, font);
        Bridge.ui.post(() -> {
            TextView v = views.get(id);
            if(v != null) style(v, style);
        });
    }
    public void show(int id, int fadeMs, int slide, int holdMs)
    {
        Bridge.ui.post(() -> {
            TextView v = views.get(id);
            if(v == null) return;
            stopAnimation(id);
            v.setAlpha(0.0f); // Not a frame of it before the animation
            whenLaidOut(v, () -> animate(id, v, true, fadeMs, slide, 0,
                (holdMs > 0) ? () -> animate(id, v, false, fadeMs, slide, holdMs, () -> removeNow(id)) : null));
        });
    }
    public void hide(int id, int fadeMs, int slide, boolean remove)
    {
        Bridge.ui.post(() -> {
            TextView v = views.get(id);
            if(v == null) return;
            whenLaidOut(v, () -> animate(id, v, false, fadeMs, slide, 0, remove ? () -> removeNow(id) : null));
        });
    }
    public void remove(int id)
    {
        Bridge.ui.post(() -> removeNow(id));
    }
}
