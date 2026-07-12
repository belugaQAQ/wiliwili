# Keep native methods
-keepclasseswithmembernames class * {
    native <methods>;
}

# Keep NativeActivity
-keep class * extends android.app.NativeActivity {
    *;
}
