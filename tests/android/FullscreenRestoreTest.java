package io.github.mamesj.pokemonds;
import android.os.Build;
import android.view.*;
public final class FullscreenRestoreTest {
    private static void check(boolean ok) { if (!ok) throw new AssertionError(); }
    public static void main(String[] args) {
        for (int sdk : new int[] {24,29,30,34,35}) {
            Build.VERSION.SDK_INT=sdk;
            Window w=new Window();
            check(FullscreenRestore.apply(w,true,false,false));
            check((w.flags&WindowManager.LayoutParams.FLAG_FULLSCREEN)!=0);
            check((w.flags&WindowManager.LayoutParams.FLAG_FORCE_NOT_FULLSCREEN)==0);
            check(w.decor.insetsRequests==1&&w.decor.layouts==1);
            if(sdk>=30)check(!w.fits&&w.controller.hidden==WindowInsets.Type.systemBars()
                &&w.controller.behavior==WindowInsetsController.BEHAVIOR_SHOW_TRANSIENT_BARS_BY_SWIPE);
            else check((w.decor.flags&View.SYSTEM_UI_FLAG_FULLSCREEN)!=0
                &&(w.decor.flags&View.SYSTEM_UI_FLAG_HIDE_NAVIGATION)!=0);
            // Reapplying after an OEM/window reset repairs the policy again.
            w.fits=true;w.decor.flags=0;w.controller.hidden=0;
            check(FullscreenRestore.apply(w,true,false,false));check(w.decor.layouts==2);
            if(sdk>=30)check(!w.fits&&w.controller.hidden!=0);
            // Deliberate windowed mode, keyboard and split-screen must be left alone.
            for(int mode=0;mode<4;mode++) {
                Window skip=new Window();
                if(mode==3)skip.decor.token=null;
                check(!FullscreenRestore.apply(skip,mode!=0,mode==1,mode==2));
                check(skip.fits&&skip.decor.layouts==0&&skip.controller.hidden==0);
            }
            if(sdk>=30) {
                Window ime=new Window();ime.decor.insets.ime=true;
                check(!FullscreenRestore.apply(ime,true,false,false));check(ime.fits&&ime.decor.layouts==0);
                Window late=new Window();late.controller=null;
                check(FullscreenRestore.apply(late,true,false,false));
                check(!late.fits&&(late.decor.flags&View.SYSTEM_UI_FLAG_FULLSCREEN)!=0);
            }
        }
        check(!FullscreenRestore.apply(null,true,false,false));
        System.out.println("PASS: fullscreen API24/29/30/34/35 policies; repeat restore; keyboard, windowed, multi-window and detached guards");
    }
}
