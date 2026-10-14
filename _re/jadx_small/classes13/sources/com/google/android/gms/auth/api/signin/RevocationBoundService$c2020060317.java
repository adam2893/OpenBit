package com.google.android.gms.auth.api.signin;

import android.app.Service;
import android.content.Intent;
import android.os.IBinder;
import android.util.Log;
import com.google.android.gms.auth.api.signin.internal.zbt;

/* JADX INFO: loaded from: /Users/Adam/Documents/bitlife/_re/dex/classes13.dex */
public abstract class RevocationBoundService$c2020060317 extends Service {
    public static IBinder onBind(RevocationBoundService revocationBoundService, Intent intent) {
        if (!RevocationBoundService.equals$002("com.google.android.gms.auth.api.signin.RevocationBoundService.disconnect", RevocationBoundService.getAction$001(intent)) && !RevocationBoundService.equals$004("com.google.android.gms.auth.api.signin.RevocationBoundService.clearClientState", RevocationBoundService.getAction$003(intent))) {
            Log.w("RevocationService", RevocationBoundService.concat$006("Unknown action sent to RevocationBoundService: ", String.valueOf(RevocationBoundService.getAction$005(intent))));
            return null;
        }
        if (Log.isLoggable("RevocationService", 2)) {
            Log.v("RevocationService", RevocationBoundService.concat$008("RevocationBoundService handling ", String.valueOf(RevocationBoundService.getAction$007(intent))));
        }
        return new zbt(revocationBoundService);
    }
}
