package cn.xfangfang.wiliwili;

import android.os.Bundle;
import android.content.Intent;
import android.net.Uri;
import java.io.File;
import java.io.FileWriter;
import java.io.PrintWriter;
import java.text.SimpleDateFormat;
import java.util.Date;
import java.util.Locale;
import org.libsdl.app.BorealisHandler;
import org.libsdl.app.PlatformUtils;
import org.libsdl.app.SDLActivity;

public class MainActivity extends SDLActivity {

    // Write a line to wiliwili_startup.log under external app-specific
    // storage so the boot sequence can be inspected without adb logcat.
    // Mirrors the C-side wiliwili_logf() in crash_helper.cpp.
    private void bootLog(String msg) {
        String ts = new SimpleDateFormat("HH:mm:ss.SSS", Locale.US).format(new Date());
        String line = ts + " [java] " + msg;
        android.util.Log.i("wiliwili", line);
        try {
            File dir = new File(getExternalFilesDir(null), "wiliwili");
            dir.mkdirs();
            File f = new File(dir, "wiliwili_startup.log");
            PrintWriter pw = new PrintWriter(new FileWriter(f, true));
            pw.println(line);
            pw.flush();
            pw.close();
        } catch (Exception e) {
            android.util.Log.e("wiliwili", "bootLog write failed", e);
        }
    }

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        bootLog("MainActivity.onCreate entered");
        // Load native libs here so we can catch UnsatisfiedLinkError and log
        // the exact missing symbol / .so — SDLActivity loads them in
        // onCreate via getLibraries(), but its error handling just logs to
        // logcat which may not be captured.
        try {
            String[] libs = getLibraries();
            for (String lib : libs) {
                bootLog("loading native lib: lib" + lib + ".so");
                System.loadLibrary(lib);
                bootLog("loaded lib" + lib + ".so OK");
            }
        } catch (UnsatisfiedLinkError e) {
            bootLog(">>> FATAL: UnsatisfiedLinkError loading native libs <<<");
            bootLog("message: " + e.getMessage());
            // Write full stack trace to the log file.
            try {
                File dir = new File(getExternalFilesDir(null), "wiliwili");
                dir.mkdirs();
                File f = new File(dir, "wiliwili_startup.log");
                PrintWriter pw = new PrintWriter(new FileWriter(f, true));
                e.printStackTrace(pw);
                pw.flush();
                pw.close();
            } catch (Exception ignored) {}
        } catch (Throwable t) {
            bootLog(">>> FATAL: " + t.getClass().getSimpleName() + " loading native libs <<<");
            bootLog("message: " + t.getMessage());
        }

        super.onCreate(savedInstanceState);
        bootLog("MainActivity.super.onCreate done");
        // Receive brightness changes from borealis native code
        PlatformUtils.borealisHandler = new BorealisHandler();
        bootLog("MainActivity.onCreate complete");
    }

    @Override
    protected void onDestroy() {
        super.onDestroy();
        // borealis heavily uses static variables; force exit to avoid
        // state issues when the activity is recreated.
        System.exit(0);
    }

    @Override
    protected String[] getLibraries() {
        // Load SDL2 first, then the app shared library.
        // libmpv.so and FFmpeg .so files are pulled in automatically
        // by the dynamic linker as NEEDED dependencies of libwiliwili.so.
        return new String[]{
                "SDL2",
                "wiliwili"
        };
    }

    // Called from native code via JNI
    @SuppressWarnings("unused")
    public void openBrowser(String url) {
        Intent intent = new Intent(Intent.ACTION_VIEW, Uri.parse(url));
        startActivity(intent);
    }

    @SuppressWarnings("unused")
    public void keepScreenOn(boolean enable) {
        runOnUiThread(() -> {
            if (enable) {
                getWindow().addFlags(android.view.WindowManager.LayoutParams.FLAG_KEEP_SCREEN_ON);
            } else {
                getWindow().clearFlags(android.view.WindowManager.LayoutParams.FLAG_KEEP_SCREEN_ON);
            }
        });
    }
}
