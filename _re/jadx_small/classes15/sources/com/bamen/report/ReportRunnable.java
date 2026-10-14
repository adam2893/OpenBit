package com.bamen.report;

import android.content.Context;

/* JADX INFO: loaded from: /Users/Adam/Documents/bitlife/_re/dex/classes15.dex */
public final class ReportRunnable implements Runnable {
    private final Context context;

    public ReportRunnable(Context context) {
        Context applicationContext = context.getApplicationContext();
        if (applicationContext != null) {
            this.context = applicationContext;
        } else {
            this.context = context;
        }
    }

    @Override // java.lang.Runnable
    public void run() {
        Reporter.report(this.context);
    }
}
