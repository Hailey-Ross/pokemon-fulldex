// NOW PLAYING (frontend/now_playing.h calls these static methods through JNI, from its own thread):
// what the player's music app (Spotify, YouTube Music...) is playing, and its previous / play-pause /
// next, for the bar under the pause menu's tiles.
//  - With notification access (Settings, given to NowPlayingListener): the music sessions
//    themselves: the song, artist, app, album art and the app's own buttons.
//  - Without it: the system's media keys still go to the music app (no song to show).
package io.github.mamesj.pokemonds;

import android.content.ComponentName;
import android.content.Context;
import android.content.Intent;
import android.content.pm.PackageManager;
import android.graphics.Bitmap;
import android.media.AudioManager;
import android.media.MediaMetadata;
import android.media.session.MediaController;
import android.media.session.MediaSessionManager;
import android.media.session.PlaybackState;
import android.os.SystemClock;
import android.provider.Settings;
import android.view.KeyEvent;

import java.util.List;

import org.libsdl.app.SDLActivity;

public final class NowPlaying {
    private NowPlaying() { }

    private static Context context() { return SDLActivity.getContext(); }

    // Whether the player has given Fulldex notification access.
    public static boolean allowed() {
        Context c = context();
        if (c == null) return false;
        String list = Settings.Secure.getString(c.getContentResolver(), "enabled_notification_listeners");
        return list != null && list.contains(new ComponentName(c, NowPlayingListener.class).flattenToString());
    }

    // The music app to show: one that is playing, else one that is paused; not Fulldex itself.
    private static MediaController current() {
        Context c = context();
        if (c == null || !allowed()) return null;
        try {
            MediaSessionManager m = (MediaSessionManager) c.getSystemService(Context.MEDIA_SESSION_SERVICE);
            List<MediaController> list = m.getActiveSessions(new ComponentName(c, NowPlayingListener.class));
            MediaController paused = null;
            for (MediaController mc : list) {
                if (c.getPackageName().equals(mc.getPackageName())) continue;
                PlaybackState st = mc.getPlaybackState();
                if (st == null) continue;
                if (st.getState() == PlaybackState.STATE_PLAYING || st.getState() == PlaybackState.STATE_BUFFERING) return mc;
                if (paused == null && st.getState() == PlaybackState.STATE_PAUSED) paused = mc;
            }
            return paused;
        } catch (SecurityException e) {
            return null;
        }
    }

    // The app's own name (the manifest's <queries> lets Android tell it for music apps), else a
    // known one by its package, else its package.
    private static String appName(String pkg) {
        try {
            PackageManager pm = context().getPackageManager();
            return pm.getApplicationLabel(pm.getApplicationInfo(pkg, 0)).toString();
        } catch (Exception ignored) { }
        switch (pkg) {
            case "com.spotify.music": return "Spotify";
            case "com.google.android.apps.youtube.music": return "YouTube Music";
            case "com.google.android.youtube": return "YouTube";
            case "com.apple.android.music": return "Apple Music";
            case "com.amazon.mp3": return "Amazon Music";
            case "deezer.android.app": return "Deezer";
            case "com.aspiro.tidal": return "TIDAL";
            case "com.soundcloud.android": return "SoundCloud";
            case "au.com.shiftyjelly.pocketcasts": return "Pocket Casts";
            default: return pkg;
        }
    }

    // "!" without notification access; "" with nothing playing or paused; otherwise the lines
    // playing (1 / 0), can go back (1 / 0), can skip (1 / 0), title, artist, app.
    public static String info() {
        if (!allowed()) return "!";
        MediaController mc = current();
        if (mc == null) return "";
        PlaybackState st = mc.getPlaybackState();
        MediaMetadata md = mc.getMetadata();
        long actions = st != null ? st.getActions() : 0;
        boolean playing = st != null && (st.getState() == PlaybackState.STATE_PLAYING || st.getState() == PlaybackState.STATE_BUFFERING);
        String title = md != null ? md.getString(MediaMetadata.METADATA_KEY_TITLE) : null;
        String artist = md != null ? md.getString(MediaMetadata.METADATA_KEY_ARTIST) : null;
        String app = appName(mc.getPackageName());
        if (title == null || title.isEmpty()) return "";
        return (playing ? "1" : "0") + "\n" + ((actions & PlaybackState.ACTION_SKIP_TO_PREVIOUS) != 0 ? "1" : "0") + "\n"
            + ((actions & PlaybackState.ACTION_SKIP_TO_NEXT) != 0 ? "1" : "0") + "\n"
            + title.replace('\n', ' ') + "\n" + (artist == null ? "" : artist.replace('\n', ' ')) + "\n" + app.replace('\n', ' ');
    }

    // The album art, at most 512 pixels square: width, height, then the pixels (ARGB); or null.
    public static int[] art() {
        MediaController mc = current();
        MediaMetadata md = mc != null ? mc.getMetadata() : null;
        if (md == null) return null;
        Bitmap b = md.getBitmap(MediaMetadata.METADATA_KEY_ALBUM_ART);
        if (b == null) b = md.getBitmap(MediaMetadata.METADATA_KEY_ART);
        if (b == null) b = md.getBitmap(MediaMetadata.METADATA_KEY_DISPLAY_ICON);
        if (b == null || b.getWidth() <= 0 || b.getHeight() <= 0) return null;
        int big = Math.max(b.getWidth(), b.getHeight()), side = Math.min(512, big);
        int w = Math.max(1, b.getWidth() * side / big), h = Math.max(1, b.getHeight() * side / big);
        Bitmap s = Bitmap.createScaledBitmap(b.copy(Bitmap.Config.ARGB_8888, false), w, h, true);
        int[] out = new int[2 + w * h];
        out[0] = w;
        out[1] = h;
        s.getPixels(out, 2, w, 0, 0, w, h);
        return out;
    }

    // 0 previous, 1 play / pause, 2 next: to the music app, or as the system's media keys.
    public static void command(int which) {
        MediaController mc = current();
        if (mc != null) {
            MediaController.TransportControls t = mc.getTransportControls();
            PlaybackState st = mc.getPlaybackState();
            if (which == 0) t.skipToPrevious();
            else if (which == 2) t.skipToNext();
            else if (st != null && st.getState() == PlaybackState.STATE_PLAYING) t.pause();
            else t.play();
            return;
        }
        Context c = context();
        if (c == null) return;
        int key = which == 0 ? KeyEvent.KEYCODE_MEDIA_PREVIOUS : which == 2 ? KeyEvent.KEYCODE_MEDIA_NEXT : KeyEvent.KEYCODE_MEDIA_PLAY_PAUSE;
        AudioManager am = (AudioManager) c.getSystemService(Context.AUDIO_SERVICE);
        long now = SystemClock.uptimeMillis();
        am.dispatchMediaKeyEvent(new KeyEvent(now, now, KeyEvent.ACTION_DOWN, key, 0));
        am.dispatchMediaKeyEvent(new KeyEvent(now, now, KeyEvent.ACTION_UP, key, 0));
    }

    // SHOW SONG NAMES: Android's notification access settings, where the player allows Fulldex.
    public static void openSettings() {
        Context c = context();
        if (c == null) return;
        Intent i = new Intent(Settings.ACTION_NOTIFICATION_LISTENER_SETTINGS);
        i.addFlags(Intent.FLAG_ACTIVITY_NEW_TASK);
        try { c.startActivity(i); } catch (Exception ignored) { }
    }
}
