package net.rusjj.aml;

import android.app.Activity;
import android.app.AlertDialog;
import android.content.Context;
import android.content.DialogInterface;
import android.content.res.ColorStateList;
import android.content.res.TypedArray;
import android.graphics.drawable.GradientDrawable;
import android.graphics.drawable.InsetDrawable;
import android.util.Log;
import android.view.Gravity;
import android.view.View;
import android.view.ViewGroup;
import android.view.Window;
import android.view.WindowManager;
import android.view.inputmethod.EditorInfo;
import android.widget.Button;
import android.widget.CheckedTextView;
import android.widget.CompoundButton;
import android.widget.EditText;
import android.widget.LinearLayout;
import android.widget.ListView;
import android.widget.SeekBar;
import android.widget.TextView;
import java.nio.charset.StandardCharsets;

// Strings go both ways as UTF-8 byte[], JNI's modified UTF-8 breaks emojis
// style is { baseTheme, background, text, accent, cornerRadius (float bits), dimAmount (float bits) }
public class Dialogs
{
    private static native void onResult(long id, boolean ok, int value, byte[] data);

    private static final class Result implements DialogInterface.OnDismissListener
    {
        private final long id;
        private boolean sent;
        Result(long id) { this.id = id; }

        void send(boolean ok, int value, byte[] data)
        {
            if(sent) return;
            sent = true;
            onResult(id, ok, value, data);
        }
        @Override public void onDismiss(DialogInterface dialog) { send(false, 0, null); } // Back button, a tap outside, "Cancel"
    }

    private interface Body { void run(Result result) throws Exception; }
    private static void post(Activity activity, long id, Body body)
    {
        Result result = new Result(id);
        activity.runOnUiThread(() -> {
            try { body.run(result); }
            catch(Exception e)
            {
                Log.e("AndroidModLoader", "Cannot show a dialog", e);
                result.send(false, 0, null);
            }
        });
    }

    private static String str(byte[] b) { return new String(b, StandardCharsets.UTF_8); }
    private static String[] strs(byte[][] b)
    {
        String[] out = new String[b.length];
        for(int i = 0; i < b.length; ++i) out[i] = str(b[i]);
        return out;
    }
    private static CharSequence label(Context ctx, byte[] b, int fallbackRes) { return (b.length > 0) ? str(b) : ctx.getString(fallbackRes); }
    private static AlertDialog.Builder builder(Activity activity, byte[] title, int[] style)
    {
        AlertDialog.Builder builder = (style[0] != 0) ? new AlertDialog.Builder(activity, style[0]) : new AlertDialog.Builder(activity);
        return builder.setTitle(str(title));
    }

    private static void apply(AlertDialog dialog, int[] style)
    {
        int background = style[1], text = style[2], accent = style[3];
        float radius = Float.intBitsToFloat(style[4]), dim = Float.intBitsToFloat(style[5]);
        Context ctx = dialog.getContext();
        Window window = dialog.getWindow();
        float density = ctx.getResources().getDisplayMetrics().density;

        if(background != 0 || radius > 0)
        {
            if(background == 0)
            {
                TypedArray attrs = ctx.obtainStyledAttributes(new int[] { android.R.attr.colorBackground });
                background = attrs.getColor(0, 0xFF303030);
                attrs.recycle();
            }
            GradientDrawable shape = new GradientDrawable();
            shape.setColor(background);
            shape.setCornerRadius(radius * density);
            window.setBackgroundDrawable(new InsetDrawable(shape, (int)(16 * density))); // Material dialogs are inset too
        }
        if(dim > 0)
        {
            window.addFlags(WindowManager.LayoutParams.FLAG_DIM_BEHIND);
            window.setDimAmount(dim);
        }
        else if(dim < 0) window.clearFlags(WindowManager.LayoutParams.FLAG_DIM_BEHIND);

        if(text == 0 && accent == 0) return;
        paint(window.getDecorView(), text, accent);
        ListView list = dialog.getListView();
        if(list != null) list.setOnHierarchyChangeListener(new ViewGroup.OnHierarchyChangeListener() // Rows are made lazily
        {
            @Override public void onChildViewAdded(View parent, View child) { paint(child, text, accent); }
            @Override public void onChildViewRemoved(View parent, View child) {}
        });
    }
    private static void paint(View v, int text, int accent)
    {
        if(v instanceof ViewGroup)
        {
            ViewGroup group = (ViewGroup)v;
            for(int i = 0; i < group.getChildCount(); ++i) paint(group.getChildAt(i), text, accent);
        }
        ColorStateList accentList = (accent != 0) ? ColorStateList.valueOf(accent) : null;
        if(v instanceof SeekBar && accentList != null)
        {
            ((SeekBar)v).setProgressTintList(accentList);
            ((SeekBar)v).setThumbTintList(accentList);
        }
        if(!(v instanceof TextView)) return;

        TextView tv = (TextView)v;
        boolean isButton = (v instanceof Button) && !(v instanceof CompoundButton);
        int color = (isButton && accent != 0) ? accent : text;
        if(color != 0) tv.setTextColor(color);
        if(accentList == null) return;
        if(v instanceof EditText) v.setBackgroundTintList(accentList);
        if(v instanceof CheckedTextView) ((CheckedTextView)v).setCheckMarkTintList(accentList);
        if(v instanceof CompoundButton) ((CompoundButton)v).setButtonTintList(accentList);
    }

    public void showAlert(Activity activity, byte[] title, byte[] message, byte[] button, int[] style, long id)
    {
        post(activity, id, result -> {
            AlertDialog.Builder builder = builder(activity, title, style);
            apply(builder.setMessage(str(message))
                .setPositiveButton(label(builder.getContext(), button, android.R.string.ok), null)
                .setOnDismissListener(result)
                .show(), style);
        });
    }

    public void showInput(Activity activity, byte[] title, byte[] text, byte[] hint, int inputType, int[] style, long id)
    {
        post(activity, id, result -> {
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

            dialog.getWindow().setSoftInputMode(WindowManager.LayoutParams.SOFT_INPUT_STATE_ALWAYS_VISIBLE);
            dialog.show();
            apply(dialog, style);
            edit.requestFocus();
        });
    }

    public void showConfirm(Activity activity, byte[] title, byte[] message, byte[] yes, byte[] no, int[] style, long id)
    {
        post(activity, id, result -> {
            AlertDialog.Builder builder = builder(activity, title, style);
            Context ctx = builder.getContext();
            apply(builder.setMessage(str(message))
                .setPositiveButton(label(ctx, yes, android.R.string.ok), (d, w) -> result.send(true, 1, null))
                .setNegativeButton(label(ctx, no, android.R.string.cancel), null)
                .setOnDismissListener(result)
                .show(), style);
        });
    }

    public void showChoice(Activity activity, byte[] title, byte[][] items, int selected, int[] style, long id)
    {
        post(activity, id, result -> {
            apply(builder(activity, title, style)
                .setSingleChoiceItems(strs(items), selected, (d, which) -> { result.send(true, which, null); d.dismiss(); })
                .setNegativeButton(android.R.string.cancel, null)
                .setOnDismissListener(result)
                .show(), style);
        });
    }

    public void showMultiChoice(Activity activity, byte[] title, byte[][] items, boolean[] checked, int[] style, long id)
    {
        post(activity, id, result -> {
            apply(builder(activity, title, style)
                .setMultiChoiceItems(strs(items), checked, (d, which, isChecked) -> checked[which] = isChecked)
                .setPositiveButton(android.R.string.ok, (d, w) -> {
                    byte[] out = new byte[checked.length];
                    for(int i = 0; i < checked.length; ++i) out[i] = (byte)(checked[i] ? 1 : 0);
                    result.send(true, 0, out);
                })
                .setNegativeButton(android.R.string.cancel, null)
                .setOnDismissListener(result)
                .show(), style);
        });
    }

    public void showSlider(Activity activity, byte[] title, int min, int max, int value, int[] style, long id)
    {
        post(activity, id, result -> {
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

            apply(builder.setView(layout)
                .setPositiveButton(android.R.string.ok, (d, w) -> result.send(true, min + bar.getProgress(), null))
                .setNegativeButton(android.R.string.cancel, null)
                .setOnDismissListener(result)
                .show(), style);
        });
    }
}
