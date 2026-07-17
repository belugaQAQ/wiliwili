package cn.xfangfang.wiliwili;

import android.os.Bundle;
import android.content.Intent;
import android.net.Uri;
import org.libsdl.app.BorealisHandler;
import org.libsdl.app.PlatformUtils;
import org.libsdl.app.SDLActivity;

public class MainActivity extends SDLActivity {

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        // Receive brightness changes from borealis native code
        PlatformUtils.borealisHandler = new BorealisHandler();
        // Initialize HTTP bridge for native code (uses OkHttp on Android)
        HttpBridge.init();
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
