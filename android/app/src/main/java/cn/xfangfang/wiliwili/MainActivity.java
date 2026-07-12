package cn.xfangfang.wiliwili;

import android.content.Intent;
import android.app.NativeActivity;
import android.net.Uri;
import android.os.Bundle;
import android.view.KeyEvent;
import android.view.View;
import android.view.WindowManager;

public class MainActivity extends NativeActivity {

    static {
        System.loadLibrary("mpv");
        System.loadLibrary("wiliwili");
    }

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);

        // Hide system UI for immersive mode on TV
        hideSystemUI();

        // Pass JavaVM to native for JNI calls
        nativeInitJNI();
    }

    @Override
    public void onWindowFocusChanged(boolean hasFocus) {
        super.onWindowFocusChanged(hasFocus);
        if (hasFocus) {
            hideSystemUI();
        }
    }

    private void hideSystemUI() {
        getWindow().getDecorView().setSystemUiVisibility(
            View.SYSTEM_UI_FLAG_IMMERSIVE_STICKY
            | View.SYSTEM_UI_FLAG_LAYOUT_STABLE
            | View.SYSTEM_UI_FLAG_LAYOUT_HIDE_NAVIGATION
            | View.SYSTEM_UI_FLAG_LAYOUT_FULLSCREEN
            | View.SYSTEM_UI_FLAG_HIDE_NAVIGATION
            | View.SYSTEM_UI_FLAG_FULLSCREEN);
    }

    @Override
    protected void onPause() {
        super.onPause();
        // Notify native code about lifecycle
        nativeOnPause();
    }

    @Override
    protected void onResume() {
        super.onResume();
        // Notify native code about lifecycle
        nativeOnResume();
    }

    /**
     * Map Android TV remote/controller keys to native input events.
     * The native side handles the actual borealis navigation.
     */
    @Override
    public boolean dispatchKeyEvent(KeyEvent event) {
        // Let the native side handle key events via NativeActivity's input queue
        return super.dispatchKeyEvent(event);
    }

    // Native lifecycle callbacks
    private native void nativeOnPause();
    private native void nativeOnResume();

    // Native JNI initialization
    private native void nativeInitJNI();

    // Called from native code via JNI
    public void openBrowser(String url) {
        Intent intent = new Intent(Intent.ACTION_VIEW, Uri.parse(url));
        startActivity(intent);
    }

    public void keepScreenOn(boolean enable) {
        runOnUiThread(() -> {
            if (enable) {
                getWindow().addFlags(WindowManager.LayoutParams.FLAG_KEEP_SCREEN_ON);
            } else {
                getWindow().clearFlags(WindowManager.LayoutParams.FLAG_KEEP_SCREEN_ON);
            }
        });
    }
}
