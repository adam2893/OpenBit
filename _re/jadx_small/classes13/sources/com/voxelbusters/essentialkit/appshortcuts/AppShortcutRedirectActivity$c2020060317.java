package com.voxelbusters.essentialkit.appshortcuts;

import android.app.Activity;
import android.content.Intent;
import android.os.Bundle;
import android.util.Log;

/* JADX INFO: loaded from: /Users/Adam/Documents/bitlife/_re/dex/classes13.dex */
public abstract class AppShortcutRedirectActivity$c2020060317 extends Activity {
    public static void onCreate(AppShortcutRedirectActivity appShortcutRedirectActivity, Bundle bundle) {
        AppShortcutRedirectActivity.onCreate$001(appShortcutRedirectActivity, bundle);
        Intent intent$002 = AppShortcutRedirectActivity.getIntent$002(appShortcutRedirectActivity);
        String action$003 = AppShortcutRedirectActivity.getAction$003(intent$002);
        String stringExtra$004 = AppShortcutRedirectActivity.getStringExtra$004(intent$002, "identifier");
        Log.d("AppShortcutsActivity", AppShortcutRedirectActivity.toString$006(AppShortcutRedirectActivity.append$005(new StringBuilder("Action : "), action$003)));
        Log.d("AppShortcutsActivity", AppShortcutRedirectActivity.toString$008(AppShortcutRedirectActivity.append$007(new StringBuilder("Identifier : "), stringExtra$004)));
        AppShortcuts.setLaunchedShortcut(stringExtra$004);
        AppShortcutRedirectActivity.launchMainActivity$009(appShortcutRedirectActivity);
        AppShortcutRedirectActivity.finish$010(appShortcutRedirectActivity);
    }
}
