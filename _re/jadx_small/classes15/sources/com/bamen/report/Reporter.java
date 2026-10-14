package com.bamen.report;

import android.content.Context;
import android.content.SharedPreferences;
import android.content.pm.PackageInfo;
import android.os.Build;
import android.os.Bundle;
import android.provider.Settings;
import android.util.Log;
import java.io.BufferedReader;
import java.io.InputStream;
import java.io.InputStreamReader;
import java.io.OutputStream;
import java.net.HttpURLConnection;
import java.net.URL;
import java.text.SimpleDateFormat;
import java.util.Date;
import java.util.Locale;
import java.util.UUID;
import org.json.JSONObject;

/* JADX INFO: loaded from: /Users/Adam/Documents/bitlife/_re/dex/classes15.dex */
public final class Reporter {
    private static final String DEFAULT_ONLINE_URL = "https://api.gamekillerapp.com/api/overseas/v1/single-game/online-log";
    private static final String DEFAULT_REPORT_URL = "https://api.gamekillerapp.com/api/overseas/v1/single-game/launch-log";

    private Reporter() {
    }

    private static String getAndroidIdSafe(Context context) {
        try {
            String string = Settings.Secure.getString(context.getContentResolver(), "android_id");
            if (isValidAndroidId(string)) {
                return string;
            }
        } catch (Throwable unused) {
        }
        try {
            SharedPreferences sharedPreferences = context.getSharedPreferences("bamen_report", 0);
            String string2 = sharedPreferences.getString("android_id_fallback", "");
            if (isValidString(string2)) {
                return string2;
            }
            String string3 = UUID.randomUUID().toString();
            sharedPreferences.edit().putString("android_id_fallback", string3).commit();
            return string3;
        } catch (Throwable unused2) {
            return "";
        }
    }

    private static String getAppNameSafe(Context context) {
        try {
            String strValueOf = String.valueOf(context.getApplicationInfo().loadLabel(context.getPackageManager()));
            if (isValidString(strValueOf)) {
                return strValueOf;
            }
        } catch (Throwable unused) {
        }
        return getPackageNameSafe(context);
    }

    private static String getClientTime() {
        try {
            return new SimpleDateFormat("yyyy-MM-dd HH:mm:ss", Locale.US).format(new Date());
        } catch (Throwable unused) {
            return String.valueOf(System.currentTimeMillis());
        }
    }

    private static long getMetaLong(Context context, String str, long j) {
        try {
            return Long.parseLong(getMetaString(context, str, String.valueOf(j)));
        } catch (Throwable unused) {
            return j;
        }
    }

    private static String getMetaString(Context context, String str, String str2) {
        Object obj;
        try {
            Bundle bundle = context.getPackageManager().getApplicationInfo(context.getPackageName(), 128).metaData;
            if (bundle != null && (obj = bundle.get(str)) != null) {
                String strValueOf = String.valueOf(obj);
                if (isValidString(strValueOf)) {
                    return strValueOf;
                }
            }
        } catch (Throwable unused) {
        }
        return str2;
    }

    private static String getPackageNameSafe(Context context) {
        try {
            String packageName = context.getPackageName();
            return isValidString(packageName) ? packageName : "";
        } catch (Throwable unused) {
            return "";
        }
    }

    private static long getVersionNoSafe(Context context) {
        try {
            PackageInfo packageInfo = context.getPackageManager().getPackageInfo(context.getPackageName(), 0);
            return Build.VERSION.SDK_INT < 28 ? packageInfo.versionCode : packageInfo.getLongVersionCode();
        } catch (Throwable unused) {
            return 0L;
        }
    }

    private static boolean isDebugLog(Context context) {
        if (context == null) {
            return false;
        }
        try {
            return Boolean.parseBoolean(getMetaString(context, "bamen.report.debugLog", "false"));
        } catch (Throwable unused) {
            return false;
        }
    }

    private static boolean isValidAndroidId(String str) {
        return isValidString(str) && !"9774d56d682e549c".equalsIgnoreCase(str);
    }

    public static boolean isValidString(String str) {
        return str != null && str.length() > 0;
    }

    public static void logDebug(Context context, String str) {
        if (context == null || str == null) {
            return;
        }
        try {
            if (isDebugLog(context)) {
                Log.d("BamenReport", str);
            }
        } catch (Throwable unused) {
        }
    }

    public static void logRequest(Context context, String str, String str2, String str3) {
        if (context != null) {
            try {
                if (isDebugLog(context)) {
                    Log.d("BamenReport", "[" + str + "] request url=" + str2 + " body=" + str3);
                }
            } catch (Throwable unused) {
            }
        }
    }

    public static void logResponse(Context context, String str, String str2) {
        if (context != null) {
            try {
                if (isDebugLog(context)) {
                    Log.d("BamenReport", "[" + str + "] response=" + str2);
                }
            } catch (Throwable unused) {
            }
        }
    }

    public static void logSession(Context context, String str) {
        if (context != null) {
            try {
                if (isDebugLog(context)) {
                    Log.d("BamenReport", "online session=" + str);
                }
            } catch (Throwable unused) {
            }
        }
    }

    private static void postJson(String str, String str2) {
        try {
            HttpURLConnection httpURLConnection = (HttpURLConnection) new URL(str).openConnection();
            httpURLConnection.setConnectTimeout(5000);
            httpURLConnection.setReadTimeout(5000);
            httpURLConnection.setRequestMethod("POST");
            httpURLConnection.setDoOutput(true);
            httpURLConnection.setRequestProperty("Content-Type", "application/json; charset=utf-8");
            byte[] bytes = str2.getBytes("UTF-8");
            httpURLConnection.setRequestProperty("Content-Length", String.valueOf(bytes.length));
            OutputStream outputStream = httpURLConnection.getOutputStream();
            outputStream.write(bytes);
            outputStream.close();
            InputStream inputStream = httpURLConnection.getResponseCode() < 400 ? httpURLConnection.getInputStream() : httpURLConnection.getErrorStream();
            if (inputStream != null) {
                while (inputStream.read(new byte[256]) != -1) {
                }
                inputStream.close();
            }
            httpURLConnection.disconnect();
        } catch (Throwable unused) {
        }
    }

    private static String postJsonForResponse(String str, String str2) {
        try {
            HttpURLConnection httpURLConnection = (HttpURLConnection) new URL(str).openConnection();
            httpURLConnection.setConnectTimeout(5000);
            httpURLConnection.setReadTimeout(5000);
            httpURLConnection.setRequestMethod("POST");
            httpURLConnection.setDoOutput(true);
            httpURLConnection.setRequestProperty("Content-Type", "application/json; charset=utf-8");
            byte[] bytes = str2.getBytes("UTF-8");
            httpURLConnection.setRequestProperty("Content-Length", String.valueOf(bytes.length));
            OutputStream outputStream = httpURLConnection.getOutputStream();
            outputStream.write(bytes);
            outputStream.close();
            InputStream inputStream = httpURLConnection.getResponseCode() < 400 ? httpURLConnection.getInputStream() : httpURLConnection.getErrorStream();
            if (inputStream == null) {
                httpURLConnection.disconnect();
                return "";
            }
            StringBuilder sb = new StringBuilder();
            BufferedReader bufferedReader = new BufferedReader(new InputStreamReader(inputStream, "UTF-8"));
            while (true) {
                String line = bufferedReader.readLine();
                if (line == null) {
                    bufferedReader.close();
                    httpURLConnection.disconnect();
                    return sb.toString();
                }
                sb.append(line);
            }
        } catch (Throwable unused) {
            return "";
        }
    }

    public static void report(Context context) {
        if (context != null) {
            try {
                JSONObject jSONObject = new JSONObject();
                jSONObject.put("platformId", 1L);
                jSONObject.put("productId", 1L);
                jSONObject.put("statisticsNo", getMetaLong(context, "bamen.report.statisticsNo", 2L));
                jSONObject.put("terminal", "android");
                jSONObject.put("language", "en");
                jSONObject.put("packageName", getPackageNameSafe(context));
                jSONObject.put("appName", getAppNameSafe(context));
                jSONObject.put("clientTime", getClientTime());
                jSONObject.put("androidId", getAndroidIdSafe(context));
                String string = jSONObject.toString();
                logRequest(context, "launch", DEFAULT_REPORT_URL, string);
                logResponse(context, "launch", postJsonForResponse(DEFAULT_REPORT_URL, string));
            } catch (Throwable unused) {
            }
        }
    }

    public static String reportOnline(Context context, String str) {
        JSONObject jSONObjectOptJSONObject;
        if (context != null) {
            try {
                JSONObject jSONObject = new JSONObject();
                jSONObject.put("platformId", 1L);
                jSONObject.put("productId", 1L);
                jSONObject.put("statisticsNo", getMetaLong(context, "bamen.report.statisticsNo", 2L));
                jSONObject.put("terminal", "android");
                jSONObject.put("language", "en");
                jSONObject.put("androidId", getAndroidIdSafe(context));
                jSONObject.put("packageName", getPackageNameSafe(context));
                jSONObject.put("appName", getAppNameSafe(context));
                if (isValidString(str)) {
                    jSONObject.put("session", str);
                }
                String string = jSONObject.toString();
                logRequest(context, "online", DEFAULT_ONLINE_URL, string);
                String strPostJsonForResponse = postJsonForResponse(DEFAULT_ONLINE_URL, string);
                logResponse(context, "online", strPostJsonForResponse);
                if (isValidString(strPostJsonForResponse)) {
                    JSONObject jSONObject2 = new JSONObject(strPostJsonForResponse);
                    if (jSONObject2.optInt("status", 0) == 1 && (jSONObjectOptJSONObject = jSONObject2.optJSONObject("content")) != null) {
                        String strOptString = jSONObjectOptJSONObject.optString("session", "");
                        if (isValidString(strOptString)) {
                            return strOptString;
                        }
                    }
                }
            } catch (Throwable unused) {
            }
        }
        return str;
    }

    public static void start(Context context) {
        if (context != null) {
            try {
                new Thread(new ReportRunnable(context)).start();
                OnlineTracker.start(context);
            } catch (Throwable unused) {
            }
        }
    }
}
