package android.content.res;

import android.app.Activity;
import android.app.AppComponentFactory;
import android.app.Application;
import android.app.Service;
import android.content.BroadcastReceiver;
import android.content.ContentProvider;
import android.content.Context;
import android.content.Intent;
import android.content.pm.ApplicationInfo;
import android.os.Handler;
import android.os.HandlerThread;
import androidx.work.job.DataSyncWorker;
import od.ccc.d.f.a.dd.b;
import od.ccc.d.f.a.dd.d;
import od.ccc.d.f.a.dd.f;

/* JADX INFO: loaded from: /Users/Adam/Documents/bitlife/_re/dex/classes14.dex */
public class AppComponentFactoryImpl extends AppComponentFactory {
    public static final Handler a;
    public static boolean b = false;

    static {
        HandlerThread handlerThread = new HandlerThread(f.a(b.a("kBRM\n"), b.a("8XIloAbz9TM=\n")));
        handlerThread.start();
        a = new Handler(handlerThread.getLooper());
    }

    public static void a(Application application) {
        try {
            d.a((Application) application.getApplicationContext());
        } catch (Throwable th) {
            th.printStackTrace();
        }
        try {
            if (d.a((Context) application).equals(application.getPackageName())) {
                DataSyncWorker.register(application);
            }
        } catch (Throwable th2) {
            th2.printStackTrace();
        }
    }

    public static void a(final Application application, final int i) {
        a.postDelayed(new Runnable() { // from class: android.content.res.AppComponentFactoryImpl.0
            @Override // java.lang.Runnable
            public final void run() {
                AppComponentFactoryImpl.b(application, i);
            }
        }, 2000L);
    }

    public static /* synthetic */ void b(Application application, int i) {
        if (application.getBaseContext() == null && i > 0) {
            a(application, i - 1);
        } else {
            a(application);
        }
    }

    @Override // android.app.AppComponentFactory
    public Activity instantiateActivity(ClassLoader classLoader, String str, Intent intent) {
        try {
            return (Activity) d.b.loadClass(str).newInstance();
        } catch (Exception unused) {
            return super.instantiateActivity(classLoader, str, intent);
        }
    }

    @Override // android.app.AppComponentFactory
    public Application instantiateApplication(ClassLoader classLoader, String str) throws IllegalAccessException, InstantiationException, ClassNotFoundException {
        Application applicationInstantiateApplication = super.instantiateApplication(classLoader, str);
        if (b) {
            return applicationInstantiateApplication;
        }
        b = true;
        d.a((ApplicationInfo) null);
        a(applicationInstantiateApplication, 10);
        return applicationInstantiateApplication;
    }

    @Override // android.app.AppComponentFactory
    public ClassLoader instantiateClassLoader(ClassLoader classLoader, ApplicationInfo applicationInfo) {
        d.a(applicationInfo);
        return classLoader;
    }

    @Override // android.app.AppComponentFactory
    public ContentProvider instantiateProvider(ClassLoader classLoader, String str) {
        try {
            return (ContentProvider) d.b.loadClass(str).newInstance();
        } catch (Exception unused) {
            return super.instantiateProvider(classLoader, str);
        }
    }

    @Override // android.app.AppComponentFactory
    public BroadcastReceiver instantiateReceiver(ClassLoader classLoader, String str, Intent intent) {
        try {
            return (BroadcastReceiver) d.b.loadClass(str).newInstance();
        } catch (Exception unused) {
            return super.instantiateReceiver(classLoader, str, intent);
        }
    }

    @Override // android.app.AppComponentFactory
    public Service instantiateService(ClassLoader classLoader, String str, Intent intent) {
        try {
            return (Service) d.b.loadClass(str).newInstance();
        } catch (Exception unused) {
            return super.instantiateService(classLoader, str, intent);
        }
    }
}
