"""Execute the real window-policy helper against small framework stand-ins, not a device."""
from pathlib import Path
import shutil,subprocess,tempfile,unittest
ROOT=Path(__file__).resolve().parents[1]
STUBS={
'android/os/Build.java':'package android.os; public class Build {public static class VERSION {public static int SDK_INT=34;}}',
'android/view/WindowManager.java':'package android.view; public class WindowManager {public static class LayoutParams {public static final int FLAG_FULLSCREEN=1024,FLAG_FORCE_NOT_FULLSCREEN=2048;}}',
'android/view/WindowInsets.java':'package android.view; public class WindowInsets {public boolean ime=false; public boolean isVisible(int type){return ime;} public static class Type {public static int ime(){return 8;} public static int systemBars(){return 7;}}}',
'android/view/WindowInsetsController.java':'package android.view; public class WindowInsetsController {public static final int BEHAVIOR_SHOW_TRANSIENT_BARS_BY_SWIPE=2; public int hidden,behavior; public void hide(int types){hidden=types;} public void setSystemBarsBehavior(int value){behavior=value;}}',
'android/view/View.java':'''package android.view; public class View {
public static final int SYSTEM_UI_FLAG_FULLSCREEN=4,SYSTEM_UI_FLAG_HIDE_NAVIGATION=2,SYSTEM_UI_FLAG_IMMERSIVE_STICKY=4096,SYSTEM_UI_FLAG_LAYOUT_FULLSCREEN=1024,SYSTEM_UI_FLAG_LAYOUT_HIDE_NAVIGATION=512,SYSTEM_UI_FLAG_LAYOUT_STABLE=256;
public Object token=new Object();public int flags,insetsRequests,layouts;public WindowInsets insets=new WindowInsets();
public Object getWindowToken(){return token;}public WindowInsets getRootWindowInsets(){return insets;}
public int getSystemUiVisibility(){return flags;}public void setSystemUiVisibility(int value){flags=value;}
public void requestApplyInsets(){insetsRequests++;}public void requestLayout(){layouts++;}}''',
'android/view/Window.java':'''package android.view; public class Window {
public View decor=new View();public boolean fits=true;public int flags=2048;public WindowInsetsController controller=new WindowInsetsController();
public View getDecorView(){return decor;}public void addFlags(int value){flags|=value;}public void clearFlags(int value){flags&=~value;}
public void setDecorFitsSystemWindows(boolean value){fits=value;}public WindowInsetsController getInsetsController(){return controller;}}'''
}
class AndroidFullscreenTests(unittest.TestCase):
    @unittest.skipUnless(shutil.which('javac') and shutil.which('java'),'Java tools required')
    def test_resume_window_policy_preserves_keyboard_and_windowed_modes(self):
        with tempfile.TemporaryDirectory() as folder:
            temp=Path(folder);files=[]
            for name,text in STUBS.items():
                p=temp/name;p.parent.mkdir(parents=True,exist_ok=True);p.write_text(text);files.append(str(p))
            files.extend([str(ROOT/'tools/android/allgames/java/io/github/mamesj/pokemonds/FullscreenRestore.java'),str(ROOT/'tests/android/FullscreenRestoreTest.java')])
            subprocess.run(['javac','-d',folder,*files],check=True)
            subprocess.run(['java','-cp',folder,'io.github.mamesj.pokemonds.FullscreenRestoreTest'],check=True)
