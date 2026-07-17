package cn.xfangfang.wiliwili;

import java.util.HashMap;
import java.util.Map;

/**
 * Plain data holder returned by HttpClient to native code via JNI.
 * Fields are accessed directly by name from C++ (android_http.cpp).
 */
public class HttpResponse {
    public int code;
    public String reason;
    public byte[] body;
    public String text;
    public String error;
    public Map<String, String> headers = new HashMap<>();
    /** Raw "Set-Cookie" header values (one entry per Set-Cookie header). */
    public String[] setCookies;
}
