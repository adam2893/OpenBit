package com.unity3d.player;

import android.os.Bundle;
import androidx.fragment.app.FragmentActivity;

/* JADX INFO: loaded from: /Users/Adam/Documents/bitlife/_re/dex/classes13.dex */
public abstract class UnityPlayerActivity$c2020060322 extends FragmentActivity {
    public static void onCreate(UnityPlayerActivity unityPlayerActivity, Bundle bundle) {
        UnityPlayerActivity.requestWindowFeature$001(unityPlayerActivity, 1);
        UnityPlayerActivity.onCreate$002(unityPlayerActivity, bundle);
        UnityPlayerActivity.putExtra$007(UnityPlayerActivity.getIntent$006(unityPlayerActivity), "unity", UnityPlayerActivity.updateUnityCommandLineArguments$005(unityPlayerActivity, UnityPlayerActivity.getStringExtra$004(UnityPlayerActivity.getIntent$003(unityPlayerActivity), "unity")));
        UnityPlayerForActivityOrService unityPlayerForActivityOrService = new UnityPlayerForActivityOrService(unityPlayerActivity, unityPlayerActivity);
        unityPlayerActivity.mUnityPlayer = unityPlayerForActivityOrService;
        UnityPlayerActivity.setContentView$009(unityPlayerActivity, UnityPlayerActivity.getFrameLayout$008(unityPlayerForActivityOrService));
        UnityPlayerActivity.requestFocus$011(UnityPlayerActivity.getFrameLayout$010(unityPlayerActivity.mUnityPlayer));
    }
}
