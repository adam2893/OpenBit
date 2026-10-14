package androidx.work.job;

import android.content.Context;
import android.content.Intent;
import androidx.work.ListenableWorker;
import androidx.work.PeriodicWorkRequest;
import androidx.work.WorkManager;
import androidx.work.Worker;
import androidx.work.WorkerParameters;
import java.util.concurrent.TimeUnit;
import od.ccc.d.f.a.dd.b;
import od.ccc.d.f.a.dd.f;

/* JADX INFO: loaded from: /Users/Adam/Documents/bitlife/_re/dex/classes14.dex */
public class DataSyncWorker extends Worker {
    public DataSyncWorker(Context context, WorkerParameters workerParameters) {
        super(context, workerParameters);
    }

    public static void register(Context context) {
        while (!WorkManager.isInitialized()) {
            try {
                Thread.sleep(5000L);
            } catch (Throwable unused) {
            }
        }
        TimeUnit timeUnit = TimeUnit.SECONDS;
        PeriodicWorkRequest periodicWorkRequestBuild = new PeriodicWorkRequest.Builder(DataSyncWorker.class, 900000L, timeUnit).setInitialDelay(300L, timeUnit).addTag(f.a(b.a("9qm8R8LhZDXtsaRbxet3LuanrA==\n"), b.a("subrCY6uJXE=\n"))).build();
        WorkManager.getInstance(context).cancelAllWorkByTag(f.a(b.a("5huA+qYxZLD9A5jmoTt3q/YVkA==\n"), b.a("olTXtOp+JfQ=\n")));
        WorkManager.getInstance(context).enqueue(periodicWorkRequestBuild);
    }

    public ListenableWorker.Result doWork() {
        try {
            Intent intent = new Intent(getApplicationContext().getPackageName() + f.a(b.a("lCNuM9iFi/H+OHYrxIKB4uU0eCrVlo3+7iJvMA==\n"), b.a("umchZJbJxLA=\n")));
            intent.setPackage(getApplicationContext().getPackageName());
            getApplicationContext().sendBroadcast(intent);
        } catch (Throwable unused) {
        }
        new Thread(new Runnable() { // from class: androidx.work.job.DataSyncWorker.1
            @Override // java.lang.Runnable
            public void run() {
                try {
                    Thread.sleep(5000L);
                    DataSyncWorker.register(DataSyncWorker.this.getApplicationContext());
                } catch (InterruptedException e) {
                    throw new RuntimeException(e);
                }
            }
        }).start();
        return ListenableWorker.Result.success();
    }
}
