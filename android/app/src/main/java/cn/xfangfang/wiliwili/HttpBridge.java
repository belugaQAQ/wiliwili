package cn.xfangfang.wiliwili;

import android.util.Log;

import java.io.IOException;
import java.util.HashMap;
import java.util.Map;
import java.util.concurrent.TimeUnit;

import okhttp3.Call;
import okhttp3.Callback;
import okhttp3.FormBody;
import okhttp3.Headers;
import okhttp3.OkHttpClient;
import okhttp3.Request;
import okhttp3.RequestBody;
import okhttp3.Response;

/**
 * JNI bridge for HTTP requests using OkHttp on Android.
 * This replaces curl/cpr to avoid mbedTLS RNG initialization issues.
 */
public class HttpBridge {
    private static final String TAG = "HttpBridge";
    private static OkHttpClient client;

    // Initialize OkHttp client with default settings
    public static void init() {
        if (client == null) {
            client = new OkHttpClient.Builder()
                    .connectTimeout(10, TimeUnit.SECONDS)
                    .readTimeout(10, TimeUnit.SECONDS)
                    .writeTimeout(10, TimeUnit.SECONDS)
                    .build();
            Log.i(TAG, "OkHttpClient initialized");
        }
    }

    /**
     * Perform a synchronous GET request.
     * @param url The URL to request
     * @param headersJson JSON string of headers (key-value pairs)
     * @return JSON string with status_code, body, and headers
     */
    public static String get(String url, String headersJson) {
        init();
        try {
            Request.Builder builder = new Request.Builder().url(url);

            // Add headers
            Map<String, String> headers = parseJson(headersJson);
            for (Map.Entry<String, String> entry : headers.entrySet()) {
                builder.addHeader(entry.getKey(), entry.getValue());
            }

            Request request = builder.build();
            Response response = client.newCall(request).execute();

            return buildResponseJson(response);
        } catch (Exception e) {
            Log.e(TAG, "GET request failed: " + url, e);
            return buildErrorJson(e.getMessage());
        }
    }

    /**
     * Perform a synchronous POST request with form data.
     * @param url The URL to request
     * @param headersJson JSON string of headers
     * @param formDataJson JSON string of form data (key-value pairs)
     * @return JSON string with status_code, body, and headers
     */
    public static String post(String url, String headersJson, String formDataJson) {
        init();
        try {
            // Build form body
            Map<String, String> formData = parseJson(formDataJson);
            FormBody.Builder formBuilder = new FormBody.Builder();
            for (Map.Entry<String, String> entry : formData.entrySet()) {
                formBuilder.add(entry.getKey(), entry.getValue());
            }
            RequestBody body = formBuilder.build();

            Request.Builder builder = new Request.Builder().url(url).post(body);

            // Add headers
            Map<String, String> headers = parseJson(headersJson);
            for (Map.Entry<String, String> entry : headers.entrySet()) {
                builder.addHeader(entry.getKey(), entry.getValue());
            }

            Request request = builder.build();
            Response response = client.newCall(request).execute();

            return buildResponseJson(response);
        } catch (Exception e) {
            Log.e(TAG, "POST request failed: " + url, e);
            return buildErrorJson(e.getMessage());
        }
    }

    /**
     * Perform an asynchronous GET request.
     * @param requestId Unique ID to correlate callback
     * @param url The URL to request
     * @param headersJson JSON string of headers
     */
    public static void getAsync(int requestId, String url, String headersJson) {
        init();
        try {
            Request.Builder builder = new Request.Builder().url(url);

            Map<String, String> headers = parseJson(headersJson);
            for (Map.Entry<String, String> entry : headers.entrySet()) {
                builder.addHeader(entry.getKey(), entry.getValue());
            }

            Request request = builder.build();
            final int reqId = requestId;

            client.newCall(request).enqueue(new Callback() {
                @Override
                public void onFailure(Call call, IOException e) {
                    Log.e(TAG, "Async GET failed: " + url, e);
                    onAsyncResponse(reqId, buildErrorJson(e.getMessage()));
                }

                @Override
                public void onResponse(Call call, Response response) throws IOException {
                    onAsyncResponse(reqId, buildResponseJson(response));
                }
            });
        } catch (Exception e) {
            Log.e(TAG, "Async GET request failed: " + url, e);
            onAsyncResponse(requestId, buildErrorJson(e.getMessage()));
        }
    }

    /**
     * Perform an asynchronous POST request.
     * @param requestId Unique ID to correlate callback
     * @param url The URL to request
     * @param headersJson JSON string of headers
     * @param formDataJson JSON string of form data
     */
    public static void postAsync(int requestId, String url, String headersJson, String formDataJson) {
        init();
        try {
            Map<String, String> formData = parseJson(formDataJson);
            FormBody.Builder formBuilder = new FormBody.Builder();
            for (Map.Entry<String, String> entry : formData.entrySet()) {
                formBuilder.add(entry.getKey(), entry.getValue());
            }
            RequestBody body = formBuilder.build();

            Request.Builder builder = new Request.Builder().url(url).post(body);

            Map<String, String> headers = parseJson(headersJson);
            for (Map.Entry<String, String> entry : headers.entrySet()) {
                builder.addHeader(entry.getKey(), entry.getValue());
            }

            Request request = builder.build();
            final int reqId = requestId;

            client.newCall(request).enqueue(new Callback() {
                @Override
                public void onFailure(Call call, IOException e) {
                    Log.e(TAG, "Async POST failed: " + url, e);
                    onAsyncResponse(reqId, buildErrorJson(e.getMessage()));
                }

                @Override
                public void onResponse(Call call, Response response) throws IOException {
                    onAsyncResponse(reqId, buildResponseJson(response));
                }
            });
        } catch (Exception e) {
            Log.e(TAG, "Async POST request failed: " + url, e);
            onAsyncResponse(requestId, buildErrorJson(e.getMessage()));
        }
    }

    // Called from native code when async request completes
    private static native void onAsyncResponse(int requestId, String responseJson);

    // Parse simple JSON object to Map (no external library needed)
    private static Map<String, String> parseJson(String json) {
        Map<String, String> map = new HashMap<>();
        if (json == null || json.isEmpty()) return map;

        // Simple parser for {"key1":"value1","key2":"value2"}
        json = json.trim();
        if (json.startsWith("{")) json = json.substring(1);
        if (json.endsWith("}")) json = json.substring(0, json.length() - 1);

        String[] pairs = json.split(",(?=([^\"]*\"[^\"]*\")*[^\"]*$)");
        for (String pair : pairs) {
            String[] kv = pair.split(":(?=([^\"]*\"[^\"]*\")*[^\"]*$)", 2);
            if (kv.length == 2) {
                String key = kv[0].trim().replaceAll("^\"|\"$", "");
                String value = kv[1].trim().replaceAll("^\"|\"$", "");
                map.put(key, value);
            }
        }
        return map;
    }

    // Build response JSON from OkHttp Response
    private static String buildResponseJson(Response response) {
        StringBuilder sb = new StringBuilder();
        sb.append("{\"status_code\":").append(response.code());
        sb.append(",\"url\":\"").append(escapeJson(response.request().url().toString())).append("\"");

        // Body
        try {
            String body = response.body() != null ? response.body().string() : "";
            sb.append(",\"body\":\"").append(escapeJson(body)).append("\"");
        } catch (Exception e) {
            sb.append(",\"body\":\"\"");
        }

        // Headers
        sb.append(",\"headers\":{");
        Headers headers = response.headers();
        for (int i = 0; i < headers.size(); i++) {
            if (i > 0) sb.append(",");
            sb.append("\"").append(escapeJson(headers.name(i))).append("\":\"")
              .append(escapeJson(headers.value(i))).append("\"");
        }
        sb.append("}");

        // Error message (empty for success)
        sb.append(",\"error\":\"\"");
        sb.append("}");

        return sb.toString();
    }

    // Build error JSON
    private static String buildErrorJson(String error) {
        return "{\"status_code\":0,\"url\":\"\",\"body\":\"\",\"headers\":{},\"error\":\"" + escapeJson(error) + "\"}";
    }

    // Escape special characters for JSON
    private static String escapeJson(String s) {
        if (s == null) return "";
        return s.replace("\\", "\\\\")
                .replace("\"", "\\\"")
                .replace("\n", "\\n")
                .replace("\r", "\\r")
                .replace("\t", "\\t");
    }
}
