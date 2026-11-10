package io.github.mamesj.pokemonds;

import android.os.Build;
import android.view.View;
import android.view.Window;
import android.view.WindowInsets;
import android.view.WindowInsetsController;
import android.view.WindowManager;

// Reassert the requested mode after Android recreates its decor/insets on unlock.
// Let SDL resize its own surface from layout; never force a display-sized buffer.
final class FullscreenRestore {
    static boolean apply(Window window, boolean fullscreen, boolean keyboard, boolean multiWindow) {
        if (window == null || !fullscreen || keyboard || multiWindow) return false;
        View decor = window.getDecorView();
        if (decor == null || decor.getWindowToken() == null) return false;
        if (Build.VERSION.SDK_INT >= 30 && Api30.keyboardVisible(decor)) return false;
        window.addFlags(WindowManager.LayoutParams.FLAG_FULLSCREEN);
        window.clearFlags(WindowManager.LayoutParams.FLAG_FORCE_NOT_FULLSCREEN);
        if (Build.VERSION.SDK_INT >= 30) Api30.restore(window, decor);
        else legacy(decor);
        decor.requestApplyInsets();
        decor.requestLayout();
        return true;
    }

    // Keep newer framework types out of the legacy path's class loading.
    private static final class Api30 {
        static boolean keyboardVisible(View decor) {
            WindowInsets insets = decor.getRootWindowInsets();
            return insets != null && insets.isVisible(WindowInsets.Type.ime());
        }
        static void restore(Window window, View decor) {
            window.setDecorFitsSystemWindows(false);
            WindowInsetsController controller = window.getInsetsController();
            if (controller != null) {
                controller.setSystemBarsBehavior(WindowInsetsController.BEHAVIOR_SHOW_TRANSIENT_BARS_BY_SWIPE);
                controller.hide(WindowInsets.Type.systemBars());
            } else legacy(decor); // A controller may not be attached until the next layout.
        }
    }

    private static void legacy(View decor) {
        decor.setSystemUiVisibility(decor.getSystemUiVisibility()
            | View.SYSTEM_UI_FLAG_FULLSCREEN | View.SYSTEM_UI_FLAG_HIDE_NAVIGATION
            | View.SYSTEM_UI_FLAG_IMMERSIVE_STICKY | View.SYSTEM_UI_FLAG_LAYOUT_FULLSCREEN
            | View.SYSTEM_UI_FLAG_LAYOUT_HIDE_NAVIGATION | View.SYSTEM_UI_FLAG_LAYOUT_STABLE);
    }
}
