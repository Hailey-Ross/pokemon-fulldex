package io.github.mamesj.omeganx;

import android.app.Activity;
import android.content.Context;
import android.content.Intent;
import android.net.Uri;
import java.io.File;

/** FullDex's way into the Omega Ruby / Alpha Sapphire kit (Oras): compiled into the FullDex Android
 *  app only when the kit is (tools/android/build_all.sh, SSR_3DS_ANDROID_KIT). FullDex's GameActivity
 *  calls it by name, so FullDex still builds without the kit. Each call is quick: copying and
 *  starting run on threads of their own, and what they come to is told once by note(). */
public final class FullDex3ds {
    private static volatile String note = "";
    private static volatile boolean busy;

    private FullDex3ds() {}

    /** the games can play here: the kit's libraries are in the app (arm64 phones). */
    public static boolean available(Context context) {
        return new File(context.getApplicationInfo().nativeLibraryDir, "libpocketnative.so").isFile();
    }

    /** the game's folder, <external files>/3ds/<title id> (game.3ds and its saves). */
    public static String folder(Context context, String id) { return Oras.folder(context, id).getAbsolutePath(); }

    public static boolean built(Context context, String id) { return Oras.built(context, id); }

    /** what to tell the player now: building's progress while the phone builds, else a message
     *  once (a dump added, code taken, something that went wrong), else "". */
    public static String note() {
        if (BuildService.running) return "BUILDING " + Oras.name(BuildService.building).toUpperCase() + ": " + BuildService.progress;
        String n = note;
        note = "";
        return n;
    }

    public static boolean busy() { return busy || BuildService.running; }

    /** a dump found with the player's ROMs copied into the game's folder (about 2 GB). */
    public static void add(final Context context, final String path) {
        if (busy) return;
        busy = true;
        new Thread(() -> {
            try {
                String id = Oras.addDump(context, Uri.fromFile(new File(path)));
                note = id == null ? "THAT FILE IS NOT OMEGA RUBY OR ALPHA SAPPHIRE"
                                  : Oras.name(id).toUpperCase() + " IS IN FULLDEX: CHOOSE IT TO PLAY";
            } catch (Exception e) {
                note = "COULD NOT ADD THE GAME: " + e.getMessage();
            }
            busy = false;
        }).start();
    }

    /** builds the game's code on the phone (a notification shows it; about 25 minutes). */
    public static void build(Context context, String id) {
        if (busy()) return;
        Oras.build(context, id);
        note = "BUILDING " + Oras.name(id).toUpperCase() + ": STARTING (ABOUT 25 MINUTES, KEEP THE PHONE PLUGGED IN)";
    }

    /** takes the game code the FullDex Builder made on a PC (<title id>.so), from the picker. */
    public static void importCode(final Context context, final String id, final Uri uri) {
        if (busy) return;
        busy = true;
        new Thread(() -> {
            // (The kit checks it is Android game code; the name says which game: the FullDex Builder
            // calls it <title id>.so, so the other game's file is refused rather than played.)
            String picked = displayName(context, uri).toUpperCase(java.util.Locale.ROOT);
            for (String[] game : Oras.GAMES) {
                if (!game[0].equals(id) && picked.startsWith(game[0])) {
                    note = "THAT IS " + Oras.name(game[0]).toUpperCase() + "'S GAME CODE: PICK " + id + ".SO";
                    busy = false;
                    return;
                }
            }
            String error = Oras.importCode(context, id, uri);
            note = error == null ? Oras.name(id).toUpperCase() + ": GAME CODE FROM PC ADDED" : error.toUpperCase();
            busy = false;
        }).start();
    }

    /** starts the game (its result comes back to the activity: Oras.RESTART, start it again). */
    public static void play(final Activity activity, final String id, final int request) {
        if (busy) return;
        busy = true;
        new Thread(() -> {
            try {
                final Intent intent = Oras.prepare(activity, id);
                activity.runOnUiThread(() -> activity.startActivityForResult(intent, request));
            } catch (Exception e) {
                note = "COULD NOT START THE GAME: " + e.getMessage();
            }
            busy = false;
        }).start();
    }

    public static int restartCode() { return Oras.RESTART; }

    private static String displayName(Context context, Uri uri) {
        try (android.database.Cursor c = context.getContentResolver().query(uri, new String[] { android.provider.OpenableColumns.DISPLAY_NAME }, null, null, null)) {
            if (c != null && c.moveToFirst() && c.getString(0) != null) return c.getString(0);
        } catch (Exception ignored) {}
        String last = uri.getLastPathSegment();
        return last == null ? "" : last.substring(last.lastIndexOf('/') + 1);
    }
}
