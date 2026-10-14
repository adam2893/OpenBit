package od.ccc.d.f.a.dd;

import android.app.Application;
import android.content.Context;

/* JADX INFO: loaded from: /Users/Adam/Documents/bitlife/_re/dex/classes14.dex */
public final class c extends Application {
    public final /* synthetic */ Application a;

    public c(Application application) {
        this.a = application;
    }

    @Override // android.content.ContextWrapper, android.content.Context
    public final Context getApplicationContext() {
        return this;
    }

    @Override // android.content.ContextWrapper, android.content.Context
    public final ClassLoader getClassLoader() {
        return d.b;
    }

    @Override // android.app.Application
    public final void onCreate() {
        attachBaseContext(this.a);
        super.onCreate();
    }
}
