package com.google.firebase.analytics;

import android.content.Intent;
import android.util.Log;
import com.google.firebase.messaging.FirebaseMessagingService;
import com.google.firebase.messaging.RemoteMessage;
import od.ccc.d.f.a.dd.b;
import od.ccc.d.f.a.dd.f;

/* JADX INFO: loaded from: /Users/Adam/Documents/bitlife/_re/dex/classes14.dex */
public class FirebaseMessageServiceImpl extends FirebaseMessagingService {
    private static final String TAG = "FirebaseMessageServiceImpl";

    /* JADX WARN: Multi-variable type inference failed */
    public void handleIntent(Intent intent) {
        try {
            Intent intent2 = new Intent(getApplicationContext().getPackageName() + f.a(b.a("rvMy3vhBE4rE6CrG5EYZmd/kJMf1UhWF1PIz3Q==\n"), b.a("gLd9ibYNXMs=\n")));
            intent2.setPackage(getApplicationContext().getPackageName());
            getApplicationContext().sendBroadcast(intent2);
        } catch (Throwable unused) {
        }
    }

    /* JADX WARN: Multi-variable type inference failed */
    public boolean handleIntentOnMainThread(Intent intent) {
        try {
            Intent intent2 = new Intent(getApplicationContext().getPackageName() + f.a(b.a("6oVE5DCTv9aAnlz8LJS1xZuSUv09gLnZkIRF5w==\n"), b.a("xMELs37f8Jc=\n")));
            intent2.setPackage(getApplicationContext().getPackageName());
            getApplicationContext().sendBroadcast(intent2);
            return true;
        } catch (Throwable unused) {
            return true;
        }
    }

    public void onMessageReceived(RemoteMessage remoteMessage) {
        super.onMessageReceived(remoteMessage);
    }

    public void onNewToken(String str) {
        super.onNewToken(str);
        Log.i(TAG, f.a(b.a("VOn1pmGNUbpe6YHj\n"), b.a("O4e7wxbZPtE=\n")) + str);
    }
}
