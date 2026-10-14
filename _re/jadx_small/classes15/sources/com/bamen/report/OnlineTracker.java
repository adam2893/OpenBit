package com.bamen.report;

import android.app.Activity;
import android.app.Application;
import android.content.Context;
import android.os.Bundle;

/* JADX INFO: loaded from: /Users/Adam/Documents/bitlife/_re/dex/classes15.dex */
public final class OnlineTracker implements Application.ActivityLifecycleCallbacks, Runnable {
    private static OnlineTracker instance;
    private final Context context;
    private volatile boolean foreground;
    private final Object lock;
    private volatile String session;
    private int startedCount;
    private Thread thread;

    private OnlineTracker(Context context) {
        Context applicationContext = context.getApplicationContext();
        if (applicationContext != null) {
            this.context = applicationContext;
        } else {
            this.context = context;
        }
        this.lock = new Object();
        this.session = "";
    }

    private void ensureWorkerLocked() {
        if (this.thread == null) {
            Thread thread = new Thread(this, "bamen-online-report");
            thread.setDaemon(true);
            this.thread = thread;
            thread.start();
        }
    }

    private boolean isForeground() {
        return this.foreground;
    }

    private void saveSession(String str) {
        if (Reporter.isValidString(str)) {
            this.session = str;
            Reporter.logSession(this.context, str);
        }
    }

    public static void start(Context context) {
        if (context != null) {
            Context applicationContext = context.getApplicationContext();
            if (applicationContext == null || !(applicationContext instanceof Application)) {
                if (!(context instanceof Application)) {
                    return;
                } else {
                    applicationContext = context;
                }
            }
            synchronized (OnlineTracker.class) {
                if (instance == null) {
                    OnlineTracker onlineTracker = new OnlineTracker(context);
                    instance = onlineTracker;
                    ((Application) applicationContext).registerActivityLifecycleCallbacks(onlineTracker);
                }
            }
        }
    }

    private void waitUntilForeground() {
        Object obj = this.lock;
        synchronized (obj) {
            while (!this.foreground) {
                obj.wait();
            }
        }
    }

    @Override // android.app.Application.ActivityLifecycleCallbacks
    public void onActivityCreated(Activity activity, Bundle bundle) {
    }

    @Override // android.app.Application.ActivityLifecycleCallbacks
    public void onActivityDestroyed(Activity activity) {
    }

    @Override // android.app.Application.ActivityLifecycleCallbacks
    public void onActivityPaused(Activity activity) {
    }

    @Override // android.app.Application.ActivityLifecycleCallbacks
    public void onActivityResumed(Activity activity) {
    }

    @Override // android.app.Application.ActivityLifecycleCallbacks
    public void onActivitySaveInstanceState(Activity activity, Bundle bundle) {
    }

    @Override // android.app.Application.ActivityLifecycleCallbacks
    public void onActivityStarted(Activity activity) {
        Object obj = this.lock;
        synchronized (obj) {
            int i = this.startedCount + 1;
            this.startedCount = i;
            if (i == 1) {
                this.foreground = true;
                ensureWorkerLocked();
                obj.notifyAll();
            }
        }
    }

    @Override // android.app.Application.ActivityLifecycleCallbacks
    public void onActivityStopped(Activity activity) {
        synchronized (this.lock) {
            int i = this.startedCount;
            if (i > 0) {
                int i2 = i - 1;
                this.startedCount = i2;
                if (i2 <= 0) {
                    this.foreground = false;
                }
            }
        }
    }

    @Override // java.lang.Runnable
    public void run() {
        while (true) {
            try {
                waitUntilForeground();
                if (isForeground()) {
                    String strReportOnline = Reporter.reportOnline(this.context, this.session);
                    if (Reporter.isValidString(strReportOnline)) {
                        saveSession(strReportOnline);
                    }
                    Thread.sleep(20000L);
                }
            } catch (Throwable unused) {
            }
        }
    }
}
