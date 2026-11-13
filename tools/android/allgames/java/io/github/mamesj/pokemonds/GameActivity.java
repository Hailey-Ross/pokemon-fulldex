// The Android all-games app's activity: SDL's own, plus what the game list needs from Android
// (frontend/pc_launcher.cpp calls these static methods through JNI):
//  - the folder the player chose for their ROMs, picked once with Android's folder picker;
//  - access to files ("All files access"), so that the program reads the ROMs and writes the
//    saves in that folder with ordinary file calls, as on a PC;
//  - starting the app again (QUIT in the pause menu goes back to the list in a new process,
//    as on the PC and the Switch).
package io.github.mamesj.pokemonds;

import android.Manifest;
import java.io.File;
import java.io.FileOutputStream;
import java.nio.charset.StandardCharsets;
import android.system.Os;
import android.system.OsConstants;
import android.content.Intent;
import android.database.ContentObserver;
import android.os.Handler;
import android.os.Looper;
import android.content.SharedPreferences;
import android.content.pm.PackageManager;
import android.content.pm.ActivityInfo;
import android.net.Uri;
import android.os.Build;
import android.os.Bundle;
import android.os.Environment;
import android.os.Process;
import android.provider.DocumentsContract;
import android.provider.Settings;
import android.util.Log;

import org.libsdl.app.SDLActivity;

public class GameActivity extends SDLActivity {
    private static final String TAG = "PokemonDS";
    private static final int PICK_FOLDER = 4201;
    private static final int ASK_FILES = 4202;
    private static final int PICK_3DS_CODE = 4203, PLAY_3DS = 4204;
    private static volatile String codeFor = "", playing3ds = "";
    private static volatile GameActivity self;
    private boolean resumed;
    private final Handler fullscreenHandler = new Handler(Looper.getMainLooper());
    private String fullscreenReason = "resume";
    private final Runnable restoreFullscreen = () -> {
        if (self != this || !resumed || isFinishing() || isDestroyed() || !hasWindowFocus()) return;
        try {
            boolean multiWindow = Build.VERSION.SDK_INT >= 24 && isInMultiWindowMode();
            if (!FullscreenRestore.apply(getWindow(), mFullscreenModeActive, mScreenKeyboardShown, multiWindow)) {
                startupTrace("[window] fullscreen restore skipped (" + fullscreenReason + ") full="
                    + mFullscreenModeActive + " keyboard=" + mScreenKeyboardShown + " multi=" + multiWindow, true);
                return;
            }
            applyCutoutMode(); // Retain the player's notch preference after SDL's style command.
            android.view.View decor = getWindow().getDecorView();
            startupTrace("[window] fullscreen restored (" + fullscreenReason + ") api=" + Build.VERSION.SDK_INT
                + " decor=" + decor.getWidth() + "x" + decor.getHeight()
                + " surface=" + (mSurface == null ? "none" : mSurface.getWidth() + "x" + mSurface.getHeight())
                + " insets=" + decor.getRootWindowInsets(), true);
        } catch (RuntimeException error) { Log.w(TAG, "Fullscreen restore failed", error); }
    };

    private void requestFullscreenRestore(String reason) {
        fullscreenReason = reason;
        fullscreenHandler.removeCallbacks(restoreFullscreen);
        fullscreenHandler.post(restoreFullscreen);
        // Some unlock transitions finish attaching/resizing the decor after focus returns.
        fullscreenHandler.postDelayed(restoreFullscreen, 250);
    }
    private SecondScreen secondScreen;
    // True from chooseFolder() until the picker (and the access question before it) is done.
    private static volatile boolean picking;
    private boolean waitingForAccess;
    private Intent startupRomIntent;
    private static final java.util.concurrent.atomic.AtomicBoolean openingRom = new java.util.concurrent.atomic.AtomicBoolean();
    private static volatile String pendingRom = "";
    private static final java.util.concurrent.atomic.AtomicInteger openRequest = new java.util.concurrent.atomic.AtomicInteger();

    @Override
    protected void onCreate(Bundle state) {
        self = this;
        startupTrace("Starting Android " + Build.VERSION.SDK_INT + " ABI " + java.util.Arrays.toString(Build.SUPPORTED_ABIS), false);
        try { startupTrace("Memory page size " + Os.sysconf(OsConstants._SC_PAGESIZE), true); }
        catch (Exception ignored) { }
        // SDL treats URI paths as local files. Resolve granted document URIs ourselves instead.
        if (isRomIntent(getIntent())) {
            startupRomIntent = new Intent(getIntent());
            Intent clean = new Intent(getIntent()); clean.setData(null); setIntent(clean);
        }
        super.onCreate(state);
        startupTrace("Activity created", true);
        getContentResolver().registerContentObserver(Settings.System.getUriFor(
            Settings.System.ACCELEROMETER_ROTATION), false, rotationObserver);
        secondScreen = new SecondScreen(this);
        applyScreenOrientation();
    }

    private static boolean isRomIntent(Intent intent) {
        return intent != null && Intent.ACTION_VIEW.equals(intent.getAction()) && intent.getData() != null;
    }

    private void romMessage(String message) {
        Log.w(TAG, message);
        runOnUiThread(() -> android.widget.Toast.makeText(this, message, android.widget.Toast.LENGTH_LONG).show());
    }

    private String prepareRom(Intent intent) throws Exception {
        Uri uri = intent.getData();
        if ("file".equalsIgnoreCase(uri.getScheme())) {
            File file = new File(uri.getPath()).getCanonicalFile();
            RomImport.name(file.getName());
            if (!file.isFile() || !file.canRead()) throw new java.io.IOException("ROM file cannot be read");
            return file.getPath();
        }
        if (!"content".equalsIgnoreCase(uri.getScheme())) throw new java.io.IOException("Unsupported ROM link");
        String name = null;
        try (android.database.Cursor cursor = getContentResolver().query(uri,
                new String[] { android.provider.OpenableColumns.DISPLAY_NAME }, null, null, null)) {
            if (cursor != null && cursor.moveToFirst()) name = cursor.getString(0);
        }
        RomImport.name(name);
        File own = getExternalFilesDir(null); if (own == null) own = getFilesDir();
        try (java.io.InputStream input = getContentResolver().openInputStream(uri)) {
            if (input == null) throw new java.io.IOException("ROM provider could not open the file");
            return RomImport.copy(input, uri.toString(), name, new File(own, "roms/imported")).getPath();
        }
    }

    @Override protected String[] getArguments() {
        // SDL calls this on its main/native thread, not Android's UI thread.
        Intent launch = startupRomIntent; startupRomIntent = null;
        if (launch != null) {
            openingRom.set(true);
            try { return new String[] { "--rom", prepareRom(launch) }; }
            catch (Exception error) { romMessage("Cannot open ROM: " + error.getMessage()); }
            finally { openingRom.set(false); }
        }
        return super.getArguments();
    }

    @Override protected void onNewIntent(Intent intent) {
        Intent clean = new Intent(intent); clean.setData(null);
        super.onNewIntent(clean); setIntent(clean);
        if (!isRomIntent(intent)) return;
        if (playingGame) { romMessage("Quit the current game before opening another ROM."); return; }
        if (!openingRom.compareAndSet(false, true)) { romMessage("A ROM is already being opened."); return; }
        final int request = openRequest.incrementAndGet();
        new Thread(() -> {
            try {
                String path = prepareRom(intent);
                if (self != this || request != openRequest.get()) return;
                if (playingGame) { romMessage("Quit the current game before opening another ROM."); return; }
                synchronized (GameActivity.class) { pendingRom = path; }
            } catch (Exception error) { romMessage("Cannot open ROM: " + error.getMessage()); }
            finally { openingRom.set(false); }
        }, "FullDex-open-ROM").start();
    }

    public static synchronized String takeLaunchRom() {
        String path = pendingRom; pendingRom = ""; return path;
    }

    private void startupTrace(String message, boolean append) {
        Log.i(TAG, message);
        try {
            File folder = getExternalFilesDir(null);
            if (folder == null) folder = getFilesDir();
            try (FileOutputStream out = new FileOutputStream(new File(folder, "startup.log"), append)) {
                out.write((message + "\n").getBytes(StandardCharsets.UTF_8));
            }
        } catch (Exception error) {
            Log.w(TAG, "Could not write startup.log", error);
        }
    }

    @Override
    public void loadLibraries() {
        startupTrace("Loading native libraries", true);
        try {
            super.loadLibraries();
            startupTrace("Native libraries loaded", true);
        } catch (UnsatisfiedLinkError error) {
            startupTrace("Native load failed: " + error.getMessage(), true);
            throw error;
        } catch (RuntimeException error) {
            startupTrace("Native load failed: " + error.getMessage(), true);
            throw error;
        }
    }

    // The launcher and both game engines share one activity. Keep its orientation tied to
    // the current screen, including when SDL recreates its window with a stale size/hint.
    private static volatile boolean playingGame;
    // A launcher page (Settings, Launcher, ...) is shown sideways, but it is still the launcher: it
    // turns the screen, not the camera cutout, to the game's way.
    private static volatile boolean landscapeOnly;
    // Launcher > Camera Cutout: ON keeps the launcher's screens clear of the camera hole.
    private static volatile boolean avoidCutout = true;
    private final ContentObserver rotationObserver = new ContentObserver(new Handler(Looper.getMainLooper())) {
        @Override public void onChange(boolean selfChange) { applyScreenOrientation(); }
    };

    // The launcher keeps clear of a camera hole or notch (the system leaves that strip black); a
    // game runs across the whole screen. SDL asks for the whole screen itself, so this is applied
    // again whenever the window gains focus.
    private void applyCutoutMode() {
        if (Build.VERSION.SDK_INT < 28) return;
        runOnUiThread(() -> {
            try {
                android.view.WindowManager.LayoutParams lp = getWindow().getAttributes();
                int want = playingGame || !avoidCutout
                    ? android.view.WindowManager.LayoutParams.LAYOUT_IN_DISPLAY_CUTOUT_MODE_SHORT_EDGES
                    : android.view.WindowManager.LayoutParams.LAYOUT_IN_DISPLAY_CUTOUT_MODE_NEVER;
                if (lp.layoutInDisplayCutoutMode != want) {
                    lp.layoutInDisplayCutoutMode = want;
                    getWindow().setAttributes(lp);
                }
            } catch (Exception ignored) { }
        });
    }

    @Override
    public void onWindowFocusChanged(boolean hasFocus) {
        super.onWindowFocusChanged(hasFocus);
        if (hasFocus) { applyCutoutMode(); requestFullscreenRestore("focus"); }
        else fullscreenHandler.removeCallbacks(restoreFullscreen);
    }

    private void applyScreenOrientation() {
        applyCutoutMode();
        runOnUiThread(() -> {
            boolean autoRotate = Settings.System.getInt(getContentResolver(),
                Settings.System.ACCELEROMETER_ROTATION, 0) != 0;
            setRequestedOrientation(playingGame || landscapeOnly
                ? ActivityInfo.SCREEN_ORIENTATION_SENSOR_LANDSCAPE
                : autoRotate ? ActivityInfo.SCREEN_ORIENTATION_FULL_USER
                             : ActivityInfo.SCREEN_ORIENTATION_SENSOR_LANDSCAPE);
        });
    }

    public static void showLauncherOrientation() {
        playingGame = false;
        landscapeOnly = false;
        setDsRunning(false);
        if (self != null) self.applyScreenOrientation();
    }

    public static void showGameOrientation() {
        playingGame = true;
        if (self != null) self.applyScreenOrientation();
    }

    // A page of the launcher opened on an upright phone: sideways, with the cutout kept as the launcher has it.
    public static void showPageOrientation() {
        playingGame = false;
        landscapeOnly = true;
        if (self != null) self.applyScreenOrientation();
    }

    public static void setAvoidCutout(boolean avoid) {
        avoidCutout = avoid;
        if (self != null) self.applyCutoutMode();
    }

    @Override
    public void setOrientationBis(int w, int h, boolean resizable, String hint) {
        applyScreenOrientation();
    }

    // CHECK FOR UPDATES on the game list's settings page (Updates).
    public static void checkUpdates() {
        if (self != null) Updates.check(self);
    }

    // At launch: a dialog only when a new version is available.
    public static void checkUpdatesQuiet() {
        if (self != null) Updates.check(self, true);
    }

    private static SharedPreferences prefs() {
        return self.getSharedPreferences("settings", MODE_PRIVATE);
    }

    // The ROM folder's path, "" until one is chosen.
    public static String romFolder() {
        return self == null ? "" : prefs().getString("roms", "");
    }

    private static boolean broadFileAccess() {
        if (self == null) return false;
        if (Build.VERSION.SDK_INT >= 30) return Environment.isExternalStorageManager();
        return self.checkSelfPermission(Manifest.permission.WRITE_EXTERNAL_STORAGE) == PackageManager.PERMISSION_GRANTED;
    }

    public static boolean canUseFiles() {
        if (self == null) return false;
        if (broadFileAccess()) return true;
        String chosen = romFolder();
        if (chosen.isEmpty()) return false;
        try {
            File own = self.getExternalFilesDir(null); if (own == null) own = self.getFilesDir();
            File root = own.getCanonicalFile(), folder = new File(chosen).getCanonicalFile();
            return folder.isDirectory() && folder.canRead() &&
                (folder.equals(root) || folder.getPath().startsWith(root.getPath() + File.separator));
        } catch (java.io.IOException error) { return false; }
    }

    public static boolean picking() {
        return picking;
    }

    // Asks for access to files if the app does not have it yet, then shows the folder picker.
    public static void chooseFolder() {
        if (self == null || picking) return;
        picking = true;
        self.runOnUiThread(() -> self.nextStep());
    }

    private void nextStep() {
        if (!broadFileAccess() && !waitingForAccess) {
            waitingForAccess = true;
            try {
                if (Build.VERSION.SDK_INT >= 30) {
                    try {
                        startActivity(new Intent(Settings.ACTION_MANAGE_APP_ALL_FILES_ACCESS_PERMISSION,
                                Uri.parse("package:" + getPackageName())));
                    } catch (Exception unavailable) {
                        startActivity(new Intent(Settings.ACTION_MANAGE_ALL_FILES_ACCESS_PERMISSION));
                    }
                } else {
                    requestPermissions(new String[] { Manifest.permission.READ_EXTERNAL_STORAGE,
                            Manifest.permission.WRITE_EXTERNAL_STORAGE }, ASK_FILES);
                }
                return;
            } catch (Exception error) { Log.w(TAG, "File permission settings unavailable; using document picker", error); }
        }
        // Declining broad access still permits an explicit grant from the document picker.
        waitingForAccess = false;
        Intent pick = new Intent(Intent.ACTION_OPEN_DOCUMENT_TREE);
        pick.addFlags(Intent.FLAG_GRANT_READ_URI_PERMISSION);
        pick.putExtra("android.content.extra.SHOW_ADVANCED", true);
        try { startActivityForResult(pick, PICK_FOLDER); }
        catch (Exception error) { romMessage("No folder picker is available."); picking = false; }
    }

    @Override
    protected void onResume() {
        super.onResume();
        resumed = true;
        if (secondScreen != null) secondScreen.resumed(true);
        applyScreenOrientation();
        requestFullscreenRestore("resume");
        // Back from Android's "All files access" page.
        if (picking && waitingForAccess) nextStep();
    }

    @Override
    public void onConfigurationChanged(android.content.res.Configuration configuration) {
        super.onConfigurationChanged(configuration);
        requestFullscreenRestore("configuration");
        if (secondScreen != null) secondScreen.refresh();
    }

    @Override
    protected void onPause() {
        resumed = false;
        fullscreenHandler.removeCallbacks(restoreFullscreen);
        if (secondScreen != null) secondScreen.resumed(false);
        super.onPause();
    }

    public static long secondScreenState() {
        GameActivity activity = self;
        return activity == null || activity.secondScreen == null ? 0 : activity.secondScreen.state();
    }
    public static void setSecondScreenMode(int mode) {
        GameActivity activity = self;
        if (activity != null && activity.secondScreen != null) activity.secondScreen.setMode(mode);
    }
    public static void setDsRunning(boolean running) {
        GameActivity activity = self;
        if (activity != null && activity.secondScreen != null) activity.secondScreen.running(running);
    }
    public static void submitDsFrame(int[] pixels,int width,int height) {
        GameActivity activity = self;
        if (activity != null && activity.secondScreen != null) activity.secondScreen.submit(pixels,width,height);
    }

    @Override
    protected void onDestroy() {
        resumed = false;
        fullscreenHandler.removeCallbacks(restoreFullscreen);
        if (secondScreen != null) secondScreen.close();
        getContentResolver().unregisterContentObserver(rotationObserver);
        if (self == this) self = null;
        super.onDestroy();
    }

    @Override
    public void onRequestPermissionsResult(int request, String[] permissions, int[] results) {
        if (request == ASK_FILES) {
            if (picking && waitingForAccess) nextStep();
            return;
        }
        super.onRequestPermissionsResult(request, permissions, results);
    }

    @Override
    protected void onActivityResult(int request, int result, Intent data) {
        if (request == PICK_3DS_CODE) {
            if (result == RESULT_OK && data != null && data.getData() != null)
                call3ds("importCode", new Class<?>[] { android.content.Context.class, String.class, Uri.class }, this, codeFor, data.getData());
            return;
        }
        if (request == PLAY_3DS) {
            // (Restart game: the game asks to be started again.)
            Object restart = call3ds("restartCode", new Class<?>[0]);
            if (restart != null && result == (Integer) restart) threeDsPlay(playing3ds);
            return;
        }
        if (request == PICK_FOLDER) {
            if (result == RESULT_OK && data != null && data.getData() != null) {
                final Uri tree = data.getData();
                String path = pathOf(tree);
                if (path != null && broadFileAccess()) {
                    prefs().edit().putString("roms", path).apply(); picking = false;
                    Log.i(TAG, "ROM folder: " + path);
                } else importTree(tree);
                return;
            }
            picking = false;
            return;
        }
        super.onActivityResult(request, result, data);
    }

    private void importTree(final Uri tree) {
        romMessage("Importing ROM copies into FullDex. Existing saves stay in place.");
        new Thread(() -> {
            try {
                File own = getExternalFilesDir(null); if (own == null) own = getFilesDir();
                RomTreeImport.Result result = RomTreeImport.copy(new RomTreeImport.Provider() {
                    public java.util.List<RomTreeImport.Entry> children(String id) throws java.io.IOException {
                        java.util.List<RomTreeImport.Entry> list = new java.util.ArrayList<>();
                        Uri children = DocumentsContract.buildChildDocumentsUriUsingTree(tree, id);
                        String[] columns = { DocumentsContract.Document.COLUMN_DOCUMENT_ID,
                            DocumentsContract.Document.COLUMN_DISPLAY_NAME, DocumentsContract.Document.COLUMN_MIME_TYPE };
                        try (android.database.Cursor cursor = getContentResolver().query(children, columns, null, null, null)) {
                            if (cursor == null) throw new java.io.IOException("ROM folder cannot be listed");
                            while (cursor.moveToNext()) {
                                if (list.size() >= 4096) throw new java.io.IOException("Too many files; choose a smaller folder");
                                list.add(new RomTreeImport.Entry(cursor.getString(0), cursor.getString(1),
                                    DocumentsContract.Document.MIME_TYPE_DIR.equals(cursor.getString(2))));
                            }
                        } catch (RuntimeException error) { throw new java.io.IOException("ROM folder could not be read", error); }
                        return list;
                    }
                    public java.io.InputStream open(String id) throws java.io.IOException {
                        return getContentResolver().openInputStream(DocumentsContract.buildDocumentUriUsingTree(tree, id));
                    }
                }, tree.toString(), DocumentsContract.getTreeDocumentId(tree), new File(own, "roms/folder-imports"));
                if (self != this) return;
                if (result.count == 0) romMessage("No .nds or .gba ROMs found in that folder. Your previous folder is unchanged.");
                else {
                    prefs().edit().putString("roms", result.folder.getPath()).apply();
                    romMessage("Imported " + result.count + " ROMs. Source files and saves were not changed.");
                }
            } catch (Exception error) { romMessage("ROM folder import failed: " + error.getMessage()); }
            finally { picking = false; }
        }, "FullDex-import-folder").start();
    }

    // The file path of a folder chosen in the picker: its tree id is "<volume>:<path>", e.g.
    // "primary:Download/ROMs" (the phone's own storage) or "1A2B-3C4D:ROMs" (an SD card).
    static String pathOf(Uri tree) {
        if (!"com.android.externalstorage.documents".equals(tree.getAuthority())) return null;
        String id;
        try {
            id = DocumentsContract.getTreeDocumentId(tree);
        } catch (Exception e) {
            return null;
        }
        int colon = id.indexOf(':');
        String volume = colon < 0 ? id : id.substring(0, colon);
        String rel = colon < 0 ? "" : id.substring(colon + 1);
        String base = volume.equalsIgnoreCase("primary") ? Environment.getExternalStorageDirectory().getPath()
                : "/storage/" + volume;
        try {
            java.io.File root = new java.io.File(base).getCanonicalFile();
            java.io.File folder = new java.io.File(root, rel).getCanonicalFile();
            if (!folder.equals(root) && !folder.getPath().startsWith(root.getPath() + "/")) return null;
            return folder.isDirectory() && folder.canRead() ? folder.getPath() : null;
        } catch (java.io.IOException e) { return null; }
    }

    // Starts the app again in a new process (Restart, in a process of its own, ends this one
    // and opens the app).
    // Pokemon Omega Ruby and Alpha Sapphire, inside FullDex when the Omega Ruby kit is in the app
    // (io.github.mamesj.omeganx.FullDex3ds; tools/android/build_all.sh, SSR_3DS_ANDROID_KIT). Called
    // by name, so the app builds and runs without it: then there are no 3DS games here.
    private static Object call3ds(String method, Class<?>[] types, Object... args) {
        try { return Class.forName("io.github.mamesj.omeganx.FullDex3ds").getMethod(method, types).invoke(null, args); }
        catch (Throwable missing) { return null; }
    }
    public static boolean threeDsAvailable() {
        return self != null && Boolean.TRUE.equals(call3ds("available", new Class<?>[] { android.content.Context.class }, self));
    }
    public static String threeDsNote() { Object n = call3ds("note", new Class<?>[0]); return n == null ? "" : (String) n; }
    public static boolean threeDsBusy() { return Boolean.TRUE.equals(call3ds("busy", new Class<?>[0])); }
    public static boolean threeDsBuilt(String id) {
        return self != null && Boolean.TRUE.equals(call3ds("built", new Class<?>[] { android.content.Context.class, String.class }, self, id));
    }
    public static void threeDsAdd(String path) {
        if (self != null) call3ds("add", new Class<?>[] { android.content.Context.class, String.class }, self.getApplicationContext(), path);
    }
    public static void threeDsBuild(String id) {
        if (self != null) call3ds("build", new Class<?>[] { android.content.Context.class, String.class }, self.getApplicationContext(), id);
    }
    // The game code the FullDex Builder made on a PC (<title id>.so), picked by the player.
    public static void threeDsImport(String id) {
        if (self == null) return;
        codeFor = id;
        self.runOnUiThread(() -> {
            Intent pick = new Intent(Intent.ACTION_OPEN_DOCUMENT);
            pick.addCategory(Intent.CATEGORY_OPENABLE);
            pick.setType("*/*");
            try { self.startActivityForResult(pick, PICK_3DS_CODE); }
            catch (Exception error) { Log.w(TAG, "No file picker for the game code", error); }
        });
    }
    public static void threeDsPlay(String id) {
        if (self == null) return;
        playing3ds = id;
        call3ds("play", new Class<?>[] { android.app.Activity.class, String.class, int.class }, self, id, PLAY_3DS);
    }

    public static void restart() {
        if (self == null) return;
        Intent i = new Intent(self, Restart.class);
        i.putExtra("pid", Process.myPid());
        i.addFlags(Intent.FLAG_ACTIVITY_NEW_TASK);
        self.startActivity(i);
    }
}
