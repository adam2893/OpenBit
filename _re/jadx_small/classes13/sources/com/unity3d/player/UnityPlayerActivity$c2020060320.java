package com.unity3d.player;

import androidx.fragment.app.FragmentActivity;

/* JADX INFO: loaded from: /Users/Adam/Documents/bitlife/_re/dex/classes13.dex */
public abstract class UnityPlayerActivity$c2020060320 extends FragmentActivity {
    public static void onDestroy(UnityPlayerActivity unityPlayerActivity) {
        UnityPlayerActivity.destroy$001(unityPlayerActivity.mUnityPlayer);
        UnityPlayerActivity.onDestroy$002(unityPlayerActivity);
    }
}
