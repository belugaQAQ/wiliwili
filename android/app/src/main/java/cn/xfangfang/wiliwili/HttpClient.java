package cn.xfangfang.wiliwili;

import java.util.concurrent.TimeUnit;
import java.util.List;
import java.util.ArrayList;

import okhttp3.FormBody;
import okhttp3.Headers;
import okhttp3.MediaType;
import okhttp3.OkHttpClient;
import okhttp3.Request;
import okhttp3.RequestBody;
import okhttp3.Response;
import okhttp3.ResponseBody;

/**
 * Native-callable HTTP client backed by OkHttp.
 * <p>
 * Replaces curl/cpr on Android: curl 8.4.0's mbedTLS backend fails to
 * initialize its RNG (-0x7400 "No RNG was provided to the SSL module"),
 * so every HTTPS request died with "ssl_init failed". OkHttp uses
 * Android's platform TLS (Conscrypt/BoringSSL) whose RNG works, so
 * HTTPS just works.
 * <p>
 * Every method is synchronous and must be called from a background
 * thread (the C++ side always calls these from cpr's thread pool or
 * the image thread pool, never the UI thread).
 */
public class HttpClient {

    private static volatile OkHttpClient client;

    private static OkHttpClient client() {
        OkHttpClient c = client;
        if (c == null) {
            synchronized (HttpClient.class) {
                c = client;
                if (c == null) {
                    c = new OkHttpClient.Builder()
                            .connectTimeout(10, TimeUnit.SECONDS)
                            .readTimeout(30, TimeUnit.SECONDS)
                            .writeTimeout(30, TimeUnit.SECONDS)
                            .followRedirects(true)
                            .retryOnConnectionFailure(true)
                            .build();
                    client = c;
                }
            }
        }
        return c;
    }

    private static void applyHeaders(Request.Builder rb, String[] keys, String[] vals, String cookie) {
        if (keys != null && vals != null) {
            int n = Math.min(keys.length, vals.length);
            for (int i = 0; i < n; i++) {
                String k = keys[i];
                if (k == null || k.isEmpty()) continue;
                rb.header(k, vals[i] == null ? "" : vals[i]);
            }
        }
        if (cookie != null && !cookie.isEmpty()) {
            rb.header("Cookie", cookie);
        }
    }

    private static HttpResponse toResponse(Response r) {
        HttpResponse h = new HttpResponse();
        try {
            h.code = r.code();
            h.reason = r.message();
            Headers hh = r.headers();
            for (String name : hh.names()) {
                h.headers.put(name, hh.get(name));
            }
            // Preserve every Set-Cookie header verbatim so native code can
            // rebuild cpr::Cookies (the login flow reads response cookies).
            List<String> sc = hh.values("Set-Cookie");
            if (!sc.isEmpty()) {
                h.setCookies = sc.toArray(new String[0]);
            }
            ResponseBody b = r.body();
            if (b != null) {
                h.body = b.bytes();
                h.text = new String(h.body);
            }
        } catch (Exception e) {
            h.error = e.getMessage();
            if (h.error == null) h.error = e.getClass().getSimpleName();
        }
        return h;
    }

    /** Synchronous GET. Returns an HttpResponse (never null). */
    public static HttpResponse get(String url, String[] headerKeys, String[] headerVals, String cookie) {
        try {
            Request.Builder rb = new Request.Builder().url(url).get();
            applyHeaders(rb, headerKeys, headerVals, cookie);
            try (Response r = client().newCall(rb.build()).execute()) {
                return toResponse(r);
            }
        } catch (Exception e) {
            HttpResponse h = new HttpResponse();
            h.error = e.getMessage();
            if (h.error == null) h.error = e.getClass().getSimpleName();
            return h;
        }
    }

    /** Synchronous POST with form-encoded body. Returns an HttpResponse (never null). */
    public static HttpResponse post(String url, String[] paramKeys, String[] paramVals,
                                    String[] headerKeys, String[] headerVals, String cookie) {
        try {
            FormBody.Builder fb = new FormBody.Builder();
            if (paramKeys != null && paramVals != null) {
                int n = Math.min(paramKeys.length, paramVals.length);
                for (int i = 0; i < n; i++) {
                    if (paramKeys[i] == null) continue;
                    fb.add(paramKeys[i], paramVals[i] == null ? "" : paramVals[i]);
                }
            }
            Request.Builder rb = new Request.Builder().url(url).post(fb.build());
            applyHeaders(rb, headerKeys, headerVals, cookie);
            try (Response r = client().newCall(rb.build()).execute()) {
                return toResponse(r);
            }
        } catch (Exception e) {
            HttpResponse h = new HttpResponse();
            h.error = e.getMessage();
            if (h.error == null) h.error = e.getClass().getSimpleName();
            return h;
        }
    }

    /**
     * Synchronous POST with a raw body (analytics JSON, DLNA SOAP XML).
     * The Content-Type is taken from the caller's headers if present,
     * otherwise no Content-Type is set. Returns an HttpResponse (never null).
     */
    public static HttpResponse postRaw(String url, String body,
                                       String[] headerKeys, String[] headerVals, String cookie) {
        try {
            // Pick up Content-Type from the caller's headers so OkHttp sends
            // the right media type (e.g. "application/json" for analytics).
            MediaType mediaType = null;
            if (headerKeys != null && headerVals != null) {
                int n = Math.min(headerKeys.length, headerVals.length);
                for (int i = 0; i < n; i++) {
                    String k = headerKeys[i];
                    if (k != null && k.equalsIgnoreCase("Content-Type")) {
                        String v = headerVals[i];
                        if (v != null && !v.isEmpty()) {
                            mediaType = MediaType.parse(v);
                        }
                        break;
                    }
                }
            }
            String safeBody = body == null ? "" : body;
            RequestBody rbBody = RequestBody.create(safeBody, mediaType);
            Request.Builder rb = new Request.Builder().url(url).post(rbBody);
            applyHeaders(rb, headerKeys, headerVals, cookie);
            try (Response r = client().newCall(rb.build()).execute()) {
                return toResponse(r);
            }
        } catch (Exception e) {
            HttpResponse h = new HttpResponse();
            h.error = e.getMessage();
            if (h.error == null) h.error = e.getClass().getSimpleName();
            return h;
        }
    }
}
