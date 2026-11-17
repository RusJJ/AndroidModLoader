package net.rusjj.aml;

import android.app.Activity;
import android.app.AlertDialog;
import android.app.Application;
import android.os.Build;
import android.os.Bundle;
import android.os.Handler;
import android.os.Looper;
import android.content.Context;
import android.content.DialogInterface;
import android.content.res.ColorStateList;
import android.content.res.TypedArray;
import android.graphics.drawable.GradientDrawable;
import android.graphics.Typeface;
import android.graphics.drawable.InsetDrawable;
import android.util.Log;
import android.view.Gravity;
import android.view.View;
import android.view.ViewGroup;
import android.view.Window;
import android.view.WindowInsets;
import android.view.WindowInsetsController;
import android.view.WindowManager;
import android.view.inputmethod.EditorInfo;
import android.view.inputmethod.InputMethodManager;
import android.widget.Button;
import android.widget.CheckedTextView;
import android.widget.CompoundButton;
import android.widget.EditText;
import android.widget.LinearLayout;
import android.widget.ListView;
import android.widget.SeekBar;
import android.widget.TextView;
import java.lang.reflect.Field;
import java.nio.charset.StandardCharsets;
import java.util.ArrayList;
import java.util.HashMap;
import java.util.Map;
import java.util.WeakHashMap;

// Strings go both ways as UTF-8 byte[], JNI's modified UTF-8 breaks emojis
public class Bridge
{
    private static native void onResult(long id, boolean ok, int value, byte[] data);
    private static native void onLifecycle(int event); // eLifecycleEvent

    private static final WeakHashMap<Activity, Boolean> focusWatched = new WeakHashMap<>();
    private static int ownDialogs; // Focus taken by AML's own dialogs is not reported, a mod could loop on it
    private static boolean ownFocusLoss;
    private static void watchFocus(Activity a)
    {
        if(a.getWindow().peekDecorView() == null || focusWatched.put(a, Boolean.TRUE) != null) return; // Do not create the game's window early
        a.getWindow().getDecorView().getViewTreeObserver().addOnWindowFocusChangeListener(hasFocus -> {
            if(!hasFocus && ownDialogs > 0) ownFocusLoss = true;
            else if(hasFocus && ownFocusLoss) ownFocusLoss = false;
            else onLifecycle(hasFocus ? 3 : 2);
        });
    }

    static final Handler ui = new Handler(Looper.getMainLooper());
    static volatile Activity visible; // Dialogs and AML's activity calls go to the screen the player sees
    private static final ArrayList<Runnable> waiting = new ArrayList<>(); // Dialogs asked for while no screen is up

    // Android's own record, for a resume that happened before the callbacks below were registered
    static Activity findResumed()
    {
        try
        {
            Class<?> threadClass = Class.forName("android.app.ActivityThread");
            Object thread = threadClass.getMethod("currentActivityThread").invoke(null);
            Field activities = threadClass.getDeclaredField("mActivities");
            activities.setAccessible(true);
            for(Object record : ((Map<?, ?>)activities.get(thread)).values())
            {
                Field paused = record.getClass().getDeclaredField("paused");
                Field activity = record.getClass().getDeclaredField("activity");
                paused.setAccessible(true);
                activity.setAccessible(true);
                Activity a = (Activity)activity.get(record);
                if(!paused.getBoolean(record) && a != null && !a.isFinishing()) return a;
            }
        }
        catch(Exception e) { Log.e("AndroidModLoader", "Cannot find the resumed activity", e); }
        return null;
    }

    // Only observes: activities and their Window.Callback stay untouched.
    // Started activities are counted, so going to an ad or another screen of the game is not a pause
    static Context appContext;
    public void init(Context context)
    {
        Application app = (Application)context.getApplicationContext();
        appContext = app;
        ui.post(() -> {
            Activity now = findResumed();
            if(now != null)
            {
                visible = now;
                watchFocus(now);
            }
            app.registerActivityLifecycleCallbacks(new Application.ActivityLifecycleCallbacks()
            {
                private int started = (now != null) ? 1 : 0;
                @Override public void onActivityStarted(Activity a) { if(started++ == 0) onLifecycle(1); }
                @Override public void onActivityStopped(Activity a) { if(started > 0 && --started == 0) onLifecycle(0); }
                @Override public void onActivityResumed(Activity a)
                {
                    visible = a;
                    watchFocus(a);
                    Overlay.attach(a);
                    for(Runnable r : waiting) r.run();
                    waiting.clear();
                }
                @Override public void onActivityDestroyed(Activity a)
                {
                    if(visible == a) visible = null;
                    Overlay.detach(a);
                }
                @Override public void onActivityPaused(Activity a) {}
                @Override public void onActivityCreated(Activity a, Bundle b) {}
                @Override public void onActivitySaveInstanceState(Activity a, Bundle b) {}
            });
        });
    }
    public Activity getVisible()
    {
        Activity a = visible;
        return (a != null) ? a : findResumed();
    }
    private final Overlay overlay = new Overlay();
    public Overlay getOverlay() { return overlay; }

    private static final class Result implements DialogInterface.OnDismissListener
    {
        private final long id;
        private boolean sent, counted;
        Result(long id) { this.id = id; }

        void send(boolean ok, int value, byte[] data)
        {
            if(sent) return;
            sent = true;
            if(counted) --ownDialogs;
            onResult(id, ok, value, data);
        }
        @Override public void onDismiss(DialogInterface dialog) { send(false, 0, null); } // Back button, a tap outside, "Cancel"
    }

    private interface Body { void run(Activity activity, Result result) throws Exception; }
    private static void post(long id, Body body)
    {
        Result result = new Result(id);
        Runnable show = () -> {
            ++ownDialogs;
            result.counted = true;
            try { body.run(visible, result); }
            catch(Exception e)
            {
                Log.e("AndroidModLoader", "Cannot show a dialog", e);
                result.send(false, 0, null);
            }
        };
        ui.post(() -> {
            if(visible == null) visible = findResumed();
            if(visible != null) show.run();
            else waiting.add(show);
        });
    }

    static String str(byte[] b) { return new String(b, StandardCharsets.UTF_8); }
    private static String[] strs(byte[][] b)
    {
        String[] out = new String[b.length];
        for(int i = 0; i < b.length; ++i) out[i] = str(b[i]);
        return out;
    }
    private static CharSequence label(Context ctx, byte[] b, int fallbackRes) { return (b.length > 0) ? str(b) : ctx.getString(fallbackRes); }
    // DialogStyle from iaml.h, floats come as their bits
    static final class Style
    {
        final int theme, background, text, title, accent, border, gravity;
        final float radius, borderWidth, dim, textSize, titleSize, width;
        final boolean cancelable;
        final String font;
        Style(int[] s, byte[] fontPath)
        {
            theme = s[0]; background = s[1]; text = s[2]; title = s[3]; accent = s[4]; border = s[5];
            radius = Float.intBitsToFloat(s[6]); borderWidth = Float.intBitsToFloat(s[7]); dim = Float.intBitsToFloat(s[8]);
            textSize = Float.intBitsToFloat(s[9]); titleSize = Float.intBitsToFloat(s[10]); width = Float.intBitsToFloat(s[11]);
            gravity = s[12]; cancelable = (s[13] != 0);
            font = (fontPath.length > 0) ? str(fontPath) : null;
        }
    }

    private static final HashMap<String, Typeface> fonts = new HashMap<>();
    static Typeface loadFont(Context ctx, String path)
    {
        if(path == null) return null;
        if(fonts.containsKey(path)) return fonts.get(path);
        Typeface font = null;
        try { font = path.startsWith("/") ? Typeface.createFromFile(path) : Typeface.createFromAsset(ctx.getAssets(), path); }
        catch(Exception e) { Log.e("AndroidModLoader", "Cannot load a dialog font " + path, e); }
        fonts.put(path, font);
        return font;
    }

    private static AlertDialog.Builder builder(Activity activity, byte[] title, Style style)
    {
        AlertDialog.Builder builder = (style.theme != 0) ? new AlertDialog.Builder(activity, style.theme) : new AlertDialog.Builder(activity);
        return builder.setTitle(str(title)).setCancelable(style.cancelable);
    }

    // Shown non-focusable first: a focusable window that is not immersive brings the system bars back
    private static void present(AlertDialog dialog, Activity activity, Style style)
    {
        Window window = dialog.getWindow();
        View game = activity.getWindow().getDecorView();
        window.addFlags(WindowManager.LayoutParams.FLAG_NOT_FOCUSABLE);
        dialog.show();
        window.getDecorView().setSystemUiVisibility(game.getSystemUiVisibility());
        WindowInsets gameInsets = (Build.VERSION.SDK_INT >= 30) ? game.getRootWindowInsets() : null;
        if(gameInsets != null && window.getInsetsController() != null && activity.getWindow().getInsetsController() != null)
        {
            int hidden = 0;
            if(!gameInsets.isVisible(WindowInsets.Type.statusBars())) hidden |= WindowInsets.Type.statusBars();
            if(!gameInsets.isVisible(WindowInsets.Type.navigationBars())) hidden |= WindowInsets.Type.navigationBars();
            window.getInsetsController().setSystemBarsBehavior(activity.getWindow().getInsetsController().getSystemBarsBehavior());
            if(hidden != 0) window.getInsetsController().hide(hidden);
        }
        window.clearFlags(WindowManager.LayoutParams.FLAG_NOT_FOCUSABLE);
        apply(dialog, style);
    }

    private static void apply(AlertDialog dialog, Style style)
    {
        Context ctx = dialog.getContext();
        Window window = dialog.getWindow();
        float density = ctx.getResources().getDisplayMetrics().density;

        if(style.background != 0 || style.radius >= 0 || style.borderWidth > 0)
        {
            int background = style.background;
            if(background == 0)
            {
                TypedArray attrs = ctx.obtainStyledAttributes(new int[] { android.R.attr.colorBackground });
                background = attrs.getColor(0, 0xFF303030);
                attrs.recycle();
            }
            GradientDrawable shape = new GradientDrawable();
            shape.setColor(background);
            shape.setCornerRadius(((style.radius >= 0) ? style.radius : 2.0f) * density); // 2dp is Material's own
            if(style.borderWidth > 0) shape.setStroke((int)(style.borderWidth * density + 0.5f), (style.border != 0) ? style.border : 0xFF808080);
            window.setBackgroundDrawable(new InsetDrawable(shape, (int)(16 * density))); // Material dialogs are inset too
        }
        if(style.dim >= 0)
        {
            window.addFlags(WindowManager.LayoutParams.FLAG_DIM_BEHIND);
            window.setDimAmount(style.dim);
        }
        if(style.gravity != 0) window.setGravity(style.gravity);
        if(style.width > 0)
        {
            int screen = ctx.getResources().getDisplayMetrics().widthPixels;
            window.setLayout((style.width <= 1.0f) ? (int)(screen * style.width) : (int)(style.width * density), ViewGroup.LayoutParams.WRAP_CONTENT);
        }

        Typeface font = loadFont(ctx, style.font);
        if(style.text == 0 && style.title == 0 && style.accent == 0 && style.textSize <= 0 && style.titleSize <= 0 && font == null) return;
        int titleId = ctx.getResources().getIdentifier("alertTitle", "id", "android");
        paint(window.getDecorView(), style, font, titleId);
        TextView title = (titleId != 0) ? window.findViewById(titleId) : null;
        if(title != null)
        {
            int color = (style.title != 0) ? style.title : style.text;
            if(color != 0) title.setTextColor(color);
            if(style.titleSize > 0) title.setTextSize(style.titleSize);
        }
        ListView list = dialog.getListView();
        if(list != null) list.setOnHierarchyChangeListener(new ViewGroup.OnHierarchyChangeListener() // Rows are made lazily
        {
            @Override public void onChildViewAdded(View parent, View child) { paint(child, style, font, titleId); }
            @Override public void onChildViewRemoved(View parent, View child) {}
        });
    }
    private static void paint(View v, Style style, Typeface font, int titleId)
    {
        if(v instanceof ViewGroup)
        {
            ViewGroup group = (ViewGroup)v;
            for(int i = 0; i < group.getChildCount(); ++i) paint(group.getChildAt(i), style, font, titleId);
        }
        ColorStateList accentList = (style.accent != 0) ? ColorStateList.valueOf(style.accent) : null;
        if(v instanceof SeekBar && accentList != null)
        {
            ((SeekBar)v).setProgressTintList(accentList);
            ((SeekBar)v).setThumbTintList(accentList);
        }
        if(!(v instanceof TextView)) return;

        TextView tv = (TextView)v;
        if(font != null) tv.setTypeface(font);
        if(titleId != 0 && v.getId() == titleId) return; // Colored and sized by apply()

        boolean isButton = (v instanceof Button) && !(v instanceof CompoundButton);
        int color = (isButton && style.accent != 0) ? style.accent : style.text;
        if(color != 0) tv.setTextColor(color);
        if(style.textSize > 0) tv.setTextSize(style.textSize);
        if(accentList == null) return;
        if(v instanceof EditText) v.setBackgroundTintList(accentList);
        if(v instanceof CheckedTextView) ((CheckedTextView)v).setCheckMarkTintList(accentList);
        if(v instanceof CompoundButton) ((CompoundButton)v).setButtonTintList(accentList);
    }

    public void showAlert(byte[] title, byte[] message, byte[] button, int[] styleData, byte[] font, long id)
    {
        Style style = new Style(styleData, font);
        post(id, (activity, result) -> {
            AlertDialog.Builder builder = builder(activity, title, style);
            present(builder.setMessage(str(message))
                .setPositiveButton(label(builder.getContext(), button, android.R.string.ok), null)
                .setOnDismissListener(result)
                .create(), activity, style);
        });
    }

    public void showInput(byte[] title, byte[] text, byte[] hint, int inputType, int[] styleData, byte[] font, long id)
    {
        Style style = new Style(styleData, font);
        post(id, (activity, result) -> {
            AlertDialog.Builder builder = builder(activity, title, style);

            EditText edit = new EditText(builder.getContext());
            edit.setSingleLine(true); // Before setInputType, it would reset a password mask
            edit.setInputType(inputType != 0 ? inputType : EditorInfo.TYPE_CLASS_TEXT);
            edit.setImeOptions(EditorInfo.IME_ACTION_DONE);
            edit.setHint(str(hint));
            edit.setText(str(text));
            edit.setSelection(edit.getText().length());

            DialogInterface.OnClickListener onOk = (d, w) -> result.send(true, 0, edit.getText().toString().getBytes(StandardCharsets.UTF_8));
            AlertDialog dialog = builder.setView(edit)
                .setPositiveButton(android.R.string.ok, onOk)
                .setNegativeButton(android.R.string.cancel, null)
                .setOnDismissListener(result)
                .create();
            edit.setOnEditorActionListener((v, actionId, event) -> {
                if(actionId != EditorInfo.IME_ACTION_DONE) return false;
                onOk.onClick(dialog, DialogInterface.BUTTON_POSITIVE);
                dialog.dismiss();
                return true;
            });

            present(dialog, activity, style);
            edit.requestFocus();
            edit.post(() -> ((InputMethodManager)activity.getSystemService(Context.INPUT_METHOD_SERVICE)).showSoftInput(edit, 0)); // Window was not focusable on show
        });
    }

    public void showConfirm(byte[] title, byte[] message, byte[] yes, byte[] no, int[] styleData, byte[] font, long id)
    {
        Style style = new Style(styleData, font);
        post(id, (activity, result) -> {
            AlertDialog.Builder builder = builder(activity, title, style);
            Context ctx = builder.getContext();
            present(builder.setMessage(str(message))
                .setPositiveButton(label(ctx, yes, android.R.string.ok), (d, w) -> result.send(true, 1, null))
                .setNegativeButton(label(ctx, no, android.R.string.cancel), null)
                .setOnDismissListener(result)
                .create(), activity, style);
        });
    }

    public void showChoice(byte[] title, byte[][] items, int selected, int[] styleData, byte[] font, long id)
    {
        Style style = new Style(styleData, font);
        post(id, (activity, result) -> {
            present(builder(activity, title, style)
                .setSingleChoiceItems(strs(items), selected, (d, which) -> { result.send(true, which, null); d.dismiss(); })
                .setNegativeButton(android.R.string.cancel, null)
                .setOnDismissListener(result)
                .create(), activity, style);
        });
    }

    public void showMultiChoice(byte[] title, byte[][] items, boolean[] checked, int[] styleData, byte[] font, long id)
    {
        Style style = new Style(styleData, font);
        post(id, (activity, result) -> {
            present(builder(activity, title, style)
                .setMultiChoiceItems(strs(items), checked, (d, which, isChecked) -> checked[which] = isChecked)
                .setPositiveButton(android.R.string.ok, (d, w) -> {
                    byte[] out = new byte[checked.length];
                    for(int i = 0; i < checked.length; ++i) out[i] = (byte)(checked[i] ? 1 : 0);
                    result.send(true, 0, out);
                })
                .setNegativeButton(android.R.string.cancel, null)
                .setOnDismissListener(result)
                .create(), activity, style);
        });
    }

    public void showSlider(byte[] title, int min, int max, int value, int[] styleData, byte[] font, long id)
    {
        Style style = new Style(styleData, font);
        post(id, (activity, result) -> {
            AlertDialog.Builder builder = builder(activity, title, style);
            Context ctx = builder.getContext();

            TextView label = new TextView(ctx);
            label.setGravity(Gravity.CENTER);
            label.setTextSize(20.0f);
            label.setText(String.valueOf(value));

            SeekBar bar = new SeekBar(ctx);
            bar.setMax(max - min);
            bar.setProgress(value - min);
            bar.setOnSeekBarChangeListener(new SeekBar.OnSeekBarChangeListener()
            {
                @Override public void onProgressChanged(SeekBar s, int progress, boolean fromUser) { label.setText(String.valueOf(min + progress)); }
                @Override public void onStartTrackingTouch(SeekBar s) {}
                @Override public void onStopTrackingTouch(SeekBar s) {}
            });

            int pad = (int)(16.0f * ctx.getResources().getDisplayMetrics().density);
            LinearLayout layout = new LinearLayout(ctx);
            layout.setOrientation(LinearLayout.VERTICAL);
            layout.setPadding(pad, pad, pad, 0);
            layout.addView(label);
            layout.addView(bar);

            present(builder.setView(layout)
                .setPositiveButton(android.R.string.ok, (d, w) -> result.send(true, min + bar.getProgress(), null))
                .setNegativeButton(android.R.string.cancel, null)
                .setOnDismissListener(result)
                .create(), activity, style);
        });
    }
}
