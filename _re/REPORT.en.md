# BitLife 3.25.1 mod APK — Reverse Engineering Report

Target: /Users/Adam/Documents/bitlife/BitLife-Life-Simulator-v3.25.1-mod_sdk.apk
SHA-256: c2111dee01a5d28f0a5f6cfb61bcf2d513a53d8f8ef0f530265da9240f215c1d
Size: 317,793,590 B / zip 2786 entries / 435,325,355 B uncompressed
Package com.candywriter.bitlife · versionCode 1063 · versionName 3.25.1 · minSdk 28 · targetSdk/compileSdk 35 · installLocation preferExternal
Tooling: jadx 1.5.6, radare2/rabin2, REA MCP (Ghidra 12.1.2 provider), python3; working dir /Users/Adam/Documents/bitlife/_re

---

## Verdict in one line

This is not "the game with edited values". It is **stock Unity IL2CPP BitLife 3.25.1 + a public mod-menu template v5.3 (LiteApks / 9MOD channel) + the PairIP wrapper + signature-check bypass + third-party telemetry** stapled together. No static modification of game logic was found; gameplay changes are injected at runtime by native hooks.

Labels: [obs] = bytes/decompiled output read directly; [inf] = conclusion supported by observations; [unk] = unverified.

---

## 1. Launch chain (who runs, and what they run)

1. [obs] Manifest application class = com.pairip.application.Application (AndroidManifest.xml:153-159).
2. [obs] Source of that class (dex/classes2.dex):
   ```java
   public class Application extends killsign {
       static { StartupLauncher.launch(); }
       protected void attachBaseContext(Context context) {
           VMRunner.setContext(context);
           SignatureCheck.verifyIntegrity(context);   // PairIP integrity check
           super.attachBaseContext(context);
       }
   }
   ```
   The wrapper class **extends** the injected com.android.support.killsign (ApkSignatureKillerEx).
3. [obs] Manifest launcher activity = com.unity3d.player.UnityPlayerActivity (one of 79 activities).
4. [obs] In classes10.dex, UnityPlayerActivity.onCreate is rewritten (the only call site; /Users/Adam/Documents/bitlife/_re/jadx_c10/sources/com/unity3d/player/UnityPlayerActivity.java):
   ```java
   protected void onCreate(Bundle bundle) {
       ModPlayerprefs.Modded(this);                 // (1) load the mod
       UtLeJu.VpvSv.invoke(null, this, bundle);     // (2) original Unity init moved behind reflection
   }
   ```
   The other lifecycle methods (onStart/onResume/onPause/onStop/onDestroy) are likewise rewritten into MethodHandle reflective invokes on obfuscated holders (pABlfjK.Kdx, DBtzQpFDSfhAJx.TcCz / .SbRMKBHTsfVw, UtLeJu.VpvSv) to hide the hook.
5. [obs] The loader itself (classes13.dex, com/ModPlayerprefs.java):
   ```java
   public class ModPlayerprefs {
       private static final String Located_Playerprefs = "MainActivity";
       public static void Modded(Context context) { System.loadLibrary("menu"); }
   }
   ```
6. [obs] libmenu.so JNI_OnLoad (vaddr 0x1f6b64, file offset 1010532, 36-byte body) calls FUN_001f6370 (0x1f6370, 1960 bytes), which does RegisterNatives and decrypts embedded ASCII-hex into the real mod-menu DEX via
   `*pbVar20 = (byte)uVar7 | (byte)(uVar6 << 4);`

---

## 2. The mod menu itself (source recovered)

[obs] libmenu.so carries two plaintext ASCII-hex blobs: 319,496 chars at file offset 0x48305 (ending 0x9630D) and 4,768 chars at 0x98464.

- Blob 1 decodes to 159,748 bytes of **valid DEX**: magic `dex\n035`, file_size 0x00027004 = 159,748 (matches actual length), string_ids 1441, type_ids 237, proto_ids 285, method_ids 717, **class_defs 63**. jadx produced 7 source files:
  /tmp/lm_dex/sources/com/android/support/{MainActivity,Menu,Main,Preferences,DialogHelper,CrashHandler,BuildConfig}.java
- Blob 2 decodes to a 2,384-byte DEX (7 class_defs; decompiles to only R.java, a resource-ID stub).

### 2.1 It is the public Mod Menu v5.3 template

[obs] BuildConfig.java: APPLICATION_ID = "com.android.support", BUILD_TYPE = "debug", DEBUG = true, VERSION_CODE = 1, **VERSION_NAME = "5.3"**.

[obs] MainActivity.java:
```java
public String GameActivity = "com.unity3d.player.UnityPlayerActivity";
public boolean hasLaunched = false;
// onCreate: guard -> System.loadLibrary("menu") -> startActivity(new Intent(this, Class.forName(GameActivity)))
// catch ClassNotFoundException -> tag "Mod_menu" / "Error. Game's main activity does not exist"
```

[obs] Main.java: REQUIRED_PACKAGE = "com.adb.apkinstaller", REQUIRED_PACKAGE_URL = "https://liteapks.com/9adb-installer.html"; Start() shows a non-cancelable "App Required" dialog whose Install button opens that URL and then calls `System.exit(0)`; Toast text "LiteApks.Com & 9MOD.COM".

[obs] Menu.java (2078 lines) = WindowManager overlay menu: MENU_WIDTH 290 / MENU_HEIGHT 210 / MENU_CORNER 35.0f / ICON_SIZE 45 / ICON_ALPHA 0.7f; colors ACCENT_COLOR #7C4DFF, MENU_BG_COLOR #141821, ToggleON #4CAF50, BtnOFF #B3261E, etc.; widget builders Button / ButtonOnOff / ButtonLink / Category / CheckBox / CheckBoxFeature / Collapse / InputLNum / InputNum / InputText / RadioButton / SeekBar / Spinner / DonateImg; contains `$$Nest$mfeatureList`, `val$featName`, `SettingsList`.

[obs] Author-side config baked into Menu.java:
- TG_TOKEN = "8992321759:AAEvrB-hjuIHdhLHYmRg_XQujQfedLOEUUs", TG_CHAT_ID = "-1004319045612"; feedback goes through the Telegram `/sendMessage` API
- FEEDBACK_COOLDOWN_MS = 30000, FEEDBACK_DAILY_LIMIT = 3; dialogs "All reports are anonymous" / "Internet connection is required to send feedback"
- donate image and developer info are embedded base64 PNGs; strings "Donate To Me" / "Join Telegram" / "LITEAPKS.COM" / "9MOD.COM"

[obs] Preferences.java = toggle persistence layer:
```java
sharedPreferences = context.getApplicationContext()
        .getSharedPreferences(context.getPackageName() + "_preferences", 0);
public static void changeFeatureBool(String str, int i, boolean z) { with(context).writeBoolean(i, z); }
// same shape: changeFeatureInt / changeFeatureLong / changeFeatureString
```
**Key inference**: the menu UI is only the display layer. Toggles are written into SharedPreferences by index i; the real feature logic lives in libmenu.so, which reads those values (`MENU_VERSION`, `_length` keys appear on both sides). So the answer to "what features does this mod have" sits in the native encrypted string table.

---

## 3. Signature-check bypass chain (PairIP vs killsign)

[obs] com.pairip.SignatureCheck constants:
- ALLOWLISTED_SIG = "Vn3kj4pUblROi2S+QfRRL9nhsaO2uoHQg6+dpEtxdTE="
- expectedSignature = expectedLegacyUpgradedSignature = expectedTestSignature = "13Gtz+XU0d5zweeer/N1aoPBO89tvA3T4ZndjymXIxg="
- verifyIntegrity() SHA-256s pi.signatures[0].toByteArray(), Base64.encodeToString(digest, 2), and throws SignatureCheck$SignatureTamperedException on mismatch

[obs] Actual signer = META-INF/TESTKEY.RSA, the AOSP public testkey (subject/issuer C=US, ST=California, L=Mountain View, O=Android, OU=Android, CN=Android, emailAddress=android@android.com). Cert DER SHA-256, base64 = `pA2oClnRcMqpUM8VwYxFTUejmyaYnYtkDs10W6cb9dw=` (openssl fingerprint A4:0D:A8:0A:...:1B:F5:DC).
[inf] That value is not PairIP's "13Gtz+...", so the check can only pass because killsign spoofs the signature handed back to PairIP.

[obs] /Users/Adam/Documents/bitlife/_re/jadx_small/classes12/sources/com/android/support/killsign.java (102,419 B, inner classes killsign$a .. killsign$l), class l implements Parcelable.Creator with ctor (Parcelable.Creator a, String b, Signature c):
```java
public final Object createFromParcel(Parcel parcel) {
    PackageInfo packageInfo = (PackageInfo) this.a.createFromParcel(parcel);
    if (packageInfo.packageName.equals(this.b)) {
        Signature[] signatureArr = packageInfo.signatures;
        if (signatureArr != null && signatureArr.length > 0) signatureArr[0] = this.c;
        if (Build.VERSION.SDK_INT >= 28 && packageInfo.signingInfo != null) {
            Signature[] apkContentsSigners = packageInfo.signingInfo.getApkContentsSigners();
            if (apkContentsSigners != null && apkContentsSigners.length > 0) apkContentsSigners[0] = this.c;
        }
    }
    return packageInfo;
}
```
It hijacks Parcel deserialization of PackageInfo and substitutes the "original" signature, so any verifier (PairIP, PackageManager, mod-detection code) sees the old signer. Sibling members: killPM, killOpen, EnvironmentData, getApkContentsSigners, `VMRuntime.setHiddenApiExemptions`, reflective `"sPackageInfoCache"` access, sun.misc.Unsafe + MethodHandles. The source retains the attribution string `https://github.com/L-JINBIN/ApkSignatureKillerEx`.

[obs] Obfuscation trick worth recording: killsign carries ~40 decoy imports from unrelated SDK packages (com.moloco.*, io.bidmachine.*, com.mbridge.*, kotlinx.coroutines.*, com.iab.omid.*, com.fyber.*, androidx.collection.internal.ycs.bRImUgx ...), and its static block **writes string constants into those foreign classes' static fields** (bRImUgx.ACF="WebvttCueParser", bRImUgx.GOkzoRuQIqY="\u001f", bRImUgx.Gxlqxn="Step is zero." ...) purely as analysis noise.

Note: com.android.support.MainActivity (the mod-menu activity) exists in **no** dex inside the APK. It only lives in the DEX embedded in libmenu.so and is loaded at runtime, so static scanners do not see it.

---

## 4. The native feature catalogue: unreadable statically, dumpable at runtime (see section 13)

[obs] libmenu.so (sha256 ab7e98cd5c9d12178e07fc0c0cee48b1bff209a1970134680c477229fd537239, 1,671,720 B, DT_NEEDED liblog/libandroid/libEGL/libGLESv2/libc/libm/libdl — linking EGL/GLESv2 means an OpenGL overlay drawn over the game).
- .rodata 0x9658D-0xA2200 (48,243 B) holds 1,958 NUL-terminated strings: 614 readable ones (including `/proc/self/maps`, `/sbin/magisk`, `/system/xbin/su`, `/dev/qemu_pipe`, `frida-gadget`, `libreveny_hook.so`, `substrate_hook_init`, `zygisk_inject`, `dlsym_hook_detected`, `libil2cpp_hook_guard`, `AES_GCM_IV_NONCE_96BIT`, `AES256_ENCRYPT_KEY_ROTATION_SALT`, `AQAB_RSA_PUBKEY_MODULUS_N`, `https://api.protection.internal/v3/verify`) and 946 random-looking ones — selective per-string encryption.
- Encryption schemes tested and ruled out: single-byte XOR (only 12 of 946 entries pass a 0.85 printable ratio, all short/coincidental); address-derived XOR (k=(off+i), (off>>8)+i, i, off-i -> 0 hits); repeating-key XOR (normalized adjacent-block Hamming distance stays ~3.9 bits/byte for every key length 1..32, no dip); keystream reuse (XOR of equal-length pairs gives a low-bit ratio 0.284-0.300 vs a 0.25 random baseline; ASCII plaintext reuse would exceed 0.4).
- [inf] The native string table is encrypted with a keyed block/stream cipher whose key is derived at runtime (consistent with the AES-GCM and key-rotation constants in the same binary).
- [correction, dynamic phase] "Not statically recoverable" does not mean "not obtainable". Those strings are **plaintext at runtime**: dump the process image of libmenu.so and diff its printable-string set against the on-disk file and 907 plaintext entries fall out (every hook target name and JNI signature included) — **without reversing its decryption routine**. See section 13.

[obs] libmenu.so has only 167 imports, including dlopen/dlsym/dl_iterate_phdr/mprotect/prctl/syscall/pthread_create — the classic inline-hook engine signature.

---

## 5. Was the game itself statically modified?

[obs] grep over libil2cpp.so (104,046,736 B) and global-metadata.dat (18,847,828 B) for `liteapks`, `9mod`, `ModPlayerprefs`, `mod_menu`, `unlimited`, `godmode`: 0 hits.
[inf] Value/logic edits are not in the files; libmenu.so reads the SharedPreferences toggles at runtime and hooks IL2CPP functions.
[resolved, dynamic phase] All 59 hook sites live in libil2cpp.so and were resolved to IL2CPP method names: one `BitLife.UI.Containers.UIDarklyPaywallExpansionPack::CultBulletPoints`, the rest are `Game.GameVars::*` and about 50 `Game.UIIAPManager::*Purchased` grant points plus a matching batch of `PurchaseXxx` initiators, plus the paywall button `BitLife.UI.Containers.UIDarklyPaywallGenericIAP::BuyButtonPress`. Full list in section 13.

---

## 6. Telemetry, distribution, ad stack

[obs] Distribution/monetization: Main.java forces installing an external installer (com.adb.apkinstaller from https://liteapks.com/9adb-installer.html); the menu embeds a Telegram bot and a donate entry; template version 5.3 with placeholder package com.android.support — the public LiteApks / 9MOD mod-menu channel.

[obs] Third-party telemetry injected into the Manifest as a ContentProvider (AndroidManifest.xml:1301-1309):
- android:name = com.bamen.report.ReportProvider, authorities = com.candywriter.bitlife.bamen.report.provider, exported = false, initOrder = 100
- meta-data: bamen.report.statisticsNo = 9, bamen.report.debugLog = false
- ReportProvider.onCreate() -> Reporter.start(context); Reporter endpoints:
  `https://api.gamekillerapp.com/api/overseas/v1/single-game/online-log` and `.../launch-log`
- Collected: Settings.Secure android_id (falls back to a UUID written into SharedPreferences "bamen_report"), package name, app label, versionCode, client time yyyy-MM-dd HH:mm:ss, session; OnlineTracker registers ActivityLifecycleCallbacks and uses a "bamen-online-report" daemon thread that waits for foreground before reporting.
[inf] This is install/online analytics for a game-mod distribution panel, unrelated to the game. Installing this mod build adds a third-party reporting channel.

[obs] Protection/壳: PairIP (com.pairip.application.Application, SignatureCheck, VMRunner, StartupLauncher) + 12 `\x00IAP` magic assets (assets/3M7BEoMfrOJfBpEL 159,500 B, ahQQhuwI7ZiFgGHs 280,034, b2GBqOK2U4MiE4qo 371,660, bSPdtT1PTPyaldHv 161,053, bsWs4yCUsnR4HPiO 159,200, cD9YCYNSWv55leZ3 159,095, dohrLT3fRgBSZXtd, FJs7BXdFi5IEAKbf, FNS1OaeRn7pXyrv0, fPD0Vc6q1J8biTN7, IhUP9onrobUq4vZ6) = PairIP IAP V2 payloads.
[obs] classes14.dex od/ccc/d/f/a/dd/d.java: static fields ApplicationInfo a / DexClassLoader b / Method h; string constants rebuilt from base64 fragments via f.a(b.a(...)) ("wDA=\n"+"9gTSLrK3How=\n", etc.); reads applicationInfo.publicSourceDir and processes ZipInputStreams -> a second-stage DEX loader / anti-tamper stub. The same dex carries misplaced-package stubs androidx/work/job/DataSyncWorker, android/content/res/AppComponentFactoryImpl, com/google/firebase/analytics/FirebaseMessageServiceImpl (package-name camouflage).
[obs] Ad/analytics SDK pile: com/inmobi/media (1769 class refs), com/applovin/impl (742), com/explorestack/protobuf (728), com/google/protobuf (631), com/flurry/sdk (524), com/appsflyer/internal (456), vungle, unity-ads, moloco, facebook-ads, ironsource, chartboost, mintegral, bidmachine, pubnative, gadsme, gameanalytics, didomi consent, OMID; libquack.so + libquickjs.so are koushikdutta QuickJS bindings with no Java-side references.

---

## 7. Runtime/dynamic verification (the static-phase "not possible" was overturned by the MuMu run — see Part II, sections 10-14)

[obs] The only available device (REA adb): serial 192.168.1.105:5555, TCL Smart_TV / G13_2K_GB, Android 14 (SDK 34), security patch 2025-03-01, ro.debuggable = 0, **ro.product.cpu.abilist = "armeabi-v7a,armeabi" (32-bit-only Google TV box)**. This APK ships arm64-v8a only (no armeabi-v7a dir), so install fails with INSTALL_FAILED_NO_MATCHING_ABIS. com.candywriter.bitlife is not installed.
[obs] /Users/Adam/Documents/bitlife contains only this APK — no stock build to diff against, so version differential analysis is impossible.
[correction, dynamic phase] That device truly cannot install it (32-bit TV box), but the host's **MuMu Player** emulator (Android 12 / arm64-v8a / rooted) plus frida 16.7.19 delivered the whole dynamic story: install + launch, gate bypass, every menu entry enumerated, 59 hooks captured and resolved to method names, and the complete content library decrypted. See Part II.

---

## 8. Open items (static-phase items 1-3 were all closed in the dynamic phase, see section 13)

1. ~~Reverse libmenu.so's string-decryption routine~~ -> **done without it**: dumping the live module image yields 907 plaintext strings (section 13).
2. ~~Decompile the SharedPreferences reader~~ -> **done**: the menu entries were enumerated straight from native `SettingsList()`/`sub2025s()`, so there is no need to reverse SharedPreferences (section 12).
3. ~~Run it once on an arm64 environment~~ -> **done**: MuMu + frida, end to end (sections 10-13).
4. [still open] Which zip entry the od/ccc second-stage DEX loader reads (d.java only touches publicSourceDir + ZipInputStream; it looks like self-verification/re-decryption, and no dropped payload was observed).
5. [still open] Two of the thirteen BinaryFormatter assets — `female0-age13` and `male1-age13` — raise RecursionError in `nrbf` (object graph too deep or cyclic).
6. [still open] Art/audio assets were not exported (the datapack's 8133 Texture2D were left alone; only content tables were exported in this phase).

---

## 9. Reproduction commands (the essential ones)

```bash
# 1) manifest + dex
jadx --no-src -d _re/jadx_res BitLife-Life-Simulator-v3.25.1-mod_sdk.apk
# 2) extract the hex-embedded DEX from libmenu.so (offset 0x48305, length 319496)
python3 -c "d=open('_re/lib/arm64-v8a/libmenu.so','rb').read();
open('/tmp/lm_embedded.dex','wb').write(bytes.fromhex(d[0x48305:0x48305+319496].decode()))"
jadx -d /tmp/lm_dex --no-res --no-debug-info /tmp/lm_embedded.dex
# 3) sections / JNI_OnLoad
rabin2 -S _re/lib/arm64-v8a/libmenu.so
# 4) signer
keytool -printcert -file _re/META-INF/TESTKEY.RSA
```

---

## 10. Quick reference

| Item | Value |
|---|---|
| APK SHA-256 | c2111dee01a5d28f0a5f6cfb61bcf2d513a53d8f8ef0f530265da9240f215c1d |
| Signer | AOSP testkey (TESTKEY.RSA), cert DER SHA-256 b64 pA2oClnRcMqpUM8VwYxFTUejmyaYnYtkDs10W6cb9dw= |
| PairIP expected signature | 13Gtz+XU0d5zweeer/N1aoPBO89tvA3T4ZndjymXIxg= |
| Signature bypass | com.android.support.killsign (ApkSignatureKillerEx, github L-JINBIN), hijacks PackageInfo parcel |
| Mod entry | UnityPlayerActivity.onCreate -> ModPlayerprefs.Modded -> System.loadLibrary("menu") |
| Mod payload | libmenu.so (sha256 ab7e98cd5c9d12178e07fc0c0cee48b1bff209a1970134680c477229fd537239) + embedded DEX (159,748 B, 7 source files) |
| Menu template | Mod Menu v5.3, package com.android.support, LiteApks / 9MOD channel |
| Toggle storage | SharedPreferences "<pkg>_preferences" (Preferences.changeFeatureBool/Int/Long/String(index, value)) |
| Telemetry | com.bamen.report -> api.gamekillerapp.com/api/overseas/v1/single-game/{online,launch}-log (android_id + package + version) |
| Feature list | [unk] — native string table encrypted, not statically recoverable |
| Static game-logic edits | [none found] — no mod traces in libil2cpp.so / global-metadata.dat |
---

# Part II: dynamic phase (measured on MuMu Player)

> Two conclusions in Part I are contradicted by the dynamic evidence and were corrected in place above:
> 1. "the native feature catalogue cannot be recovered" — wrong: the names are plaintext at runtime; dumping the process image yields them (section 11/13).
> 2. "no dynamic verification possible in this pass" — wrong: install, launch, gate bypass, hook capture and full content decryption were all done on MuMu Player (sections 10-14).

## 10. Dynamic environment [obs]

- Host: Apple M1 / macOS 15.4. Emulator /Applications/MuMuPlayer.app, VM "我的安卓", Android 12 (SDK 32), arm64-v8a, model spoofed as SM-S9180.
- adb endpoint: `/opt/homebrew/bin/adb -s 127.0.0.1:16384` (after a cold boot the port moves from 5555 to the vm.json adbPort 16384).
- Root: set `vmRootEnable: false -> true` in `~/Library/Application Support/com.netease.mumu.nemux/bundles/mumu_0_*/setting/vm.json`, restart the VM; `adb shell id` then returns uid=0(root). The flag survives shutdown; MuMu does not rewrite it.
- frida: version 16.7.19 is mandatory (from frida 17 on the Java bridge is no longer bundled and scripts die with `ReferenceError: 'Java' is not defined`). frida-server 16.7.19 android-arm64 lives at `/data/local/tmp/fsrv16` and runs as root; client frida 16.7.19 + frida-tools 13.7.1.
- Gotcha: `frida.get_usb_device()` silently returns an unrelated TCL TV on the LAN (192.168.1.105:5555). Always `frida.get_device("127.0.0.1:16384")`. `dev.attach("com.candywriter.bitlife")` raises ProcessNotFoundError; take the pid from `ps -A` and attach to that.

## 11. Gate bypass and the loader chain [obs]

- First launch is blocked by the mod's own dialog "App Required / Please install 9ADB Installer from our website to continue." backed by `com.android.support.Main`:
  - `REQUIRED_PACKAGE = com.adb.apkinstaller`
  - `REQUIRED_PACKAGE_URL = https://liteapks.com/9adb-installer.html`
  - methods: `Start`, `StartWithoutPermission`, `ShowRequiredAppDialog`, `isPackageInstalled`, `getTopActivity` plus three `-$$Nest$sm*` bridges.
- Bypass (no installer needed): frida spawn, find the loader with `Java.enumerateClassLoaders`, `loadClass("com.android.support.Main")`, replace every overload of `isPackageInstalled` with `return true`, then resume.
- The loader class observed at that moment:
  `dalvik.system.InMemoryDexClassLoader[DexPathList[[dex file "InMemoryDexFile[cookie=[-5476376643880247920, -5476376644419490032, -5476376644419483984]]"], ...]]`
  => the mod's Java half is **three in-memory dex files**; no dex exists on disk (the mod writes /data/data/com.candywriter.bitlife/files/<ts>.dex, loads it, then unlinks it — only the deleted-inode mapping shows up in maps).
- Dumping those three `[anon:dalvik-DEX data]` regions through /proc/<pid>/mem and trimming the tail padding gives:
  - mod_a 5,237,600 B (`dex\n035`, Facebook/Instagram/Kotlin dependency dex, 2822 java files)
  - **mod_b 159,748 B (`dex\n035`) — the mod menu itself**: `com/android/support/{Main,Menu,MainActivity,Preferences,DialogHelper,CrashHandler,BuildConfig}`
  - mod_c 1,334,156 B (`dex\n038`, org.xbill.DNS + com.google.android.gms)

## 12. The complete mod feature set (enumerated at runtime, [obs] verbatim)

The menu table is served by native code; calling it on `com.android.support.Menu` at runtime (`native String[] SettingsList()` / `native String[] sub2025s()`) returns:

SettingsList()
```
0: Category_<font color='red'>Settings</font>
1: -1_Toggle_Save Feature
2: -3_Toggle_True_Auto Menu Size
3: Category_<font color='red'>Game Info</font>
```
sub2025s()
```
0: Collapse_NOTE
1: CollapseAdd_RichTextView_<div style="text-align: left;"><font color='yellow'>➥ You need to install Google Play Services for the mod to work.</font></div>
2: Category_<font color='red'>MOD FEATURES</font>
3: Button_Free In-App Purchase
4: Category_<font color='red'>CONTACT US</font>
```
Everything else is branding/feedback: `⏩ LITEAPKS.COM ⏪`, `⏩ 9MOD.COM ⏪`, https://liteapks.com, https://9mod.com, https://t.me/liteapks, https://t.me/newbiexabo, a PayPal donation link, Quick Feedback/Report Bug (Telegram bot `8992321759:AAEvrB-hjuIHdhLHYmRg_XQujQfedLOEUUs`, chat_id `-1004319045612`, 3 messages/day with a 30 s cooldown).

Conclusion: **this mod has exactly one cheat — "Free In-App Purchase"** — plus two menu settings. The rest is distribution funnel and feedback.

## 13. How "Free In-App Purchase" works ([obs], end to end)

1. `libmenu.so` installs inline hooks with ShadowHook 2.0.1 in `JNI_OnLoad`; logcat tags `Mod_Menu`/`shadowhook_tag` print it: `shadowhook version 2.0.1: shadowhook init(default_mode: UNIQUE, debuggable: false), init-errno: 0, real-init: yes` followed by **59 `shadowhook HOOK(addr) OK: 0x...` lines**.
2. Converting those addresses through the module base from /proc/<pid>/maps shows all 59 sites are inside **libil2cpp.so**: one at 0x2fcf8a8, the other 58 packed into 0x31ddad0-0x31f5040 (about 1.4 MB).
3. Enumerating IL2CPP at runtime with frida (128 assemblies / 150,007 methods) and reading each MethodInfo's entry pointer maps every site to a name:
   - 0x2fcf8a8 -> `BitLife.UI.Containers.UIDarklyPaywallExpansionPack::CultBulletPoints` (the only hook unrelated to IAP)
   - from 0x31ddad0 -> `Game.GameVars::{SaveGameVars, LoadDefaults, SecondsSinceLastInterstitial, ...}`
   - the bulk from 0x31e19c8 -> `Game.UIIAPManager::*`: about 50 `XxxPurchased` grant points plus a matching batch of `PurchaseXxx` initiators.
4. libmenu.so contains no literal table of those offsets (full 8-byte and 4-byte little-endian scans for all 59 values: 0 hits) => it **resolves the methods by name** through the IL2CPP API at runtime, which is exactly why the names must be encrypted on disk.
5. Dumping libmenu.so's live module image (base 0x7404c03000, 0x1a2000 bytes) and diffing the printable-string sets against the on-disk file yields 1053 new strings, 907 after noise filtering, including:
   - grant points: `Game.UIIAPManager.AssassinsBladePurchased`, `AuraRingPurchased`, `BattlePassPurchased`, `BitizenshipPurchased`, `BitizenshipAndGodModePurchased`, `BossModePurchased`, `Bundle*`, `Career*`, `ChallengeVaultPurchased`, `ExpansionPack{BlackMarket,Casino,Cult,Investor,Landlord,Racing,SecretAgent,Vampire,Zoo}Purchased`, `FighterModePurchased`, `GetOutJailCardPurchased`, `GodModePurchased`, `GoldThemePurchased`, `Golden{Diploma,NekoStatue,Pacifier,Passport,PiggyBank,Resume,Wrench}Purchased`, `HollywoodStarPurchased`, `LuxuryAccessPurchased`, `MovieDirectorPurchased`, `OutdoorLifestylePurchased`, `PromiscuityPotionPurchased`, `RemoveAdsPurchased`, `SpikedKnucklesPurchased`, `StreakSaverPurchased`, `TimeMachinePurchased`, `UnlimitedTimeMachinePurchased` ...
   - initiators (`PurchaseXxx`, one per product above)
   - **`BitLife.UI.Containers.UIDarklyPaywallGenericIAP.BuyButtonPress`** — the real paywall button the mod intercepts
   - Java-side targets `com.android.support.Main` / `Menu` / `Preferences`, and JNI signatures `(Landroid/content/Context;ILjava/lang/String;IJZLjava/lang/String;)V` (= `Preferences.sub2025`), `([Ljava/nio/ByteBuffer;Ljava/lang/ClassLoader;)V` (= InMemoryDexClassLoader ctor)
   - the IL2CPP API it uses: `il2cpp_domain_get_assemblies`, `il2cpp_method_get_name`; anti-analysis probes: `/proc/self/maps`, `/sbin/magisk`, `/system/xbin/su`, `/dev/qemu_pipe`, `frida-gadget`, `zygisk_inject`, `libreveny_hook.so`
   - embedded compression: `/system/lib64/liblzma.so` + `XzUnpacker_*` (statically linked XZ SDK) => the three in-memory dex files are what it decompresses and hands to InMemoryDexClassLoader.
6. Semantics: tapping "Free In-App Purchase" goes to `Preferences.sub2025(ctx, 3, "Free In-App Purchase", ...)`; native code then makes every `PurchaseXxx`/`*Purchased` above report "already owned" and lets the real `BuyButtonPress` through => **no Google Play transaction is ever started; the entitlement is simply marked as owned**.
7. Cross-checks ([obs]): attaching `Interceptor` to all 59 hook sites and then triggering a purchase logged no hits; invoking `BitizenshipPurchased(bool)` directly through `il2cpp_runtime_invoke` hits an IL2CPP assert (`Error: breakpoint triggered`), and passing NULL params gives `access violation accessing 0x0` (**that is a missing-argument failure, not a threading problem**: `RemoveAdsPurchased(true)` and `GoldenPacifierPurchased(true)` both return cleanly). Conclusion for the port: drive the button -> record -> entitlement path, do not call the internals directly.

## 14. The content database, fully decrypted ([obs])

The game ships its whole content library as Unity TextAssets:

1. `assets/bin/Data/datapack.unity3d` (121,483,836 B). UnityPy 1.25.4 enumerates 8133 Texture2D / 8094 Sprite / 7653 MonoBehaviour / 3988 GameObject / **1086 TextAsset** / 327 Canvas; `data.unity3d` holds UI only (0 TextAssets).
2. All 1086 TextAssets exported to 1084 unique files (19.3 MB) under `_re/gamedata/`.
3. **967 of them (`*-enc`) are base64 text** encrypted by the game's own `EncryptionManager`. At runtime `EncryptionManager.DefaultCipherKey()` returns **`com.wtfapps.apollo16`** (20 chars) and `DecodeAndDecryptString(text, key)` returns plaintext (`DecryptString` throws — do not use it).
4. Batch-running that oracle over frida RPC decrypted **966 files**; of the remaining 118, 105 were plaintext to begin with (the engine never encrypted them, e.g. `a16_voltron_componentFilenames`, `BillingMode`, `IAPProductCatalog`) and **13 are .NET BinaryFormatter streams** (`00 01 00 00 00 FF FF FF FF` + `Assembly-CSharp, Version=0.0.0.0`). 11 of those parsed to JSON with `nrbf` (`a16_countries-enc` = 102 countries with real backing-field keys such as `<Name>k__BackingField`, `<Cities>k__BackingField`, `<Currency>k__BackingField`; plus 8 age4/age13 save samples with 297 fields each).
5. Result: `_re/gamedata_plain/` — **1071 files / 13.3 MB of plaintext** (939 `a16_*` + 77 `genesis*` + others), indexed in `_re/gamedata_plain_index.json` (and `gamedata_INDEX.md` for the raw exports); parsed BinaryFormatter JSON in `_re/gamedata_json/`.
6. [unk] `female0-age13` and `male1-age13` make `nrbf` raise RecursionError (object graph too deep / cyclic) and remain unparsed.
7. Representative tables: `genesis_master_tags_reference_2` (1.3 MB, columns `tag_id,tag_value,weight,categorization_a...`), `genesis_movie_director` (~900 KB of movie scripts), `a16_dumb-scenarios-calendar`, `a16_dumb-scenarios-career`, `genesis_lookup_emoji_visuals` (4488 emoji rows), `a16_countries-enc` (102 countries), and so on.

## 15. Revised conclusions

- [obs] The mod = the generic LGL Mod Menu Java template + a custom native libmenu.so (ShadowHook + IL2CPP name resolution + LZMA). It **does not modify game files**: libil2cpp.so and global-metadata.dat inside the APK are clean, everything happens at runtime. Its entire gameplay cheat surface is one switch: free in-app purchase.
- [obs] The installer gate and feedback module identify this as a liteapks/9mod funnel build rather than host-targeted malware; the injected telemetry provider `com.bamen.report` reports install/online events to `api.gamekillerapp.com`.
- [inf] For a port, the valuable part is not the mod but the **content library + IL2CPP method inventory**: together they are enough to re-implement this game on another platform (Part III).



## 16. Hook-site re-verification and correction ([obs], second dynamic pass)

1. [obs] Correct addressing: libil2cpp.so's mapped segments are **not** a simple base+file-offset shift (the segments drift by 0x1000-0x4000). The file offset must be converted through the `/proc/<pid>/maps` segment table (start/end/file_offset) before reading bytes; plain base+offset reads a mismatched page — which is the source of the earlier misreading in section 13.
2. [obs] With segment-table addressing, each of the 59 hook sites from section 13 was read (16 bytes) and compared against the same file offset in the on-disk ELF: **all 59 differ**, and the difference is **only the first 4-byte instruction**; the prologue bytes right after it match the file exactly (e.g. `fe5fbda9 f65701a9 f44f02a9` preserved verbatim).
3. [obs] Those 4 bytes are always `0x17xxxxxx`: top six bits `0b000101` = ARM64 **B (unconditional branch)**, and the sign-extended imm26 is **negative** (roughly -15 to -19 MB) => each site branches forward into shadow memory.
4. [obs] The pages holding those 59 sites are **rwxp** in /proc/maps (`73db5e3000-73db5e7000 rwxp off 0x31dd000`, `73db5e7000-73db5ef000 rwxp off 0x31e1000`, `73db5f1000-73db5f5000 rwxp off 0x31eb000`), while the rest of the library stays r-xp => the mod raised those pages with mprotect: this is the ShadowHook inline-hook site.
5. [correction] Section 13's earlier claim ("58 sites still show the original prologue, only 0x31ddad0 has an adrp patch") is **wrong** — it came from mis-addressed reads. The correct finding: **all 59 hook sites are patched in place**.
6. [obs] The hooked addresses are **not** method entries: `Game.UIIAPManager::GodModePurchased's methodPointer sits at file offset 0x31e24e8 and its first 16 bytes (`fe0f1bf8 fa6701a9 f85f02a9 f65703a9`) match the file, unpatched; likewise `BitizenshipPurchased`(0x31e1b54), `CareerActorPurchased`(0x31e66e8), `GoldenPacifierPurchased`(0x31e9078), `RestorePurchases`(0x31f9bbc), `get_Inst`(0x31e19e0). => The patches sit at interior check points (0x31ddad0-0x31f5040), consistent with name-based method resolution plus interior interception.
7. [unk] The exact trampoline destination is unresolved: decoding the B relative offset lands on read-only data or a segment gap, so no executable stub could be read; the trampoline layout stays unknown (it does not affect the "hooked" conclusion — the byte diff and the RWX pages are direct evidence).


## 17. Part III: native C++ port (dynamic-phase output)

Deliverable `/Users/Adam/Documents/bitlife/_re/port/` (C++17 + CMake, **no third-party dependencies**, 11 files, ~1,590 lines):

| file | lines | role |
|---|---|---|
| CMakeLists.txt | 19 | static lib blport + executable blsim |
| include/bl/table.h · src/table.cpp | 63 · 153 | reads the real decrypted CSV/TSV in gamedata_plain (quoted fields, # comments, column lookup, Commonness Score weighted pick) |
| include/bl/game.h · src/game.cpp | 100 · 125 | state + 60-row entitlement catalogue taken from the real Game.GameVars / MonetizationVars fields in il2cpp_dump.txt |
| include/bl/sim.h · src/sim.cpp | 83 · 439 | age-up life loop driven by real a16_* scenario tables (stats, karma/discipline, placeholder substitution, death) |
| include/bl/mod.h · src/mod.cpp | 92 · 228 | the mod model: grant(productId), both real toggles, LGL menu grammar |
| src/main.cpp | 157 | real-data demo |
| README.md | 131 | finding-to-module map + not-ported list |

**Build and run (independently re-verified here: cmake 4.4.3 / Apple clang 17.0.0 arm64, configure+build exit 0, `./build/blsim` exit 0, 183 output lines)**

```
cd /Users/Adam/Documents/bitlife/_re/port
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j4
./build/blsim
```

**Real data wired [obs]**: `a16_dumb-scenarios-universal-enc` (258 rows / 42 cols: Index, Hero Min/Max Age, Student, Job, Vampire, Both Parents Alive, Hero Gender, Title, Body, CTA, Choice 1..4, Narrative 1..4, Karma/Discipline/Happiness/Intelligence/Appearance Impact 1..4), `a16_employee-recognition-scenarios-enc` (14 rows, supplies the Commonness Score weighted path), `a16_casino-casino-celebrity-name-enc` (106-row name pool), `a16_hero-filler-enc` (1132 rows), `a16_death-types-enc` (110 rows), `gamedata_json/a16_countries-enc.json` (102 countries). Placeholders `<a/b/c>`, `<num2-5>` and `<friend> <teacher> <classmate> <coworker> <parent> <lover> <company> <employee>` are substituted; anything unresolvable stays verbatim rather than guessed.

**Detail that was checked**: the `row N` printed by the demo is the **load ordinal**, not the table's Index column — the Teething row is load ordinal 117 while its Index column reads 143. The text itself matches the decrypted table verbatim.

**Mod side**: `kEntitlements[60]` is generated from Game.UIIAPManager's 60 `*Purchased` grant points plus 58 `Purchase*` initiators, each cross-referenced to its IAPManager product-id field; `grant()` accepts four real addressing forms (grant method, short name, IAPManager field, Purchase* initiator); `grantAll()` grants all 60. The menu tree is reproduced verbatim: NOTE / RichTextView (Google Play Services notice) / MOD FEATURES / Button Free In-App Purchase / CONTACT US / Settings (Toggle Save Feature, Toggle Auto Menu Size) / Game Info.

**Not ported (honest list, full text in README)**: careers/education/relationships/money/property (a16_careers-enc loads but is unused), mini-games, heirlooms/artifacts, Darkly paywalls, time machine, Battle Pass, daily quests, notifications, ads, the whole UI layer (menu geometry is known: MENU_WIDTH 290 / HEIGHT 210 / CORNER 35.0f / ICON_SIZE 45 / ACCENT #7C4DFF but not reproduced), art and audio (8133 Texture2D not exported), 11 BinaryFormatter tables (including the two age13 saves that hit Nrbf RecursionError), unresolved placeholders such as `<rb>`, and a port-side simplified mortality rule.


## 18. Phase 1: differential test bench (RNG parity + observed corpus)

Goal: make a ported life comparable to a captured one, draw for draw and line for line.

### 18.1 RNG parity — solved [obs]
The sim's randomness comes from `System.Random`. New frida hooks (in `/tmp/blh.js`): `System.Random..ctor(I)` records the seed per instance pointer, and `Next()` / `Next(int)` / `Next(int,int)` record every draw as `{self, seed, seq, value}`; RPC `rand(n)` returns them.

Captured with `rng_vectors.py` (`blhrpc.rpc('rand', n=6000)` -> `rng_vectors.json`): 9 instances, 3003 raw `Next()` samples.

Ported as `port/include/bl/dotnet_rng.h` + `port/src/dotnet_rng.cpp` (Mono/.NET subtractive generator, incl. the `Int32.MinValue` clamp) and wired into `bl::Rng` (`port/include/bl/table.h`) so the whole port now draws from the game's own stream instead of `mt19937_64`. `blsim --rngcheck` replays each captured instance from its seed:

```
seed 8           pairs=226  consecutive-matches=32
seed 7           pairs=231  consecutive-matches=32
seed 10          pairs=234  consecutive-matches=32
seed 13          pairs=190  consecutive-matches=32
seed 14          pairs=201  consecutive-matches=32
seed 1407739333  pairs=120  consecutive-matches=32
seed 2139187394  pairs=1797 consecutive-matches=2
rngcheck instances=7 verified=6   (exit 0)
```
6 of 7 instances are bit-exact for 32 consecutive samples. The 7th diverges because it also feeds unhooked consumers (`NextDouble`/`NextBytes`), which advance the same internal stream.

Determinism after the swap: `blsim --script {seed:12345, ages:8}` -> sha256 `77b83db7a9ab4127` twice, identical. (The old `mt19937_64` hash `e05274f207a84863` is superseded.)

Open: draw mapping is not attributed to call sites yet (which draw picks which table row).

### 18.2 RandomDeck is dead code in the base game [obs]
`RandomDeck..ctor/ShuffleAll/FillAndShuffle/Draw/SetPercent` and `RandomDeckCollection.Draw` all fired **0 times** across four aged-up years, while `LifeLog.Log`, scenarios and ticks kept firing. The class exists in `il2cpp_dump.txt` (30247 / 30263 / 30265) but the base life loop never builds a deck. `port/src/deck.cpp` implements the decomposition anyway (`ComputeDeckParameters(p) -> {100/g, p*100/g}`, Fisher-Yates) and `blsim --deckcheck` self-checks it (`p=0.25 -> 4/1`, `0.1 -> 10/1`, `0.01 -> 100/1`, `0.75 -> 4/3`; 0 failures) — but it is unverifiable against the game until a path that uses it is found.

### 18.3 UnityEngine.Random is the other source [obs]
A census hook over every `UnityEngine.Random` overload shows `Range(int,int)` firing (~17 calls at boot) with argument pairs like `(7,0)`/`(8,0)`/`(24,0)` and results in `0..N`. Interpretation: those are `Range(0, N)` denominators of 7/8/24. Unity's xorshift128 is **not** implemented in the port yet — needed for any path that goes through it.

### 18.4 Text has two sources, and neither is the whole story [obs]
`xd_meta.py` classified the 31 observed scenario bodies and 32 observed log lines: table 9 / metadata literal 10 / neither 12 (bodies), and table 9 / metadata 2 / neither 21 (logs). Table-driven text (e.g. `a16_stepparent-cruelty-scenarios-enc`, `a16_podcast-dumb-enc`, `genesis_movie_director`) is already loadable; script-coded scenarios (`ScenarioFriendMeet`, `ScenarioSchoolEnroll`, ...) only exist as il2cpp string literals inside `global-metadata.dat`.

Metadata literal tables are currently unreachable: the 18,847,828-byte file's header does not decode with the assumed layout, and the 8-byte table at 18314580 (59,446 ascending entries, second field `0x02xxxxxx`) is not a (offset,length) pair table. Probing stopped; the observed corpus is the working substitute.

### 18.5 Observed corpus + templates (port-side text ground truth) [obs]
- `corpus.py` -> `obs_corpus/scenarios.csv` (27 unique scenarios) and `obs_corpus/logs.csv` (28 unique log lines), with the ages each was seen at.
- `templatize.py` mines name<->relation pairs from the same text and emits `obs_corpus/templates.csv` (55 rows: 27 scenarios + 28 logs; 21 rows carry slots) with `{halfsister}`, `{friend}`, `{classmate}`, `{spouse}`, `{dog}`, `{mother}` placeholders.
- `blsim --templates`: loads that CSV through the ordinary `bl::Table` reader, counts 55/27/28, and checks every `{slot}` resolves (0 unresolved) — so the corpus is directly instantiable by the port later.

### 18.6 Runner unblocked (UI side) [obs]
The blocker was a classifier ordering bug, not the game: BitLife's own dialogs carry an orange border, so an "orange panel" check that ran before the button-band scan tapped the dialog border forever. Fixed order in `/tmp/dlg5.py::state`: button bands -> dialog; band-less near-white card with blue **text** -> native alert (`alert()` picks the lowest blue text cluster, i.e. the OK/button row, not the title); then orange/blue panels. `drive9.py` also gained a blind recovery battery (panel X corners, BACK, card bottom) plus `/tmp/stuck_r<N>.png` capture on failure. With that, one run walked the life from age 32 to 39 (logs/scenarios/ticks advancing each round).

Open: the Google Play "error connecting" alert can still sit on top of an IAP panel where the panel's bands are behind it; that path ends in the recovery battery instead of a clean dismissal.

## 19. Phase 1: the differential bench itself (tools/)

Goal (from `PORT_PLAN.en.md` §3 Phase 1): an instrument that makes every later porting decision falsifiable by comparing a life run in the APK against the same run in the port, and printing the *first* divergence.

### 19.1 What was built [obs]
`tools/` (new):

| file | role |
|---|---|
| `tools/trace_common.py` | the single normalization both sides share: lower-case, digits->`#`, quoted strings and capitalised name runs->`{name}`, other punctuation->space. Compares what happened, not how a name or number was spelled. |
| `tools/game_trace.py` | drive rounds (`trace_game*.json`) -> canonical trace; reads the six UI stats out of the il2cpp backing fields (`<Att_intelligence>k__BackingField` is UI *Smarts*, `<Att_appearance>` is *Looks*). |
| `tools/port_trace.py` | runs `blsim --script` and folds each event's `deltas` forward so every age has its own stat line. |
| `tools/diff.py` | first divergence in priority order: age coverage -> per-age log lines (exact normalized sets, then token-Jaccard near-miss) -> per-age stat drift. `--stat-tol N`, `-v`. |
| `tools/README.md` | pipeline, usage, how to read a result. |

Both sides now emit the same canonical shape `{source, ages:[{age, logs, scenarios, stats}]}`, so the bench is symmetric: either side can be replaced without touching the comparator.

### 19.2 First real divergence [obs]
Game trace `trace_game9.json` (ages 39-42, captured by the pixel driver) vs a 42-year port run (seed 12345, Finland, male):

```
window [39, 42]
stream=logs  age=39: game logged 6 line(s) the port did not
  game-only first: "i followed some customers outside and passive aggressively told them that we
                    typically get tipped and asked them if our"
  port-only first: "{name} told {name} she is integral to the company s success"
stream=stats age=40: happiness drifts 9 from the aligned start (game 79, port 88)
```

`stat_offset` is measured at the first compared age because the two runs start from different lives; later ages report drift from that alignment. **Logs diverge first**, i.e. the two runs selected different events; that is the selection layer, not a rule bug.

### 19.3 Picker instrumentation: a negative result that saves work [obs]
`/tmp/blh.js` now installs frida hooks on every `DumbScenarioInterface.Get*` (23 methods) and every `DataHub.GetRandom*` (300, capped from the real 339) and records `{tag, method, returnType, args, hero age, returned object class, row fields}` into `st.picks` (RPC `picks(n,tag)`, dump saved as `picks_run1.json`).

Result over four aged-up years:

```
323 records, 2 distinct methods
  GetRandomNumber            322   (generic int roller)
  GetARandomPushDumbScenario   1   -> DumbScenario
```

Meanwhile 6 scenarios were presented in the same window. **So the yearly scenarios the player sees are not produced by `DataHub.GetRandom*` nor by `DumbScenarioInterface`** - they are constructed by the `Sim*` systems directly (each `Scenario*` class has its own constructor), which is why the earlier per-feature `GetRandomXScenario` catalogue (339 methods) looked like the picker layer but is not the one on this code path.

`Scenario` (base) has no table/row identity either - its fields are `Title/Body/ImageString/ChoiceArray/HasDismissButton/...` plus `_ChoiceToOriginalIndex` (a `Dictionary<ScenarioChoice,int>`), so the originating table can only come from the producer, not from the scenario object.

### 19.4 Still-open in Phase 1
- One *passing* end-to-end trace (the plan's acceptance test) needs the selection layer: the port picks from 5 tables while the game mixes table-driven sets with script-coded scenarios (`xd_meta.py`: 9/31 bodies table, 10/31 metadata literal, 12/31 neither).
- Producer attribution per scenario class is the missing link; the cheap hooks are exhausted (DataHub/DumbScenarioInterface ruled out above, `RandomDeck` still 0 fires, `UnityEngine.Random` census shows only `Range(0,N)` boot calls).
- The game's `Life` per-year pipeline (`IncrementAge` internals) is not mapped; 231 of its 755 methods mention Age/Year/Increment, but they are mostly `AgeAtLastX` accessors.

## 20. Phase 1: the push-scenario selector, located and captured ([obs])

### 20.1 Where per-age scenarios actually come from

Hooking `DataHub.GetRandom*` (339 methods) and `DumbScenarioInterface.Get*`
(23 methods) produced 323 records with only two distinct methods — a negative
result. Hooking the base `Scenario::.ctor` and taking a backtrace showed the
real path:

    Scenario<Derived>::.ctor
      <- System.Reflection.RuntimeConstructorInfo::InternalInvoke
      <- ... <- Life code region

Scenarios are constructed **by reflection from a type name**, so the selector
never calls a "GetSomeScenario" factory that our earlier probes could see.
The selection layer is:

    Life.ValidateAndSetScenario(System.String typeName,
                                System.Int32 limit,
                                System.Collections.ArrayList valid)   -> Boolean
    Life.GetAgeSpecificPushScenarioArray()                          -> ArrayList
    Life.GetRandomPushScenario()                                    -> Scenario

`ValidateAndSetScenario` is called once per scenario **type name**; it returns
whether that type is eligible now and appends the name to the candidate
ArrayList. `GetRandomPushScenario` then returns one `Scenario` instance that
was built from the winning name.

### 20.2 First captured registry

One age transition produced **174** `ValidateAndSetScenario` calls over **173**
unique type names, with the second argument distributed as

    {70: 72, 14: 40, 7: 28, 2: 14, 3: 12, 5: 6, 1: 2}

and a single pick:

    age 49 -> ScenarioLoveHookup      (Life.GetRandomPushScenario)

The names are the whole push-scenario surface of the game — e.g.
`ScenarioHorseRaces`, `ScenarioJobReferral`, `ScenarioExLoverPaternityTest`,
`ScenarioSexualityDeclare`, `ScenarioGenderIdentity`, `ScenarioPrisonBullyInbound`,
`ScenarioRoyalRevolt`, `ScenarioVampireAgeUpCouncilCommunion`,
`ScenarioChildhoodEmigration`, `ScenarioEthicsSomeoneChokes`.

Artifacts:

* `tools/scenario_registry.py` — turns `prod2` dumps into a registry.
* `obs_corpus/scenario_registry.json` — candidates, offered-only names, picks.

### 20.3 Harness stability work in this round

* Backtraces inside il2cpp hooks crash the app: tombstone_07 and tombstone_08
  are SIGSEGV in `/memfd:frida-agent-64.so`, not in the game. Frame capture is
  now gated behind `st.armBt` (default off); the reflection finding above was
  already recorded before the gate went in.
* `il2cpp_runtime_invoke` must not be hooked by default: armed, it faults the
  same way within a minute (422 records at boot are all
  `SerializedPushScenario{CategoryDraw,Content,SubcategoryDraw}`).
* `dlg5.capture()` now rejects frames whose size is not 1440x2560, and
  `drive9.age_visible()` bounds-checks before sampling. The emulator had
  silently rotated to landscape (screencap 2560x1440) which produced
  `IndexError: image index out of range` in the driver.
* Android-side `AlertDialog.show` is now wrapped to auto-dismiss the
  "error connecting to Google Play" dialog; the message body is read from
  `android.R.id.message`.

Correction to the acceptance signal: the first pass through
 `tools/scenario_registry.py` reported 27 accepted / 146 rejected for the same
 dump. That was an artifact: the hook read the `Boolean` return register
 without masking, so 147 rows came back null while 27 rows returned an
 unrelated integer that a truthy test counted as acceptance. The tool now
 keeps acceptance tri-state (1 accepted / 0 rejected / anything else unknown)
 and the registry honestly records 173 unknown, 0 accepted for that dump.
 Lesson: never let an unmasked register read become a boolean.

### 20.4 Still open

* The per-age **valid** subset was not captured cleanly yet: the boolean return
  register read needs masking (`ret.toInt32() & 1`) and a batch only fires on
  the age transitions that rebuild the push queue. The hook now also records the
  ArrayList size before/after each call and the appended element, which gives an
  acceptance signal independent of the return register.
* Each `Scenario*` class must still be mapped to its content source (encrypted
  `a16_*` table vs `global-metadata.dat` literal) before the port can replay a
  year end-to-end.

### 20.5 Correction: the third argument is a weighted bag, not a validity list
Re-captured with the Boolean return masked and the ArrayList inspected
before/after each call, one age transition shows the real mechanism:
    ValidateAndSetScenario("ScenarioSocialMediaJoinPush", 70, bag)
        bag.size  0 -> 70    (70 copies of the name appended)
    ValidateAndSetScenario("ScenarioLoveDate", 14, bag)
        bag.size  70 -> 84   (14 copies)
    ValidateAndSetScenario("ScenarioLoveHookup", 14, bag)
        bag.size  84 -> 98
So the second argument is a **weight**, the ArrayList is a **bag of type-name
strings**, and `Life.GetRandomPushScenario()` draws uniformly from that bag and
reflectively constructs the winning type. The pick is therefore an index into a
concatenated multiset, exactly like the `WeightedList<T>` / `RandomDeck` pattern
used elsewhere in the game.
Clean capture over two age transitions (ages 51->53, 348 calls = 2 x 174):
    318 accepted, 30 rejected;  173 distinct type names offered
    weights: {70: 70, 14: 40, 7: 24, 2: 11, 3: 8, 5: 4, 1: 2}
    picks:   age 49 -> ScenarioLoveHookup
             age 52 -> DumbScenario
             age 53 -> ScenarioSocialMediaJoinPush
`DumbScenario` appearing in the same bag is useful: the port already implements
the `a16_dumb-scenarios-universal-enc` table, so one bag member is replayable
today. `obs_corpus/scenario_registry.json` now carries accepted / rejected /
picked with weights.
Driver fix that unblocked trace collection: the pixel classifier scanned only
the detected card band (`top`..`bot`), and card-bottom estimation underestimates
when a dialog contains a grey info strip. The `Will & Testament` dialog was
therefore classified as an unknown alert and the driver looped for an hour on
the wrong rows. The button scan now uses a fixed dialog-wide x range
(`200..1240`) and `top..min(bot+560, 2400)`, which classifies that dialog
correctly (`choice` at y=1625 and y=1744) and took the game from age 50 to 53 in
one run. Note the shell preview of a 1440x2560 screencap is 961x1708 - exactly
2/3 scale - and reading button rows off the preview without converting misled an
earlier diagnosis.


## 21. Phase 1: replaying the game's own pick (push) + where a scenario's text lives

21.1 Port side: `push` replay.
`blsim --script` now accepts a per-age override:
`"push": [ {"table": "<real table>", "row": <1-based>} | null, ... ]` where
`push[i]` is the age-up that ends at age `i+1`. `Sim::forceScenario()` loads the
named table on demand (the wired-up loop only preloads five) and
`Sim::renderRow()` renders that exact row - Title/Body/CTA, the filled-in
`Choice N` columns and the chosen branch's impacts - falling back to a
narrative-only table ('Narrative'/'Scenario'/'String'). An unusable override
falls through to the normal draw instead of losing the year, and a failed table
load is reported on stderr, so the bench sees the hole rather than a silent
substitution. Determinism is preserved: the same script prints byte-identical
output on rerun (sha256 2b7fe1dae0412e2e, twice). The override consumes no RNG
draw, so years after an override no longer consume the same stream positions as
a pure-draw run; that is deliberate, because the game's own draws are unknown
anyway, and it matters only to stream-position arguments.

21.2 Recovering the row from the observed text: `tools/row_recover.py`.
The game's Scenario object carries no table/row identity, but the rendered
Title/Body are enough to find the row. Matching must be strict: the bench's own
normalizer collapses both `Area 69` and `Canyon Neuron 5` onto `{name} #`, so
`row_recover.py` compares exact lower-cased word/digit sequences, then ranks
candidate tables by how many long body words the table shares with the rendered
body (table placeholders such as `<parent>` are dropped rather than compared),
breaking ties by the class-to-table ranking in `obs_corpus/scen_source.json`.
Result on a fresh United States life, ages 0..6: 1 of 5 scenarios recovered -
age 2 `DumbScenario`/`Favorite Drink` -> `a16_dumb-scenarios-universal-enc` row
131 - and the other four recovered nothing.

21.3 The other four scenarios do not come from a table at all.
Phrase census over `gamedata_plain/`, `gamedata/` and
`assets/bin/Data/Managed/Metadata/global-metadata.dat`:

    "is taking you to the doctor"          metadata 1, tables 0
    "have decided you"                     metadata 1, tables 0
    "You are starting elementary school"   metadata 0, tables 0
    "You broke your parents"               metadata 0, tables 0
    "Canyon Neuron 5"                      metadata 0, tables 0

`ScenarioChildhoodVaccination` and `ScenarioOutdoorAgeUpParentsGiveBike` keep
their prose as il2cpp string literals; `ScenarioSchoolEnroll` and
`ScenarioEthicsBreakSomething` are composed at runtime (their titles appear in
tables - "Elementary School" in 18 - but the rendered sentences appear nowhere),
and `Canyon Neuron 5` is assembled from per-manufacturer name tables. Consequence
for the plan: table recovery cannot cover the script-coded half of the scenario
set. Either the port ports those classes by hand (each is a small state machine
over named placeholders) or the literal pool has to be extracted from
`global-metadata.dat`; the header layout of this build does not match the
documented il2cpp metadata header, so that extraction is still open (section 18.4).

21.4 First end-to-end diff on a brand-new life (ages 0..6).
`tools/fresh_trace.py` builds the canonical trace from the live harness: a new
life's own log records keep `AgeAtLogStart` at 0, so ages are bucketed by the
`SimHero` upgrade ticks instead of by the log's own age field. Diff output:
window [0, 6], logs first diverge at age 3 - the game logged
"i remained calm when my mother took me to the doctor s office to get vaccinated"
where the port logged its dumb-table rows. The port's only lines at ages 3, 4 and
6 are `a16_dumb-scenarios-universal-enc` rows, which is exactly the 21.3 gap: the
port replays one table-backed pick and substitutes dumb rows for the four
script-coded ones. Stats at the window start already differ (happiness +4,
health +4, smarts +4, looks +57, karma -47, discipline -81), i.e. birth stats are
the first thing to align before any stat comparison is meaningful.

## 22. Phase 1/2: the age-0 birth sequence in the port

22.1 What was added. Sim::birthLines(Life&) (include/bl/sim.h, src/sim.cpp) writes the
seven age-0 lines the game writes, in order, from real tables: the place/gender line plus
the conception circumstance (a16_birth-circumstances-enc, filtered on the mother's age and
weighted by Commonness Score), the birthday and star sign (the sign is *derived* from the
date in zodiacOf(), the game has no zodiac table for this), the hero's name, both parents
(a16_careers-enc career weighted by Commonness Score, parent first names from the same
celebrity-name table createLife() uses, matched to the parent's gender), an optional
sibling and an optional family pet (a16_pets-enc domestic rows + a16_pet-names-enc).
The four tables are added to Sim::load()'s preload list. blsim --script now prints
"birth": [...] between hero and events, and tools/port_trace.py puts those lines in the
age-0 bucket with the newborn stats.

22.2 Determinism. Two runs of the same script hash b0089b62de751d31 (sha256, 16 chars).

22.3 First diff with age 0 in the window. window [0, 6] now reports ages_game 7 /
ages_port 7, and the first divergence moves from *missing* age-0 content to a draw
difference: the game's sibling line was "i have an older brother named {name} age #", the
port's "i have a younger brother named {name} age #" -- the same normalized shape, a
different coin flip. The remaining port-only line is the conception circumstance row, i.e.
a table pick the port makes from its own stream.

22.4 Stats are still the real blocker. stat_offset at the window start is happiness -4,
health +5, smarts +1, looks +56, karma -63, discipline -100. The port's newborn stats are
still the placeholder draws in createLife() (happiness 70+d21, health 80+d21, smarts
40+d41, looks 40+d41, karma 0, discipline 0); the game's newborn (Elvis Butkus) was
happiness 83 / health 82 / smarts 69 / looks 17. The next candidate source is the
Min Looks / Max Looks / Min Smarts / Max Smarts columns we already load in
a16_birth-circumstances-enc -- recorded as a hypothesis, not yet coded, because it is not
yet clear whether those bounds gate the hero or the mother.

## 23. Phase 1/2: the newborn's attributes, solved from the shipping binary

23.1 Why the draws were invisible. Two earlier probes came back empty and both are now
explained by inlining. `set_Att_*` hooks never fire (the setters are inlined into their
callers), and no `GameReference.GetRandomNumber` / `DataHub.GetRandomNumber` call anywhere
in the birth window has `SimPerson..ctor` or `SimHero.SetupEverythingElseForNewHero` in its
backtrace -- the four primary attributes are drawn through a path the wrapper probes cannot
see. `MemoryAccessMonitor.monitor` is also unusable on this device (Frida 16.7.19 throws
`TypeError: not a function`), so page-write monitoring is out.

23.2 File offset of a runtime address. Reading /proc/<pid>/maps for the mapped segment and
using `fileoff = map_offset + (addr - map_start)` gives the file offset; here runtime
0x77a9abc048 sits in 77a9abb000-77a9abd000 (rwxp, map fileoff 0x2a73000), so the setup
function is at file offset 0x2a74048. The il2cpp dump addresses map with the constant
delta 0x73F049E000 (checked against three dump addresses). `python3 capstone` then
disassembles arm64 at that offset directly.

23.3 The formula (SetupEverythingElseForNewHero, x19 = the new SimHero). Everything else is
`SimPerson..ctor(bool)`, which draws the attributes and stores them as floats. The setup
first reads GameVars.<LivesStarted>k__BackingField (GameManager.Inst -> +0x48 GameVars
-> +0x14c) and compares it with 1:

- `LivesStarted > 1`: happiness = R(0,50) + 51, health = R(0,20) + 81; intelligence and
  appearance keep their ctor values.
- `LivesStarted <= 1` (first life): happiness = R(0,20) + 81, health = R(0,20) + 81, then
  r = R(0,2); `r != 0` -> intelligence = R(0,10) + 10, appearance = R(0,10) + 61;
  `r == 0` -> happiness = R(0,10) + 81, health = R(0,10) + 81, intelligence = R(0,10) + 61,
  appearance = R(0,10) + 10.

R is GameReference.GetRandomNumber(Int32,Int32) (file offset 0x2e0d2a8). Ten call sites were
read off the disassembly: max 50 and 20 in the later-life branch, 20/20/2/10/10/10/10/10/10
in the first-life branch. The stores are `str s0` at happiness 0x88, health 0x8c,
intelligence 0x90, appearance 0x94, each `add w8,w0,#imm` then `scvtf` -- integer draws
turned into floats. Seven independent captures (run6/7/9/10/11/12/13) all landed in the
first-life branch and satisfy one of its two cases exactly, e.g. run11 before 76/83/51 ->
after 100/94/17/64 = 81+19 / 81+13 / 10+7 / 61+3.

23.4 Upper bounds are open. Across 1803+ `Next/2` records the observed maximum for each
declared upper bound is exactly max-1 (mx=1 -> 0, 2 -> 1, 10 -> 9, 20 -> 19, 101 -> 100,
344 -> 342, 483 -> 482), so System.Random.Next(min,max) is [min,max) and the port's
rng_.below(n) has the same semantics.

23.5 The ctor's draw order. A probe on every `SimPerson..ctor` records the RNG stream the
constructor consumes; every ctor consumes exactly 25 `Next(min,max)` calls, with the upper
bounds 101,101,101,101,101,101, 5,10,5,11, 101 (x13), 100,100. Aligning those values
against the 22-attribute hero snapshots of 14 captures shows the first six draws are
happiness, health, intelligence, appearance, discipline and karma, the next four small
draws (max 5/10/5/11) do not land in an attribute, and the following thirteen fill
willpower ... dealing in ctor code order -- note charisma is assigned after modeling even
though its field offset (0xcc) precedes music's (0xd0). The last two Next(0,100) draws were
not attributed. homo and fertility are random in the game too (0..82 and 14..99 in the
captures) but are not consumed on this stream, and fame stayed 0 in all 14 captures.

23.6 Port. `Sim::createLife` (src/sim.cpp:294) now makes the same draws in the same order --
six 0..100 attributes, four small draws, thirteen more, two Next(0,100) -- then applies the
first-life setup branch: happiness and health 81..100 and one coin flip that makes the
newborn either book-smart (intelligence 10..19, appearance 61..70) or good-looking
(intelligence 61..70, appearance 10..19). The later-life branch is documented in the code
but not implemented, because the port plays a single life per run. `shortcut:` the port keeps
only the six attributes it models; the other draws are made and dropped so the stream keeps
the ctor's shape.

23.7 Self-check. tools/check_newborn.py runs 60 seeds through `blsim --script /dev/stdin`
and asserts the ranges above plus karma/discipline in 0..100: the smart/good-looking split
came out 32/28 and there were no violations. Note `blsim --script /dev/stdin` without
`--data` prints JSON; with `--data` it prints the human-readable report.

23.8 Effect on the bench. stat_offset at the window start moved from
happiness -4, health +5, smarts +1, looks +56, karma -63, discipline -100 to
happiness -3, health -2, smarts -5, looks -3, karma +11, discipline -84. Ages 0 through 4
now report per-age worst deltas of 0, 19, 19, 22 and 11, and at age 0 all six stats match
after the shift (game 90/85/71/17/63/100 vs port 90/85/71/17/63/100). The remaining stat
drift is discipline and karma, plus two defects the diff exposes: the port's discipline
exceeds 100 (103 at age 3) where the game stops at 100, and no annual adjustment is
modelled yet. The age-0 logs still differ in wording (the game drew 'I was an accidental
pregnancy.' where the port drew an IVF/donor circumstance row) -- the same table, a
different row.

## 24. Scripted-event scenarios: registry, push form and log-line shape

Built from the campaign-3 capture in section 21.3's follow-up: every scenario card the game
presented, with its title, exterior title, body, button labels and the `Scenario.PushScenario`
flag, plus the outcome sentences the game writes into its own LifeLog. Rows 24.2-24.6 are the
raw material; 24.7 is what changed in the port.

24.1 Where the literals come from. The button labels and bodies are runtime strings, not table
cells: campaign 3 (trace `/tmp/c3_scen.json`) read them straight off the live Scenario object
(`Scenario.ChoiceArray` at 0x108, each `ScenarioChoice.<Title>` at 0x10, `Scenario.Body` at 0x48,
`Scenario.Title` at 0x38, `Scenario.ExteriorTitle` at 0x30, `Scenario.PushScenario` at 0x131 --
`il2cpp_dump.txt` line 56590). The outcome sentences are `global-metadata.dat` literals located
with `tools/metadata_strings.py` (layout in METADATA_STRINGS.en.md), e.g. the vaccination calm
branch at file offset 964794 / index 24970.

24.2 Captured scenes, verbatim.

- `ScenarioChildhoodVaccination` (push yes) -- title "Vaccination", body "Your mother is taking
  you to the doctor's office to get vaccinated.\n\nHow will you behave?", buttons "Throw a
  tantrum" / "Bite her" / "Try to stay calm".
- `ScenarioSchoolEnroll` (push yes) -- exterior "Education", title "Elementary School", body
  "You are starting elementary school.", no buttons.
- `ScenarioFriendMeet` (push yes) -- exterior "Friend", title "New Friend", body "Your
  classmate, Julian, wants to become your friend.", buttons "Become friends with him" /
  "Reject him".
- `ScenarioRetentionCrime` (push yes) -- exterior "Crime", title "Mischief Maker", body "You're
  wondering how much trouble you'll get into if you do something a little bit naughty.\n\nWhat
  will you do?", buttons "Ignore the thought" / "Cause some trouble".
- `ScenarioSchoolClassmateActUp` (push yes) -- title "Troublemaker", body "Your classmate,
  Sabine, is marching around your music teacher, Mrs. Stevens's, classroom and pretending to
  play the trombone while smacking other students with the trombone slide.\n\nWhat will you
  do?", buttons "Ignore her antics" / "Report her to the principal" / "Attack her" / "Laugh at
  her".
- `ScenarioScriptEventSiblingBorn` -- title "Baby", body "Your parents just had a baby boy
  named Thomas, your new brother.", no buttons. The log sentence the game writes for it is
  different from the card ("My mother and my father had a baby boy named Walter, my new
  brother."), which is why the registry keeps a card body and a log sentence separately.

24.3 Vaccine branch to log sentence (metadata literals, index in parentheses).

- "Throw a tantrum" -> "I threw a tantrum when my {0} took me to the doctor's office to get
  vaccinated." (25238)
- "Bite her" -> "I bit my {0} when {1} took me to the doctor's office to get vaccinated." (23636)
- "Try to stay calm" -> "I remained calm when my {0} took me to the doctor's office to get
  vaccinated." (24970)

24.4 Port registry. `port/src/sim.cpp` holds `kScenarios[]` (`RegScenario`: class, exterior
title, title, body, up to four `RegChoice` label/log pairs) and `findScenario()`.
`Sim::renderScenarioClass()` renders one entry into an `AgeUp`; `Sim::forceScenarioClass(cls,
choice)` arms it for the next age-up. The body's `{parent}`/`{name}`/`{teacher}` tokens are the
port's stand-ins for the game's `{0}`/`{1}` fill points (`{parent}` is fixed to "mother", which
is what every capture showed). Branch deltas are all zero: no impact has been measured for a
scripted scene yet, and the exterior title is dropped because `AgeUp` has no field for it.

24.5 Push form. `"push"` entries now accept either shape, and the bench emits whichever one
applies:

```json
{"seed": 12345, "country": "United States", "gender": "male", "ages": 6,
 "push": [null, null, {"scenario": "ScenarioChildhoodVaccination", "choice": 2},
          {"scenario": "ScenarioSchoolEnroll"},
          {"scenario": "ScenarioFriendMeet", "choice": 0}]}
```

`choice` is the 0-based button index. `port/src/script.cpp` parses the form and dispatches to
`forceScenarioClass()`; an unknown class reports `forceScenarioClass: unknown scenario class:
<name>` on stderr and falls back to the normal draw, so a bench run shows the gap rather than
hiding it.

24.6 Choice recovery. `tools/row_recover.py` keeps a `REGISTRY` mirroring the port's log
templates and, for a trace scenario whose class is in it, matches the age's game log lines
against those templates (placeholder tokens dropped, token overlap >= 0.6) to recover the button
index; table-backed scenarios still go through the strict title/body match. On trace_fresh.json
the run now reads: age 2 dumb row 131, age 3 vaccination -> choice 2 ("Try to stay calm"), age 5
school enrol (no buttons), and two still UNRECOVERED (integrity, bicycle).

24.7 Log-line shape fix. The game's LifeLog holds the outcome sentence of the branch it took,
not the prompt it showed; `logLines()` in `port/src/script.cpp` now emits only `narrative` when
there is one and falls back to title/body for auto-advancing scenes. Effect on the bench
(window [0,6]): age 3 dropped from "port_only 2" to "port_only 0" -- the port's vaccine line is
now byte-equal after normalization to the game's own -- and the never-plausible port-only lines
"Try to stay calm" and the prompt body are gone. stat_offset at the window start is now
happiness -3, health -2, smarts -5, looks -3, karma +11, discipline -84, and age 0 matches
exactly (90/85/71/17/63/100).

24.8 Still missing. The yearly *selection* is not modelled: the port draws from
`a16_dumb-scenarios-universal-enc` where the game picks among scripted scenes (section 21.3's
four scenes are code-built, not table rows). A probe for `Life.IncrementAgeScenarioQueueArray`
(`il2cpp_dump.txt` line 28266, field 0x18) was added to `blh.js` to capture the per-age candidate
bag directly; the port will be pointed at the captured pool once that run lands.

24.9 Queue-array probe: what the per-age bag actually is. `blh.js` now hooks
`Life.IncrementAge` and reads `Life.IncrementAgeScenarioQueueArray` (field 0x18, dump line 28266)
on entry and on return. Result from campaign 4 (`/tmp/c4_queues.json`, 9 hits): the field is not a
candidate bag but the *next* scenario to present -- it holds 0, 1 or 2 entries and each
IncrementAge call advances it to the following one. The observed chain for one life is
DumbScenario -> ScenarioChildhoodVaccination -> ScenarioScriptEventSiblingBorn ->
ScenarioEthicsBreakSomething -> ScenarioSchoolEnroll -> ScenarioPetDeath -> ScenarioFriendMeet ->
[ScenarioScriptEventSiblingBorn, ScenarioRetentionCrime] -> DumbScenario, which is exactly the
scenario sequence the same life's LifeLog shows. So the per-age pool is built in code inside
IncrementAge and cannot be read off a live field; modelling the game's yearly selection means
disassembling that method (same capstone workflow as section 23).

24.10 Registry now six classes. `ScenarioEthicsBreakSomething` was added with the two captured
items ("mailbox", "bedroom window") and the two evidenced branches: "Admit that I broke it" ->
"I admitted to my parents that I broke their {item}." (game log at age 4 of trace_fresh, plus
metadata index 10855 for the button) and "It wasn't me" -> "I broke my parents's {item} and then
lied about it." (metadata index 23652). A pushed replay of age 4 now produces the game's sentence
shape ("I admitted to my parents that I broke their bedroom window." where the game drew
"mailbox" -- the item is a runtime draw, so the words differ and the normalized diff scores it a
near match rather than an exact one).

24.11 Bench state after 24.7 and 24.10 (window [0,6], trace_fresh.json). Exact-match log lines at
age 3 (vaccine), shape-match at age 4 (integrity). Per-age logs: age 2 1/1, age 3 1/0, age 4 2/1,
age 6 1/1 game-only/port-only; stat_offset at the window start happiness -3, health -2, smarts -5,
looks -3, karma +11, discipline -84, and age 0 matches exactly (90/85/71/17/63/100). The two
remaining log divergences are the dumb-table row draw at ages 2 and 6 (the game drew "milk",
"fort" where the port drew "water", "naughty club") -- a selection difference, not a rendering
one, and both come from the same table.

24.12 Registry log sentences for the auto-advancing scenes. The game's LifeLog does not store the
card body for these scenes; it stores a first-person sentence, captured from gameplay:
"Your parents just had a baby boy named Manuel, your new brother." on the card versus
"My mother and my father had a baby boy named Walter, my new brother." in the LifeLog, and
"The family mutt, Coby, died at the age of 12." on the card versus
"My cat, Ramona, died while sleeping peacefully." in the LifeLog. Three scenes now carry the
evidenced LifeLog sentence (ScenarioScriptEventSiblingBorn, ScenarioPetDeath,
ScenarioDiseaseContract), encoded as a single branch with an empty label so the scene renders no
button, and renderScenarioClass only consumes an RNG draw when the branch list has more than one
entry. The drawn nouns (sibling sex and name, pet animal and name, ailment) still come from the
port's own name/ailment tables, so only the sentence shape is faithful -- the card body and the
log each name their own noun in the game too.

25. Which pool the yearly dumb scenario comes from (campaign 5 and static evidence).

25.1 Campaign 5 census. One life driven with drive9.py for 16 rounds, then
tools/fresh_trace.py /tmp/trace_c5.json: ages 0..10, 19 logs, 19 scenarios. Classes seen
(Count): ScenarioScriptEventSiblingBorn 3, DumbScenario 2, ScenarioSchoolEnroll 2,
ScenarioFriendMeet 2, and one each of ScenarioChildhoodVaccination, ScenarioEthicsBreakSomething,
ScenarioPetDeath, ScenarioFriendNew, ScenarioRetentionCrime, ScenarioLoveFirstKiss,
ScenarioSchoolBully, ScenarioRetentionFreelanceGig, ScenarioSchoolActivityJoin, Scenario.
tools/pool_stats2.py prints this table; tools/row_recover.py on the same trace recovers
age 4 DumbScenario "Deplorable Spoils" from a16_dumb-scenarios-holiday-enc row 76 -- not from the
universal table the port uses.

25.2 There are 44 context-specific dumb tables. gamedata_plain/ holds
a16_dumb-scenarios-{universal,holiday,calendar,friend,career,prison,vacation,...}-enc plus
per-activity variants (astronaut, black-market, casino, dealer, cult, mafia, music, model, orphan,
royal, sports, zoo, landlord, prison, political, secret-agent, hustler, album, actor, business).
Headers carry their own gates: a16_dumb-scenarios-friend-enc selects on friend age/gender/craziness/
job/student/married/enemy, a16_dumb-scenarios-calendar-enc has Author, Holiday Name, Month, Date
before the hero gates.

25.3 DataHub sets the game keeps loaded (il2cpp_dump.txt:17346-17350): _dumbScenarioUniversalSet
0x480, _dumbScenarioHolidaySet 0x488, _dumbScenarioCareerSet 0x490, _dumbScenarioPrisonSet 0x498,
_dumbScenarioVacationSet 0x4a0, loaded by LoadDumbScenarioHolidaySet and friends
(il2cpp_dump.txt:18365). Pickers (il2cpp_dump.txt:19509-19515):
GetRandomUniversalDumbScenario(System.Boolean) @0x73f2cbe2b8, GetRandomHolidayDumbScenarioForHoliday
(System.Int32) @0x73f2cbe72c, GetRandomCareerDumbScenarioForCareer(System.String),
GetRandomVacationDumbScenario(), GetRandomFriendDumbScenario(), GetRandomPrisonDumbScenario(),
GetRandomCareerDumbScenarioForMilitaryBranchAndStatus(int,int).

25.4 The universal picker is uniform over a filtered copy of the set. Disassembled at file offset
0x28202b8 (dump address minus 0x73F049E000): it lazily builds _dumbScenarioUniversalSet, walks it,
filters, and at 0x2820644 calls GameReference.GetRandomNumber (0x2e0d2a8) with the surviving count,
so the port's uniform pick over eligible universal rows is right. Its boolean argument branches at
0x28205f0 (cbnz x20 -> 0x28206c0).

25.5 DataDumbScenario fields (il2cpp_dump.txt:8325-8345) are the table columns the model needs:
<DumbScenarioType> 0x10, <Index> 0x14, <ExteriorTitle> 0x18, <Title> 0x20, <Body> 0x28, <Cta> 0x30,
<RandomizeChoices> 0x38, <GenderBracket> 0x40, <ChoiceArray> 0x50, <CommonnessScore> 0x64,
<MinAge> 0x68, <MaxAge> 0x6c, <Gender> 0x70, <MustHaveJob> 0x74, <MustBeStudent> 0x75,
<MustHaveBothParents> 0x76, <Vampire> 0x77.

25.6 Normal play does not call the universal picker. The existing picks probe in blh.js hooks every
DataHub method whose name starts with "GetRandom". Over 1500 retained records from the current
harness session: GetRandomNumber 1498, and GetARandomPushDumbScenario 2 (ages 1 and 9), with zero
calls to GetRandomUniversalDumbScenario or GetRandomHolidayDumbScenarioForHoliday. So the yearly
dumb scenario is fetched through DumbScenarioInterface.GetARandomPushDumbScenario()
(il2cpp_dump.txt:56500, @0x73f2eb3238) and the set choice happens inside that method or above it.

25.7 What this means for the port. The port's universal-table draw is correct in shape (uniform over
gate-eligible rows, never the previous year's row) but incomplete in pool: holiday, calendar, career,
vacation, prison, friend and the per-activity tables are all candidates in the game, selected with
state the port does not model yet (in-game date for calendar/holiday, friend ids for friend
scenarios, career for career scenarios). Modelling that needs either the caller of
GetARandomPushDumbScenario or a long capture that records the picked DataDumbScenario.Index per year
so the pool rule can be inferred from data. tools/pool_stats2.py, tools/pool_port.py,
tools/picks_stats.py and tools/dis_dumb.py are the instruments added for it.

## 26. The yearly dumb-scenario pool as a set union (port)

### 26.1 Evidence: the gate chain inside the set picker
`DumbScenarioInterface.GetARandomPushDumbScenario` @0x73f2eb3238 (il2cpp_dump.txt:56500,
file offset 0x2815238) is the function the game actually reaches each year -- the DataHub
`GetRandomUniversalDumbScenario` / `GetRandomHolidayDumbScenarioForHoliday` extractors stay
unused in normal play (picks probe: 1498 GetRandomNumber, 2 GetARandomPushDumbScenario, 0 for
either extractor). Disassembling it in full (8324 bytes, 2081 instructions, ending where
`GetAVacationDumbScenarioAndIsCruise` begins) gives the order in which the state gates are
tested, in address order:

    0xeb3464 / 0xeb34bc  CurrentlyInPrison()
    0xeb34e0 / 0xeb3598  IsARoyal()
    0xeb35e0             CurrentlyHasAJob()   + 0xeb3648/0xeb3664 get_Career()
    0xeb38a4 / 0xeb398c  GetRandomNumber(Int32,Int32)      (dev-mode branch, 0xeb390c/0xeb3a58 get_DevMode())
    0xeb3c58             get_TeacherArray()   + 0xeb3cc4 CurrentlyHasAJob()
    0xeb411c / 0xeb4138 / 0xeb43dc  get_StepchildArray()
    0xeb4af8             IsARoyal()
    0xeb4c74             MinutesSinceLastCalendarDumbScenario()
    0xeb51b4 / 0xeb51dc  .ctor(DataDumbScenario, SimPerson, Boolean) / .ctor(DataDumbScenario, Boolean)

The 104 `FillDictionary` calls in the same function are its lazy per-activity set
initialisation, not part of the per-year decision. So the yearly draw is a priority chain over
sets -- prison, royal, career, school, family, calendar/holiday -- with the universal set as the
fallback, which is why a childhood life mostly sees universal rows and why a Halloween-themed
card appears only in the dated sets.

### 26.2 The sets and what gates them (from the tables themselves)
    a16_dumb-scenarios-universal-enc  258 rows  Index,Hero Min Age,Hero Max Age,Student,Job,Vampire,Both Parents Alive,Hero Gender,...
    a16_dumb-scenarios-holiday-enc     95 rows  + Type = Halloween(39)/Christmas(39)/Thanksgiving(13)/New Years(4)
    a16_dumb-scenarios-calendar-enc   366 rows  + Author,Holiday Name,Month,Date  (one row per real day)
    a16_dumb-scenarios-career-enc     419 rows  Index,Type,...  -- 'Type' IS the career name; no gating columns at all
    plus prison(65), royal(72), friend(38), vacation(60) and 22 per-activity tables
    (astronaut, black-market, casino, cult, dealer, hustler, landlord, mafia, model, music, orphan,
    political-campaign, political-office, secret-agent, sports, zoo, actor, album, business, ...)

### 26.3 Port design
`Sim::dumbPool(life)` returns the (table, row) pairs the year may draw from, and `ageUp` step
(2) picks uniformly over them -- uniform because none of these tables has a Commonness Score
column. Row gates reuse `scenarioEligible` (Hero Min/Max Age, Hero Gender, Student, Job,
Vampire), the same gate the universal pool already used. Per-set rules:

    universal  always in the pool
    career     only when the hero holds a job AND the row's Type equals that job
               (the game's GetRandomCareerDumbScenarioForCareer); the port only ever labels the
               hero "Student", so no row matches yet -- correct, not a stub
    holiday    only when a real date is fixed (Sim::setDate) and the row's Type maps to the same
               month (Halloween 10, Thanksgiving 11, Christmas 12, New Years 1)
    calendar   only when a real date is fixed and the row's Month + Date equal it

    shortcut: prison, royal, school (teacher), family (stepchild), friend, vacation and the
    per-activity sets are real sets the game consults, but the port models none of the hero state
    they gate on, so they are left out rather than approximated (comment in src/sim.cpp).

### 26.4 The date is now script-visible
`{"date": "10-31"}` sets the real-world date; absent, the dated sets stay out and the run stays
seed-only and reproducible. `Sim::setDate(int, int)` is the only writer.

### 26.5 Self-check (tools/check_dumbpool.py, 400 seeds x 40 ages)
    no date   :: universal 13520, employee 2181                    (total 15701)
    date 10-31:: universal 11332, holiday 1984, calendar 250, employee 2197  (total 15763)
    halloween rows first eligible at age 4 -- matches the table's own Hero Min Age (min 4)
A single 25-age run with date 10-31: universal 20, holiday 2, calendar 1, employee 2.

### 26.6 Bench: unchanged
Window [0,6] is unchanged, as expected -- a childhood hero is never employed and the bench passes
no date, so the pool is the universal set either way: age 2 1/1, age 3 1/0, age 4 2/1 (near:
item, mailbox vs bedroom window), age 6 1/1.

### 26.7 What the remaining sets still need
Prison/royal need the port to model those states; school needs a teacher relation; family needs
stepchildren; friend/vacation need their own threads. None of them is on the Phase 1 bench path
(push replay feeds the port the game's own pick), so they are deferred rather than guessed.

---

## 27. The yearly dumb scenario is a 6-way roulette, not a set union

§26 modelled the yearly dumb scenario as one uniformly-sampled union of the universal / career /
holiday / calendar tables. That model is wrong. Static disassembly of the picker says otherwise,
and the port now implements what the disassembly says.

### 27.1 Where the picker is

`DumbScenarioInterface.GetARandomPushDumbScenario` @ `0x73f2eb3238` (il2cpp_dump.txt:56500, file
offset `0x2815238`), 8324 bytes / 2081 instructions. Disassembled with `tools/dis_func.py 0x73f2eb3238`
(uses the *next* method's address, `GetAVacationDumbScenarioAndIsCruise(Boolean)` @ `0x73f2eb52bc`,
to size the function) and cross-checked with `tools/dis_range.py`, which maps every `bl` target back
to an `il2cpp_dump.txt` method name via the constant delta `0x73F049E000`.

### 27.2 The loop

```
0x73f2eb3408  mov  w26, wzr                 ; attempt = 0
0x73f2eb3414  <- loop top
0x73f2eb3430  bl   GetRandomNumber(0, 6)    ; one 6-way draw per attempt
              cmp  w0, #1 / b.gt 0xeb3490 ; cbz w0 -> 0xeb357c (k=0)
              cmp  w0, #2 / b.eq 0xeb369c ; cmp w0, #3 / b.ne 0xeb350c (k=3 -> 0xeb34a0)
              ...                          ; k>=4 -> 0xeb3514
0x73f2eb4ce0  add  w26, w26, #1             ; every gate failure / empty set lands here
              cmp  w26, #0x64
              b.ne 0x73f2eb3414             ; < 100 -> re-draw the 6-way
0x73f2eb5074  mov  x20, xzr                 ; 100 failures -> return null
```

Each branch calls one category extractor, and the callee is the category:

| k | gate | extractor |
|---|------|-----------|
| 0 | hero is an orphan | `GetRandomOrphanDumbScenario` @ `0x73f2cbfb0c` |
| 1 | `CurrentlyInPrison` | `GetRandomPrisonDumbScenario` @ `0x73f2cbf5f0` |
| 1 (not in prison) | nested `GetRandomNumber(0,4)` @ `0x73f2eb3748`; `IsSomeHolidaySeason` | `GetRandomHolidayDumbScenarioForHoliday(Int32)` @ `0x73f2cbe72c`, else `GetRandomUniversalDumbScenario(Boolean)` @ `0x73f2cbe2b8` |
| 2 | — | `GetRandomFriendDumbScenario` @ `0x73f2cbf958` |
| 3 | — | `GetTodaysCalendarDumbScenario` @ `0x73f2cbfcc0` |
| 4 | `CurrentlyHasAJob` + `get_Career` | `GetRandomCareerDumbScenarioForCareer(String)` @ `0x73f2cbec08` |
| 5 | `CurrentlyInTheMilitary` / `IsARoyal` / `IsVampire` | military @ `0x73f2cbf0c8`, royal @ `0x73f2cc04a4`, universal tail |

Gate function addresses: `CurrentlyInPrison` `0x73f2f0e1d8`, `IsARoyal` `0x73f2f0e128`,
`CurrentlyHasAJob` `0x73f2f0e040`, `CurrentlyInTheMilitary` `0x73f2f0e0b4`, `IsVampire` `0x73f30e1058`,
`IsSomeHolidaySeason` `0x73f3479f18`, `IsHalloweenSeason` `0x73f3479f48`, `IsChristmasSeason` `0x73f3479fe4`,
`IsNewYearsSeason` `0x73f347a06c`, `IsThanksgivingSeason` `0x73f347a108`, `get_Career` `0x73f2fbc400`.

Two things this settles: (a) the model is a per-attempt 6-way category roulette with **rejection
re-sampling** of the *whole* 6-way draw, capped at 100 attempts, not a weighted union; (b) picking
*within* a set is uniform — the dumb tables carry no `CommonnessScore` column — so uniformity holds
inside a set and nowhere else. §26.6's "set union" self-check numbers are superseded.

The ~104 `FillDictionary` calls near the top of the function are guarded by a static flag at
`[x19,#0x1fa]`: they are a one-time lazy load of the activity sets, not per-call work.

### 27.3 Port implementation

`include/bl/sim.h` drops `dumbPool` and the `lastScenarioTable_/lastScenario_` dedup pair (that
"don't repeat last year" rule was a port invention; the game has no such rule), and takes
`std::vector<size_t> dumbSetRows(const char* name, const Life& life)` plus
`bool pickDumbScenario(const Life& life, std::string* table, size_t* row)`.

`pickDumbScenario` runs the same loop: `k = rng_.below(6)`, map k to a set, build its eligible rows,
`pickFrom(rows, rng_)`; an empty set or an unmapped category `continue`s, and 100 failures return
false so `ageUp` falls through to the filler path. k=0 (orphan) always continues — the port's hero
always has a mother. k=1 splits on `rng_.below(4)`: `j > 2 || dateMonth_ == 0` goes universal,
otherwise holiday (the port has no nested-season test yet). k=2 friend, k=3 calendar, k=4 career
(needs `hero.employed` and `Type == hero.job`), k=5 universal.

`shortcut:` comments in `src/sim.cpp` record what is approximated — prison/orphan/royal/military read
as "gate failed, re-roll", which is exactly the game's own behaviour for a hero who is none of them;
the k=4..5 tail is only partially mapped; and the friend set is treated as non-empty because the port
keeps no friend list (the game's `GetRandomFriendDumbScenario` yields nothing for a friendless hero
and re-rolls).

### 27.4 Self-check

3 seeds × 30 years, `date=10-31` (so holiday + calendar rows are live), counting `"table"` in
`blsim --script` output:

| seed | universal | friend | career | calendar | holiday | employee-recognition |
|------|-----------|--------|--------|----------|---------|----------------------|
| 11 | 17 | 7 | 1 | 1 | 0 | 4 |
| 12 | 12 | 8 | 2 | 0 | 6 | 2 |
| 13 | 15 | 6 | 2 | 2 | 3 | 2 |

Every roulette-reachable set appears; universal lands at 40–50% against the ~1/3 its two k values
(k=1 tail + k=5) predict once the nested draw and the rejection re-rolls are counted. `tools/check_dumbpool.py`
(its §26.6 expectations) is superseded by this table.

### 27.5 Bench after the change

`bash tools/bench.sh /tmp/trace_c9.json` still runs the whole 16-year capture without aborting.
`tools/gap_report.py`: push coverage 10/16 years, real gaps on pushed years 7 (was 8), all of them
log-line composition rather than option content — age 2 missing the "longing for a cat" line, age 4
`flu` vs the `rb` line, age 6 `I assured him it wouldn't happen again` vs the scripted table's
title+body echo, age 7 missing the sibling line, age 9 `flu` vs `climbed trees`, age 13
`depression` vs `posters`, age 14 `joined the …` vs `are starting elementary school`.

Stat drift stays at health/happiness +17…+69 across the window: the port does not reproduce the
game's RNG stream, so every unpushed year's draws differ and the stats necessarily separate. The
bench only claims per-year option/line agreement on pushed years, and those agree.

### 27.6 Scripted table rows render now

The push that used to abort the bench (`{table: scenario1, row 13}`) renders: title `Argument`, the
body with `<parent>` resolved to "mother", and three buttons. Fixed on the way:

- `src/table.cpp`: `Table::at(row, column)` no longer throws `std::out_of_range` for a missing column
  (returns an empty string); `at(row, int)` still throws for a missing row. The bench replays tables the
  port never wired, and one missing column must not kill a whole life.
- `scriptedFamilyTokens` substitutes `<parent>` → "mother", `<him/her>` → "her", `<his/her>` → "her".
- The `ChoiceList` split runs *after* that substitution and tracks angle-bracket depth, because
  `[Apologize to <him/her>/…]` contains the same `/` it is split on — splitting the raw cell produced
  the button text `her>`.

`shortcut:` the scripted table's `ActionList`/`Impacts`/`ResultNarratives` are not evaluated, so a
scripted year logs title+body plus the chosen label instead of the game's outcome sentence.


---

## 28. A year is several lines: per-person lifecycle, not one scenario

§27.5 left seven missing log lines on pushed years. All seven are the same thing: the port emits
one event per year, the game emits several. This section settles where those lines come from.

### 28.1 The measurement

Ages 0–15 of the campaign-9 capture versus the same window of `blsim`:

| age | 0 | 1 | 2 | 3 | 4 | 5 | 6 | 7 | 8 | 9 | 10 | 11 | 12 | 13 | 14 | 15 |
|-----|---|---|---|---|---|---|---|---|---|----|----|----|----|----|----|----|
| game lines | 9 | 0 | 2 | 3 | 4 | 0 | 7 | 2 | 0 | 3 | 3 | 2 | 2 | 3 | 2 | 2 |
| port lines | 6 | 1 | 1 | 1 | 1 | 1 | 2 | 1 | 2 | 1 | 1 | 1 | 1 | 1 | 1 | 1 |

44 game lines against 23 port lines. The ages where the game logs nothing (1, 5, 8) also matter:
the port's own filler draw is not something the game does every year.

### 28.2 Provenance test: it is not the filler table

Every one of the 44 game lines was matched against all 1132 rows of
`gamedata_plain/a16_hero-filler-enc` (columns `Condition,String,Hero Min Age,Hero Max Age,Hero
Gender,Hero Sexuality,Hero Min/Max Happiness,Subject…,Min/Max Fame,Min/Max Reputation,Clean`;
`Condition` histogram: blank 502, Vampire 119, Fame 91, Royal 77, Actor 74, Student 60, Prison 56,
Vampire(space) 50, Overwork 41, Addict 23, Job 14, Car 12, Former Addict 8, Parents 3, Social Media 2).

**Exactly one line matched** — age 4's "starting to think the bathtub is the…" → row 601. The yearly
lines are therefore not `GetRandomHeroFillerForCondition` draws; that table is condition-gated
flavour for special states (fame, royal, prison, vampire, actor…).

### 28.3 What they actually are

Reading the 44 lines as a set, a year is composed of:

- **family lifecycle**: "My big brother, Jerry, has been diagnosed with lactose intolerance.", "My
  mother has been promoted…", "My big brother, Jerry, started middle school.", "Jerry graduated from
  high school.", "My mother and my father had a baby girl named Mavis."
- **hero disease arcs**: "I am suffering from the flu." / "My big brother, Jerry, is no longer
  suffering from…", "I have been diagnosed with anxiety." / "…depression."
- **school milestones**: "I started middle school at Velocity Prep", "I joined the Weightlifting Club
  at my high school."
- the year's **interactive scenario**, whose outcome line is the one the port already emits.

These sentences are **code templates, not table rows**. A `tools/metadata_strings.py grep` of the
metadata literal blob (71223 literals) shows them as string literals:

| literal | index | text |
|---------|-------|------|
| #9389 | 523412 | '<person> has been diagnosed with <disease-new>.'` |
| #9390 | 523459 | '<person> has been promoted to <job-title>.'` |
| #9385 | 523216 | '<person> graduated from <high-school>.'` |
| #24360 | 941449 | 'I have been diagnosed with {0}.'` |
| #2617 | 360098 | ' is suffering from '` |

Corroborating negatives: `grep` over all of `gamedata_plain/` finds none of "started middle school",
"graduated from high school", "has been promoted" or "suffering from the"; "middle school" appears only
as the UI action 'Go to middle school'` (#21921) — the sentence itself is assembled from fragments at
runtime.

### 28.4 The templating engine is the same one scenario1 uses

`<person>`, `<disease-new>`, `<high-school>`, `<job-title>` are the same placeholder syntax the
plain-CSV scripted table uses (`<parent>`, `<him/her>` in `gamedata_plain/scenario1`). The port's
`scriptedFamilyTokens` is a hand-rolled stand-in for that resolver — locating the real one (and its
placeholder dictionary) is worth more than adding cases to the stand-in, because it also covers the
filler and lifecycle templates above.

Yearly lifecycle entry points, from `il2cpp_dump.txt`: `SimPerson.IncrementAge()` @
`0x73f30d22ec`, `SimPerson.IncrementAgeOrDeath()` @ `0x73f30e1130`,
`SimPerson.OnLifeIncrementAgeEnded()` @ `0x73f30e1068`, `SimPerson.DidIncrementAge()` @
`0x73f30e108c`, `SimPerson.MarkIncrementAgeForCurrentGeneration()` @ `0x73f30e0ff8` — all driven
from `Life.IncrementAgeSimulation()` @ `0x73f373b9fc`. Modelling them needs person objects for the
family (spouse, children, siblings, in-laws), which the port does not have yet.

### 28.5 Bench reading rule (tooling change)

`tools/gap_report.py` now prints a per-year line-provenance block: for each missing line on a pushed
year it looks at that year's own game scenario classes, and tags the line `expected (unmodelled: …)`
when the year contains a class outside the port's `IMPLEMENTED` registry, `UNEXPLAINED` otherwise.
Current output on the 16-year bench:

```
age 2  1 missing -> UNEXPLAINED
age 4  4 missing -> UNEXPLAINED
age 6  7 missing -> expected (unmodelled: Scenario,ScenarioCustomAttack,ScenarioRelationshipArgueInbound)
age 7  2 missing -> UNEXPLAINED
age 9  3 missing -> UNEXPLAINED
age 11 2 missing -> UNEXPLAINED
age 13 3 missing -> expected (unmodelled: Scenario,ScenarioRetentionFreelanceGig)
age 14 2 missing -> expected (unmodelled: Scenario,ScenarioSchoolActivityJoin)
```

So the seven "real gaps" of §27.5 are: three years of unimplemented scenario classes, and four years
of family/disease lifecycle lines. Neither is a defect in the mechanism §27 fixed.

**Consequence:** per-year log equality cannot be the Phase-1 acceptance bar until the person
lifecycle exists. What the bench does claim, and still holds, is: option/choice agreement on pushed
years, card and button text agreement, and stat deltas read only inside a pushed year.


---

## 29. The text engine behind every line: BracketParser

§28.4 said the port's `scriptedFamilyTokens` is a stand-in. The real resolver is
`BracketParser` (`il2cpp_dump.txt:1024`), and it is the same engine for scenario bodies, scenario
narratives, filler strings and the lifecycle templates of §28.3 — anything with `<...>` in it.

### 29.1 State

| field | offset | meaning |
|-------|--------|---------|
| `BodyBracketDictionary` | 0x10 | `<`…`>` → value, for bodies |
| `NarrativeBracketDictionary` | 0x18 | same, for narrative lines |
| `SelectedNumberBracketList` | 0x138 | chosen `<number>`-style brackets |
| `SelectedRandomBracketList` | 0x140 | chosen `<random>` brackets |
| `CacheRandomBrackets` | 0x148 | whether a random bracket is drawn once and reused |
| `RB_Brackets` (static) | 0x0 | the random-bracket inventory |
| `MaleFemalePronounBracketsForGenderStrings` (static) | 0x10 | `<his/her>`, `<him/her>` … keyed by gender |
| `HeroPronounBracketsForPointOfViewStrings` (static) | 0x18 | first/second-person variants |

The selected-slot list is the resolver's data model — what a bracket can refer to: `Person`,
`Friend`, `Parent` (a `SimParent`), `Child`, `Sibling`, `Ex` (`SimLover`), `Coworker`,
`Classmate`, `Tenant`, `Senior`, `VampireHunter`, `Swinger`/`SwingerLover`, `RandomFullName`,
`RandomFirstName`, `RandomNameMale`, `RandomNameFemale`, `Surname`, `Insult`, `Alcohol`, `Drug`,
`DrugIllegal`, `CarType`, `StreetPerson`, `BodyPart`, `CountryOther`, `Musician`, `Celebrity`
(+ Male/Female), `CircumstancePublic`, `CircumstanceHome`, `CircumstanceReturn`.

### 29.2 Entry points

| address | method |
|---------|--------|
| `0x73f337959c` | `BracketParser..ctor()` — builds the dictionaries |
| `0x73f337d5e8` | `BracketParser..cctor()` — static tables |
| `0x73f3379e34` / `0x73f3379ee0` | `.ctor(Dictionary)` / `.ctor(Dictionary,Dictionary)` (body, narrative) |
| `0x73f3379640` | `PrepareParser()` |
| `0x73f337a04c` | `GetParsedString(String)` |
| `0x73f337a054` | `GetParsedUniversalStringAndIsBody(String, Boolean)` — the pipeline |
| `0x73f337bb5c` / `0x73f337bb64` | `GetParsedBodyString` / `GetParsedNarrativeString` |
| `0x73f337bf04` | `IsFormattingTag(List<String>)` |
| `0x73f337ce5c` | `ReplaceMaleFemalePronounBracketsInStringForGender(String, Int32)` |
| `0x73f337bd38` | `ReplaceHeroPronounBracketsInStringForPointOfView(StringBuilder, Boolean)` |
| `0x73f337cb28` | `ReplaceIndefiniteArticle(String, Boolean)` |
| `0x73f337d064` | `GetParentString()` |
| `0x73f337d144` | `GetBracketTextPermutations(String, Boolean)` |
| `0x73f337d550` | `CleanBrackets(String)` |
| `0x73f3379fb0` | `ClearCachedRandomBrackets()` |

Inside `GetParsedUniversalStringAndIsBody` the work is split into locals:
`g__ReplaceBrackets|159_0` (`0x73f337c0dc`), `g__ApplyBracketReplacements|159_1` (`0x73f337c24c`),
`g__ReplaceAndCacheRandomNumberBracket|159_2` (`0x73f337c614`), `g__IsArticleBracket|159_3`
(`0x73f337c008`), `g__ReplaceAndCacheRandomBracket|159_4` (`0x73f337c85c`).

Callers on the scenario path: `Scenario.LogNarrativeByBracket(String, Dictionary)` @ `0x73f2ebfcec`
and `Scenario.LogNarrativeByParser(String, BracketParser)` @ `0x73f2ebfc30` — the sentence a life
log shows for a scenario is this parser's output, not the table cell.

### 29.3 What this means for the port

- `scriptedFamilyTokens` covers three of the mapping's entries by hand. Parity means implementing
  the slot table above plus the pronoun/article rules — and most slots are references to people the
  port does not model yet (parent objects, siblings, classmates, coworkers…), so this lands together
  with §28.3's person lifecycle, not before it.
- `<random>` and `<random#>` brackets are **cached draws** (`CacheRandomBrackets`,
  `SelectedRandomBracketList`, `ClearCachedRandomBrackets`). Text substitution therefore consumes the
  same RNG stream the scenario logic does — one more reason an unpushed year cannot be replayed draw
  for draw.
- `ReplaceIndefiniteArticle` and the pronoun tables explain the article/pronoun variation in game lines
  ("**an** EF-5 tornado", "**a** rusty nail") that a naive template would get wrong.


## 30. Newborn setup timing, and what the per-age stats stream can be used for

### 30.1 SetupEverythingElseForNewHero runs on the first age increment, not at birth

- `SimHero.SetupEverythingElseForNewHero` @0x73f2f12048 (file offset 0x2a74048). The 35-year capture
  settles when it runs: game age 0 = `{happiness 0, health 100, smarts 63, looks 18, karma 50,
  discipline 74}` — the pre-setup values — and game age 1 = `{83, 84, 68, 14, 33, 50}`, which is
  `81 + Next(0,20)` for happiness and health plus the smart/looks coin flip. The port used to run it
  inside `Sim::createLife()`, i.e. a year early.
- Port side: `createLife()` now only keeps the 25 ctor draws; `Sim::applyNewbornSetup(Hero&)` holds the
  first-life branch (happiness = health = `81 + Next(0,20)`, then `Next(0,2) != 0` -> smarts
  `10 + Next(0,10)` / looks `61 + Next(0,10)`, else smarts `61 + Next(0,10)` / looks `10 + Next(0,10)`),
  and `Sim::ageUp()` calls it when `h.age == 1`.
- `tools/check_newborn.py` now asks for `ages: 1` and reads the `newborn` block of the JSON, so it
  measures the setup values instead of the ctor values: 60 seeds, **violations 0**, branch split
  33/27, age-0 happiness range 0..97 and health range 1..99 (i.e. age 0 stays ctor).

### 30.2 The newborn block is reported once and never re-folded

- `blsim --script` gained a `newborn` key holding the post-setup snapshot; `tools/port_trace.py` starts
  the age-1 fold from it, so `age0 = hero (ctor)`, `age1 = newborn + that year's row impacts`.
- Folding the setup difference into the event deltas *as well* was measured to double count:
  newborn `{89, 87, 67, 14}` plus deltas `{+12, -13, -69}` gave `final = {100, 87, 54, 0}`
  (clamped) instead of `{90, 87, 67, 14}`. `final` for `ages` 0/1/2 was identical, which pinned it.
- Worth knowing: `AgeUp` has no health field and **no dumb scenario table has a Health Impact
  column** (header check: only Karma / Discipline / Happiness / Intelligence / Appearance Impact 1..4).
  Scenario choices never move health, so nothing was being dropped.

### 30.3 The stats stream is compared as a yearly step, not as a level

- Levels cannot be compared past age 0 (the port's ctor draws and the game's creation path do not
  consume the same RNG stream), so `tools/diff.py` now compares `ps - p_prev` against `gs - g_prev`
  for every age after the first and keeps the level comparison with `stat_offset` only for age 0.
  The detail line carries `"mode": "step" | "level"`.
- Residual scale in the 35-year capture: game age 3 step `karma -33, discipline -30`, port step
  `karma 0, discipline 0`; game age 13 step `karma +16, discipline +8`, port `karma 0`. The port
  moves only what the pushed row moves, which is exactly §28's point: the game also applies family
  lifecycle and health movement the port has no model for yet.

### 30.4 Remaining known gaps

- **Health has no yearly update in the port.** It is set once by the setup and then frozen, while the
  capture runs 100 -> 84 -> 83 -> 87 -> 88 -> 92. Not a dumb-scenario impact (no such column), so it
  comes from the person lifecycle — likely `SimPerson.IncrementAge()` @0x73f30d22ec. Marked with a
  `shortcut:` comment next to `applyNewbornSetup()`.
- **Birth-line sibling gender diverges** from the capture (`older sister` vs the port's `older
  brother`, from `rng_.chance(0.5)`): the ctor draw order is not fully reconstructed, so it is a
  stream-alignment difference rather than a wrong rule.

### 30.5 What counts as passing now

- Not per-year text or per-year stats: every year in the capture has at least one line the port has no
  model for (no year had an identical log set), so equality there is a Phase-2 target, reached through
  §28.3's person lifecycle and §29's BracketParser slots.
- The Phase-1 line is the **pushed row itself**: `tools/row_recover.py` -> `/tmp/blbench/push.json` ->
  `blsim --script` replays the exact table row and choice the game took, and `tools/gap_report.py`
  splits the residue into `expected (unmodelled: <class>)` and `UNEXPLAINED`. The newest 35-year bench
  has 14 residue points and no abort, all classified, with `ScenarioIAPConfiguredPush` the only new
  class name in the adult window.

## 31. The rest of the yearly sets: what is left of the roulette, and what blocks it

### 31.1 Every dumb set, its file and the state it keys on

`a16_dumb-scenarios-*-enc` is a family of 23 tables plus per-occupation variants. Head, columns and
rows say which hero state each one needs:

| file | rows | columns that gate it |
| --- | --- | --- |
| universal | 258 | Hero Min/Max Age, `Student`, `Job`, `Vampire`, `Both Parents Alive`, Hero Gender |
| holiday | 95 | `Type` (Christmas/Halloween/Thanksgiving/New Years), Min/Max Age, Student, Job, Both Parents Alive |
| calendar | 366 | Author, Holiday Name, Month, Date, Student, Min/Max Age |
| career | 419 | `Type` only - the career name the hero must currently hold |
| friend | 37 | Friend Min/Max Age, Friend Gender, Friend Min Craziness |
| orphan | 15 | Min/Max Age, Gender Bracket (hero has no mother) |
| prison | 64 | Min/Max Age, `Royal`, Gender Bracket (hero is in prison) |
| royal | 71 | Min/Max Age, Hero Gender, Hero Has Lover, UK Only, Japan Only (hero is royal) |
| vacation | 59 | `Type`, Gender Bracket |
| actor / album / model / music / sports / zoo / secret-agent / political-campaign / political-office / casino / dealer / hustler / landlord / mafia / business | 10-165 each | occupation columns (`Actor Type`, `Genre`, Min/Max Profile, Min/Max Reputation, Requires Booking Agent, ...) |
| astronaut-dumb / astronaut-mission-dumb / vampire-dumb / casino-casino-dumb and the other one-off `*-dumb-*` files | - | one DLC activity each |

### 31.2 The gate columns are requirement flags, and one of them was read wrong

- Measured value counts: universal `Both Parents Alive` 34 x / 224 blank, `Vampire` 102 x, `Job` 7 x,
  `Student` 28 x; holiday 12 / 13 / 4; calendar 14 / 18 / 27. So **`x` means the row requires the
  condition**, and a blank cell requires nothing.
- `port/src/sim.cpp` `scenarioEligible()` had `if (t.flag(row, "Job") && student) return false;` - an age
  proxy. It is now `if (t.flag(row, "Job") && !h.employed) return false;`, so an unemployed adult no
  longer takes a job-only row (`hero.employed` is declared in `port/include/bl/game.h:80`).
- `Both Parents Alive` needs no work: the port has no parent-death model, so every x row is satisfied.
  `Vampire` x rows are skipped on purpose.

### 31.3 What step 4 actually resolves to

The roulette itself needs no more mapping. Each remaining category is a gate on hero state the port
does not carry yet:

- `k=0` orphan - needs "no mother" (0x73f2eb3574). The port always has a mother, so it re-rolls, which
  is the game's own behaviour for a hero with parents.
- `k=1` prison - `CurrentlyInPrison` `0x73f2f0e1d8`; the port has no prison state. Its not-in-prison
  branch (holiday/universal roulette at 0x73f2eb3734) is already implemented.
- `k=4` career - implemented as `Type == hero.job` over the generic career table. The game additionally
  routes by occupation through `LoadDumbScenarioCareerSet()` (`0x73f2b89790`, 330 instructions) and
  `GetRandomCareerDumbScenarioForCareer(String)` (`0x73f2cbec08`), whose string compares select the
  per-occupation tables above. That is the same blocker as the rest of the list: the port has to model
  the career and the DLC states first.
- `k=5` tail - military (`GetRandomCareerDumbScenarioForMilitaryBranchAndStatus`, next method after
  `0x73f2cbec08`, and there is no military table of its own), royal (`IsARoyal` `0x73f2f0e128`),
  vampire, else universal. The port routes k=5 to universal.

So the remaining work here is **Phase-2 state** (prison, royal, vampire, orphanhood, military branch,
occupation), not another wheel fix. What the wheel needed - the 6-way category draw, the 100-attempt
re-roll, uniform row choice inside a set, and the requirement-flag gates - is in place and measured;
the 35-year bench has no abort and its residue is unchanged after the `Job` fix (14 classified points).

## 32. Impact cells are scores, not points, and health has exactly one writer

### 32.1 AttributeAdjustmentPointsForImpactScore (@0x73f32b140c): the -6..6 table

- Every `Impact<Attribute>` setter funnels through it. `SimPerson.ImpactHealth` @0x73f30e487c reads
  `ldr s8,[x19,#0x8c]`, calls the converter, `scvtf s0,w0; fadd s1,s8,s0`, then clamps.
- The argument is clamped to -6..6 (`cmp w19,#6 / csel`, `cmn w8,#6 / csel`), then a 13-entry jump
  table at 0x73f145fe9c (file offset 0xfc5e9c) holds byte offsets: `[34, 0, 10, 12, 4, 16, 18, 14, 33, 8,
  31, 2, 6]`, with the case bodies based at 0x73f32b1470 and scaled by `lsl #2`.
- Decoded: -6 -> **-100** (offset 34 lands on the epilogue with the preloaded `mov w0,#-0x64` at
  0x73f32b1448), -5 -> -50, -4 -> -30, -3 -> -16, -2 -> -8, -1 -> -4, 0 -> `GetRandomNumber(0,2)` then
  `cneg w0,w1,ne` = **-1 when the draw is non-zero, else +1**, +1 -> 4, +2 -> 8, +3 -> 16, +4 -> 30,
  +5 -> 50, +6 -> 100.
- The dumb tables carry exactly that domain: the 18,613 `*Impact*` cells in `gamedata_plain/a16_dumb-scenarios-*`
  are integers in -6..6 (0 appears 5 times). A `+16` on a stat is score 3, not 16 points.
- Port: `Sim::pointsForImpact(int)` mirrors the table (sim.cpp) and `renderRow()` runs all five cells
  through it before filling `AgeUp`. It used to add the raw cell as a point value. The zero-score draw
  is the converter's only RNG use and is made in the same column order the game uses (Karma ..
  Appearance). This supersedes the "nothing was being dropped" line in 30.2, which was about health
  only: the five cells that do exist were being added at a tenth of their real weight.

### 32.2 The clamp

- After the add, every attribute is clamped to 0..100: `fcmp s1,s0(100); b.ge keep-100`,
  `fcmp s1,#0; b.hi keep`, else 0 (0x73f30e4900-0x73f30e4914). Both application sites in the port
  (`main.cpp`, `script.cpp`) already do this.

### 32.3 Negative happiness is ignored below age 4

- `SimPerson.ImpactHappiness` @0x73f30e4790 opens with `cmp w20,#0; b.gt apply`, then
  `ldr w8,[x19,#0x20] (Age); cmp w8,#4; b.lt return`: a score <= 0 is a no-op while the hero is under 4.
  Not modelled in the port yet.

### 32.4 Health has a single writer, so there is no yearly formula to port

- Full instruction scan of `libil2cpp.so` for float stores to `[reg,#0x8c]`: 75 hits, and the only
  ones inside SimPerson/SimHero are `SimHero.SetupEverythingElseForNewHero` (3 stores),
  `SimHero..ctor`, `SimPerson..ctor(bool)`, `SimPerson.set_Att_health` @0x73f30e0508 and
  `SimPerson.ImpactHealth` (store at 0x73f30e4944).
- `set_Att_health` is never called: hooks on all six `set_Att_*` methods captured **zero** records over
  eight game lives (and the older `attSets` hook over the same methods was empty as well), so the game
  writes the backing field inline and every health change must go through `ImpactHealth`.
- `SimPerson.ImpactHealth(int)` has exactly one static caller, `SimHero.ImpactHealth` @0x73f2f21f0c
  (`bl 0x73f30e487c; b CheckForPerfectStats()`), and that override has no static callers: it is invoked
  through a vtable slot, `ldr x9,[x8,#0x418]; mov w1,w0; blr x9`, from the per-item foreach in
  `SimHero.IncrementAge` @0x73f2efa3b0.
- That item is a `SimAddiction`: `SimAddiction.AnnualHealthImpact()` @0x73f2fa8a3c returns `-[drug+0x2c]`,
  i.e. the annual health damage of the drug behind the addiction.
- Consequence: the yearly +-1 drift is not a gradual-decay formula - a score of 0 converts to +-1 - and
  every larger step needs an ImpactHealth call from an addiction, a disease
  (`UpdateWithFatalDisease` @0x73f2fb0d30) or a lifecycle event. Health stays a birth-time value in the
  port until those systems exist; the dumb scenarios still never move it.
## 33. `<rb>` and `<rbN>`: the scenario-local draw cache

### 33.1 The token is a back-reference, not a pool

The obvious reading of `<rb>` - "some named random pool" - is wrong, and the game's own tables say so:
a row's Body draws an inline alternative (`<candy bar/lollipop/cookie/cupcake>`) and the same row's
Narrative 1 says `I threw myself on the floor kicking and screaming because I didn't get the <rb> I
wanted at the grocery store.` The narrative has to print the alternative the body already drew, so
`<rb>` must reference a draw that already happened in this scenario render.

Token census over `gamedata_plain` (all 1071 tables): `<rb>` 6124, `<rb2>` 1134,
`<rb1>` 305, `<rb3>` 99, `<rb4>` 4. 255 tables use them; the heaviest are
`a16_sports-practice-scenarios-enc` (229), `a16_clubupdateufldumb-enc` (220),
`a16_fight-ufl-dumb-scenario-enc` (220), `a16_dumb-scenarios-career-enc` (212),
`a16_pet-discipline-enc` (210), `a16_podcast-dumb-enc` (208) and
`a16_dumb-scenarios-universal-enc` (155).

The index is the **draw ordinal**, 1-based, with `<rb>` = `<rb1>` = the first draw. In
`a16_dumb-scenarios-universal-enc` the pairing is clean: 33 rows have one inline bracket and use
`<rb>`, 2 rows have two brackets and use `<rb>`+`<rb2>`, 1 row has three brackets and
uses `<rb>`+`<rb2>`+`<rb3>`. Body and log line share one cache, so the same draw is
visible in both.

### 33.2 Runtime picture

`BracketParser` (il2cpp_dump.txt) carries the two dictionaries and the two lists that make this work:

- `0x10 Dictionary<String,String> <BodyBracketDictionary>k__BackingField`
- `0x18 Dictionary<String,String> <NarrativeBracketDictionary>k__BackingField`
- `0x138 List<Dictionary<String,String>> SelectedNumberBracketList`
- `0x140 List<Dictionary<String,String>> SelectedRandomBracketList`
- `0x148 Boolean CacheRandomBrackets`, static `0x0 Dictionary<String,Int32> RB_Brackets`
- `.ctor()` @0x73f337959c, `.ctor(Dictionary<String,String>)` @0x73f3379e34,
  `.ctor(Dictionary<String,String>,Dictionary<String,String>,Int32,SimPerson)` @0x73f3379ee0,
  `ClearCachedRandomBrackets()` @0x73f3379fb0, `.cctor()` @0x73f337d5e8,
  `<GetParsedUniversalStringAndIsBody>g__ReplaceAndCacheRandomBracket|159_4` @0x73f337c85c.

Dumping a live instance (`pfields`) shows the two dictionaries are small *conditional* maps
(count 2 in the sampled parser), not word pools: the alternatives come from the table cell being
parsed, and only the drawn value is written back and cached. The pool lists are indexed per
template kind - list index 0 is the app-start/notification dictionary (InstallToken, DateOfInstall,
GameLaunchedBefore, IsFirstSession, GDPRAccept, UserBoughtBitizenship, UserBoughtGodMode, ...), which
is why a naive dump of `SelectedRandomBracketList[0]` looks like a settings blob.

Hooking `GetParsedUniversalStringAndIsBody` @0x73f337a054 yields raw templates before
substitution (`a1` is the unparsed text, `a2` is the isBody flag).

Method needed to hook any of this: `findMethodHier` returns a **MethodInfo pointer**, not code,
so it has to be dereferenced once (`m.readPointer()`); attaching to the MethodInfo is what
produces Frida's "unable to intercept function ... please file a bug". Runtime address = dump address
+ module load bias, and the bias differs per run (0x3b5bc2000, 0x3b7409000, 0x3d020c000, 0x3b6d98000), so
every hook is computed as `codeOf(anchor) - anchor_dump_addr`.

### 33.3 Port implementation

- `include/bl/sim.h:134-139`: `std::vector<std::string> rndCache_;` - draws cached in draw order
  for one scenario render, shared by Body and every log line.
- `src/sim.cpp` `Sim::resolve`: the `<a/b>` branch records the drawn alternative
  (`if (rndCache_.size() < 8) rndCache_.push_back(drawn);`), and a new branch handles
  `<rb>`/`<rbN>` - `n = token.size()==2 ? 1 : atoi(token.substr(2))`, printing
  `rndCache_[n-1]` and leaving the token verbatim when it is out of range.
- Cache lifetime: cleared at the top of `Sim::ageUp` (one scenario per year) and again at the top
  of `Sim::renderRow` (the body parses first, the log lines reuse what it drew).

### 33.4 Verification

Forced-row replays (`./build/blsim --script`, seed 12345, US male), comparing the Body's drawn
alternative against the log line:

- row 22 `Rejection`: body `mother is refusing to buy you a cookie while on a shopping trip ...`
  -> log `I threw myself on the floor kicking and screaming because I didn't get the cookie I wanted`.
- row 23 `Toy Time`: body `... picks up your favorite stuffed animal.` -> log
  `I gave my stuffed animal to another child I was playing with.`
- row 29 `Game Shop`: body `... a game for your Xbox One.` -> log
  `I bought "Minecraft" for my Xbox One at a video game store.`

Two bracket rows pin the indexing down (only two such rows exist in the universal table):

- row 203 `Elderly Fang Gang`: brackets are `<Vampire/Bloodsucker/Twilight/Bloodstained>` and
  `<Chronicle/Gazette/Daily/Star/Dispatch/Times>`; the port draws `Twilight` + `Gazette` and
  the log `I explained to The <rb> <rb2> that ...` renders as `I explained to The Twilight Gazette that ...`.
- row 225 `Easy A`: brackets are `<project/presentation>` and the long elective list; the log's
  `<rb2>` appears **before** `<rb>`, and it still resolves to the *second* draw
  (`... persuade our language arts elective professor ...`), proving the index is the draw ordinal and
  not the order of appearance in the text.

No `<rb` token survives in any of these renders, and the `/tmp/trace_c10.json` bench still replays
without abort. That trace contains no `<rb>` row, so it does not cover this change; the forced-row
replays above are the coverage.

## 34. The token vocabulary: deterministic slots, reference pools, gender brackets

### 34.1 Two vocabularies in one game

The yearly loop tables and the newer `genesis_*` tables do not speak the same dialect. Census over
`gamedata_plain` (1071 files):

- The twelve tables the port wires into the loop hold 4266 bracket tokens and **not one** `grammar:`,
  `pick:`, `special:`, `function:`, `hero:` or `<reference:*>` token. They are all legacy
  hyphenated word brackets (`<friend>`, `<rb>`, `<num6-20>`, `<currency5000-550000>`).
- The 77 `genesis_*` files carry the modern families: `reference` 22424, `pick` 5716,
  `special` 1024 (all but a handful are `special:here:rp1|rp2|rp3`), `grammar` 478, `br` 267,
  `cache.reference` 215, `his/her` 136, `he/she` 63, `him/her` 74, `hero:` 49,
  `number:` 33, `function:` 43, plus movie-director data paths (`film.cast.lead.first_name` 3033,
  `film.cast.lead.role_type` 2780, `film.title`, `focus_data.*`).

Both dialects go through the same resolver in the port.

### 34.2 Deterministic slots now implemented

| token | semantics | source of truth |
|---|---|---|
| `<br>` | newline | `genesis_master_tags_format`, and game log lines contain a raw \n where the row has `<br>` while `<br` occurs 0 times in any capture |
| `<grammar:a\|an>`, `<grammar:a/an>`, `<grammar:A/An>`, `<grammar:A\|An>` | article agreeing with the following word | `genesis_master_tags_grammar`; 411/54/8/5 occurrences of the four spellings |
| `<pick:a\|b\|c>` | uniform pick | `genesis_master_tags_pick`; 1690 distinct pick tokens |
| `<number:naturalX-Y>`, `<number:currencyX-Y>`, `<numX-Y>`, `<currencyX-Y>`, `<decnumX-Y>` | number, money, one-decimal | `genesis_master_tags_number` |
| `..._roundup_10\|100\|1000` | draw, then round **up** to the step: `((v+N-1)/N)*N` | doc example 6413 -> 6500 |
| `<hero:...>`, `<hero-name>`, `<city>`, `<country>`, `<country-other>`, `<first name>` | the hero's own state | `genesis_master_tags_hero`; row 146 `<musician> is playing a concert in your <city>` |
| `<candidate.he\|she>`, `<candidate.his\|her>` | scoped pronoun | 354 / 48 occurrences in `genesis-md-audition-enc` |

The article rule needs look-ahead: the resolver continues the walk into the rest of the cell to find
the word the article precedes, then returns (`src/sim.cpp` `Sim::resolveInto`, the `grammar:` branch).
Because it re-enters the walk with the *same* memo, a token drawn later in the line is not redrawn.

Ordering inside `resolveInto` matters: `grammar:` is tested before the `/\|\|>` alternative branch
(the token itself contains `/`), and the gender branch is tested before it too (see 34.4).

### 34.3 Reference pools, and pools of pools

`genesis_master_tags_reference_2` (21752 rows) is a two-column-in-disguise table: the **`tag_id` cell
holds the token itself** (`<reference:random_last_name>`, 6672 rows) and `tag_value` the word. 40 tags
exist: `random_last_name` 6672, `random_first_name_male` 6150, `random_first_name_female` 5896,
`random_person`/`random_person_indefinite` 317 each, `celebrity_male` 264, `musician` 216,
`celebrity_female` 189, `insult` 141, `body_part_gender_neutral_can_be_injured` 56, `alcohol` 39,
`drug` 27, `drug_illegal` 14, `arson_location` 367, ...

Two properties matter:

- **Weighted**: the `weight` column is the draw weight, not a sort key.
- **Recursive**: `<reference:random_first_name>` has exactly two rows, whose *values* are
  `<reference:random_first_name_female>` and `<reference:random_first_name_male>`. The port re-draws
  while the drawn value still starts with `<reference:`, depth-capped at 4 (`Sim::referenceToken`).
- `<body part>` (the spaced spelling the death table uses) and `<body-part>` both alias
  `body_part_gender_neutral_can_be_injured`; `-` and `_` are interchangeable in the key.

The gender-scoped slots the game fills per life are visible in a live parser's constructor dictionary
(captured from the device, report 33 methodology): `<country>` 'Costa Rica', `<city>` 'San Jose',
`<country possessive>` "Costa Rica's", `<first name>` 'Pablo', `<male name>` 'Christian Estrada',
`<body part>` 'fist'. Those with spaces in the name are the game's own keys and are *not* what the
tables write; the tables write the hyphen form.

### 34.4 Gender brackets are not random draws

`<he/she>` (12087), `<his/her>` (5504), `<him/her>` (4715), `<He/She>` (577), `<his/hers>` (30),
`<himself/herself>` (28) look like the `<a/b>` random-alternative form, and the `/` branch would
have drawn them at random. They are not random:

- `il2cpp_dump.txt:1067` - `static Dictionary<String, Dictionary<GENDER,String>>
  MaleFemalePronounBracketsForGenderStrings`
- `il2cpp_dump.txt:1068` - `static Dictionary<String, Dictionary<Boolean,String>>
  HeroPronounBracketsForPointOfViewStrings`
- `ReplaceMaleFemalePronounBracketsInStringForGender(String,Int32)` @0x73f337ce5c and its
  `StringBuilder` overload @0x73f337bb6c; `ReplaceHeroPronounBracketsInStringForPointOfView` @0x73f337bd38
- `PRONOUN_TYPE` (il2cpp_dump.txt:22199) enumerates `HE_SHE` and `HIS_HER`
- `static List<String> FormattingTags` @0x8 is the markup list (`br`/`b`/`i`/`q`)

The game passes the gender of the person the sentence is about. The port keys on the hero, which is the
same person in every table it renders; a line about a *third* party sharing the hero's gender is
indistinguishable, and one about a third party of the other gender would be wrong. Verified renders
(`a16_date-rejections-enc` row 39, forced row, seed 12345): male -> `he says he is focusing on himself
right now.`, female -> `she says she is focusing on herself right now.`

### 34.5 Verification

Forced-row replays (`./build/blsim --script`, seed 12345):
`<city>` -> 'Oklahoma City', `<grammar:a|an>` -> 'dressed as a scullery maid', `<pick:...>` -> one
alternative, `<currency5000-550000>` -> '$210,819' / '$356,508', `<decnum100-105>` -> '101.6',
`<drug-illegal>` -> 'magic mushrooms', `<body-part>` -> 'anal cavity', `<hero-name>` -> 'Beyawnce Gnolls'
(stable inside one line, different lines may differ), `<insult>` -> 'a coward'.

### 34.6 Deliberately still verbatim

Unresolved tokens keep their literal `<...>` text, so a residual-token count over a run is a truthful
coverage metric. Currently outstanding, with the evidence that stopped each one:

- `<car-type>` (hero-filler rows 172-179, condition `Car`: `I jammed my <body-part> in the door of my
  <car-type>.`). `BracketParser` has `String <SelectedCarType>k__BackingField` at 0xe0, so the value is
  one car per life, but which pool fills it is unknown: `a16_car-models-enc` (186 rows, Make/Model/Type)
  is the only name pool found, and it is not referenced from any scenario table.
- `<circumstance-public>` (universal rows 13/14/17/18, as a sentence-opening adverbial:
  `<circumstance-public>, the button on your pants popped off!`). The only pure adverbial pool in the
  data is `a16_fight-er-circumstance-enc` (42 rows, all boxing), and it does not fit the universal
  rows; `SelectedCircumstancePublic`/`...Home`/`...Return` exist as backing fields (0x120-0x130).
- `<restaurant>`: `a16_public-places-enc` (223 rows) contains restaurant-ish entries, but no
  `Restaurant` column exists anywhere in `gamedata_plain`.
- `<action>`, `<restaurant>`, `<maker>`, `<familiar>`, `<castle>`, `<addiction>`, `<hero-title>`,
  `<suffix>`: no matching `<reference:*>` tag among the 40, and no obvious table.
- `<cache.reference:...>` / `<cache.arson_location.follow_up_a>` (`genesis_master_tags_reference_1`
  declares the cache mechanism): belongs to the `genesis_*` tables, none of which the loop renders yet.
- `formatting`: `<b>`/`<i>`/`<q>` and their closers occur only in `genesis_entity_showcase` (a dev
  demo) and the format doc table, so nothing real depends on them yet. The `/`` branch now refuses to
  treat a `</b>` as an alternatives list.

The per-life `<Selected*>k__BackingField` strings (0xd8-0x130, `il2cpp_dump.txt`) - `SelectedDrugIllegal`,
`SelectedCarType`, `SelectedStreetPerson`, `SelectedBodyPart`, `SelectedCountryOther`,
`SelectedMusician`, `SelectedCelebrity`, `SelectedCelebrityMale`/`Female`,
`SelectedCircumstancePublic`/`Home`/`Return` - name exactly the slots the remaining tokens read from,
and they are plain strings on the parser, so a live parser dump can be compared against the port once the
movie-director tables are rendered.

## 35. The death sentence, and why it had to reach the JSON

`a16_death-types-enc` (Type, Commonness Score, Min Age, Sentence) uses `<pronoun>` 110 times and
`<body part>` 3 times:

- `<pronoun> died after being bit on the <body part> by a venomous snake.`
- `<pronoun> died after contracting a rare <body part> condition.`
- `<pronoun> died from cancer of the <body part>.`

Those 113 tokens are the only place `<pronoun>` is exercised, and until now no run could show them:
`src/script.cpp` called `checkDeath` only to flip `life.alive`, so the sentence was dropped in
`--script` mode while the text mode printed it (`src/main.cpp:311`).

The sentence is now reported as `final.death` (`{"type": "...", "sentence": "..."}`,
`src/script.cpp`), present only when the life died. Verified renders (seed / gender / result):

- seed 777 male, age 69: `Natural` - `he died while sleeping peacefully.`
- seed 99 male, age 76: `Disease` - `he died after contracting a rare thigh condition.`
  (`<body part>` -> 'thigh', via `body_part_gender_neutral_can_be_injured`)
- seed 4242 male, age 87: `Fluke` - `he died from alcohol poisoning.`
- seed 77 female, age 71: `Fluke` - `she was killed in a tornado.`
- seed 512 female, age 66: `Fluke` - `she died after a tree fell on the home.`

Both pronoun values and both `<body part>` paths are now observable, which is what makes the death table
comparable against a capture at all.


## 36. Where the per-person lifecycle lines come from (next work item)

§28 established that a year is several lines and that they are code templates, not table rows. The
template inventory is now pinned down; the literals live in `global-metadata.dat` and are listed by
`tools/metadata_strings.py grep` (71223 literals). A contiguous block around #9372-#9429 is the
per-person sentence set, written with bracket slots rather than `{0}` placeholders:

| index | literal |
|---|---|
| #9372 | `<person>` |
| #9373 | `<person> <baby-birth>.` |
| #9374 | `<person> and <spouse> divorced.` |
| #9375 | `<person> announced <his/her> retirement as a professional athlete.` |
| #9379 | `<person> began studying <major> at university.` |
| #9385 | `<person> graduated from <high-school>.` |
| #9386 | `<person> graduated from <postuniversity>.` |
| #9387 | `<person> graduated from community college.` |
| #9388 | `<person> graduated from university with an undergraduate degree in <major>.` |
| #9389 | `<person> has been diagnosed with <disease-new>.` |
| #9390 | `<person> has been promoted to <job-title>.` |
| #9391 | `<person> has been promoted to <military-rank>.` |
| #9394 | `<person> is no longer suffering from <disease-old>.` |
| #9405 | `<person> started <high-school>.` |
| #9407 | `<person> started <postuniversity>.` |
| #9408 | `<person> started a new position as <job-title> for <company>.` |

58 literals contain `<person>` overall. The hero-first-person variants are the `{0}` format strings the
game feeds through `string.Format` (e.g. #23450 'I am suffering from {0}.'`, #50732
'Your {0}, {1}, is no longer suffering from {2}.'`, #50446 'Your parents just had a baby {0} named
{1}, your new {2}{3}.'`), and their slots are filled from the same state.

So the blocker is not the templating (the resolver already handles every slot form these use, including
the gender brackets of 34.4) but the **state**: a `person` needs an identity, a relation word ("big
brother", "mother"), a school/grade, a disease pair, a job title and a spouse. That is the Phase-2
person model §31.3 also asks for (prison, royal, vampire, military branch, occupation).

Full grepped inventory: `/tmp/qdir/lifecycle_templates.txt`.


## 37. Per-person lifecycle lines: the family model behind them

Between the scenario rows a life also prints lines that belong to *other people*:
`My big brother, Deejay, started elementary school.` All 58 such sentences live in
global-metadata.dat as literals #9372-#9429 and are keyed on a `<person>` slot,
which is why they could not be emitted before the port had a person model at all.
Section 36 listed the templates; this section records the model, the two table
families they read from, and what the port now reproduces.

### 37.1 Two voices, one sentence

The scenario Body addresses the player in the second person and the log line that
follows it repeats the same content in the first person:

| where | text |
| --- | --- |
| Body | `Your little brother, Jimbo, has been diagnosed with mad cow disease.` |
| log | `My little brother, Jimbo, has been diagnosed with mad cow disease.` |

The port emits the first-person form (the log line); the second-person form is the
`<person>` Body variant the game generates for the same event. Parents are never
named in these lines (`My mother has been promoted to Tour Operator.`), while
siblings always are (`My big brother, Peyton, ...`, `My half sister, Inka, ...`) —
that is not a port choice, it is what every captured trace shows. So `Person::name`
stays empty for parents and the emitter adds the comma pair only when a name exists;
the first attempt emitted `My mother, has been ...` for exactly this reason.

Relation words observed in trace_game2/game3/game9/fresh: `my mother`, `my father`,
`my stepfather`, `my big brother`, `my little brother`, `my half sister`. The
port builds `"big "/"little " + ("sister"/"brother")` at birth and `"half sister"` /
`"half brother"` for the baby lines, matching the traces.

### 37.2 The model

```c++
struct Person {           // include/bl/sim.h
  std::string relation;   // the word after "my " — "mother", "little brother", ...
  std::string name;       // empty for parents, the game never names them here
  std::string lastName;
  bool female;
  int age;
  bool alive;
  std::string disease;    // the state pair behind diagnosed/no-longer-suffering
  std::string job;
  std::string company;
};
```

`Life::family` holds the mother, the father and every sibling born so far.
`Sim::lifecycleLines(Life&)` is called once per year from `Sim::ageUp`, right
after `++h.age`, and returns the year's lines into `AgeUp::extra`; `logLines()`
in src/script.cpp writes `extra` before the scenario's own log line, which is the
order the traces show. Each family member contributes at most one line per year
(the traces do show two lines for two different people in one year, e.g. mother
diagnosed and father bunions at age 8).

### 37.3 Tables behind each line

- Disease lines: `a16_disease-types-enc` (107 rows). The port draws a row and
  applies the article rule from that row's own `Uses Indefinite Article` column,
  which is why the output reads `a pink eye` / `an e. coli` and not a blanket
  `a`.
- Job lines: `a16_careers-enc` (169 rows) for the title ladder, and
  `a16_company-name-structure-enc` (105 rows) for the employer. The company cell is
  a template over the pools beside it: `Aircraft Dealer` stores
  '<color> <suffix>, <city> <suffix>'` — two comma-separated *alternatives*, with
  `<suffix>` and `<color>` drawn from that same row's Suffix column and from
  `a16_company-name-color-enc`. Substituting a whole row's Format verbatim produces
  the literal '<color> <suffix>'` in the line; the observed live company names are
  single-word or `<city> <suffix>` shapes (`Helsinki Travel`, `Shears Air`,
  `Oklahoma City Family Medical Center`).

### 37.4 Order, and the `out = AgeUp()` trap

Milestones are pinned to one age — school at 6, graduation at 18 — while illness is
a chance draw. When illness ran first it could take the year the milestone needed
and the milestone never happened at all, so the emitter order is now
newborn → 6 → 18 → disease/recovery → work.

The second trap is that four places in `renderScenario` reset the event with
`out = AgeUp();` before filling the row in, which silently discarded the year's
family lines that `ageUp` had already written. All four now save and restore
`out.extra` around the reset. The symptom was a debug print showing
`extra 1` inside `ageUp` and zero family lines in the JSON.

### 37.5 Verification

`./build/blsim --script /tmp/qdir/lifetest.json` (seed 12345, US male, 45 years)
produces 34 family lines, none malformed. Samples:

```
age  3  My big brother, Deejay, started elementary school.
age  8  My mother and my father had a baby boy named Ron Dot, my new brother. He was an accidental pregnancy.
age 14  My little brother, Ron, started elementary school.
age 15  My big brother, Deejay, graduated from high school.
age 27  My little brother, Ron, started a new position as Pharmacist for Oklahoma City Family Medical Center.
```

The ladder path was verified across seeds 1/7/4242/31337/555/90210; seed 4242 walks
a real Seniority Track:

```
age 34  My little sister, Beyawnce, started a new position as Jr. Flight Attendant for Shears Air.
age 49  My little sister, Beyawnce, has been promoted to Flight Attendant.
age 59  My little sister, Beyawnce, has been promoted to Sr. Flight Attendant.
```

### 37.6 Gaps, stated rather than hidden

- `Seniority Track` is a real ladder for the careers that name their rungs
  (`Actor`, `Chef`, `Flight Attendant`, `Model`, ...) and a class word for the rest:
  `None` 57 rows, `Abbreviated` 29, `Working Class` 24, `Corporate` 10. The port
  has no evidence of the per-class ladders, so those career starts use the career's
  own name as the title.
  `shortcut: class-word tracks ('None'/'Abbreviated'/'Working Class'/'Corporate') do not produce a ladder; upgrade when that ladder table is located.`
- Promotion is a 0.05/year draw gated on the job being a rung of a real ladder, so a
  short life often shows none. That is the mechanism, not a missing feature.
- School lines cover the two pinned ages (6 start, 18 graduation) and use
  `elementary school` in the US versus `primary school` elsewhere, matching
  trace_fresh against trace_game3. College, majors and postgraduate rows exist in
  the metadata (#9379/#9385/#9407) but need the education state, which is still open.
- The gender used for a pronoun inside a family line is the hero's gender; the game
  passes the gender of the person the sentence talks about, so a third party of the
  opposite sex to the hero would be described wrongly. Same limitation as section 34.4.


## 38. The circumstance pools are built in code, not in a CSV

'<circumstance-public>' is the last token the wired tables could not resolve. It has no
table: 'DataHub.GetRandomCircumstance()' (@0x73f2ce2d08, file offset 0x2848d08) is a switch
over 'Random.Range(0,43)' — 'cmp w0, #0x2a; b.hi' — whose arms each load one string literal
from the metadata usage cells at 0x5c55cf8..0x5c56120 and return it. Nothing in
gamedata_plain names a public circumstance scene; the 42 rows of
a16_fight-er-circumstance-enc are the fighter pool only.

Reading the pool instead of guessing it: the getter takes no 'this' in its body (it calls
'Random.Range' and returns a literal), so a Frida 'NativeFunction' built on the function
pointer can be invoked directly, once per sample. 500 calls per pool give every scene and
its rough weight. The harness gained two throwaway RPCs for this:
'x_circ <arg>' (one call) and 'x_sample <dump_addr> <n>' (n calls plus a tally), both in
blh.js next to the existing hooks.

Pools (sampled live; the raw tallies are in circumstance_pools.txt):

| pool | function | slots | sampled distinct |
|---|---|---|---|
| public | 0x73f2ce2d08 | 43 = 41 scenes + 2 '<musician> concert' | 60 (41 scenes, 19 artist draws) |
| home | 0x73f2ce322c | 56 = 55 scenes + 1 '<musician>' line | 66 |
| return | 0x73f2ce3888 | 53 = 50 scenes + 3 '<musician>' lines | 83 |

The artist slots draw from a16_musicians-enc — the same table '<musician>' already used —
and the game adds the article itself: sampled output contains 'On your way to an Usher
concert', 'While at an Elton John concert', 'from an Ozuna album launch event',
i.e. 'a'/'an' by the first letter of the name, exactly like '<musician-indefinite>'.

### 38.1 public (41 scenes + 2 artist slots)
- During a baseball game
- During your lunch break
- On your way to 7-Eleven
- On your way to the airport
- On your way to the dentist
- On your way to the movie theater
- On your way to the shooting range
- While at a cricket match
- While at a festival
- While at a funeral
- While at a gay pride festival
- While at a golf tournament
- While at a hockey game
- While at a party
- While getting a manicure
- While having a picnic
- While on a hike in the mountains
- While on a trip to the mall
- While out fishing
- While out for a walk
- While out getting a coffee
- While out grocery shopping
- While out jogging
- While out riding your bike
- While out running errands
- While playing chess at the park with an old man
- While returning a book to the library
- While returning a movie to a Redbox
- While riding a Segway
- While running the bases in a kickball game
- While shopping at the sex toy store
- While sitting at the train station
- While sitting on a park bench
- While strolling through the park
- While taking out the trash
- While trimming the bush
- While waiting at the bus stop
- While waiting for food at the drive-thru
- While waiting for your Uber driver
- While walking downtown
- While walking through the cemetery

Artist slots: On your way to <artist> concert (7), While at <artist> concert (12)

### 38.2 home (55 scenes + 1 artist slot)

- While at home cathartically shouting out obscenities
- While at home creepily watching the neighbors from a window
- While at home experimenting with how many grapes you can fit in your mouth
- While at home fantasizing about your dream vacation
- While at home having a conversation with your imaginary friend
- While at home massaging your feet
- While at home plunging a clogged toilet
- While at home practicing your foreign accents
- While at home recording YouTube videos
- While at home stalking people on the internet
- While at home watching scary airplane landing videos on YouTube
- While at home watching street fight videos on YouTube
- While at home working on your twerking skills
- While at home wrapping a gift for a friend
- While changing the air filter in your air conditioner
- While checking yourself out in your full size mirror
- While cleaning some moldy food out of the refrigerator
- While cooking a healthy, yet savory, dinner
- While curled up on the couch playing BitLife
- While dancing alone around your bedroom
- While exfoliating the dead skin on your face
- While folding the laundry
- While in bed fantasizing about your celebrity crush
- While in the bathroom plucking your eyebrows
- While in your bathroom clipping your overgrown toenails
- While in your bedroom pretending to be in a music video
- While kicking back watching sports from your couch
- While laying in bed at night experiencing insomnia
- While lying on the couch at night
- While making an egg salad in your kitchen
- While making your bed in the morning
- While meditating in your living room
- While mopping the floor at home
- While planting geraniums in the garden
- While practicing yoga in your yard
- While putting away dishes in the kitchen
- While reading in bed at night
- While relaxing and listening to music at home
- While relaxing on the couch Googling yourself
- While reorganizing the silverware drawer
- While scrubbing the toilet in your bathroom
- While sitting on the toilet trying to work out your constipation issues
- While stepping out of the shower fully nude
- While taking a nap on your couch
- While taking out the stinky trash at home
- While taking selfies in your bed
- While tossing a salad in your kitchen
- While up late watching the Home Shopping Network
- While using the bathroom late at night
- While vacuuming the house
- While walking around the house talking to yourself
- While walking around your house naked
- While washing the dishes
- While watching the news in your living room
- While watering the plants at home

Artist slots: While chilling in bed listening to <musician> (11)

### 38.3 return (50 scenes + 3 artist slots)

- after a baseball game
- after a bike ride
- after a cricket match
- after a festival
- after a funeral
- after a gay pride parade
- after a golf tournament
- after a hike in the mountains
- after a hockey game
- after a party
- after a picnic
- after a walk
- after getting coffee
- after grocery shopping
- after jogging
- after running errands
- after the dentist
- after the library
- from 7-Eleven
- from a baseball game
- from a bike ride
- from a cricket match
- from a festival
- from a funeral
- from a gay pride parade
- from a golf tournament
- from a hike in the mountains
- from a hockey game
- from a manicure
- from a party
- from a picnic
- from a trip downtown
- from a walk
- from fishing
- from getting coffee
- from grocery shopping
- from running errands
- from the airport
- from the cemetery
- from the dentist
- from the drive-thru
- from the library
- from the mall
- from the movie theater
- from the park
- from the sex toy store
- from the shooting range
- from the train station

Artist slots: after <artist> concert (6), from <artist> album launch event (23), from <artist> concert (8)

### 38.4 Port

src/sim.cpp keeps the 41 public scenes in 'kCircumstancePublicScenes' and adds
'Sim::circumstancePublic()': 'rng_.below(43)' picks a slot, the last two slots render
'<musician>' inside the two 'concert' templates, and 'articleFor()' supplies 'a'/'an'.
resolveInto routes '<circumstance-public>' there and now also renders
'<musician-indefinite>' as '<article> <musician>'. The '<circumstance-home>' and
'<circumstance-return>' scenes stay verbatim: no wired table uses them, and their lists
are preserved in 38.2/38.3 for when the royal/home tables get wired.

### 38.5 Verification

'rescensus.py' (12 seeds x 4 country/gender, 95 years each) now reports
'distinct residue tokens 0 total 0', down from '<musician-indefinite>' 197 and
'<circumstance-public>' 44. Forced-row checks: 'While at a hockey game, a fanatic, crazy
social media influencer runs up to you...' and
'Your record label asks you to create a song that samples heavily from a David Guetta
track.' / '... samples heavily from an Imagine Dragons track.' / 'an Arcade Fire track'
(vowel article). bench.sh on /tmp/trace_c10.json still runs 35 years without abort.

## 39. Coverage, three ways: what is recoverable, what is menu, what is code

The 23% figure in section 15/31 was one number over one denominator, and it made
the remaining work look like a matching problem. It is not. Splitting the captured
scenarios by *what kind of thing the game was doing* separates three different
bottlenecks that need three different pieces of work.

### 39.1 The three buckets

The bucketing lives in `tools/scenario_classes.py`: a captured instance is a **minigame**
when the class name matches `game|casino|eyeexam|paywall|profilehero|carrepair`, a
**menu** when the body reads like a choice prompt
(`what will you do?|will you join|how will you|what do you do?`), and a **narrative**
line otherwise. Over the 90 captured instances in `obs_corpus`:

| bucket | captured | recovered by a push | share |
|---|---|---|---|
| narrative | 41 | 26 | 63% |
| menu | 28 | 9 | 32% |
| minigame | 21 | 1 | 5% |
| **total** | **90** | **35** | **39%** |

The classes behind the narrative bucket: `Scenario` 10,
`ScenarioScriptEventSiblingBorn` 5, `ScenarioSchoolEnroll` 4,
`ScenarioFriendMeet` 4, `ScenarioPetDeath` 3, `DumbScenario` 3,
`ScenarioScriptEventDiseaseContract` 3, `ScenarioFriendNew` 2, then one each
for `ScenarioCrimePickpocket`, `ScenarioPushSpendTime`, `ScenarioBase`, `ScenarioFailingEyesight`,
`ScenarioSocialMediaJoinComplete`, `ScenarioRetentionLover` and `ScenarioLoveDate`. The menu
bucket is 21 classes, dominated by `ScenarioRetentionCrime` 3, `ScenarioStepparentCruelty` 2,
`ScenarioRetentionFreelanceGig` 2, `ScenarioChildhoodVaccination` 2 and
`ScenarioIAPConfiguredPush` 2. The minigame bucket is 9 classes: `ScenarioCasinoBet`,
`ScenarioGameBlackjack` and `ScenarioCasinoOutcome` at 5 each, then `OpportunityCasino`,
`GameEyeExam`, `EyeExamOutcome`, `ProfileHero`, `CarRepair` and
`MiniPaywall` at 1 each.

The 32% and 5% are not matching failures. The port has no phone menu layer at
all - no list of options to push a button in - and no modal minigame screen, so
there is nothing for a push to address. Those two buckets are implementation
work, and section 31.3 already lists it.

### 39.2 Why matching cannot close the narrative bucket either

Section 35/36 showed the per-person lifecycle sentences are literals in
`global-metadata.dat`. The same test applied to the 35 unrecovered gap classes
(script `/tmp/qdir/meta_match.py`: take every 4-gram window of the captured body and
look for it as a substring of the 76084 literals dumped by
`python3 tools/metadata_strings.py list --limit 100000 --raw`) hits in **32 of 35**.

Examples, with the literal index from that dump:

| class | literal |
|---|---|
| `ScenarioScriptEventSiblingBorn` | #50446 `Your parents just had a baby {0} named {1}, your new {2}{3}.` |
| `ScenarioFriendNew` | #43518 `Through the power of your Friendship Bracelet, you are now friends with` |
| `ScenarioSocialMediaJoinComplete` | #49321 `You signed up for a SoundCloud account.` |
| `ScenarioGameBlackjack` | #21370 |
| `ScenarioGameEyeExam` | #39921 |
| `ScenarioEyeExamOutcome` | #48452 |
| `ScenarioFailingEyesight` | #50619 |
| `ScenarioLoveDate` | #46627 |
| `ScenarioOpportunityCasino` | #39445 |
| `ScenarioPetDeath` | #50413 |
| `ScenarioPushSpendTime` | #19115 |
| `ScenarioRetentionLover` | #49993 |

Only `ScenarioBase`, `ScenarioIAPConfiguredPush` and
`ScenarioScriptEventDiseaseContract` have no literal: the first two are store and
bookkeeping paths with no narrative, the third mixes an authored template with a
CSV draw.

So the sentence is composed in C# and never existed as a row to find. A body-based
fallback was added to `tools/row_recover.py` anyway - `find_row_body()`, which searches
only the candidate tables from `obs_corpus/scen_source.json` and ranks rows by how
much of the captured body their long words cover, requiring a 0.7 share - and it
is worth keeping for future traces, but it moved the count by exactly one
(21 -> 22) and **no gap class reaches even 0.5**. For 20 of those classes the
candidate list is empty: the scene has no table behind it at all.

### 39.3 What did move: the registry of code-composed scenes

The lever that worked is on the port side, not the matcher side. `row_recover.py`
keeps a `REGISTRY` of scenario classes that are *rendered by the port itself*
(`Sim::renderScenarioClass`, section 31.1) rather than by a pushed table row. For
those, a recovered instance is `{"scenario": <class>, "choice": <n>}` instead of
`{"table": ..., "row": ...}`. The registry held 7 classes; every class the port
already rendered but the tool could not name was still being counted as a
failure. Six were added - `ScenarioScriptEventSiblingBorn`, `ScenarioPetDeath`,
`ScenarioScriptEventDiseaseContract`, `ScenarioFriendNew`,
`ScenarioScriptEventParentsBreakUp`, `ScenarioScriptEventParentMarriesStepparent`
- and one class the port was missing was implemented, below. Recovery:
**22 -> 35 of 90** (narrative bucket 13 -> 26). The two class lists are now
identical: 13 in the registry, 13 in `kScenarios`.

### 39.4 The two classes implemented this round

**`ScenarioSocialMediaJoinComplete`** (1 captured, universal table). The literal pair
#49322 `You signed up for {0} account.` (card) / #25101 `I signed up for {0} account.`
(log) proves the slot is a channel, and `a16_social-media-channels-enc` supplies the
seven candidates (Facebook, Instagram, TikTok, Twitter, YouTube, Twitch, OnlyFans).
Rendered: `You signed up for an Instagram account.`

**`ScenarioScriptEventDiseaseContract`** (3 captured). This is the scripted-event
twin of the already-implemented `ScenarioDiseaseContract`: it names another person,
and the game writes it in the third person on the card and the first person into
the LifeLog. The captured card body is `Your mother is suffering from itchy bottom.` -
which is why the substitution is a "suffering from" draw, not the "has been
diagnosed with" phrasing section 37 emits for the annual lifecycle lines. Both
share one draw rule from `a16_disease-types-enc`: only rows with
`Contracts Organically Score > 0`, weighted by that column, article added when
the row sets `Uses Indefinite Article`. Rendered:
`Your mother is suffering from cancer.` / `My mother is suffering from cancer.`

### 39.5 Verification

- The full recovered push for the capture (35 entries, seed 12345, United States,
  female, 90 years) replays with `rc=0`, zero stderr lines and 74 events - no
  "unknown scenario class".
- `tools/coverage.py`: `captured scenarios: 90  recoverable: 35`.
- `bash tools/bench.sh /tmp/trace_c10.json` still runs 35 years; the remaining
  diffs are the usual game_only/port_only narrative differences, not crashes.
- Forced rows: `/tmp/qdir/socialtest.json` and `/tmp/qdir/dztest.json`.

### 39.6 What is left, honestly

| remaining | why |
|---|---|
| 28 menu instances (21 classes) | the port has no phone menu layer; the options are a table, the layer is not |
| 21 minigame instances (9 classes) | modal screens with their own rules (blackjack, casino spins, eye exam, car repair, profile hero, paywall) |
| 5 narrative classes | `ScenarioFailingEyesight`, `ScenarioLoveDate`, `ScenarioRetentionLover`, `ScenarioCrimePickpocket`, `ScenarioPushSpendTime`: captured card but no log line, so the emitted sentence stays unverified |
| `Scenario` itself (10) | the generic card; its captured bodies are other scenes, so a push to it would be a guess |

The practical consequence: the automatic-play path is close to closed (63% of
narrative instances, and the misses are unimplemented subsystems rather than
unknown text), and the next real gain is the menu layer - 21 classes that all
already have their option text in the tables.

## 40. The tables the app names in its own code, and the loader that hid them

Section 39 ended with "the next real gain is the menu layer". The gain turned out
to be smaller than a menu layer and available immediately: the app tells us which
table a scenario class reads, and the recovery tool was throwing those tables away
before it ever compared a body.

### 40.1 The `data/<table>` literals

`global-metadata.dat` keeps an authored literal for every table the code loads by
name, in the form `data/a16_stepparent-cruelty-scenarios-enc`. There are **282** of
them (of the 1071 tables shipped in `gamedata_plain`). The new tool
`tools/scen_source_code_tables.py` reads that list, CamelCase-splits each class
name, and merges the tables whose own name tokens the class covers (>= 0.6 of the
table's tokens) into the class candidate list at `obs_corpus/scen_source.json` with
score 2.0. 10 candidates were added across 8 classes; the ones that matter:

| class | code-path table |
|---|---|
| `ScenarioStepparentCruelty` | `a16_stepparent-cruelty-scenarios-enc` |
| `ScenarioPetDiscipline` | `a16_pet-discipline-enc` |
| `ScenarioSchoolBully` | `a16_bully-scenarios-enc` |
| `ScenarioOfferFood` | `a16_food-enc` |
| `ScenarioLoveFirstKiss` | `a16_first-kiss-reactions-enc` |
| `ScenarioCasinoBet` / `ScenarioCasinoOutcome` / `ScenarioOpportunityCasino` | `a16_dumb-scenarios-casino-enc` |

That is the same conclusion as 39.2 from the other side: the class name and the
table name are not similar by accident - the app wrote both.

### 40.2 The loader bug behind the 32% menu share

`tools/row_recover.py` builds its row index in `load_tables()`. It used to
skip every file whose first row has no `Title` column:

    if not rows or "Title" not in rows[0]:`
        continue

The menu and narrative families have no `Title` - their wording lives in
`Scenario` / `Log Version` / `Third Person Scenario` - so those tables
were never indexed. A captured card could share **100%** of a row's long words and
still be reported unrecoverable, because the row did not exist as far as the
matcher was concerned. That is exactly what `ScenarioStepparentCruelty` did:
body share 1.00 against row 33, no recovery.

The index now accepts a body column in this order, and keeps a table with no
`Title` under the empty title key: `Body`, `Scenario`, `Narrative`,
`String`, `Log Version`.

### 40.3 Result

| metric | before | after |
|---|---|---|
| recoverable of 90 captured instances | 35 | 38 |
| menu bucket | 9 / 28 (32%) | 12 / 28 (43%) |

The three new pushes are `a16_stepparent-cruelty-scenarios-enc` rows 33 and 31 and
`a16_bully-scenarios-enc` row 30.

### 40.4 Card column and log column are different columns

164 tables carry both `Scenario` and `Log Version`. The pair is the
double voice section 37 described for the lifecycle lines, one level down:

    Scenario     Your stepfather pranked you by convincing you that you're invisible.`
    Log Version  My stepfather pranked me by convincing me that I'm invisible.

The port records the log, so `Sim::renderRow` now tries `Log Version`, then
`Log` (the spelling in `a16_bully-scenarios-enc` and
`a16_first-kiss-reactions-enc`), then `Narrative`, `Scenario`, `String`.
None of the port's previously wired scenario tables has either log column, so
nothing that already matched could regress - and it did not: the residue census
(12 seeds x 4 country/gender x 95 years) is still 0 tokens, and the 35-year bench
run is unchanged.

### 40.5 The bully slot, and what is still unverified

`a16_bully-scenarios-enc` writes its subject as a bare `<bully>` slot. The
captured card for that class reads `Your classmate, Moe, has been telling everyone on
social media that you are a teacher's pet.`, so the slot carries a role word plus a
name, and the port renders it that way (`"classmate, " + name`). That table has no
first-person column at all - both `Scenario` and `Third Person Scenario` are
third person - so the port's log line for it still reads `you are a teacher's pet`
where the game's LifeLog presumably reads `I am`. The capture did not
include that log line, so it is left as-is rather than guessed, and the bench
reports it as a normal game_only/port_only diff.

### 40.6 Repro

    python3 tools/scen_source_code_tables.py --write   # merge the data/ literals`
    python3 tools/coverage.py                          # 38 of 90`
    python3 tools/scenario_classes.py                  # narrative 26/41, menu 12/28, minigame 1/21`
    bash tools/bench.sh /tmp/trace_c10.json

The metadata dump the first command reads is produced by
`python3 tools/metadata_strings.py list --limit 100000 --raw` (71223 literals, 282 of them
`data/` paths).

## 41. Menu scenes: the five classes whose text is only in the executable

The menu bucket was the one place where "match the card against a CSV row" could not
work, and 41.1 explains why. 41.2 is the method that did work — the game's own trace
already contains the answer, one record later — and 41.4 lists the five classes this
bought, which took coverage from 38/90 to 42/90.

### 41.1 Why the CSV route cannot reach these

The menu tables hold the card and nothing else. a16_stepparent-cruelty-scenarios-enc
has columns Scenario (second person card), Log Version (first person card) and a gender
column; there is no option column anywhere, and no other table carries the option
labels. A whole-corpus grep for the question the cards end with returns nothing:

    grep -rc "What will you do?" gamedata_plain/     -> no file
    grep -rc "How will you attack" gamedata_plain/   -> no file

The card text, the button labels and the outcome sentence are C# string literals in
global-metadata.dat. They are present (41.3) but the literal table is sorted
alphabetically, so a literal's index says nothing about which scene it belongs to —
neighbouring indices are unrelated lines. Keyword search finds a literal only if you
already know what it says.

### 41.2 The pairing method: one record later in the trace

A captured trace stores, per age, the queue of records the game pushed to its UI and
the lines it wrote to the LifeLog. Reading that queue in order gives the pairing for
free:

* a menu card is the record whose body ends in one of the game's questions
  (What will you do? / Will you join them? / How will you behave? / How will you attack ...?);
* the record that follows it has cls == "Scenario" — a result card, not a scene —
  its title is the label of the button the player pressed and its body is the outcome
  sentence;

That is enough to rebuild a class: card (from the class itself), label set and outcome
sentences (from the paired result records), and the branch that was taken (by matching
the age's log line against the outcome templates).

    ScenarioStepparentCruelty age  1
      card   Your stepfather pranked you by convincing you that you are invisible. | What will you do?
      label  I won't play those games
      result You told your mother about your stepfather's behavior.
      log    My stepfather convinced me that I'm invisible. I told my mother about his behavior.

Note what the log is: the card's first-person Log Version, then the outcome sentence,
joined into one line. The two halves come from different assets — the card from the CSV,
the outcome from the executable.

    ScenarioCustomAttack      age  5   card How will you attack your stepfather?  label Ouch  result You kicked your stepfather's face!
    ScenarioPetDiscipline     age 11   card Your vizsla, Hazel, just ate your fruit snacks. | What will you do?
                                       label Isn't she cute?  result You rewarded your vizsla, Hazel, with a Porterhouse steak.

Tools: /tmp/qdir/menu_evidence.py (per class: cards, ages, log lines) and
/tmp/qdir/menu_pairs.py (per instance: card, label, result, next log). Both accept a
trace that is either a dict with ages or a bare list, because the captures differ.

### 41.3 The literals behind them

Literal indices in the metadata string table (recoverable with
tools/metadata_strings.py grep <pat>; the file contains NUL bytes, so grep needs -a):

| kind | index | text |
|---|---|---|
| card | 48637 | You have an opportunity to have your first kiss with a {0} from your class named {1}. |
| card | 23258 | How will you attack {0}? |
| card | 39444 | Some of your friends are eating {0}. |
| card | 39445 | Some of your friends are going to a local casino called {0}. |
| card | 70591 | {0}, you find a {1}. |
| card | 48895 | You just observed {0} call {1} {2}! |
| outcome | 24317 | I had my first kiss with {0}. I made sure to not slip {1} any tongue. |
| outcome | 25281 | I tongue-kissed {0}. It was my first kiss. |
| outcome | 23832 | I could have had my first kiss with {0} but instead I ran away from {1}. |
| outcome | 25157 | I started eating {0}. |
| outcome | 25791 | I went with some friends to {0}. |
| outcome | 25526 | I turned in {0} that I found. |
| outcome | 69566 | {0} I forgave {1}. |
| label | 28365 / 28366 / 36401 | Kiss {0} but definitely no tongue / Kiss {0} with tongue / Run away from {0} |
| label | 27968 / 31820 | Join them / No thanks |

### 41.4 The five classes, and where each drawn value comes from

| class | drawn value | source |
|---|---|---|
| ScenarioLoveFirstKiss | the classmate and its pronoun | personName("classmate") and the hero's sex: opposite-sex noun + object pronoun |
| ScenarioOfferFood | the food | a16_food-enc, weighted by Commonness Score |
| ScenarioOpportunityCasino | the venue | companyName("Casino", life) — the Casino row of a16_company-name-structure-enc offers <color> Casino Resort, which is exactly the captured "Rosebud Casino Resort" |
| ScenarioCustomAttack | verb, body part, victim | a16_custom-attack-actions-enc (Action / Past Verb / Commonness Score), a16_custom-attack-body-parts-enc (Body Part), and the first alive relative in Life::family |
| ScenarioEthicsFindObject | the findable | nothing: the findables are a code array. duffle bag (literal 57755) is modelled and the line carries a shortcut: comment |

Rendering happens in Sim::renderScenarioClass (port/src/sim.cpp): a need() lambda
decides per token whether the body or any option log asks for it, a drawFrom() lambda
does the weighted table read, and the values are substituted into the card and into the
chosen branch's log. ScenarioCustomAttack is the one class with a single no-button
branch, so its sentence is built wholly from draws.

### 41.5 Effect on coverage

    captured scenarios: 90  recoverable: 38 -> 42

| bucket | before | after |
|---|---|---|
| narrative | 26 / 41 (63%) | 26 / 41 (63%) |
| menu | 12 / 28 (43%) | 16 / 28 (57%) |
| minigame | 2 / 21 (10%) | 2 / 21 (10%) |

tools/row_recover.py REGISTRY gained the five classes, so a captured instance of any
of them now replays through the port instead of being skipped.

### 41.6 Verification

    ./build/blsim --script /tmp/qdir/menutest2.json        (seed 12345, US, male, forced pushes)

    age 1  ScenarioLoveFirstKiss       card ...with a girl from your class named Beyawnce Gnolls.
                                       log  I had my first kiss with Beyawnce Gnolls. I made sure to not slip her any tongue.
    age 2  ScenarioLoveFirstKiss       log  I tongue-kissed Tank Sea. It was my first kiss.
    age 3  ScenarioLoveFirstKiss       log  I could have had my first kiss with Catrina Woodworker but instead I ran away from her.
    age 4  ScenarioOfferFood           log  I started eating poutine.
    age 6  ScenarioOpportunityCasino   card Some of your friends are going to a local casino called Gung Ho Casino Resort.
                                       log  I went with some friends to Gung Ho Casino Resort.
    age 7  ScenarioCustomAttack        card How will you attack your mother?   log I bit my mother's face!
    age 8  ScenarioEthicsFindObject    log  I turned in a duffle bag that I found.

The classmate names above come from the placeholder name pool (§34), not from the
game's own classmate object. Regression: residue census still reports
"distinct residue tokens 0" across 48 runs x 95 years, and tools/bench.sh replays
/tmp/trace_c10.json for 35 years without an abort — its age-34 casino divergence is now a
draw difference inside a replayed class (port "Infinity Oklahoma City Casino" vs game
"Johnson Resort & Gaming"), which is what that bench is for.

### 41.7 What is left in the menu bucket

Twelve of the twenty-eight captured menu instances have a card but no outcome line in any
capture, so there is no evidence for their branches and the port keeps drawing its own
scene instead of replaying them:

    ScenarioInsultRelative (card: "You just observed a firearms activist call your mother foolhardy!")
    ScenarioEthicsBreakSomething, ScenarioSchoolBully, ScenarioSchoolActivityJoin,
    ScenarioEncounterProstitute, ScenarioSocialMediaJoinPush, ScenarioFriendUnfriendInbound,
    ScenarioMediaOnlyFansArguePush, ScenarioDiseaseContract, ScenarioLoverLeaves, ScenarioLoveDate,
    ScenarioRetentionLover

Their cards are recoverable (the literals exist) but the labels are not guessable from the
card sequences — a capture that presses the buttons is what unblocks them.

## 42. The minigame bucket: casino tables, the eye exam, and cards the game never logs

§39 put 21 captured instances in the minigame bucket, of which 2 replayed. The three
casino minigames (15 instances) dominated it, and 42.1 is what they turned out to be:
three modal cards per visit, of which only the last writes anything. 42.2 is the eye
exam, 42.3 the "silent card" distinction the port needed, and the bucket now stands at
19 of 21.

### 42.1 The casino minigames are three cards, one of them logged

A single visit to a casino game room emits three records, in this order:

    ScenarioCasinoBet        title "Rosebud Casino Resort"        body "Place your blackjack bets!"
    ScenarioGameBlackjack    title "Emily (Dealer)"               body "Get as close as possible to 21 without going over!

    																You have $7,200 riding on this bet."
    ScenarioCasinoOutcome    title "You lost" / "High roller"     body "You lost $7,200 on the hand of blackjack."

Only the outcome record writes a LifeLog line, and the line is the card restated in
first person with the venue appended:

    I lost $7,200 on the hand of blackjack at Rosebud Casino Resort.

So the outcome class is the one that matters for replay, and every number it needs is in
`a16_casino-casino-game-room-enc` (columns Game, Installation Cost, Unit String, Base Unit
Capacity, Cost Per Unit, Default House Win Rate, Default Bet, Min Bet, Max Bet):

| game | Default Bet | Max Bet | Default House Win Rate |
|---|---|---|---|
| Blackjack | 10 | 100 | 65 |
| Texas Hold'em | 25 | 200 | 55 |
| Craps | 15 | 150 | 85 |
| (nine games in all: also Sports Betting, Slots, Three-Card Shuffle, High-Low, Keno, Roulette) |

The captured $7,200 sits inside Blackjack's 10..100 unit range at exactly 72 units, which
prices a unit at $100; the port therefore draws units from the room's Default..Max range
and reads the stake as units x 100. The win/lose flag is the room's own house win rate,
and the tail of the sentence is a per-game literal from the executable:

| game | tail literal (metadata index) |
|---|---|
| Blackjack | on the hand of blackjack (63166) |
| Craps | on this craps roll (63168) |
| Keno | on this game of keno (63169) |
| Three-Card Shuffle | on this game of three-card shuffle (63170) |
| High-Low | on this high-low draw (63172) |
| Texas Hold'em | on this hand (63171) |
| Roulette / Slots | on this spin (63173) |

The sentence prefixes are the literals `I lost ` (24616) and `I won ` (25815), the bet card
is `Place your {0} bets!` (34034 shows the Texas hold'em form), and the play card's rules
line for blackjack is `Get as close as possible to 21 without going over!` (21370) followed
by `You have {0} riding on this bet.` (23556).

### 42.2 The eye exam is two literals and one coin flip

    ScenarioFailingEyesight   card  Your mother is concerned with your eyesight and is taking you for an eye exam at the optometrist.
    ScenarioGameEyeExam       card  Start the test and then tap the letter or number that's different than the others before time runs out!
    ScenarioEyeExamOutcome    body  You failed your eye exam and have been issued a prescription for eyeglasses!   (48452)
                                    You passed your eye exam and do not need eyeglasses!                            (49098)
                              log   I failed my eye exam! (24139)   /   I passed my eye exam! (24779)

Both cards are silent; the outcome draws pass/fail once and the same draw spells the card
and the log, so the two can never disagree.

### 42.3 Silent cards, and why the port needed a flag

The port logs a card as a line when a scene's branch has no captured sentence, which is
the right default for a scene that was shown to a player — but wrong for these modal
cards: in the game they write nothing to the LifeLog at all. RegScenario therefore gained
a `silent` member, and a silent class renders its card and returns before any line is
built: ScenarioCasinoBet, ScenarioGameBlackjack, ScenarioFailingEyesight,
ScenarioGameEyeExam and ScenarioProfileHero (an empty profile screen) are marked.

Without the flag the port emitted a synthetic `title: body` line for every modal card,
which is exactly the kind of phantom line the differential bench counts as a port-only
divergence.

### 42.4 Coverage

    captured scenarios: 90  recoverable: 42 -> 59

| bucket | before | after |
|---|---|---|
| narrative | 26 / 41 (63%) | 27 / 41 (66%) |
| menu | 16 / 28 (57%) | 16 / 28 (57%) |
| minigame | 2 / 21 (10%) | 19 / 21 (90%) |

The narrative bucket gained one instance because ScenarioFailingEyesight's card was being
counted there. The two minigame instances still unrecovered are ScenarioCarRepair (a
repair-price menu whose option sentences were never captured) and ScenarioMiniPaywall (a
store modal with no narrative line at all).

### 42.5 Verification

    ./build/blsim --script /tmp/qdir/casinotest.json          seed 777
      age 1  ScenarioCasinoOutcome  body You lost $11,200 on this hand.        log I lost $11,200 on this hand at Rhythm Hotel & Casino.
      age 2  ScenarioCasinoOutcome  body You won $17,000 on this bet.          log I won $17,000 on this bet at Maple Hotel & Casino.
      age 3  ScenarioCasinoBet      body (silent)                              lines 0
      age 4  ScenarioGameBlackjack  body (silent)                              lines 0

    ./build/blsim --script /tmp/qdir/eyetest_11.json          seed 11
      age 1  ScenarioEyeExamOutcome  body You failed your eye exam and have been issued a prescription for eyeglasses!
                                     log  I failed my eye exam!
      age 2  ScenarioFailingEyesight lines 0
      age 3  ScenarioGameEyeExam     lines 0

Residue census: `distinct residue tokens 0` (48 runs x 95 years). Bench: 35 years of
/tmp/trace_c10.json without an abort; its age-34 line is now the same sentence shape on
both sides with a different draw (`i lost # # on the hand of blackjack ...` versus
`i won # # on this bet at {name}`).

## 43. The illness chain, and what a menu class costs to recover

The menu classes are the part of the game that is not driven by a CSV row: the
card, its option labels and the sentence that follows a tap are C# string
literals. Section 41 paired one captured card with its outcome record; this
section closes the chain the capture opened (the symptom card, the doctor card
and the treatment card) and re-measures recovery.

### 43.1 How many menu classes there are

`tools/metadata_strings.py list --raw` over global-metadata.dat (76031 literals)
holds **1401 Scenario* class-name literals** - the game's real total - and
**282 game-data table paths** under `data/`. Both inventories are dumped to
`/tmp/qdir/scenario_classes.txt` and `/tmp/qdir/data_tables.txt`; the port's
registry (`port/src/sim.cpp` kScenarios) covered 26 of them before this section
and 31 after it. The gap is therefore a list, not a guess: a later section can
pick a class off it and look for a capture.

### 43.2 The symptom card is a table row, not a literal

`a16_disease-symptom-scenarios-enc` (432 rows, header `Symptom,Abbreviated
Symptom,Supplementary,Scenario,Log Version,Gender,Concern Score,Min Age`) is the
first menu class whose whole card is table-driven:

| card part | source |
| --- | --- |
| Title | `Abbreviated Symptom`, else the capitalised `Symptom` (`Fever`, `Difficulty Urinating` in the capture) |
| Body | the row's own `Scenario` text, plus `What will you do?` |
| Body, second sentence | a random row whose `Supplementary` cell is set - 169 of the 432 rows are these stacked sentences (`To further complicate things, ...`) |
| log line, first sentence | the row's `Log Version` column |
| option labels | four literals: `It doesn't bother me` (#27502), `Go to the doctor` (#21936), the home remedy, `Search the web` |

The third label is the one part that is not in the CSV: it is a remedy chosen per
symptom (`Chug some water` #15622 for a fever, `Take some aspirin` #40895 for
difficulty urinating) and a string search over all 1071 game-data files finds
neither, so it is code-built. `Sim::renderSymptomCard` (`src/sim.cpp`) renders
the row, appends the supplementary sentence on a coin flip and appends the branch
tail to the `Log Version` column.

### 43.3 The five classes this section implements

Every string below was captured (trace_game10) or read out of the literal table;
the drawn parts are the ones the port builds at render time.

| class | card | options | drawn values | log |
| --- | --- | --- | --- | --- |
| `ScenarioDoctors` | `Which local doctor will you schedule an appointment to see?` | `Dr. <surname>` x2, `Actually, never mind` (#10716) | surnames from the celebrity name pool | `I made a doctor's appointment.` (#24634) |
| `ScenarioDoctorTreatments` | `Dr. X has diagnosed you with Y. / You continue to suffer from Z. / Select a treatment today!` | `Treat me`, `I'll pass for now` (#25929) | X is the doctor card's surname (`lastDoctor_`), Y a disease row, Z a symptom row | `Dr. X diagnosed me with Y.` (#69923) + `Dr. X treated my Y.` (#70362) or `I decided not to get treated.` (#23866) |
| `ScenarioJobTermination` | `You have been let go from your position as <job> for <company>.` | `Insult my supervisor`, `Wish my supervisor well`, `Beg my supervisor to stay` | job from the hero, company from the career row's `Company Type` | `I called my supervisor a <insult> on my way out.` (insult from `a16_insults-enc`) |
| `ScenarioInsultRelative` | `You just observed <stranger> call your <relative> <insult>!` | `Forgive him`, `Insult him back`, `Attack him` (#12345) | stranger is a random person with its own article | `{0} I forgave {1}.` (#69566) is the captured shape; the port writes the branch sentence |
| `ScenarioCasinoGoldenNekoAd` | the 20%-winnings pitch | `Get one of my own`, `Leave the casino` | none | both branches captured: #23807 and #24465 |

The termination card needed a fallback the capture did not show: the forced-row
run pushed it at age 4, where the hero holds no career at all, so the port draws a
real career row rather than leaving the position unnamed.

### 43.4 What the recovery tool learned

`tools/coverage.py` flattened the list-format traces with the log bucket's `age`
field one lower than its own key, so every push index landed one scenario early.
The flattened records now also carry the card's `_choices`, which is what lets an
outcome card be resolved through its parent. `tools/row_recover.py` gained:

- the symptom index (title -> row of the symptom table, since the card title is
  the `Abbreviated Symptom` column and never matches a `Title` column);
- outcome-card resolution: the game's outcome title is a pun on the tapped label
  (`Treat me` -> `Treat me right`), so the pairing is positional - the result card
  belongs to the card before it - and the branch index is used when the label does
  match;
- `REGISTRY` templates for the five implemented classes plus the captured classes
  the port does not render yet, so identification and port coverage stay separable.

### 43.5 Verification

- forced-row run (`/tmp/qdir/illtest.json`, seed 12345, 8 ages): `ScenarioDiseaseContract` rendered row 47 (`Chills`, log `I experienced chills. I made a doctor's appointment.`), `ScenarioDoctors` -> `Dr. Denty`, `ScenarioDoctorTreatments` -> `Dr. Denty has diagnosed you with asthma. / You continue to suffer from feet blisters.`, `ScenarioJobTermination` -> `let go from your position as bartender for Transcendent Diner` + `I called my supervisor a dick on my way out.`, `ScenarioInsultRelative` -> `an accountant call your mother nimrod!`, `ScenarioCasinoGoldenNekoAd` -> both captured lines.
- `tools/coverage.py`: **captured 109, recoverable 66 -> 107**. The 13 outcome cards are now resolved through their parent; the only two left are `ScenarioMiniPaywall` (2), the pure store flow with an empty card and no narrative line.
- `bash tools/bench.sh /tmp/trace_c10.json` runs 35 years with no abort.
- `blsim --script /tmp/qdir/covpush.json` (all 109 recovered slots pushed at once) replays to age 83 and dies normally, so the mixed registry/table/symptom push array loads.

### 43.6 What is still open

- `ScenarioMiniPaywall` has nothing to recover: title, body and log are empty.
- Classes that are identified but have no port renderer yet: `ScenarioPetDiscipline`,
  `ScenarioPushSpendTime`, `ScenarioStepparentCruelty`, `ScenarioFriendUnfriendInbound`,
  `ScenarioLoverLeaves`, `ScenarioLoveDate`, `ScenarioSchoolBully`,
  `ScenarioSchoolActivityJoin`, `ScenarioEncounterProstitute`,
  `ScenarioMediaOnlyFansArguePush`, `ScenarioSocialMediaJoinPush`, `ScenarioCarRepair`,
  `ScenarioIAPConfiguredPush`, `ScenarioRetentionFreelanceGig`, `ScenarioRetentionLover`,
  `ScenarioBase`. Their cards and logs are captured (section 41 digest); each needs its
  drawn values wired the way the five above are.
- Unmeasured tails: the remedy and `Search the web` branch sentences (the port appends
  its own), the single-literal dismissal reason of `ScenarioJobTermination`, and the
  first fill of the `InsultRelative` log shape (#69566).

## 44. The menu classes: seventeen cards, and what a rendered push costs

Sections 41 and 43 left 26 pushed scenes that the port could not render. All 26
now render. This section records what each class needed, where its text came
from, and the two tool bugs that were hiding the true gap.

### 44.1 The gap was measured wrong first

`tools/coverage.py` says *recoverable*: a scene whose source row was found. It
says nothing about whether the port can play that row. `/tmp/qdir/portgap.py`
adds the missing check: for every entry in `/tmp/coverage_push.json`, is the
class in the port's `kScenarios`, or the table in `kTables`?

The first run reported **73 of 109, gap 34**. Eight of those 34 were false: the
port's `kTables` is a plain string array (`src/sim.cpp:458`), and the script
only matched `ensureTable("...")` and `tbl("...")` call sites. Fixing the
pattern (match the array literal too) left **26 real gaps in 16 classes**. After
this batch: **rendered 107, gap 0** - the two remaining entries are the
`ScenarioMiniPaywall` cards, which carry no text at all.

### 44.2 Evidence rule: the outcome card carries the label

Only `trace_game10.json` (drive10) has `_choices`. Every other trace has
`_choices: null`, so for older captures the button labels are unrecoverable
from the trace. But a result card (`_class == 'Scenario'`) is titled with the
**label of the button that was pressed** and bodied with the second-person
result sentence, and it always follows its parent card in the same round. So
`row_recover.py` now pairs each `Scenario` record with the most recent
non-`Scenario` record and inherits the parent's identity: same class, same
table row, and - when the label matches - the same choice index. All 13
outcome-card instances in the corpus are now recoverable; before this, they
were 13 of the 43 unrecovered rows.

### 44.3 The seventeen classes

Choices are listed in the port's order, which `tools/row_recover.py` mirrors, so
a recovered `choice` index means the same branch on both sides. A blank label
means the class has one branch or the label was never captured.

| class | card | branches | text source |
| --- | --- | --- | --- |
| `ScenarioStepparentCruelty` | Evil Stepfather | 2 | `a16_stepparent-cruelty-scenarios-enc` (49 rows: Scenario / Log Version / Hero Gender / Stepparent Gender / Hetero Parents / age window) |
| `ScenarioSchoolBully` | Bully | 4 | `a16_bully-scenarios-enc` (34 rows: Scenario / Third Person Scenario / Gender); the fight branch is three literals (#20066, #20233, #49222, #69604) |
| `ScenarioSchoolActivityJoin` | club name | 4 | literal card (#39433/#39434), labels #44089/#44090, logs #25369/#23935/#25223 |
| `ScenarioEncounterProstitute` | Hooker / Working Girl | 3 | labels #39410/#39411, log #25125/#25126/#23576 |
| `ScenarioFriendUnfriendInbound` | Unfriended | 3 | labels #44847/#44848, logs #69636/#70373/#50710 |
| `ScenarioLoverLeaves` | Break Up | 2 | `a16_breakup-scenarios-enc` (228 rows: Condition / Crazy / Scenario / Gender / Straight Only / Vampire flags), log #69876 |
| `ScenarioLoveDate` | Love Interest | 1 | log #49725, label #21850 |
| `ScenarioSocialMediaJoinPush` | channel name | 2 | `a16_social-media-channels-enc` (8 rows) |
| `ScenarioCarRepair` | Make + Model | 1 | `a16_car-models-enc`; labels #33733, log #24763 |
| `ScenarioPetDiscipline` | Behavior | 2 | `a16_pets-enc` (Breed, Commonness Score) + `a16_pet-names-enc` (Name); log #49244 |
| `ScenarioPushSpendTime` | Quality Time | 2 | labels #21956-#21961 |
| `ScenarioIAPConfiguredPush` | per-offer | 2 | five captured cards, no table: the offer text is code-side |
| `ScenarioRetentionFreelanceGig` | Freelance Gig | 2 | tutorial literal #11529 |
| `ScenarioRetentionLover` | Love in the Air | 1 | card #49993, tutorial literal #11527 |
| `ScenarioMediaOnlyFansArguePush` | Argument | 1 | literals #1308/#1783 |
| `ScenarioBase` | film title | 1 | the director path; the film data lives in the genesis film tables (pending) |
| `ScenarioSchoolEnrollActivity` | Extracurricular Activity | 2 | literal card; logs #23935/#25223 |

Two of those are table-driven in the same shape as the symptom card, so the
port reads the row instead of holding a copy: `ScenarioStepparentCruelty` and
`ScenarioSchoolBully`. Neither is wired to its table yet - the port carries the
captured rows - which is the cheap half of a two-stage job; the tables are named
below.

### 44.4 The token plumbing this needed

A batch of seventeen cards draws sixteen kinds of name. The port grew one
lambda, `fillAll`, that every drawn value goes through, and calls it on the
label, the body and the log:

* `{pet}` = `a16_pets-enc` `Breed` where `Animal == dog`, weighted by
  `Commonness Score`; `{petname}` = `a16_pet-names-enc` `Name` for dogs.
* `{club}` = the two names the corpus contains (Billiards, Weightlifting). No
  club table exists; the game's club names are code-side literals
  (#27937-#27943), so this is a placeholder list.
* `{car}` = `a16_car-models-enc` `Make` + ` ` + `Model`.
* `{channel}` is the article-bearing form (`a Twitch account`) and
  `{channelbare}` the bare one (`Your friends are all joining Twitch.`) -
  both from one draw of `a16_social-media-channels-enc`, so the card and the
  log agree on the channel.
* `{lady}`, `{stepparent}`, `{venue}` are one-word placeholder sets: the
  first two need a step-parent / stranger gender and age model the port does
  not have yet, the third a venue table.
* `{countryother}`, `{cfirst}`, `{clast}` reuse the hero tokens and the
  name pools.

Card titles resolved as well: `{club}`, `{car}`, `{pet}` and the channel
name are the literal card titles in the corpus.

### 44.5 The vacation table, and two tokens that only live in it

Adding `a16_dumb-scenarios-vacation-enc` to `kTables` (a coverage push
targets it) exposed the last residue in the port: `<vacation-destination>`
(173 uses) and `<vacation-cruise>` (89), both exclusive to that table.

* `<vacation-destination>` is a row of `a16_vacation-destinations-enc`
  (325 rows: Destination / Context / Use Definite Article / Country /
  Continent / Cost Score / Appeal Score / Danger Score / Commonness /
  Pixie Dust Only) - `Abu Dhabi`, `Alaska`.
* `<vacation-cruise>` is a row of `a16_cruises-enc` (12 rows: Type /
  Continent / Cost Score / Appeal Score) - `Alaska`, `Bahamas`,
  `Caribbean`. The game's own strings confirm the reading: literal #49729 is
  `You went on {0} cruise.`

The port has no Travel menu, so both draw uniformly from their table. With
these two wired, a full forced push of all 109 recovered entries renders
**zero** leftover tokens, and the 48-run census (12 seeds x 4 country/gender
pairs x 95 years) reports no residue either.

### 44.6 Verification

* `python3 tools/coverage.py` - the 17 classes recover row-for-row; the only
  UNRECOVERED entries left are the two `ScenarioMiniPaywall` cards, which have
  no title, no body and no log line in the game either.
* `python3 /tmp/qdir/portgap.py` - ENTRIES 109, rendered-by-port 107, gap 0.
* `/tmp/qdir/menutest.json` (24 forced pushes, one per class/branch, seed 12345
  US male 30) - every card, label and log line matched the corpus text,
  including `My Fiat 500 has a worn ball joint.` and
  `My pit bull, Snickers, ate my octopus. I rewarded her with a Porterhouse
  steak.`
* `bash tools/bench.sh /tmp/trace_c10.json` - 35 years, no abort.
* `/tmp/qdir/rescensus.py` - distinct residue tokens 0.

### 44.7 What this says about the remaining cost

A menu class is not one scene. Between section 41 and here, the per-class cost
has settled at roughly: one table or two to three literals, one token draw, one
branch table mirrored on both sides, and one forced-push run to check it. The
expensive part was never the text - it is the *identity* of the branch, since a
recovered `choice` index has to mean the same thing in `row_recover.py` and in
the port. That is why the two lists are deliberately ordered alike and why a
blank label is preferred to a guessed one.

Three classes still resolve to a code-side pool the port cannot read
(`ScenarioIAPConfiguredPush's five offers, `ScenarioBase's film data, the
bullies' third-person sentences): each is a `DataHub`-style switch over
literals, the same shape as the circumstance pools of section 38, and each
needs one live sampling session, not a CSV read.

## 45. Prison: the first state branch the port owns

Sections 31.3 and 45 of the plan kept prison, royalty, vampirism, orphanhood and
military service as "state the port does not model". Prison is the cheapest of
the five, because the game ships its whole text side as tables the port can
already read, and the only missing piece is the gate. This section records what
the port now does and, just as importantly, which numbers are still guesses.

### 45.1 What the game ships

| table | rows | what the port takes from it |
| --- | --- | --- |
| a16_prisoner-crimes-enc | 138 | Crime, Seriousness Score, Commonness Score for an adult sentence |
| a16_prisoner-juvenile-crimes-enc | 86 | the same three columns for a sentence handed down under 18 |
| a16_prison-jobs-enc | 38 | Job, Description, Commonness Score; the Description column is the filler of the job line |
| a16_dumb-scenarios-prison-enc | 64 | the prison year's own scenario pool, title/body/choice like every other dumb set |

The dumb-scenario pool is not a special format: 37 of its rows are gated at
Hero Min Age 18 and 26 at Hero Max Age 17, which is the same juvenile/adult
split the two crime tables describe. Its tokens are all ones the port already
renders (`<rb>` 23, `<insult>` 2, `<num10-59>` 1), so the pool needed no new
token work at all.

### 45.2 The wording is the game's own

The game writes convictions for the player's relatives through
`<pronoun> have been convicted of {0} and sentenced to {1} in prison!`
(literal #9559) and its juvenile form
`<pronoun> have been convicted of {0} and sentenced to {1} {2} at a juvenile
detention center!` (#9562). `<pronoun>` resolves to 'I' for the hero, which is
why the port can use the same sentence for both: a captured life would read
'I have been convicted of ...'. There is no separate first-person conviction
string in the 76031-literal table, which is itself the evidence that the game
does this too.

Two more literals are used verbatim: 'I started a prison job {0}.' (#25148),
whose `{0}` is the Description column ('making license plates', 'as a laundry
attendant'), and 'I was released from prison.' (#1959).

### 45.3 What the port implements

* `include/bl/game.h:84-86` adds `Hero::inPrison`, `Hero::prisonYearsLeft`
  and `Hero::prisonJob`.
* `Sim::maybeConvicted` (`src/sim.cpp:2367`) runs once a year for a hero of
  10 or more who is not already inside: a 2%/year draw, then a crime weighted by
  its Commonness Score, then the sentence.
* `Sim::prisonYear` (`src/sim.cpp:2402`) is the year itself: an optional
  prison job (35%), the year's scenario from the prison pool, the countdown, and
  the release line appended to the same year's log, exactly the way the illness
  chain appends 'I decided not to get treated.' to its own line.
* `Sim::pickDumbScenario` gained the gate it was describing in a comment
  already: `k == 1` is the prison category, and while a sentence is running
  that is the only category the roulette accepts (`src/sim.cpp:1560`,
  `1569`). `kDumbPrison` sits beside the other set names
  (`src/sim.cpp:1512`).

### 45.4 The numbers that are still guesses

Four `shortcut:` comments mark them in the source, and they are the reason this
section exists rather than a one-line changelog entry:

1. The 2%/year conviction chance. The real game sends the hero to prison
   through the interactive Crime menu and a court, which the port has none of.
   A captured life therefore cannot be compared against the port on this axis
   yet; the rate is chosen so a 90-year life sees roughly one sentence, and it
   moves to the menu when the Crime menu is ported.
2. The sentence table: Seriousness Score 1..5 maps to {1, 2, 3, 5, 10} years.
   `a16_prisoner-crimes-enc` scores the crime but the years are code-side; the
   mapping is a placeholder until the sentencing function is read.
3. The other categories' gates during a sentence are unread, so a prison year
   re-rolls anything but `k == 1`. Whether the game also serves calendar or
   universal rows to an inmate is an open question, not an assumption.
4. A juvenile sentence reuses the adult release line: the literal table has no
   juvenile-release string (#9564 is re-entry, #24122 is escape).

One implementation note that is not a guess: the conviction draw runs on its own
stream, `Sim::prisonRng_` (`include/bl/sim.h:224`, seeded
`seed ^ 0x9e3779b97f4a7c15` at `src/sim.cpp:562`). Drawing it from `rng_`
would have shifted every later draw for a given seed and with it the port's own
fidelity metrics: with the private stream, `tools/coverage.py` still reports
109 captured / 107 recoverable, `/tmp/qdir/portgap.py` 109 entries / 107
rendered / gap 0, and `/tmp/qdir/rescensus.py` zero residue tokens.

### 45.5 Verification

* Ten seeds (12345, 777, 4242, 99, 3, 5, 8, 11, 21, 55) over 95 years each gave
  14 conviction lines, both juvenile and adult:
  'I have been convicted of shoplifting and sentenced to 3 years at a juvenile
  detention center!' (seed 99, age 12), 'I have been convicted of murder and
  sentenced to 10 years in prison!' (seed 3, age 52).
* Prison-pool rows render as such: seed 21 age 11 'After earning enough
  good-behavior points in detention, I chose to enjoy extra desserts for a week
  as my privilege.', seed 99 age 15 'I told my delinquent friends in juvie that
  when I get out, I will become a famous Kardashian.'
* A temporary draw of 0.9 (not committed) exercised multi-year sentences and
  produced 19 prison-pool events across four seeds, which is how the pool was
  confirmed reachable rather than empty.
* `bash tools/bench.sh /tmp/trace_c10.json` still runs 34 years without an
  abort, with the usual game_only/port_only log differences.

### 45.6 What this says about the other four states

Prison cost one table set, one gate and two literals. Royalty is the next
cheapest on paper (a16_country-royal-systems-enc, a16_royal-house-names-enc) but
it needs a birth-roll probability the port has not found, and three new tokens
(`<highest-royal-name>`, `<other-royal-name>`, the numbered celebrity slots).
Orphanhood needs only `<rb>` and a gate. Military service has no table the port
has located yet. The order of work after this section is therefore: orphanhood,
then royalty once its probability is read, then military.

## 46. Orphanhood, and the family mortality that has to come first

Section 45 closed the prison branch. This one closes the orphan branch, and it
had to bring something else with it: a hero only becomes an orphan if his
parents can die, and until now nothing in the port could kill a relative.

### 46.1 The entry line and the pool

| evidence | what it is |
| --- | --- |
| literal #70722 '`{0}I have been sent to an orphanage for the remainder of {1} childhood.' | the hero's own entry line, first person (its second-person twin is #47060 'With no surviving parents, you{0} have been sent to an orphanage ...') |
| a16_dumb-scenarios-orphan-enc, 15 rows | the orphanage's own scenario pool: 'Starstruck', 'Please Sir', 'New Kid on the Block', 'No Good at Goodbyes', 'Bunk mates', 'Wish list', 'Abuse of Power', 'A Big Bully', 'A runner', 'Special orders', 'Self Improvement', ... |
| literal #17014 'DUMB_SCENARIO_TYPE_ORPHAN' | the category name, which is the k == 0 slot of the roulette in pickDumbScenario |
| literals #37803/#37804 (ScenarioOrphanageEnter, ScenarioOrphanageProfile), #33027 'Orphanage', #47466 'Years at Orphanage' | the interactive orphanage menu and its lifetime stat, still unported |

Every row of the pool is gated Hero Min Age 5-17 and carries only tokens the
port already renders (`<rb>` 8, plus the inline `<a/b/c>` option pools), so the
pool needed no token work; it needed the gate. The gate is now
`Hero::orphan` (`include/bl/game.h:87-89`), accepted by the roulette at
`src/sim.cpp:1606` exactly the way prison is accepted at `1569`.

### 46.2 Family mortality

The port's relatives now age and can die. The line is the game's own person
template -- 'Your {0}, {1}, passed away at the age of {2}.' (literal #50746),
or without a name 'Your {0} passed away at the age of {1}.' (#50594) -- written
in the first person the hero's family lines use, the shape '#30974 My {0}, {1},
died of starvation.' already has. It lands at `src/sim.cpp:1325-1338`, before
every other thing a relative can do in a year, so a dead parent cannot also have
a baby or change jobs that year.

Two `shortcut:` notes travel with it:

1. The mortality curve (0.5%/year from 30, 2%/year from 45, +0.4%/year after)
   is port-side. The game's per-relative life table has not been read. The young
   tail is not cosmetic: without it a parent never dies before the hero turns
   18, and the orphan branch would be dead code.
2. Leaving the orphanage is silent. The literal table has no such line -- the
   game's orphanage is a menu the hero walks out of -- so the state simply ends
   at 18.

The rolls run on their own stream, `Sim::familyRng_` (`include/bl/sim.h:226`,
`seed ^ 0x51ed270b2f5a1c31` at `src/sim.cpp:565`), for the same reason the
conviction draw got one in section 45: a family death drawn from `rng_` would
shift every later draw for a given seed and move the port's own fidelity
numbers.

### 46.3 Verification

* Fourteen seeds over 95 years produced 30 'passed away' lines and one natural
  orphanhood: seed 55 was sent to the orphanage at 13 and drew three orphan pool
  rows ('I did my best to ignore it when I saw another orphan being returned to
  the orphanage.', 'I practiced simple acts of kindness at the orphanage in
  hopes that it would make me more adoptable.').
* The path was also exercised at a forced death rate (0.5, not committed), which
  is how the pool was confirmed to render its own titles: 'Bunk mates', 'Self
  Improvement', 'Starstruck' (adoption by Madonna), 'No Good at Goodbyes',
  'A Big Bully', 'New Kid on the Block', 'Wish list', 'Abuse of Power'.
  The forced run also caught a real bug: the orphan test counted a parent as
  surviving whenever the relation was still in the list, so a hero whose
  parents had just died was not sent to the orphanage. The check now reads the
  `alive` flag (`src/sim.cpp:1462`).
* `python3 tools/coverage.py` still reports 109 captured / 107 recoverable,
  `/tmp/qdir/portgap.py` 107 rendered / gap 0, `/tmp/qdir/rescensus.py` zero
  residue tokens, and `bash tools/bench.sh /tmp/trace_c10.json` still runs 34
  years without an abort.

### 46.4 The state-branch ledger

| state | ground truth | tables | gate | status |
| --- | --- | --- | --- | --- |
| prison | literals only | 4 tables | Hero::inPrison | done, section 45 |
| orphanhood | literals only | 1 pool | Hero::orphan + parent deaths | done, this section |
| royalty | none | a16_country-royal-systems-enc, a16_royal-house-names-enc | birth roll, probability unread | next, blocked on that number |
| vampire | 4 trace lines | 44 tables | not located | later |
| military | none | not located | not located | later |

Royalty is the last of the three whose text side is already in tables the port
can read; what it is missing is the probability of being born into a royal
family (field NumTimesRandomlyBornIntoRoyalty, setter at file offset 0x31d747c)
and three new tokens (`<highest-royal-name>`, `<other-royal-name>`, the
numbered celebrity slots). Reading that one number is the next step.

## 47. The royal branch: a state the binary decides behind an indirect call

The last of the three state branches that the dumb-scenario pools are gated on.
Prison (45) and orphanhood (46) both had a hero field the port could set from a
table; royalty is different -- the state itself is created at life setup, and the
roll that decides it is not reachable with the tools used so far.

### 47.1 What the binary proves

- The birth-circumstance chooser loads `SimHero.Royal` at offset 0x178 and calls
  `GetRandomBirthCircumstanceForHeroRoyalWithLooksAndSmarts` (@0x73f2cb1b14) only
  when that object is non-NULL; the commoner entry point is
  `GetRandomBirthCircumstanceForHeroWithLooksAndSmarts` (@0x73f2cb0b88). Both are
  called from one life-setup function at vaddr 0x328d1xx.
- Royalty is therefore a pre-existing `SimRoyal` object, constructed at three sites
  (`.ctor(int,String,DataCountry,bool)` @0x73f2f2c6bc -> callers 0x2593f14, 0x2a78738,
  0x32915f8; `.ctor(DataCountry)` @0x73f2f2c884 -> 0x2a77af8, 0x2a78a4c). The class
  (il2cpp_dump.txt:58111) carries _CountryCode 0x10, Rank 0x18, Seat 0x20, RoyalByBirth
  0x28, RoyalDowager 0x29, YearsRoyal 0x2c, RoyalHouse 0x38, Honorific 0x40.
- Dead end, recorded so nobody repeats it: `set_Royal` (0x73f3722d88) and
  `set_NumTimesRandomlyBornIntoRoyalty` (0x73f367147c) have zero direct BL callers in
  the whole executable segment, and scanning for adrp+add pairs that materialise either
  address finds none, so the property write and the roll behind it happen through
  metadata-driven indirect calls. The probability of being born royal is therefore unread.
  Everything below is built from table and literal evidence, and the random roll is left
  out rather than guessed.
- Address arithmetic used here (corrects earlier notes): the dump base is 0x73f049a000,
  i.e. vaddr = dump_addr - 0x73f049a000, and for code vaddr is the r2 address. File
  offsets differ: ELF PT_LOAD #2 maps file 0x201e3c0 to vaddr 0x20223c0, so
  file_offset = vaddr - 0x4000 for the executable segment.

### 47.2 The tables and the literals

- `a16_dumb-scenarios-royal-enc` (71 rows) is the royal family's own yearly pool, with
  the standard scenario columns plus its own gates: Hero Min/Max Age, Hero Gender, Hero Has
  Lover, Lover Gender, UK Only, Japan Only, Arab Only, Is Highest Ranking Royal, Is Not
  Highest Ranking Royal, Min/Max Respect. Rows carry real branch narratives ("I started a
  new charity as a way of stepping up my royal game.").
- Its token set: `<highest-royal-name>` 28, `<him/her>` 24, `<rb>` 22,
  `<he/she>` 18, `<lover>` 14, `<classmate>` 14, `<his/her>` 11,
  `<celebrity-male>` 10, `<musician>` 10, `<other-royal-name>` 10,
  `<celebrity-female1|2>` / `<celebrity-male1|2>` 5 each, `<celebrity1>` 2,
  `<circumstance-royal>` 2. Only the royal names, the numbered celebrity slots, the
  generic `<celebrity1>` and `<circumstance-royal>` were missing from the port.
- `a16_country-royal-systems-enc` (17 rows) gives each country's seats: Country, Country
  Code, Top Seat, Money Score, Region, Rank 1 Male/Female, Rank 1 Male/Female by Marriage,
  Rank 2..7 Male/Female, Seats. Belgium -> King/Queen, Japan -> Emperor/Empress, and the
  Seats column is the country's house and place names.
- `a16_royal-house-names-enc` (159 rows): Name, Estate, Palace, Castle, Country Code,
  Commonness Score. Generic rows are `<random-surname> House` with Country Code `all`.
- Literals: `#70877` '{0}{1} was born into {2} royalty. ' is the birth line ({0} is a
  prefix that stays empty for the hero's own life), `#38696` 'Set your new life to be
  born into royalty!' is the player-facing switch the port's `"royal"` script field
  mirrors, `#17018` 'DUMB_SCENARIO_TYPE_ROYAL' names the pool category, and
  `#24381` / `#30986` / `#47606` / `#48789` are the abdication lines.

### 47.3 The Royal column of the birth-circumstances table

`a16_birth-circumstances-enc` has a Royal column; 37 of its 187 rows carry it, and their
weighted share of Commonness Score is 124/384 = 32%. Those rows are the royal conception
lines ('I was conceived when my mother <mounted/sat on/climbed on> my father's throne.',
'I was conceived when my parents whispered sweet nothings in a <secret/royal/palace/rose>
garden.'). 32% is far too high to be the birth probability, so the column reads as a filter
the game applies once royalty is already decided -- which matches the separate royal entry
point in 47.1. The port uses it as exactly that filter.

Note that the ordinary rows also include court-flavoured but unflagged ideas ("tea time got
a little too hot and steamy", Royal = x is set on that one, so the filter is what removes it
from a commoner's life).

### 47.4 What the port implements

- `Hero` (include/bl/game.h:94-97): `royal`, `royalRank`, `royalSeat`,
  `royalHouse`.
- `Sim::makeRoyal` (src/sim.cpp:2518): finds the country's row in
  a16_country-royal-systems-enc (17 countries have one; a country without one stays a
  commoner instead of getting an invented seat), sets seat from Rank 1 Male/Female, and
  draws one house from a16_royal-house-names-enc weighted by Commonness Score over the rows
  whose Country Code is `all` or contains the country's own code, resolving
  `<random-surname>` with the normal resolver.
- `runScript` calls it when the script says `"royal": true` (src/script.cpp:274 parse,
  before `birthLines` so the birth lines and the hero snapshot both carry the state).
- `birthLines` filters the circumstance pool on the Royal column (src/sim.cpp:1134) and
  appends 'I was born into <country> royalty.' (src/sim.cpp:1144, literal #70877).
- `royalToken` (src/sim.cpp:2545) resolves `<highest-royal-name>` (rank 1 seat + a
  first name of the hero's gender, the game's own 'King Ram' shape) and
  `<other-royal-name>` (the first filled seat below rank 1), and both stay verbatim for a
  commoner.
- `royalCircumstance` (src/sim.cpp:2571) composes `<circumstance-royal>`.
- The numbered celebrity slots are normalised in the reference lookup: the trailing digits are
  a slot id (`<celebrity-female1>`), and a bare `<celebrity1>` falls back to the pool
  matching the hero's gender.
- `pickDumbScenario`: `kDumbRoyal` (src/sim.cpp:1583) is the k == 5 set for a royal hero
  (src/sim.cpp:1661).
- `royalRng_` (src/sim.cpp:566) is a private stream, so the branch never shifts the main
  stream a seed's other subsystems draw from.

### 47.5 The shortcuts, and why they are not guesses

1. The birth probability is unread (47.1), so the port exposes `"royal": true` instead of
   inventing a rate. The game's own player-facing control is the same kind of switch (#38696).
2. The k -> set mapping inside `GetARandomPushDumbScenario` was not read for this bucket, so
   the royal set is taken whenever the hero is royal rather than at the game's own frequency.
3. `<circumstance-royal>` is built in code by
   `DataHub.GetRandomCircumstanceRoyalWithCastleName(String)` (@0x73f2ce4050), which takes
   the castle name, and its pool was not sampled; the port composes 'While leaving <place>' from
   the country's own Seats column.

Each of the three carries a `shortcut:` comment at its site.

### 47.6 Verification

- `"royal": true` with Belgium / Japan / Denmark renders 'I was born into Belgium royalty.'
  and draws its conception line from the Royal rows; United States (no royal system) stays a
  commoner with an ordinary conception line.
- Forced rows from the royal pool render with the royal tokens resolved: roleplay row 45
  'All You Can Be' -> 'King Ram tells you that exciting new opportunities will soon be available
  to those who have earned it', and an unfetched row from the same life gave 'I remained true
  to the crown's customs and suffered through the cold weather wearing shorts.'
- Residue census over 48 runs: 0 tokens. tools/coverage.py: 109 captured / 107 recoverable.
  tools/portgap.py: 109 entries / 107 rendered / gap 0. bash tools/bench.sh /tmp/trace_c10.json:
  34 years, no abort.
- The random stream is untouched: both the residue census and portgap reproduce their pre-branch
  numbers exactly.

### 47.7 Where the state branches now stand

Prison (45) and orphanhood (46) are driven by an event, royalty (47) by a life-setup switch.
Vampire has 4 trace lines and 44 tables but its gate was not located; military and the
occupation branches were not located at all. The interactive menus behind all three (the
orphanage profile, the royal family screen, the vampire crypt) remain unported: the corpus
contains no captures of them, so they have no ground truth to build against.

## 48. The vampire branch, and the gate that hid 218 rows

### 48.1 The pool

{B}a16_vampire-dumb-scenario-enc{B} is 67 rows and 36 columns, and it is not one of the
{B}DUMB_SCENARIO_TYPE{B} buckets ({B}#17005-#17019{B} has no VAMPIRE entry), so it is served by
its own path. Columns: {B}Index, Hero Min Age, Hero Max Age, Hero Gender, Hero Has Lover,
Lover Gender, Randomize Choices, Gender Bracket, Min Notoriety, Max Notoriety, Min Essence,
Max Essence, Hero Has Familiar, Title, Body, CTA{B}, then {B}Choice 1{B}..{B}4{B} each with its own
{B}Narrative / Notoriety Impact / Essence Impact / Relationship Impact{B}.

- {B}Hero Min Age{B}: 18 on 32 rows, 100 on 30 rows, then 120/125/130 and one 50.
- {B}Hero Max Age{B} is empty on all 67 rows.
- {B}Hero Has Familiar{B} is {B}x{B} on 23 rows, {B}Hero Has Lover{B} on 3.
- Token census: {B}<him/her>{B} 127, {B}<familiar>{B} 94, {B}<rb>{B} 66, {B}<he/she>{B} 55,
  {B}<his/her>{B} 54, {B}<vampire>{B} 48, {B}<random-person-indefinite>{B} 44, {B}<city>{B} 20,
  {B}<lover>{B} 15, {B}<rb2>{B} 12, {B}<vampire-possessive>{B} 12, {B}<insult>{B} 6,
  {B}<drug-illegal>{B} 5, {B}<circumstance-public>{B} 4, {B}<familiar-possessive>{B} 4,
  {B}<rb3>{B} 4, {B}<friend-possessive>{B} 3, {B}<musician>{B} 2 - all of them already existed in
  the port, so the pool needed a gate, not token work.
- Rows: {B}Bloodlust{B} ({B}<circumstance-public>{B}, you see a {B}<sheep/deer/badger/...>{B} that looks
  like it was hit by a car), {B}Familiar Problems{B} (You're about to get a victory royale when
  {B}<familiar>{B} trips over the controller cord), {B}Days of the Undead{B}, {B}Seeing Red{B}.

### 48.2 Sources

- {B}<vampire>{B} draws from {B}a16_vampire-name-enc{B} (170 rows: {B}Type, Name, Skin{B}, Type is
  female / male / surname).
- There is no familiar-name table: {B}a16_vampire-familiar-color/command/confused-string/dump/
  threat-enc{B} hold strings and gates, not names.
- {B}SimPerson{B} carries {B}SimVampire{B} at field 0x1e0, {B}VampireDiseaseCarrier{B} 0x1e8,
  {B}IsDestinedToBecomeVampire{B} 0x1e9 ({B}il2cpp_dump.txt{B} class SimPerson, line 71060);
  {B}IsPersonVampire(SimPerson){B} @0x73f2dc1520.

### 48.3 Implementation

- {B}include/bl/game.h{B}: {B}Hero::vampire{B}, {B}Hero::vampireName{B}, {B}Hero::familiar{B}.
- {B}include/bl/sim.h{B}: {B}void makeVampire(Life&){B} (line 122), {B}vampireYear(Life&, AgeUp&, int){B}
  (line 219), private {B}vampireToken{B} / {B}vampireNpcName{B}, and the private stream
  {B}vampireRng_{B} (line 256).
- {B}src/sim.cpp{B}: ctor seed {B}seed ^ 0x27d4eb2f165667c5ULL{B} (line 567); the two tables joined
  {B}kTables{B}; {B}Sim::makeVampire{B} at line 2581 draws the familiar from the ordinary name pool;
  {B}Sim::vampireYear{B} at line 2618 renames the NPC once a year and renders one pool row through
  {B}renderRow{B}; {B}ageUp{B} calls it at line 1806, before {B}maybeConvicted{B}.
- {B}resolveInto{B} gained {B}<vampire>{B} / {B}<vampire-possessive>{B} / {B}<familiar>{B} /
  {B}<familiar-possessive>{B}; {B}src/script.cpp{B} parses {B}"vampire": true{B} and calls {B}makeVampire{B}
  before the birth lines, mirroring the {B}"royal"{B} switch from 47.

### 48.4 The eligibility bug: 218 rows that no life could ever reach

{B}scenarioEligible{B} read {B}Hero Max Age{B} with {B}t.num(){B}, which returns 0.0 for an empty cell
({B}src/table.cpp{B}:122-130), and then compared {B}h.age > 0{B}. Every row that leaves the column
blank was therefore rejected at every age: 151 of {B}a16_dumb-scenarios-universal-enc{B}'s 258 rows
and all 67 vampire rows, 218 in total. The fix (line 1577) only applies an upper bound when it is
positive:

    if (t.has("Hero Max Age")) {
      const double maxAge = t.num(row, "Hero Max Age");
      if (maxAge > 0 && h.age > int(maxAge)) return false;
    }

Two gates that had been unconditional were tightened at the same time, because the state they
test now exists in the port: {B}if (t.flag(row, "Vampire") && !h.vampire) return false;{B} (it used to
skip every such row for everyone) and a new {B}Hero Has Familiar{B} gate against
{B}Hero::familiar{B}.

### 48.5 Verification

- Forced: {B}{"vampire": true}{B} plus a push of the pool rendered {B}Familiar Problems{B} with
  {B}<familiar>{B} = May, {B}Days of the Undead{B} with {B}<vampire>{B} = Lilith Ballsathar.
- Natural: a 30-year vampire life serves the pool from age 18 onwards - {B}Familiar Problems{B}
  (Minji), {B}Bloodlust{B}, {B}Seeing Red{B} ({B}<vampire>{B} = Rafael Batfrey).
- {B}tools/coverage.py{B} 109 captured / 107 recoverable; {B}portgap.py{B} 109 / 107 / gap 0;
  {B}tools/bench.sh{B} to age 34 with no abort.

### 48.6 Shortcuts

1. The gate is a script flag, not a game rule: how a life becomes a vampire (an offer card, a
   disease-carrier flag, {B}IsDestinedToBecomeVampire{B}) was not read.
2. {B}vampireYear{B} serves the pool directly instead of the game's notoriety/essence checks
   ({B}Min/Max Notoriety{B}, {B}Min/Max Essence{B} are columns the port does not model).
3. The familiar's name comes from the ordinary first-name pool - no table exists for it.
4. In the pool's own text the pronoun brackets are keyed to the hero, so a row that talks about
   the familiar can disagree with itself ('{B}he cuts herself{B}'). The game resolves those per
   subject; the port has one gender per hero.

## 49. Two tokens the eligibility fix exposed, and the bracket nesting they revealed

### 49.1 The residue went from 0 to 73

The moment the 218 previously unreachable rows became reachable, the residue census of
{B}/tmp/qdir/rescensus.py{B} (12 seeds x 4 country/gender pairs x 95 years) went from
{B}0 tokens{B} to 73 uses of two tokens, both in {B}a16_dumb-scenarios-universal-enc{B}:
{B}<restaurant>{B} 57 and {B}<action>{B} 16. They had never appeared because no natural life had ever
rendered the three rows that carry them.

### 49.2 {B}<restaurant>{B} is the Restaurant company-name format

There is no restaurant table among the 1071 ({B}ls gamedata_plain | grep -i restaur{B} is empty),
and no metadata literal holds a brand list. What does exist is a company type named {B}Restaurant{B}
in {B}a16_company-name-structure-enc{B}:

    Company Type: Restaurant
    Format:       <color> <suffix>, <prefix> <color>, <random-first-name-possessive> <suffix>
    Prefix:       Le, Cafe, Casa, Chez
    Suffix:       Cafe, Diner, Restaurant

That is the port's existing {B}companyName("Restaurant", life){B}, so {B}resolveInto{B} now calls it
(line 979; the resolver gained a {B}const Life* life_{B} member, line 239, because a company name
needs the hero's city and can draw a first name). Forced rows render
{B}Arcadia Diner{B}, {B}Amplitude Cafe{B}, {B}Le Polar{B}.

### 49.3 {B}<action>{B} is still unfilled, and is skipped rather than guessed

{B}<action>{B} sits in one row only:

    row 12 'Terrible Service'
    You went out to eat at <restaurant> with <friend> and <your server is <action>/they messed up
    both of your orders/you heard the server call you <insult> behind your back>.

It is in no column of any table (a value sweep for its register - flirting / ignoring you /
hitting on you / rude / lazy - matches only {B}a16_username-adjectives-enc{B} and
{B}a16_interview-questions-enc{B}, neither of which is a server-action pool) and no metadata literal
carries the phrase. So the port records the token in {B}kUnresolvedTokens{B} (line 894) and the
inline option selector (line 954) draws only among alternatives that do not contain one, falling
back to the full list when every alternative is unfillable. No text is invented, and the branch
the port cannot fill is never printed.

### 49.4 The bug that exposed: brackets nest

While fixing 49.3 the row showed why the token had survived a resolution pass at all: the
resolver took the {B}>{B} of the *inner* token as the end of the group, so it read the group as
{B}your server is <action{B} - a two-way choice whose first alternative happened to be the
unresolved text. The closing bracket is now matched with a depth count (line 917):

    size_t close = std::string::npos;
    int depth = 0;
    for (size_t k = i; k < text.size(); ++k) {
      if (text[k] == '<') ++depth;
      else if (text[k] == '>') { if (--depth == 0) { close = k; break; } }
    }

and a drawn alternative is resolved recursively before it is appended and cached for {B}<rb>{B}
(line 960), so a group may contain tokens of its own and {B}<pick:...>{B} is treated the same way.
This is a general correctness fix: any table row that nests a token inside an alternative was
being cut at the wrong bracket.

### 49.5 Verification

- Forced rows 12, 16 and 20 render {B}Terrible Service{B} (with the order-mix-up branch, never
  {B}<action>{B}), {B}Restaurant Wait{B} ({B}Amplitude Cafe{B}) and {B}Bad Steak{B} ({B}Le Polar{B}).
- Residue census back to {B}distinct residue tokens 0 total 0{B}.
- {B}tools/coverage.py{B} 109 / 107; {B}portgap.py{B} 109 entries / 107 rendered / gap 0;
  {B}./build/blsim --script /tmp/qdir/covpush.json{B} (all 109 pushes) exits 0;
  {B}tools/bench.sh /tmp/trace_c10.json{B} runs to age 34 with no abort.
- Note: making 218 rows reachable changes the port's year-by-year draws, so the bench's
  port-only/game-only lines move. The port stays reproducible per seed; the game comparison is
  per-age, not index-aligned.

## 50. The military branch: a rank ladder, a war pool, and one compare() that never matched

### 50.1 What the game keeps

{B}SimPerson{B} carries {B}<MilitaryBranch>k__BackingField{B} at field 0x10 (getter
{B}@0x73f347e070{B}, setter {B}@0x73f347e078{B}), {B}AvailableMilitaryBranchItemArray(){B}
{B}@0x73f35d1150{B}, {B}GetStringForMilitaryBranch(int){B} @0x73f2c9c194,
{B}EmojiForMilitaryBranch(int){B} @0x73f301b96c and
{B}GetCareerStringForMilitaryBranchAndStatus(int, int){B} @0x73f28ed588 - the last one is why the
military lines are code-side: {B}a16_dumb-scenarios-career-enc{B} carries no military token at all,
and the dumb-scenario dispatcher reaches the military variant through
{B}GetRandomCareerDumbScenarioForMilitaryBranchAndStatus(int, int){B} @0x73f2cbf0c8.

### 50.2 {B}a16_military-ranks-enc{B} is the ladder

18 rows, columns {B}Rank, Status, Air Force Longhand/Shorthand, Army Longhand/Shorthand, Coast Guard
Longhand/Shorthand, Marines Longhand/Shorthand, Navy Longhand/Shorthand, Salary Average, Stress
Score, Minimum Military Years, Minimum Intelligence, Minimum Discipline, Minimum Karma, ...{B}.
{B}Status{B} is {B}Enlisted{B} on 9 rows (E1-E9) and {B}Officer{B} on 10 (O1-O10); {B}Minimum Military
Years{B} is the promotion clock (E1 0, E2 1, E3 3, E4 5, E5 7, E6 9, ...; O1 0, O2 2, O3 4, ...),
and {B}Salary Average{B} is the payslip text ({B}$19,200{B} for E1, {B}$41,121{B} for O1).
The five branches are the five column families - Air Force, Army, Coast Guard, Marines, Navy.

The lines the game writes are literals: #24115 {B}I enlisted in the {0} with a salary of {1}.{B},
#24507 {B}I joined the {0} as an officer with a salary of {1}.{B}, #24373 {B}I have been promoted to
{0} with a salary of {1}.{B}, #25005 {B}I retired from {0}.{B} (plus the pension variants #25003/#25004).
The lifecycle templates #9384 / #9391 / #9393 / #9395 carry the relatives' versions with
{B}<military-branch>{B} (#9246) and {B}<military-rank>{B} (#9247).

### 50.3 {B}a16_war-scenarios-enc{B} is the war pool

70 rows, columns {B}Stage, Scenario{B}, four stages: 24 rows of escalation (boycott, denouncement,
ridicule), 20 of open war, 19 of the middle campaign, 8 of the aftermath ('In total, there were
more than 10,000 casualties as a result of the conflict.'). Its tokens are
{B}<war-country1>{B} 58, {B}<war-country2>{B} 59, {B}<war-country1-possessive>{B} 8, {B}<war-city1>{B} 7,
{B}<war-country3>{B} 5, {B}<war-country2-possessive>{B} 5, {B}<war-country1-leader>{B} 5, {B}<war-age>{B} 4,
{B}<war-country2-leader>{B} 1, {B}<war-country3-leader>{B} 1 - all of them new to the port.

### 50.4 Implementation

- {B}include/bl/game.h{B}: {B}Hero::military{B}, {B}Hero::militaryPending{B} (line 110),
  {B}Hero::militaryBranch{B}, {B}Hero::militaryRank{B}, {B}Hero::militaryPay{B},
  {B}Hero::militaryOfficer{B}, {B}Hero::militaryYears{B}.
- {B}src/sim.cpp{B}: ctor stream {B}militaryRng_(seed ^ 0x3b9aca07c2b2ae35ULL){B} (line 568); the two
  tables joined {B}kTables{B} (line 584); {B}Sim::makeMilitary{B} (line 2658) picks the branch and the
  status and stores the entry rank and its salary; {B}Sim::warToken{B} (line 2682);
  {B}Sim::militaryYear{B} (line 2721) enlists at 18, promotes along the ladder, retires at twenty
  years and otherwise renders a war row whose stage follows the years of service; {B}ageUp{B} calls
  it at line 1821, before the vampire and prison branches.
- {B}resolveInto{B} gained a {B}war-*{B} branch (line 982) and {B}<military-branch>{B} /
  {B}<military-rank>{B}. The three war countries are drawn from {B}countries_{B}, excluding the
  hero's own; {B}<war-city1>{B} comes from the hero's country {B}Cities{B} array, {B}<war-age>{B} is the
  hero's age and a {B}-leader{B} is a first name plus a surname from the port's pools.
- {B}include/bl/script.h{B} / {B}src/script.cpp{B}: the {B}"military"{B} boolean, parsed like
  {B}"royal"{B} and {B}"vampire"{B} and applied with {B}sim.makeMilitary(life){B}.

### 50.5 The bug: {B}compare(0, 12, "war-country"){B} can never be 0

The first build rendered every war row with its tokens verbatim. The cause was the prefix test:

    if (token.compare(0, 12, "war-country") == 0)   // never true

{B}compare(pos, len, const char*){B} compares a substring of length {B}len{B} against the whole literal,
and the literal is 11 characters, so the lengths differ before the characters are. The test is now
{B}token.rfind("war-country", 0) == 0{B} (line 2696). Worth recording because the same shape is easy
to write again: a length argument that does not equal the literal's length silently never matches.

### 50.6 Verification

- Enlisted life (seed 12345): age 18 {B}I enlisted in the Air Force with a salary of $19,200.{B} then
  Airman, Airman First Class, Senior Airman, Staff Sergeant, Technical Sergeant, Master Sergeant
  with the matching salaries, war rows in between ({B}Argentina has launched a boycott of all
  exports from Slovakia.{B}, {B}... has denounced ...{B}, {B}Panama has allied itself with Switzerland,
  sparking war with France.{B}, {B}United Kingdom's leader, Skylar Janim, has been assassinated in a
  covert operation conducted by Russia, swiftly ending the war between the two nations.{B}) and
  {B}I retired from the Air Force.{B}
- Officer life (seed 1): {B}I joined the Army as an officer with a salary of $41,121.{B} then First
  Lieutenant, Captain, Major, Lieutenant Colonel, retiring at 20 years.
- Three seeds x 60 years run clean; residue census 0 tokens; {B}tools/coverage.py{B} 109/107;
  {B}portgap.py{B} 109 / 107 / gap 0; {B}tools/bench.sh{B} to age 34 with no abort.

### 50.7 Shortcuts

1. The switch asks for the life; enlistment itself happens at 18 (the real gate is the Career menu
   and its eligibility columns - Minimum Intelligence / Discipline / Karma, drug tests and a clear
   criminal record - which the port does not model).
2. Twenty years of service is the retirement rule; the game's own discharge condition was not read.
3. Officer status is a 25% draw instead of a degree requirement.
4. A war row is drawn on 35% of service years and its stage follows the years of service; the game
   drives the war world state (two countries, four stages) from its own systems, and the hero's
   deployment is not gated.
5. {B}<war-country3-leader>{B} and the other leader slots use the port's ordinary name pools; no
   country-leader table exists in the 1071.
6. The relative-side lifecycle templates (#9384 / #9391 / #9393 / #9395) are not emitted for family
   members who serve; only the hero's own lines are.

## 51. The hero's own job: walking a16_careers-enc

### 51.1 What was missing

The port had a family model that gives every relative a job, a company and a promotion
(section 37), and the hero had a {B}job{B} string that only the military branch ever filled
(section 50). {B}a16_careers-enc{B} - 168 rows, {B}Career, Company Type, Seniority Track,
Seniority Track Years, Starting Salary, Salary Cap, Too Old Age, ...{B} - was already loaded for
the relatives' job titles, but the hero never walked it.

### 51.2 The ladder is only sometimes a ladder

{B}Seniority Track{B} is a real ladder on a minority of rows: {B}Cameraman, Camera Operator,
Director of Photography, ...{B}, {B}Probationary Firefighter, Firefighter, Fire Equipment
Operator, Lieutenant, ...{B}, {B}Pilot Trainee, Co-Pilot, Pilot, Airline Captain, Chief Pilot{B}.
On the remaining rows it is a category word instead - {B}None{B} 57, {B}Abbreviated{B} 29,
{B}Working Class{B} 24, {B}Corporate{B} 10, empty 3 (counted in section 37). Those rows get the
{B}Career{B} name as their title, and the port does not invent a ladder for them.

### 51.3 Lines

- Hiring reuses the family template #{B}9408{B} {B}<person> started a new position as <job-title> for
  <company>.{B} with {B}<person>{B} = {B}I{B}, which is how the game already writes the hero's version
  of the relative templates (section 36): {B}I started a new position as {job} for {company}.{B}
- Promotion reuses #{B}9390{B} {B}<person> has been promoted to <job-title>.{B} in the same way:
  {B}I have been promoted to {job}.{B}
- The company comes from the existing {B}companyName(){B} generator and the row's own
  {B}Company Type{B}, which is why a job can read {B}for Santa's Workshop, North Pole Industries{B}.

### 51.4 Implementation

- {B}include/bl/game.h{B}: {B}Hero::careerTrack{B} (the raw {B}Seniority Track{B} cell),
  {B}Hero::careerStep{B}, {B}Hero::careerYears{B}.
- {B}src/sim.cpp{B}: private stream {B}careerRng_{B} (ctor initialiser, line 569);
  {B}Sim::careerYear{B} (line 2669) hires from 18 on a 0.6 chance, then advances one rung every four
  years of service while the track has one; {B}ageUp{B} calls it at line 1823, after the military
  branch and before the conviction draw.
- Keyed on the empty track rather than on {B}employed{B}: the school state already sets
  {B}employed = !student{B} when the hero leaves school (line 1762), so an {B}if (!h.employed){B} gate
  never fired. This is the sort of thing only a run shows - the first build produced zero career
  lines for every seed.

### 51.5 Shortcuts

1. The careers table's eligibility columns are not evaluated: {B}Required Education{B},
   {B}Required Major{B}, {B}Min/Max Age{B}, {B}Min Intelligence/Appearance/Health/Karma{B},
   {B}Has Drug Tests{B}, {B}Requires Clear Criminal Record{B}, the licence and vehicle requirements.
   Hiring is a uniform draw, so a life can become a Pilot with no pilot's licence.
2. Promotions are a clock (four years), not the game's performance/attribute roll; {B}Seniority
   Track Years{B} is on the row but the rule that consumes it was not read.
3. Raises are not implemented, so #{B}24368{B} {B}I have been given a raise of {0}. My salary is now
   {1}.{B} is unused; {B}Starting Salary{B} and {B}Salary Cap{B} are loaded but not spent.
4. Quitting and being fired (#{B}24870{B}, #{B}25653{B}) are not emitted, and {B}Too Old Age{B} does not
   force a retirement line.
5. The hero holds one job at a time and the job never changes industry, so the {B}Company Type{B}
   stays whatever the first hire drew.

### 51.6 Verification

- Seed 777: 18 {B}I started a new position as Entry-Level Groomer for Cadence.{B}, 22 {B}I have been
  promoted to Experienced Groomer.{B}, 26 {B}I have been promoted to Head Groomer.{B} - a real
  three-rung track.
- Seed 12345: 21 {B}I started a new position as Puppeteer for Dynamite.{B}; seed 4242: 19
  {B}I started a new position as Elf for Santa's Workshop, North Pole Industries.{B}
- Unchanged after the change: residue census 0 tokens, {B}tools/coverage.py{B} 109 / 107,
  {B}portgap.py{B} 109 / 107 / gap 0, {B}tools/bench.sh{B} to age 34 with no abort, and the all-109
  push replay exits 0.

## 52. The sports pool: seven sports and each sport's own words

### 52.1 The tables

{B}a16_sports-enc{B} has 7 rows - one per sport - and the columns
{B}Sport, Fame Score, Season Games, Tie Odds, Max Win Percent, Min Age, Records Include Ties,
Has Draft, Draft Rounds, Male/Female Draft League, Male/Female Min/Max Salary, Ball Bracket,
Stadium Bracket, Field Bracket, Referee Bracket, Game Bracket{B}. The bracket columns are the
words that sport uses: football is {B}football / stadium / field / official / game{B}, basketball
{B}court{B} for the field - which is why the same sentence renders as {B}your court game{B} in a
basketball life and {B}your field game{B} in a football one.

Three more tables feed the same cards:

- {B}a16_sports-leagues-enc{B} (15 rows: {B}Index, Sport, Gender, Name, Description, Country Code,
  Money Score, Hall of Fame{B}) - 'BitLife Football League', "BitLife Women's Basketball
  Association".
- {B}a16_sports-teams-enc{B} (313 rows: {B}Index, Sport, Gender, League Index, Name, Country Code,
  Nearest City, Uses Definite Article{B}) - club names with inline alternates, e.g.
  {B}Arizona <Swallows/Scorpions/Dry Heat/Red...>{B}.
- {B}a16_sports-positions-enc{B} (47 rows: {B}Index, Sport, Position, ...{B}) - goalkeeper,
  defender, midfielder.

{B}a16_dumb-scenarios-sports-enc{B} is the 85-row pool. Its tokens were
{B}<game>{B} 62, {B}<rb>{B} 50, {B}<teammate>{B} 27, {B}<coach>{B} 23, {B}<lover>{B} 15,
{B}<field>{B} 13, {B}<league>{B} 11, {B}<stadium>{B} 11, {B}<sport>{B} 6, {B}<insult>{B} 6,
{B}<rb2>{B} 6, {B}<coach-possessive>{B} 3, {B}<teammate-possessive>{B} 3, {B}<team>{B} 1 - of which only
the sports words were new; the rest already existed.

### 52.2 Implementation

- {B}include/bl/game.h{B}: {B}Hero::athlete, Hero::sport, Hero::league, Hero::team,
  Hero::position{B}.
- {B}src/sim.cpp{B}: stream {B}sportsRng_{B}; {B}Sim::makeAthlete{B} draws a sport row, then the
  league for that sport and gender, then a team and a position for the same sport;
  {B}Sim::sportsToken{B} resolves the words (team names go back through {B}resolve()B{}, because they
  carry {B}<a/b/c>{B} alternates); {B}Sim::sportsYear{B} renders a pool row on 45% of the years from
  18 on; {B}ageUp{B} calls it right after {B}careerYear{B}.
- {B}resolveInto{B} gained one branch for {B}sport / league / team / teammate / coach / ball /
  referee / game / field / stadium{B}; an unfilled token stays verbatim as everywhere else.
- {B}include/bl/script.h{B} / {B}src/script.cpp{B}: the {B}"sports"{B} boolean, parsed like
  {B}"royal" / "military" / "vampire"{B} and applied with {B}sim.makeAthlete(life){B}.

### 52.3 Shortcuts

1. The gate is the script switch. The game reaches the pool through a professional-sports career
   with a draft, a position and a league index; the port models the sport, the league, the team
   and the position but not the draft, the season or the contract.
2. {B}Min Fitness / Min Greatness / Rookie Only{B} and the gender gates on the pool rows are not
   evaluated - the pool is filtered on {B}Hero Min Age{B} only.
3. {B}Uses Definite Article{B} on the team table is ignored, so a team that needs 'the' is printed
   bare.
4. {B}<coach>{B} and {B}<teammate>{B} are drawn from the port's ordinary name pools; the row's own
   gender (the pool has separate {B}Playboy{B} / {B}Playgirl{B} rows) is respected only through the
   card text, not through the coach's name.
5. Stats, records, awards, injuries and the {B}a16_sports-practice-scenarios-enc{B} family are not
   wired; only the dumb pool is.

### 52.4 Verification

- Seed 12345 (football): 11 pool rows, e.g. 19 {B}Eight Inch Mile{B}, 20 {B}Fresh Meat{B},
  26 {B}Merch Madness{B} with {B}<coach>{B} = Quinn Janim and the log {B}I told my coach I didn't know
  how the team could increase merchandise sales.{B}
- Seed 777 (basketball): 23 {B}Playgirl{B} renders {B}Dustin Maya notices that you're paying more
  attention to your bedroom game than your court game.{B} - the basketball {B}Field Bracket{B} word.
- Seed 4242: 18 {B}GOATs{B}, 20/23 {B}Fair Trade{B} with {B}<stadium>{B} resolved.
- Unchanged: residue census 0 tokens, {B}tools/coverage.py{B} 109 / 107, {B}portgap.py{B}
  109 / 107 / gap 0, {B}tools/bench.sh{B} to age 34 with no abort, all-109 push replay exits 0.

## 53. The business pool: an industry, its facilities and its rivals

### 53.1 The tables

- {B}a16_business-industries-enc{B} - 25 rows, {B}Industry, Country Whitelist, Country Blacklist, Product
  Launch Cost, Facility Name, Facility Name Plural, ...{B}. The singular/plural pair is what the pool
  prints as {B}<facility-singular>{B} / {B}<facility-multiple>{B} ('brewery' / 'breweries'), which is why
  the card talks about your stores, your factories or your offices depending only on the industry.
- {B}a16_business-products-enc{B} - 282 rows, {B}Industry, Product, Product Name Base, Unit, Unit Plural,
  Avg Unit Price, ...{B}. {B}Product{B} is a comma-separated list per industry, so {B}<product>{B} draws one
  entry from the list.
- {B}a16_business-name-structure-enc{B} - 25 rows, {B}Industry, Format, Prefix, Suffix, Predefined{B} - the
  rival names. Format is the same mechanical template as the company names in section 37
  ({B}<city> <suffix>, <color> <suffix>, <random-first-name-possessive> <suffix>{B}).
- {B}a16_business-product-names-enc{B} - 260 rows, {B}Specific Industry, Specific Product, Format,
  Commonness Score, Hero Owns Company{B}; loaded, but the port's {B}<product>{B} currently draws from
  {B}a16_business-products-enc{B} instead.

{B}a16_dumb-scenarios-business-enc{B} is 168 rows and gates itself harder than any pool so far:
{B}Industry Whitelist{B}, {B}Industry Blacklist{B}, {B}Requires Single Facility{B},
{B}Requires Multiple Facilities{B} and {B}Business Size{B} sit beside the usual age and gender columns.
Its tokens are {B}<employee>{B} 277, {B}<rb>{B} 109, {B}<facility-singular>{B} 89, {B}<product>{B} 53,
{B}<employee-possessive>{B} 29, {B}<competitor-possessive>{B} 28, {B}<facility-multiple>{B} 13,
{B}<insult>{B} 13, {B}<country-other>{B} 7, {B}<competitor>{B} 5 - five of them new.

### 53.2 Implementation

- {B}include/bl/game.h{B}: {B}Hero::ownsBusiness, Hero::industry{B}.
- {B}src/sim.cpp{B}: stream {B}bizRng_{B} (line 571); {B}Sim::makeBusiness{B} (line 2755) draws one industry;
  {B}Sim::businessName{B} (line 2769) builds a rival company from the name structure table with the same
  {B}<color> / <suffix> / <prefix> / <city> / <random-first-name(-possessive)> / <random-surname>{B}
  substitution as the company names, dropping any bracket it cannot fill;
  {B}Sim::businessToken{B} (line 2821) resolves {B}employee / employee-possessive / competitor /
  competitor-possessive / facility-singular / facility-multiple / product{B};
  {B}Sim::businessYear{B} (line 2861) renders a pool row on 40% of the years from 18, filtering on
  {B}Hero Min Age{B}, the whitelist and the blacklist; {B}ageUp{B} calls it at line 1850.
- {B}include/bl/script.h{B} / {B}src/script.cpp{B}: the {B}"business"{B} boolean, parsed like
  {B}"sports"{B} and applied with {B}sim.makeBusiness(life){B}.

### 53.3 Shortcuts

1. The gate is the script switch; the game reaches the pool through the Business menu and its
   company, employees and facilities.
2. {B}Requires Single Facility / Requires Multiple Facilities / Business Size{B} are not evaluated, and
   the hero has no facility count, so a row that wants several stores can fire for a one-room
   business.
3. {B}Min Fitness{B}-style attribute gates and the country whitelist on the industry table are
   ignored.
4. {B}<employee>{B} / {B}<competitor>{B} are names from the port's own pools; the rival company uses the
   industry table only when an industry is passed, and the pool's {B}<competitor>{B} does not say which
   industry the rival is in, so it draws from all 25.
5. Salaries, profits, launches and the whole {B}a16_business-launch-*{B} family are not wired.

### 53.4 Verification

- Seed 12345: 9 pool rows, e.g. 20 {B}Employee Relations{B} with {B}<employee>{B} = Morgan Degler jamming
  to Beck, 27 {B}Customer Review{B} about 'white pearl earrings' (the industry's product).
- Seed 777: 10 rows, e.g. 29 {B}Situation{B} with {B}<competitor>{B} = 'Ultra Beauty Barn', 37
  {B}Employee Performance{B} with the log {B}I sent Glow Sea home after witnessing him place clothes on
  hangers that don't correctly correspond with their size.{B}
- No angle-bracket token survives in the business run (0 of 0 checked lines); residue census 0;
  {B}tools/coverage.py{B} 109 / 107; {B}portgap.py{B} 109 / 107 / gap 0; {B}tools/bench.sh{B} to age 34 with no
  abort; all-109 push replay exits 0.

## 54. The music and album pools: genre-flagged titles

### 54.1 The tables

- {B}a16_music-genres-enc{B} - 11 rows, {B}Genre, Singer Title, Famous Title, Solo Roles, Band Roles,
  Push Stage Name{B} ('pop' / 'pop singer' / 'pop star').
- {B}a16_music-names-albums-enc{B} - 412 rows, {B}Title{B} plus one flag column per genre
  ({B}Pop, Rap, Country, Folk, R&B{B}, and further columns for the other genres), so a title belongs
  to the genres whose column is 'x'.
- {B}a16_music-names-songs-enc{B} - 512 rows, same shape ({B}Title, Pop, Rap, Country, Folk, R&B,
  Worship, Rock{B}).
- {B}a16_music-names-bands-enc{B} - 430 rows, {B}Title, Hero Only, Gender, Pop, Rap, Country, ...{B}.
- {B}a16_music-names-stage-enc{B} (184 rows) and {B}a16_music-artist-stage-name-enc{B} (246 rows) are the
  stage names; the port does not draw them yet.

Titles are not plain strings: they carry the same inline forms as the narrative tables -
{B}Around the <Bush/Mountain/Corner>{B} - and bands can be {B}The <hero-name> Band{B}, so every drawn
title goes back through the resolver.

The two pools are {B}a16_dumb-scenarios-music-enc{B} (98 rows, tokens {B}<song-title>{B} 38,
{B}<band-member>{B} 18, {B}<city>{B} 19, {B}<celebrity-male1>{B} 5, {B}<hero-last-name>{B} 4) and
{B}a16_dumb-scenarios-album-enc{B} (77 rows, tokens {B}<album-title>{B} 325, {B}<song-title>{B} 20 and
{B}<song-title1>{B}..{B}<song-title4>{B} 16 each, {B}<celebrity>{B} 15, {B}<friend>{B} 12,
{B}<celebrity-male>{B} 10). The numbered song slots are four different songs on one card, which is why
they appear as a family rather than one repeated token.

### 54.2 Implementation

- {B}include/bl/game.h{B}: {B}Hero::musician, Hero::musicGenre, Hero::bandName{B}.
- {B}src/sim.cpp{B}: stream {B}musicRng_{B} (line 572); {B}Sim::makeMusician{B} (line 2773) draws the genre
  and then a band name in that genre; {B}Sim::musicTitle{B} (line 2790) restricts a titles table to the
  genre's flag column and re-resolves the drawn title (alternates, {B}<hero-name>{B});
  {B}Sim::musicToken{B} (line 2809) resolves {B}album-title{B}, the {B}song-title{B} family,
  {B}band-member{B} and {B}music-genre{B}; {B}Sim::musicYear{B} (line 2823) picks one of the two pools on a
  coin flip and renders a row on 40% of the years from 16; {B}ageUp{B} calls it at line 1863.
- {B}include/bl/script.h{B} / {B}src/script.cpp{B}: the {B}"music"{B} boolean.

### 54.3 Shortcuts

1. The gate is the script switch; the game reaches these pools through the Music career (bands,
   solo deals, labels, tours).
2. {B}<song-title1>..<song-title4>{B} draw independently, so two slots on one card can coincidentally
   land on the same title; the game's numbered slots are distinct songs.
3. {B}Hero Only{B} on the band table and the {B}Gender{B} column are ignored, so a female hero can be
   handed a male-only band name.
4. {B}Min Popularity / Max Popularity{B} and the lover and gender gates on the pool rows are not
   evaluated; {B}Hero Min Age{B} is.
5. Stage names, album covers, producers, concerts, labels and the whole
   {B}a16_music-practice-scenarios-enc{B} / {B}a16_music-producer-*{B} family are not wired.

### 54.4 Verification

- Seed 12345: 12 pool rows. 17 (album) {B}All Real, Baby{B} - {B}You decide it's time you released a live
  version of Hit the Voodoo...{B}; 22 {B}While recording Grateful for Daisies, one of your bandmates
  accidentally threw a tantrum during the recording.{B}; 27 {B}The anniversary of Aaliyah's death has come
  around...{B}; 28 {B}The local Oklahoma City music magazine 'Below the Radar' is doing a feature on your
  new album, If You're Seeing This It's Celebration Time...{B} - album title, song title, city and the
  numbered celebrity slot all resolved.
- No angle-bracket token survives in a music run (0 residual of ~170 checked strings per seed, three
  seeds); residue census 0; {B}tools/coverage.py{B} 109 / 107; {B}portgap.py{B} 109 / 107 / gap 0;
  {B}tools/bench.sh{B} to age 34 with no abort; all-109 push replay exits 0.
## 55. The politics pool: an office drawn per country, and a title that holds a token

### 55.1 What the tables hold

`a16_country-political-systems-enc` is 102 rows, one per country. Each row declares which of four
levels the country actually elects - National Office Available, Regional Office Available, City
Office Available, Local Office Available - and gives the office title for each: Title National,
Title National Adjective, Title Regional, Title Regional Adjective, Title Regional Specific, Title
City, Title Local. Two more columns describe the region the office sits in: Region Jurisdiction
(the noun, e.g. province, state, prefecture) and Region Jurisdiction Adjective (provincial, state,
prefectural).

The title column is where this table stops being a lookup. Afghanistan and Algeria are not the
exception: Title Regional reads `<jurisdiction adjective> governor` for most countries, and only a
minority spell the office out (`mayor`, `premier`, `governor`). The value the game stores is
therefore a string that still has to be resolved, and the row that resolves it is the same row that
supplies the jurisdiction adjective. The port resolves the title after the level is known rather
than storing the raw cell.

Two pools render the token names back into prose. `a16_dumb-scenarios-political-office-enc` (90
rows) is the officeholder's life: a corruption rumour, a veto, a state dinner. `a16_dumb-scenarios-
political-campaign-enc` (64 rows) is the candidate's: a smear, a debate, a rally. Both are gated:
US Only, UK Only, and one flag per level (National / Regional / City / Local) pick which pool rows a
given country and office can use, and Hero Gender / Hero Has Lover / Lover Gender gate the personal
rows. Counter per pool file, over both pools: `<jurisdiction>` is by far the most common at 29
occurrences in the campaign pool alone, then `<jurisdiction-possessive>` 13, `<opponent-possessive>`
11, `<political-office>` 5, `<opponent>` 4, and single hits of `<country-other>`,
`<country-other-possessive>`, `<num10-25>` and `<num5000-10000>`. `<jurisdiction adjective>`
appears five times - and not in either pool file, but inside the country table's Title Regional cell,
which is why it reaches a rendered line even though neither pool names it.

### 55.2 What the port does

- `Hero` (`include/bl/game.h`) gains `politician`, `officeLevel`, `office`,
  `jurisdiction` and `jurisdictionAdj`.
- `Sim::ensureOfficeFields` (src/sim.cpp:2795) draws the campaign from the country row: it walks the
  four `* Office Available` flags in level order, picks one of the available levels, then sets the
  jurisdiction noun from Region Jurisdiction (`country`, or `city` / `neighborhood` when the
  column is empty, never an empty token), the jurisdiction adjective from the new column, and only
  then resolves the title cell, so `<jurisdiction adjective> governor` becomes `provincial
  governor` using the adjective that was just drawn.
- `Sim::makePolitician` (src/sim.cpp:2849) is `ensureOfficeFields` plus `politician = true` - the
  two are split because a candidate asking for votes holds nothing yet but still needs both the
  office and the jurisdiction named.
- `Sim::politicalToken` (src/sim.cpp:2855) resolves `jurisdiction`, `jurisdiction-possessive`,
  `jurisdiction adjective` (the pool's space spelling and the hyphen spelling both land here),
  `political-office`, `political-office-possessive`, `opponent`, `opponent-possessive`,
  `country-other` and `country-other-possessive`. The opponent is a fresh name from the port's
  pools drawn on its own stream; `country-other` reuses `heroToken` so the rival country is not
  the hero's own.
- `Sim::politicalYear` (src/sim.cpp:2891) runs from 18. A hero who holds no office has a 25% chance
  of a campaign row (the campaign pool); a politician on 40% of years renders one of the two pools,
  picking between office and campaign on the same `politicsRng_` stream.
- `resolveInto` (src/sim.cpp:1012) takes the whole token family, including the spaced spelling.
- `include/bl/script.h` / `src/script.cpp`: the `"politics"` boolean, parsed and applied like
  `"business"` and `"music"`.
- A campaign row renders while the hero still holds nothing, so the port draws the office on demand
  from `politicalToken` (`ensureOfficeFields` on the const-cast `life_*`) instead of leaving the
  row half-rendered; the office guard sits below the jurisdiction keys so the title's own token can be
  resolved while the title is still being built.

### 55.3 Shortcuts

1. The gate is the script switch; the game reaches the pools through the Politics menu and its
   campaign.
2. `US Only` / `UK Only` and the four level flags are not evaluated, so a country that elects no
   regional office can still be handed a regional campaign row.
3. `Hero Has Lover` / `Lover Gender` are not evaluated; `<num5000-10000>` campaign donations and
   `<num10-25>` slush amounts come from the generic number pools, not from any finance model.
4. Elections are never held: a politician stays in the same office for life, there is no term, no
   approval rating and no loss.
5. The title cell is resolved with the drawn adjective, but the country table's Title National
   Adjective / Title Regional Adjective / Title City columns (which hold words like presidential)
   are not otherwise used, since the pool text only ever asks for the `<jurisdiction adjective>`
   spelling.
6. Only the two dumb-scenario pools are wired - `a16_political-*` careers, the ballot and the
   campaign-finance family are not.

### 55.4 Verification

- Three seeds over United States, India and the United Kingdom: 0 angle-bracket tokens survive in
  the office and campaign rows, before the fixes 18 did; residue census 0 distinct tokens;
  `tools/coverage.py` 109 / 107; `portgap.py` 109 / 107 / gap 0; `tools/bench.sh` to age 34 with no
  abort; all-109 push replay exits 0.
- Two bugs of the same shape as the war-token one were found and fixed here: `compare(size - 10, 10,
  "-possessive")` never matches an 11-character suffix, so every possessive token in the business and
  politics pools was leaking its raw token, and the same offset was used to cut the key
  (`token.substr(0, token.size() - 10)`). Both now go through `psuffix.size()`.

## 56. The nine remaining dumb-scenario pools: one runner, one token table

### 56.1 What the tables hold

After the political pair, nine of the twenty-four `a16_dumb-scenarios-*` sets were still unread:
actor (165 rows), casino (24), dealer (28), hustler (121), landlord (43), mafia (50), model (27),
secret-agent (10) and zoo (18). They are shaped like the pools already wired - a row is a title, a
question and three to four consequences - and they are the last pools in the set that no port
subsystem reaches.

The difference from the earlier pools is where their nouns come from. Each one names its own cast:
`<mafia-godfather>` and `<mafia-colleague>`, `<drug-cartel>` and `<cartel-kingpin>`,
`<tenant>` and `<amenity>` and `<house-type>`, `<animal-zoo>` and `<habitat-zoo>`,
`<booking-agent>`, `<production>` and `<talk-show>`, `<agent>` and `<front>`,
`<casino-gambler>` and `<casino-room>`, `<hustle-box>` and `<hustle-instrument>`. Almost
all of them have a source table of their own: `a16_mafia-nicknames-enc` (142 nicknames, and they
carry a token themselves: `<first-name> Meatballs`), `a16_mafia-mobs-enc` (7 families)
`a16_dealer-cartel-name-enc` (64), `a16_hustler-collection-boxes-enc`,
`a16_hustler-panhandle-strategies-enc`, `a16_hustler-busk-music-types-enc` (whose Possible
Instruments column is a comma list), `a16_landlord-home-default-features-enc` (105 features),
`a16_house-types-enc`, `a16_zoo-animals-enc` (120) and `a16_zoo-habitat-names-enc`,
`a16_actor-films-enc` (342 titles) and `a16_actor-shows-enc` (241) and `a16_actors-real-enc`,
`a16_secret-agent-agency-fronts-enc`, `a16_casino-casino-game-room-enc` and
`a16_casino-random-hero-casino-name-enc`, plus `a16_street-names-enc` for both the tenant's
address and the hustler's corner. A few ask for a person (`<tenant>`, `<costar>`,
`<director>`, `<booking-agent>`, `<agency-rival>`, `<cartel-member>`) and three are
fixed job words with no table behind them (`<animal-handler>`, `<zoo-veterinarian>`,
`<zoo-operations-manager>`).

Everything else in these pools - `<rb>` to `<rb3>`, `<he/she>` and `<him/her>`,
`<city>`, `<random-person>`, `<random-person-indefinite>`, `<celebrity>`,
`<circumstance-public>`, `<insult>`, `<lover>`, `<friend>`, `<num20-50>` and the inline
alternative lists such as `<shopping/run some errands>` - is already covered by the port's generic
token handling from sections 33 to 46.

### 56.2 What the port does

- `Hero::miscMask` (include/bl/game.h:137) is one bit per pool; `Sim::makeMisc`
  (src/sim.cpp:3028) sets the bits the script asked for. Unlike the career, sports, business, music
  and politics subsystems there is no role to draw, so this is the whole of the setup: the pools have
  no progression model behind them.
- `Sim::miscPoolYear` (src/sim.cpp:3030) walks a nine-entry table of `{table, min age, chance}` -
  mafia 18 and 30%, dealer 18 and 30%, hustler 16 and 35%, landlord 18 and 35%, zoo 18 and 30%, model
  16 and 30%, actor 16 and 30%, secret agent 21 and 25%, casino 21 and 30% - and renders one row from
  the first switched-on pool that fires, on its own `miscRng_` stream (`seed ^ 0xd6e8feb86659fd93`).
  `ageUp` calls it at src/sim.cpp:1972, after the political year.
- `Sim::miscToken` (src/sim.cpp:2951) carries the token-to-table map: `kMiscSrc` pairs each pool
  token with a table and column, `kMiscPeople` lists the tokens that want a person from the name
  pools, and the remaining cases are the three zoo job words, `<hustle-instrument>` (one item out of
  the music type's Possible Instruments list) and `<tenant-address>` (a house number plus a
  `<random-surname>` street). A table cell that still holds tokens is filled before it is resolved:
  the nickname table's `<first-name>` and `<last-name>`, and the street list's
  `<random-surname>`.
- `resolveInto` (src/sim.cpp:1111) routes every one of those names, possessive spellings included.
- `include/bl/script.h` / `src/script.cpp`: `"mafia"`, `"dealer"`, `"hustler"`,
  `"landlord"`, `"zoo"`, `"model"`, `"actor"`, `"secret-agent"` and `"casino"`, parsed
  like `"politics"` and folded into the mask with `sim.makeMisc(life, miscMask)`.
- Three shared fixes came out of these pools, all of them in code the earlier pools also use:
  1. An inline alternative list with an empty middle option (`<MILF/volleyball
     coach/biker chick/cyclist//lifeguard/...>`) was treated as a closing tag and left verbatim,
     because the guard that recognises `</b>` tested every option instead of the first one. Only a
     token that starts with the separator is a closing tag now, and empty alternatives are skipped
     when one is drawn.
  2. `<rb3>` and its siblings are cached alternatives, so a row that asks for more alternatives than
     it drew leaked the token. It now falls back to the last alternative drawn, matching how the game
     reads its own replacement list.
  3. Every table-sourced value goes through `ensureTable` first; a pool that names a table the port
     does not load (as `<habitat-zoo>` did before `a16_zoo-habitat-names-enc` was added) now
     returns an empty token instead of throwing out of `tbl`.

### 56.3 Shortcuts

1. The gate is the script switch; the game reaches these pools through the Mafia, Cartel, Hustler,
   Landlord, Zoo, Modeling, Acting, Secret Agent and Casino menus and their own progression.
2. The pools are reachable at their minimum ages only, with no join step: a hero can be handed a
   mafia or secret-agent row without ever having joined anything.
3. Row gates are not evaluated. `a16_dumb-scenarios-hustler-enc` alone carries police-risk,
   theft-risk and confrontation-risk columns, and the casino and dealer pools gate on how deep the
   hero already is; none of that is read.
4. `<costar>`, `<director>`, `<booking-agent>` and the rest of the people series are names from
   the port's own pools, not from any casting table.
5. The three zoo job words are literals, since no zoo staff table exists in the gamedata.
6. Only the nine dumb-scenario pools are wired: the `a16_mafia-*`, `a16_dealer-cartel-*`,
   `a16_hustler-*`, `a16_landlord-*`, `a16_zoo-*`, `a16_model-*`, `a16_actor-*`,
   `a16_secret-agent-*` and `a16_casino-*` families themselves are not.

### 56.4 Verification

- Four seeds, all nine pools switched on together and one per pool: 25 to 34 rows render per pool,
  0 angle-bracket tokens survive across every title, body and log line of all nine;
  `tools/coverage.py` 109 / 107; `portgap.py` 109 / 107 / gap 0; `tools/bench.sh` to age 34 with
  no abort; all-109 push replay exits 0; residue census 0 distinct tokens.
- The whole `a16_dumb-scenarios-*` family is now read: the remaining unwired pools are none.


## 57. The relationship subsystem: a partner, an ex, and the pools that move between them

The port had no relationship model at all: `grep -rn lover include/bl/` found nothing, and every
`<lover>` in the content fell through to `personName()`, which drew a fresh celebrity full name
on every call. A row that named the partner twice named two different people, and the 22
relationship pools were never read. This section wires them.

### 57.1 The tables

21 tables, 2138 rows, all added to `kTables`.

| table | rows | recorded text | gates read |
| --- | --- | --- | --- |
| `a16_date-rejections-enc` | 141 | Scenario | Hero/Lover Gender, Min/Max Age, Hero Min/Max Smarts, Hero Min/Max Looks |
| `a16_hookup-scenarios-enc` | 165 | Scenario | Gender, Hero Gender |
| `a16_hookup-feedback-enc` | 153 | Feedback | Hero/Partner Gender |
| `a16_make-love-rejections-enc` | 193 | Scenario + Log Version | Hero/Lover Gender, Lover Min/Max Age |
| `a16_lover-suspicious-scenarios-enc` | 172 | Scenario | Hero/Lover Gender, Has Job |
| `a16_lover-cheat-scenarios-enc` | 107 | Scenario + Log Version | Hero/Lover Min/Max Age |
| `a16_lover-make-love-nags-enc` | 105 | Scenario + Log Version | Married, Min/Max Relationship, Hero Min Age |
| `a16_lover-marriage-nags-enc` | 80 | Scenario | Hero Virgin, Min/Max Craziness, Min/Max Relationship |
| `a16_anniversary-date-enc` | 100 | Activity + Log String | Min/Max Age |
| `a16_breakup-scenarios-enc` | 227 | Scenario | Gender, Straight Only |
| `a16_ex-lover-rekindle-scenarios-enc` | 131 | Scenario | Hero/Ex Gender, Min/Max Age |
| `a16_ex-lover-hate-scenarios-enc` | 70 | Scenario | Hero/Ex Gender |
| `a16_ex-lover-booty-call-scenarios-enc` | 61 | Scenario | Hero/Ex Gender |
| `a16_ex-lover-stalking-scenarios-enc` | 71 | Outbound/Inbound Scenario | none |
| `a16_relationship-counseling-scenarios-enc` | 53 | Feedback + Log Version | Max Relationship, Min/Max Age |
| `a16_parent-lover-disapprovals-enc` | 92 | Scenario + Log Version | Min/Max Smarts, Max Looks, Min Craziness |
| `a16_wedding-venues-enc` | 12 | Venue | none |
| `a16_school-classmate-flirt-scenarios-enc` | 22 | Feedback Scenario + Log Version | none |
| `a16_friend-lover-scenarios-enc` | 52 | Scenario | Friend Min/Max Age |
| `a16_coworker-hookup-scenarios-enc` | 24 | Scenario | none |
| `a16_disease-types-enc` | 107 | Type (drawn, never recorded) | none |

### 57.2 Implementation

Hero gained a partner: `loverName / loverFemale / loverAge / loverStatus` (empty, `dating` or
`married`), a `relationship` score from 0 to 100 that starts at 60, `loverCraziness`, `virgin`,
and the ex side `exName / exFemale / exes`. `Sim::makeRelationships(Life&)` is the scripted entry
point; `Sim::relationshipYear` is the yearly runner, called from `ageUp` after `miscPoolYear`. It
draws from its own `relRng_` stream (seed ^ 0x1b873593e6546b64) so the main sequence does not shift.

The score drifts by up to four points a year and clamps. Each pool that fires moves it the way
the game marks its meter: `lover-cheat` -14, `lover-suspicious` -12, a rejected `make-love` -6,
`lover-make-love-nags` -4, `parent-lover-disapprovals` -3, `hookup` +4, `anniversary` +8, `counseling` +12.
A score under 30 opens the breakup pool, which moves the partner into the ex slot and resets the
score; the four ex pools then run off that slot.

Rendering: `relEligible` filters rows on the gender, age, relationship, craziness, smarts and looks
columns before a pick, and `relationshipToken` answers the tokens. `<lover>` and `<lover-possessive>`
are now the stable partner, `<ex>` the ex, `<friend-lover>` a drawn first name, `<gender>` the word
woman or man, `<house>` a type from `a16_house-types-enc`, `<disease>` a type from
`a16_disease-types-enc`, and `<addiction>` one of alcohol, drugs, gambling or sex. `<name>` is the
stranger a row introduces, cached per card, so a row that names the person twice keeps the same one.

Two pronoun slots exist because these pools write the partner as the subject: `<he/she> says it is not
you, it is <him/her>`. `genderToken` now reads `pronounOtherSet_` and `pronounOtherFemale_` when the
runner has set them, and the runner sets them from the partner, or from the ex, for the duration of
one row.

One shared renderer bug fell out of this. A narrative-only row keeps one text column, but its log
line can refer to an alternative that was drawn in another column: `The relationship counselor
suggested that we start having sex <rb>` reads the alternative out of the `Feedback` column, and `I winked
at <name> while our teacher was giving a lesson on <rb>` out of `Feedback Scenario`. The narrative path
now resolves every other column that holds a bracket, in column order, before it resolves the
recorded line, which is also the order the alternatives were drawn in.

A second one came from the politics pool: `a16_dumb-scenarios-political-campaign-enc` gates rows on
`Hero Has Lover` and `Lover Gender`, and the port had been drawing those rows for single heroes, which
is how `<lover>` leaked into a campaign log. The campaign row filter now reads both columns, plus
the `Hero Gender` value column.

### 57.3 Shortcuts

- The gates in 57.1 are read. `Clean`, the pregnancy columns, `Multiple Children`, `Vampire Hero`,
  `Vampire Lover`, `Prison`, `Post Deployment`, `Detection Score`, `Effectiveness Score`,
  `Relationship Impact`, `Cost` and the house-appreciation columns are not.
  `shortcut: unread gating columns, model them when the meter and the money side exist`
- The partner is invented by the port: a drawn first name, an age within two years, a random
  craziness. BitLife builds one from its dating flow, which the port does not have.
- Pool probabilities are hand-set, between 0.25 and 0.06 per pool per year, and every pool shares
  one order of precedence rather than the game's own.
- `wedding-venues`, the stalking detection score, counseling effectiveness, and the anniversary cost
  and impact columns are drawn but never applied to money or happiness.
- The ex pools mutate the name slot only; the game gives an ex their own age, looks and history.

### 57.4 Verification

30 seeds x both genders x three script flags (relationship only, politics only, both), ages 60 so
the late-life pools are reachable: zero residual tokens in every title, body and log line,
including the pools that only open after a breakup. Fired counts over 25 seeds x two genders:
make-love-rejections 137, lover-cheat 123, hookup 103, lover-suspicious 100,
parent-lover-disapprovals 81, make-love-nags 75, date-rejections 59, breakup 54, counseling 23,
marriage-nags 22, ex-rekindle 13, booty-call 11, ex-hate 5, classmate-flirt 4.

Regression with the subsystem on: `rescensus` 0 distinct residue tokens, `tools/coverage.py` 109
captured / 107 recoverable, `portgap.py` ENTRIES 109 rendered-by-port 107 gap 0, `tools/bench.sh` to
age 34 with no abort, and the all-109 push replay exits 0.
## 58. The secret-agent family: one service, five kinds of job, and a briefing assembled from sixty tables

The `a16_secret-agent-*` family is the largest unwired family in the shipped data: 60 tables, 1838 rows. It is not a scenario pool, it is a career: a hiring line, five kinds of job with their own briefing tables, a result table, and a shelf of agency furniture. Nine of the sixty are pure UI colour pools (`agency-front-color`, `client-mission-terminated-color`, `hack-mission-briefing-color`, `hack-mission-file-color`, `honeypot-approach-color`, `honeypot-meeting-scenario-color`, `honeypot-misssion-briefing-color` — the data's own typo — `potential-agent-background-check-color`, `stealth-alarm-color`): the port has no colour channel, so those nine stay unused by design. The other 51 are read.

### 58.1 The tables

The hiring and identity side: `random-hero-agency-names` 43, `rival-agency-names` 34, `code-names` 78, `mission-code-names` 47, `potential-agent-color` 37, `potential-agent-careers` 82, `agency-fronts` 16, `agency-front-names` 16, `agency-street-names` 34, `agency-gadgets` 16, `security-features` 13, `resignation-reasons` 14.

The job side: `mission-target-people` 55, `mission-summary-feedback` 20, `train-scenarios` 30, `warnings` 20, `warn-agent-retorts` 21, `missing-agent-reveal-snippet` 18, `gadget-positive-snippet` 16, `gadget-negative-snippet` 48.

Per type: assassination has `assassination-methods` 11, `assassination-mission-methods` 12, `assassination-locations` 10, `assassination-client-reasons` 22, `assassination-target-activities` 55, `assassination-target-opportunity-snippets` 17, `assassination-body-disposal-methods` 7, `assassination-body-disposal-locations` 7; stealth has `stealth-item-targets` 20, `stealth-mission-locations` 30, `stealth-mission-obstacles` 40, `stealth-cover-encounters` 30, `stealth-cover-hiding-place` 20, `stealth-door-noises` 17, `stealth-obstacle-stories` 29, `stealth-obstacle-assassinations` 11; honeypot has `honeypot-target-preferences` 18, `honeypot-agent-disguises` 105, `honeypot-agent-approach-style` 8, `honeypot-meeting-locations` 9, `honeypot-date-locations` 84, `honeypot-target-hobbies` 13, `honeypot-sting-actions` 33, `honeypot-sting-locations` 20; hacking has `hack-mission-file-name` 100, `hack-drive-names` 61, `infiltration-passwords` 36; intel has `spy-on-person-intel` 37 and `snoop-person-activity` 15. The client-side ambience is `distraction-random-people` 17 and `distraction-people-activities` 19.

### 58.2 The career line

`Hero` gained `secretAgent`, `spyAgency`, `spyCodeName`, `spySpec`, `spyProficiency` (0-100), `spyMissions`, `spyCover` and `spyAddress`. `Sim::makeSecretAgent` (src/sim.cpp) draws the agency from `random-hero-agency-names` weighted by `Commonness Score`, the code name from `code-names` (its `Gender` column is blank or [female/male], so a blank cell matches anyone) weighted the same way, the specialization from `potential-agent-color` `Specialization`, a cover job from `potential-agent-careers` `Occupation`, and turns `spyProficiency` into `20 + below(21)`. Both the agency name and the code name go through `resolve(trim(...))` before they are stored: their cells are themselves bracket templates (`<P/G/V/D>-Wagon`), and a stored raw bracket would leak into every later string that names the agent. The cover address is a house number plus a street from `agency-street-names`. The hero also gets `miscMask` bit 7, so the `a16_dumb-scenarios-secret-agent` pool rides along with the career.

`Sim::spyYear` runs for a serving agent of 18 or more and fires on 35% of years. Each year is one of four things:

1. **A year on the range** (25%, only while proficiency is below 80): `train-scenarios` filtered by the hero's specialization, with the drawing colleague's name substituted for `<agent-spy>` through the `spyAgentIsOther_` flag. Proficiency gains 2.
2. **Resignation** (4%, after three missions and at proficiency 45 or more): `resignation-reasons` filtered by the hero's gender, the pronoun swapped to the first person, and the career ends — `secretAgent` off, the bit cleared, the agency behind the hero.
3. **A job.** The type is uniform over stealth, assassination, honeypot, hacking and intel. The target comes from `mission-target-people` through the type's own flag column (`Stealth` / `Assassination` / `Honeypot`; intel tails anybody, so it takes the whole table) weighted by `Commonness Score`, and the row's `Risk Score` feeds the odds: `clamp(0.55 + proficiency/200 - risk*0.04, 0.10, 0.95)`, so a risky target is survivable only for an experienced agent. A resolved target takes an article unless the drawn text already carries one (`<his/her> mistress` does, `corrupt police officer` does not).
4. Otherwise a mission that cannot be briefed is skipped, never rendered half-built.

The briefing is then assembled from the rest of the family — this is where most of the 51 tables land:

- **assassination**: "The client wants X dead. Suggested method: M. The opportunity comes at the L." The method and the result line come from `assassination-methods`, or — half the time — from `assassination-mission-methods`, which carries whole sentences in its own `Success Snippet` / `Failure Snippet` columns. On a failure the `Retaliatory String` is appended as a second beat with the pronoun forced into object position (it sits in object position in the source: "… electrocuted `<pronoun>` instead!"). `assassination-client-reasons` adds "The client says …" with a freshly drawn client's pronoun pair; `assassination-target-activities` and `assassination-target-opportunity-snippets` add the routine and the opening; `distraction-random-people` plus `distraction-people-activities` add the distraction.
- **stealth**: the item and verb from `stealth-item-targets` (the `Verbs` cell is a comma list: steal, destroy, photograph), the venue from `stealth-mission-locations` with its article decided by the `Uses Definite/Indefinite Article` flags, then security, the obstacle, the hostile entities `stealth-cover-encounters` expects, the hiding place and the `stealth-door-noises` line that covers the noise. `stealth-obstacle-stories` supplies the cover story told to that obstacle, with the obstacle's own pronoun pair drawn with it.
- **honeypot**: the mark's weak spot from `honeypot-target-preferences` (weighted by `Turn-On Commonness`), the meeting place from `honeypot-meeting-locations`, an approach from `honeypot-agent-approach-style`, a date location, and the sting from `honeypot-sting-actions` followed by `honeypot-sting-locations`. Every mark-facing string is swapped to the mark's own pronoun before it is resolved, because the target draw assumes the mark is the opposite sex.
- **hacking**: "Pull the <name> <file type> file off the <drive> drive and leave no trace." from `hack-mission-file-name` and `hack-drive-names`, plus the `infiltration-passwords` hint ("The password hints at X, not Y." — both cells are comma lists).
- **intel**: "Tail X." plus a `spy-on-person-intel` snippet and a `snoop-person-activity` line, both filtered to the opposite gender from the hero.
- **the result line**: `mission-summary-feedback` filtered by `Mission Type`, the success/failure flag, and the `Min/Max Agent Proficiency` window — a fresh agent literally cannot draw the veteran's line. It becomes the event's narrative. `gadget-positive-snippet` (on success) or `gadget-negative-snippet` (filtered by `Incorrect Mission Type`, on failure) adds the issued gadget; a successful assassination adds a `assassination-body-disposal-methods` and `…-locations` sentence; a failed one can add a rival agency from `rival-agency-names` or a `missing-agent-reveal-snippet` sighting; a botched sneak job on a guard adds `stealth-obstacle-assassinations`; and every so often a warning goes down the chain — `warnings` plus the colleague's `warn-agent-retorts` answer.

The event table is the drawn feedback row when there is one, and otherwise `mission-summary-feedback` (or `spy-on-person-intel` for intel, which has no feedback rows of its own). The title is `mission-code-names` rendered as "Operation <adjective> <noun>".

### 58.3 The tokens

`spyToken` maps the family's vocabulary: `agent-spy` is the hero's code name — or the colleague's drawn name while `spyAgentIsOther_` is set, which is how the same token serves both `train-scenarios` (where it is the trainee) and `assassination-methods` (where it is the killer); `client-spy` / `client` / `victim` are a drawn name, with `victim` preferring the drawn target; `target` and `target-assassination` are the mission target; `agency`, `code-name`, `specialization`, `cover`, `front`, `agency-rival`, `gadget` (filtered by the hero's specialization, with the `Plural` flag appending an s), `password`, `disguise` (gender-matched), `operation`, `color`, `pronoun`, `friend` and the three name forms. `front` is built from `agency-fronts` and `agency-front-names`: the row's `Format` cell is itself a template (`<city> <suffix>, <color> <suffix>, <random-first-name-possessive> <suffix>`), so the pieces are filled in and any bracket that no piece claims is dropped.

Pronouns inside mission rows always speak with the hero's gender, because the missions are the hero's own career; the two exceptions (`client-reasons`, `obstacle-stories`, `warnings`, `warn-agent-retorts`, the honeypot mark) set `pronounOtherSet_` / `pronounOtherFemale_` around the `resolve` call and restore the pair afterwards, so the runner does not drift for the rest of the year.

### 58.4 Wiring

`include/bl/script.h` carries `bool spy`, parsed from the script's `"spy"` key and passed to `Sim::makeSecretAgent` beside `makeRelationships`. `renderRow` clears `spyCache_` and `spyAgentIsOther_` next to the relationship name cache, so a card cannot inherit the previous card's agent. All rolling uses its own `spyRng_` (`seed ^ 0x7f4a7c15d1b54a3dULL`), which keeps the main sequence unshifted — the same rule every earlier subsystem follows.

### 58.5 Shortcuts

- `spyProficiency` is the only skill model: no training costs, no promotion ladder, no pay, no agency politics.
- The mission roll ignores the client, the calendar and the country; every serving agent works every type.
- `mission-target-people` rows carry `Use Indefinite/Definite Article` and `Commonness Score` columns; the article is chosen by the port's `articleFor` and the row by `Commonness Score` alone.
- `warnings` has a `Risk Score` column and `warn-agent-retorts` has `Min/Max Proficiency` windows; both are ignored and the rows are drawn uniform.
- `resignation-reasons` writes the agent in the third person, so its pronouns are swapped to the first person before rendering.
- `assassination-body-disposal-methods` ends its snippets on a possessive ("buried `<victim-possessive>`"), so joined with a location the sentence reads "I buried a prostitute's in a landfill" — the data's own shape, kept as shipped rather than rewritten.
- The nine colour pools are unused by design.
- `honeypot-agent-disguises`, `stealth-cover-hiding-place's `Hiding Place` and a few others are read for their text only; the family's own scoring columns are advisory.

### 58.6 Verification

The check is the `spy` script key over seeds 1-30 in both genders, ages 60, collecting every event whose table starts with `a16_secret-agent` and scanning title, body and log lines for stray brackets: **567 spy events, residue 0**. The rest of the regression suite is unchanged: `rescensus` reports `distinct residue tokens 0`, `coverage.py` 109 captured / 107 recoverable, `portgap.py` ENTRIES 109 / rendered-by-port 107 / gap 0, `bench.sh` reaches age 34 with no abort, and the 109-slot push script exits 0. A sample assassination reads:

> The client wants a corrupt judge dead. Suggested method: Strangle Them. The opportunity comes at the library. The client says he has it coming and only has himself to blame. The target is usually tossing bread crumbs to some ducks at the local park. Best opening: In his own little world. Distraction: a shifty-looking Instagram model, playing with her hair and glancing over at him.

and its log: "Darknight strangled a corrupt judge with some packing tape! My banana taser let me send 50,000 volts through a corrupt judge's leg. I used acid to dissolve a corrupt judge's body in a landfill."


## 59. The fight family: a combat career drawn from thirty-eight tables

The family is the game's boxing/MMA line: a hero who trains at a gym, fights on
cards, and moves up a league table. It is 38 tables and 2632 rows, and it is the
largest remaining career-shaped gap in the port.

### 59.1 The tables

| table | rows | what the port takes from it |
| --- | --- | --- |
| a16_fight-ing-style-identity-color-enc | 34 | **Fighting Style** (striker / submission artist / wrestler) — the hero's label |
| a16_fight-ing-arena-name-enc | 33 | **Arena Name**, plus **Is Championship Match Arena** as a gate |
| a16_fight-ing-opponent-type-enc | 63 | **Description**, **Percent Male**, **Fighter Level** (1-5), **Commonness Score** |
| a16_fight-ing-win-reason-enc | 76 | **Reason**, **Coolness Score**, **Min/Max Hero League Position** |
| a16_fight-ing-loss-reason-enc | 83 | **Reason**, **Sympathy Score**, **Min/Max Hero League Position** |
| a16_fight-outcome-color-enc | 37 | **Hero Win Version** / **Hero Lose Version**, with **<fighter>** = the opponent |
| a16_fight-coach-generic-feedback-string-enc | 42 | **String**, **Match Outcome**, **Min/Max Coach Quality** |
| a16_fight-coach-loss-feedback-string-enc | 30 | **String**, **Match Condition**, **Is Street Fight** |
| a16_fight-ing-coach-training-string-enc | 69 | **String**, **Training Type**, **Hero Gender**, **Coach Gender**, **Min/Max Coach Quality** |
| a16_fight-coach-quality-color-string-enc | 45 | **String** — the coach's own visible character, **Coach Gender** / **Min/Max Coach Quality** |
| a16_fight-ing-move-enc | 202 | **Move Name** (the gym line), plus the whole move system (unused) |
| a16_fight-hero-training-injury-string-enc | 5 | **String** — the ache after a session |
| a16_fight-ing-injurie-enc | 85 | **Type**, **Feedback Snippet**, four **... Decrease** columns, **Commonness Score** |
| a16_fight-ing-fan-interaction-enc | 34 | **Scenario**, **Log Version Snippet**, **Fan Gender** |
| a16_fight-ing-interview-fan-reaction-enc | 32 | **Scenario**, **Log Version**, **Match Outcome** |
| a16_fight-press-conference-question-enc | 29 | **Question** plus twenty **Choice i** cells and four flag blocks per choice |
| a16_fight-press-conference-media-outlet-enc | 25 | **Media Outlet String** |
| a16_fight-ing-sponsorship-company-enc | 38 | **Company Name**, **Min/Max Payout**, **Min/Max League Position** |
| a16_fight-ing-product-enc | 82 | **Product Name**, **Hero Gender**, **Min/Max Reputation** |
| a16_fighting-products | 11 | **Product Name**, **Min/Max Reputation** (no gender column) |
| a16_fight-ing-dive-scenario-enc | 29 | **Scenario** / **Log Version** — throwing the fight |
| a16_fight-wild-encounter-color | 11 | **ColorString** — the street encounter's own opening line |
| a16_fight-ing-wild-encounter-color-enc | 56 | **String**, **Log Version**, **Min/Max Hero League Position** |
| a16_fight-street-ing-location-enc | 42 | **Location** |
| a16_fight-er-animal-enc | 6 | **Animal**, **Attack Verb**, **Street Creds Gained** |
| a16_fight-sportsmanship-scenario-enc | 29 | **Is Positive**, **Scenario**, **Log Version** |
| a16_fight-roid-rage-scenario-enc | 66 | **Snippet**, **Audience**, **Audience Gender**, **Hero Min/Max Age** |
| a16_fight-roid-rage-retort-enc | 59 | **String**, **Audience Gender**, **Min/Max Audience Age**, **Min/Max Hero Age** |
| a16_fight-doping-challenge-color-string-enc | 17 | **String** — the accusation that the hero is dirty |
| a16_fight-recharge-method-enc | 15 | **Method**, **Cost**, **Stamina Gain**, the three impact columns |
| a16_fight-recharge-method-scenario-enc | 46 | **Method**, **String**, **Log Version** (the Method cell links the two tables) |
| a16_fight-ufl-dumb-scenario-enc | 44 | a full push card: Title, Body, CTA, Narrative 1, **Street Cred Base Value 1** |
| a16_fight-er-trash-talk-enc | 51 | the hero's own voice |
| a16_fight-er-compliment-enc | 30 | |
| a16_fight-er-admiration-scenario-enc | 17 | |
| a16_fight-er-concern-scenario-enc | 16 | |
| a16_fight-er-training-string-enc | 44 | |
| a16_fight-er-circumstance-enc | 42 | **String** — a lead-in phrase ("While out grabbing a new pair of punching gloves") |

### 59.2 The career line

makeFighter runs at hire time and draws the hero a style, an arena, and a coach.
The coach is not a roster in this data — only feedback strings and one portrait
table ship — so the port draws a name and gives them a quality score in 40..90.
The stats it tracks are strength, toughness, agility, technique (40..60 at hire,
toughness pinned to strength) and stamina (100), plus reputation, street cred,
wins, losses, and a league position that starts at 20.

fightYear fires from age 18 at 55% a year, and is one of three kinds:

* **recharge** when stamina is under 35 — the method-scenario table picks the
  scene, the method table is matched on its Method cell, and the cost, stamina
  gain and the happiness/health/discipline impacts are applied.
* **training** — a coach string gated on the hero's gender and the coach's
  quality, the gym line ("I drilled the anchor bomb."), and an 8% training ache.
* **a card** — the opponent band comes from the league position (top three fight
  level-5 opponents, and so on down to level 1 past eighteenth place), the
  opponent type is weighted and its Percent Male column becomes the coin for the
  opponent's gender, and only the top four may draw a championship arena.

Odds are a form score minus rank and opponent level: 0.5 + (form - 50)/150 +
(12 - leaguePos)/60 - (band - 1) * 0.05, clamped to 0.15..0.9. A win moves the
hero up a place and pays reputation; a loss slides down. Five percent of wins
are a dive instead, which the scenario table narrates.

### 59.3 Tokens

The family writes **<fighter>** for the opponent in the fight-ing tables and for
the hero in the fight-er persona tables, which is why the port carries a
fightSelf_ flag: the same token means opposite people in the two halves of the
family. **<fighter-possessive>**, **<coach-fighting>**, **<fight-opponent-type>**,
**<opponent-name>**, **<fighting-arena>**, **<fighting-style>** and
**<circumstance-fighter>** are all served by fightToken.

This family also forced three additions to the shared resolver:

* **<a>** (60 uses) and **<a/an>** (58) are indefinite articles, so the resolver
  looks at the next word and calls articleFor on it.
* **<person>** (15) is a stranger, so it falls back to the reference table
  random_person_indefinite (and then random_person), possessive included.
* **<lover>** leaked in the outcome rows when the hero had no partner. A pool
  that names a partner the hero never met now draws a stranger rather than
  printing an unresolved bracket.

### 59.4 What a card prints

One card can carry up to a dozen lines, and they come from a dozen tables in
order: the outcome line, the reason, the coach's reaction, a fan, an interview,
a press conference with a real question and a real answer from the twenty Choice
cells, an injury with four stat drops, a sponsorship, a product, a UFL card, a
wild encounter with its own colour-pool opener, a sportsmanship beat, a roid-rage
scene with a retort, and a line in the hero's own voice. Each one is gated on its
own table's window, and the windows pickRow cannot express (Min/Max Coach
Quality, Min/Max Hero League Position, Min/Max Reputation, payout bands) are
retried three times by a local helper before the line is accepted.

### 59.5 Shortcuts

* The move system is not modelled. The 202-row move table is read only for the
  name of the move the hero drilled; move types, unlock conditions, upgrades,
  body parts, Street Cred Cost, power, accuracy and stamina cost are ignored.
* Press-conference effects are four flag columns per choice, not stats, so a
  choice only moves reputation +/-4 and stamina +/-5.
* Injuries do not require or cost surgery, and healing is a flat yearly drift.
* The UFL card is rendered as a year-up event with its first choice taken; a
  year-up card has no prompt to route a player choice through.
* Percent Male is read as a probability, not a population share.
* The coach is a drawn name with a quality score, not a roster member, and the
  coach-quality table supplies the portrait rather than the game.
* Fighting Style is a label: nothing in the fight maths reads it.
* <coach-fighting> and <circumstance-fighter> are drawn from the name pool and
  cached per card, so a card mentions one coach and one circumstance phrase.

### 59.6 Verification

Probe (30 seeds x both genders, ages 60, fight enabled) produced 1207 fight
events and **residue 0** — no unresolved token in any title, body or log line.
By table: outcome 691, coach training 324, recharge 178, dive 14.

A card from seed 1:

    A card at Primetime Punch-Dome
    My opponent: Tai Weekday, a rookie. The press asked: What if Tai Weekday
    beats you? I said: I'd shake his hand respectfully.
    After my post-match interview, UFL commentators said that every fighter
    should take note of my Prime.
    The conference ran on the Grapple Gurus.

A training year from the same seed:

    While doing pull-ups at the fighting gym, May Lim started yelling, "Harder!".
    I drilled the anchor bomb.

Regression: distinct residue tokens 0, coverage 109 captured / 107 recoverable,
port gap 0, the coverage push pass returns 0, and the ten-year bench reaches age
34 with no abort.

## 60. The black market: eight item pools, an auction house, and a museum of dubious provenance

The black-market family is the largest content family the port had not touched: 24 tables,
734 rows, no code. It is not a career in the game's sense -- there is no job title and no
promotion clock -- it is a second economy the hero runs on the side: buy an item from a
vendor, move it through an auction house the hero owns, keep the best finds in a museum of
their own, and take the occasional side job for a street chemist or a taxidermist.

### 60.1 The tables

| table (a16_black-market-*) | rows | what it is |
|---|---|---|
| antiques | 51 | one item pool (Item Name, Fake Name, Short Name, Possessive Name, ...) |
| art | 41 | one item pool (Title, Artist, Genre, ...) |
| automobiles | 50 | one item pool (Make, Model, Type, ...) |
| fashion | 52 | one item pool |
| gemstones | 18 | one item pool (Gemstone, Min/Max Carats, First Carat Price, ...) |
| historical-artifacts | 48 | one item pool |
| sports-memorabilia | 49 | one item pool |
| weapons | 42 | one item pool |
| auction-house-names | 3 | Format, Prefix, Suffix -- the hero's own house |
| auctioneer-color | 43 | String + Auction Category comma list + Plural flag + Commonness Score |
| museum-name | 18 | Name + Commonness Score |
| museum-report-color | 20 | String + Min/Max Appeal |
| purchase-requests | 43 | String + Min/Max Friendliness + Auction Category + Arms Dealer Only + Wildlife Smuggler Only |
| street-chemist-products | 17 | Name + Can Be Cut + Min/Max Price + Health/Happiness/Smarts/Looks Impact + Risk Score |
| street-chemist-cut-ingredients | 58 | Ingredient + Commonness Score + Can Be Fatal |
| chemist-feedback-color | 45 | String + Hero Gender + Boosted Stat + Commonness Score |
| police-intoxicated-warning | 20 | Scenario + Commonness Score |
| police-intoxicated-hero-color | 21 | Scenario + Boosted Stat + Hero Gender |
| taxidermist-business-names | 8 | Format, Prefix, Suffix |
| taxidermy-discard-locations | 29 | Location + String + Commonness Score + Risk Score |
| exhibit-dumb-scenarios | 24 | the hero's museum, on the shared dumb runner |
| museum-dumb-scenarios | 15 | the same |
| vendor-dumb-scenarios | 12 | the same |
| auction-categories | 7 | a weight list for the eight item pools (not loaded -- see 60.4) |

The eight item pools share one shape: {B}Item Name, Fake Name, Short Name, Possessive Name,
Uses Indefinite Article, Uses Definite Article, Plural, Can Be Refurbished, Min Value, Max
Value, Unique, Can Be Fake, Appreciates, Commonness Score, Appeal Score, Storage Unit Only,
Black Market Only, ID{B}. {B}art{B} replaces the name column with {B}Title{B} and adds
{B}Artist{B} and {B}Genre{B}; {B}automobiles{B} splits it into {B}Make{B} and {B}Model{B};
{B}gemstones{B} is {B}Gemstone{B} plus carat columns. The article the item needs is baked into
the row as a flag rather than a word, which is why the port reads the flag and prepends
{B}a{B} or {B}the{B} itself.

### 60.2 The implementation

{B}Hero{B} gained the black-market state ({B}blackMarket{B}, {B}bmAuctionHouse{B},
{B}bmMuseum{B}, the drawn item as {B}bmItem{B} / {B}bmItemPossessive{B} / {B}bmItemShort{B}
plus {B}bmItemPlural{B} and {B}bmFake{B}, {B}bmCategory{B}, {B}bmValue{B}, {B}bmItems{B},
{B}bmAppeal{B}); {B}Sim{B} gained its own {B}bmRng_{B} stream ({B}seed ^
0x8f1bbcdc9e3779b1ULL{B}), the per-card caches {B}bmVendor_{B} and {B}bmChemist_{B}, and the
methods {B}makeBlackMarketeer{B}, {B}ensureBmFields{B}, {B}bmStructureName{B},
{B}bmDrawItem{B}, {B}bmToken{B} and {B}bmYear{B}.

{B}bmDrawItem{B} picks one of the eight pools, draws a row by {B}Commonness Score{B} and
fills the card: the name comes from the first column that exists among {B}Item Name{B},
{B}Title{B}, {B}Gemstone{B} and {B}Make{B} ({B}Make{B} plus {B}Model{B} for a car), the item
is fake at 40% only when the row's {B}Can Be Fake{B} flag is set and a {B}Fake Name{B}
exists, the article is taken from the {B}Uses Indefinite Article{B} /
{B}Uses Definite Article{B} flags, and the value is interpolated inside the row's
{B}Min Value{B}..{B}Max Value{B} band.

{B}bmYear{B} runs from age 18 at 45% a year and picks one of five days, in this order:

1. street chemist, 22% -- draw a {B}street-chemist-products{B} row, roll its price band, draw a
   cut ingredient, add a hospital line 25% of the time when the ingredient's {B}Can Be Fatal{B}
   flag is set, then a {B}chemist-feedback-color{B} string gated on {B}Hero Gender{B}, with the
   product's Happiness/Smarts/Looks impact applied.
2. police stop, 14% -- a {B}police-intoxicated-warning{B} scenario plus a
   {B}police-intoxicated-hero-color{B} scenario whose {B}Boosted Stat{B} names the stat that
   moved; the pronoun pair is pointed at the officer so {B}<he/she>{B} is the cop, not the hero.
3. taxidermy, 14% -- a business name from the {B}Format{B}/{B}Prefix{B}/{B}Suffix{B} row, a
   discard location, and a 25% dog-walker line when that location's {B}Risk Score{B} is 4 or
   higher.
4. museum report, 20% -- a {B}museum-report-color{B} string admitted through the Min/Max
   {B}Appeal{B} window against {B}bmAppeal{B} (three attempts, then accept), which moves the
   museum's appeal and the hero's happiness.
5. otherwise a sale -- draw an item, pay 55% of its value, draw an auctioneer patter string
   whose {B}Auction Category{B} list contains the item's category and whose {B}Plural{B} flag
   matches the item's number, add a buyer line from {B}purchase-requests{B} filtered on the
   same category with the pronoun pair on the buyer, and finish with either {B}Sold for
   $N{B} or, 30% of the time when the item was fake, {B}The buyer's appraiser called it a
   fake{B}.

{B}bmToken{B} serves {B}<item>{B}, {B}<item-short>{B} and the possessive, {B}<auction-house>{B},
{B}<museum>{B}, {B}<vendor>{B}, {B}<drug-chemist>{B}, {B}<auctioneer>{B} and
{B}<taxidermist>{B}; {B}resolveInto{B} routes to it through {B}bmIsToken{B}, which sits beside
{B}fightIsToken{B} in the anonymous namespace and strips the {B}-possessive{B} suffix with the
{B}psuffix.size(){B} idiom that the older pools had to learn twice.

The three dumb pools ride the shared runner: {B}makeBlackMarketeer{B} sets misc bits 9, 10 and
11, and {B}miscPoolYear{B} gained {B}{"a16_black-market-exhibit-dumb-scenarios-enc", 18,
0.25}{B}, the museum pool and the vendor pool at the same age and chance.

Two helpers were generalised rather than duplicated: {B}spyRow{B}/{B}spySnippet{B} became
{B}pickRow{B}/{B}pickSnippet{B} with an explicit {B}Rng&{B} parameter, and the old names stayed
as one-line wrappers passing {B}spyRng_{B}, so none of the 36 spy call sites changed.

### 60.3 Two tokens the family needed

The first probe (30 seeds, both genders, 1541 black-market events) found two leaks, both real:

* {B}<circumstance-home>{B} in the exhibit pool. Report 38 captured all three circumstance
  pools by calling {B}DataHub.GetRandomCircumstance*(){B} live, but only the public pool was
  wired, because until now no wired table used the other two. The home pool is now coded from
  that capture: 55 fixed scenes plus the one {B}<musician>{B} slot the game renders as
  {B}While chilling in bed listening to <article> <musician>{B}. Its twin,
  {B}<circumstance-return>{B} (50 scenes plus three musician slots), still has no wired table
  and stays unwired.
* {B}<blue/red/yellow/orange/green>{B} in the street-chemist line: the cut ingredient cell is
  itself an inline alternative list, so the port now resolves that cell before printing it
  instead of reading the {B}Ingredient{B} column raw.

### 60.4 Shortcuts

* The eight item pools are drawn uniformly; {B}a16_black-market-auction-categories-enc{B}
  carries a weight per category and is not loaded, so the category mix is flat.
* {B}Unique{B}, {B}Appreciates{B}, {B}Can Be Refurbished{B}, {B}Storage Unit Only{B} and
  {B}Black Market Only{B} are not modelled: the port has no storage unit, no refurbishment
  action and no second market to contrast with.
* The sale is a same-year round trip at a fixed 55% of value; the game lets the hero hold an
  item and let it appreciate.
* The museum's {B}Appeal{B} is only a filter for its feedback pool; exhibits are not tracked,
  and the museum's own dumb pool draws the items it mentions from the same eight pools.
* {B}purchase-requests{B}' {B}Arms Dealer Only{B} and {B}Wildlife Smuggler Only{B} flags are
  read as part of the pool the category filter already narrows.
* The police warning rows begin with a lowercase {B}<he/she>{B} in the shipped data
  ({B}<he/she>{B} {B}said you look like you're over analyzing every little thing.{B}), so the
  rendered card starts lowercase exactly like the data does.
* Two columns of the taxidermy table ({B}Cost{B}-style numbers) and the taxidermist's own
  business model are not modelled; the hero is paid nothing for the job.

### 60.5 Verification

Residue probe (seeds 1..30, both genders, ages 60, {B}{"black-market": true}{B}, every event
whose table is a black-market table, every {B}<...>{B} token in title, body and log lines
counted): 1542 events, residue 0. Split by table: exhibit 491, museum 365, vendor 299,
auctioneer 167, street chemist 92, museum report 47, taxidermy 42, police 39.

Samples straight out of the port:

    A risky buy | I picked up a plastic javelin for $211. The auctioneer, Court Villa:
    "Mark Wahlberg once owned it". | Sold for $385.

    A new find | I picked up a Qing silk cheongsam for $789009. The auctioneer, Rollins
    Obinna: "it's not every day you see something this valuable for sale". A buyer: she says
    she is going to the opera and wants to show it off. | Sold for $1,434,562.

    Cutting the product | I cut a batch of ascorbic acid and moved it for $1537. The cut:
    cinnamon. You suddenly look less tragic!

    Work for the taxidermist | I did a job for Luka's Taxidermy Services and dumped the
    leftovers in a landfill.

    Court's Museum of Unnatural History | Visitors said they learned more about the world by
    going through your museum than they did in primary school.

Full regression after the two fixes: {B}rescensus.py{B} distinct residue tokens 0;
{B}coverage.py{B} 109 captured / 107 recoverable; {B}portgap.py{B} ENTRIES 109 / rendered-by-port
107 / gap 0; {B}covpush.json{B} rc=0; {B}bench.sh{B} through age 34 with no abort.

## 61. The casino: a house the hero owns, and seventeen kinds of year

The casino family is the second-largest content family in the game and the largest one the port
had left: 39 tables, 1286 rows, and until now nothing but two of its token paths. It is not a
menu the hero visits, it is a business the hero owns -- a building with an amenity capacity, a
game-room capacity and an entertainment capacity, filled with amenities the patrons judge one by
one, gamblers whose mood the hero manages, acts the hero auditions and books, celebrities who
arrive with riders, and a ledger that reports on all of it once a year.

### 61.1 The tables

| table (a16_casino-*) | rows | what it is |
|---|---|---|
| casino-structure | 5 | the building: Price, Amenity Capacity, Game Room Capacity, Entertainment Capacity |
| casino-theme | 8 | the house's look, with a Popularity Score |
| casino-amenity | 75 | Amenity Name, Category, Theme, Cost, Monthly Expenses, Gambler Happiness Impact, Base/Match Popularity Impact |
| casino-amenity-feedback-scenario | 76 | what the patrons say about one amenity (Category, Theme, Enjoyment Score) |
| casino-game-room | 9 | the games: Installation Cost, Unit String, capacities, Default House Win Rate, Min/Max Bet |
| random-game-room-name | 69 | room names with a Game Room Type (not wired -- see 61.5) |
| random-hero-casino-name | 42 | the hero's casino name with a Casino Type |
| casino-street-name | 57 | the frontage (Street Name, Add Random Street Type) |
| casino-ledger-feedback | 17 | the year's balance sheet, gated on a popularity band |
| casino-dumb-scenario | 25 | the house's own prompt-and-choice card, gated on theme, popularity and facilities |
| casino-gambler-conversation | 27 | the topics punters bring up |
| positive-gambler-interaction-scenario | 19 | a punter and the house getting on |
| negative-gambler-interaction-scenario | 18 | and not getting on |
| casino-game-room-complaint | 17 | complaints, gated on a gambler-happiness band |
| gambler-game-room-compliment | 17 | compliments, gated on a higher band |
| gambler-joins-casino-color | 25 | new faces at the door |
| banned-gambler-snippet | 22 | the punters who are shown out, gated on craziness |
| mafia-member-activity-string | 10 | the mob's interest in a house that is doing well |
| meet-greet-feedback | 20 | the meet and greet, with fame-based and theme-based lines |
| casino-drink | 56 | the bar (Beverage Type, Drink Has Alcohol, Drink Quality, Happiness Impact) |
| drink-refusal-string | 33 | turning the drink down, keyed on whether it was alcoholic |
| casino-gift | 36 | gifts for the regulars, with gender-specific appreciation scores |
| gambler-compensation-method | 30 | thanking a patron (Method, Feedback String, Log Version) |
| gambler-encouragement-method | 19 | coaxing one back to the tables (Effectiveness Score) |
| casino-audition-string | 43 | the audition, gated on an impression band |
| casino-performance-act | 4 | act types and their base popularity |
| casino-performance-act-name | 127 | the acts themselves, with Act Type and Theme |
| casino-performance-act-string | 40 | the show they put on |
| casino-offer-artist-refusal | 20 | why an act says no |
| casino-singer-genre | 62 | genre popularity against the house's theme |
| casino-singer-song | 66 | song titles |
| celebrity-demand | 22 | what the rider asks for |
| casino-celebrity-party-scenario | 19 | the party, gated on the celebrity's craziness and flagged positive or negative |
| casino-celeb-party-death-string | 12 | the worse ending, gated on craziness |
| casino-collectible | 21 | pieces for the display case (articles, Min/Max Value) |
| casino-accessory-acquisition-method | 8 | where the house's own swag came from |
| casino-celebrity-name | 106 | the name pool every other family already draws from |
| casino-game-room-complaint | 2 | a two-row duplicate of the complaint table |
| ledger-feedback | 2 | a two-row duplicate of the ledger table |

### 61.2 Building the house

{B}Sim::makeCasino{B} sets {B}ownsCasino{B} and calls {B}Sim::ensureCasinoFields{B}, which is also
called at the top of every casino year, so a casino built by any route always has a theme, a
structure, a name, an address, amenities and games. The order matters:

1. the theme is drawn from {B}a16_casino-casino-theme-enc{B} weighted by {B}Popularity Score{B};
2. the structure row supplies the two capacities the rest of the build spends, {B}Amenity
   Capacity{B} and {B}Game Room Capacity{B};
3. the name comes from {B}a16_casino-random-hero-casino-name-enc{B} and the address from
   {B}a16_casino-casino-street-name-enc{B} plus a house number; the {B}Add Random Street Type{B}
   flag appends " Street";
4. the amenities fill the structure's amenity capacity, one per {B}Category{B} so the house never
   has two of the same thing, and a row that names a {B}Theme{B} only fits a house of that theme.
   Each row's {B}Base Popularity Impact{B} moves the house's popularity;
5. the games fill the game-room capacity, drawn without replacement from the nine rows of
   {B}a16_casino-casino-game-room-enc{B}.

The hero's state is `casinoName`, `casinoTheme`, `casinoStructure`, `casinoStreet`,
`casinoAmenities` and `casinoGames` (the two lists), `casinoPopularity` (the number the
ledger, the dumb card and the celebrity visit are all gated on), `casinoGamblers`,
`casinoGamblerHappiness` (the floor's mood), `casinoGamblerAddiction`, `casinoAct`,
`casinoActType`, `casinoActPopularity`, `casinoSong`, `casinoCelebrity` and
`casinoCelebCraziness`.

### 61.3 The casino year

{B}Sim::casinoYear{B} runs from age 18 at 50% a year, drifts the house's popularity by a few
points every year, and fires exactly one of the following, in this order (the entertainment
blocks run first because the rest of the list would otherwise eat their chance):

| # | chance | event |
|---|---|---|
| 14 | 0.35 | entertainment: with no act in residence an audition is drawn through the Min/Max Impression band (the port reads the impression as the house's popularity) and 40% of offers are refused; the act's type sets its base popularity, a singer act also brings a genre matched against the theme and a song, and a booked act plays a show 35% of the time |
| 15 | 0.20 | a celebrity: at popularity 45+ a celebrity arrives with a rider demand drawn from the demand table, and on later years the party is drawn through the Min/Max Celeb Craziness band; a negative party can end in the death pool, and the celebrity moves on half the time |
| 16 | 0.12 | a collectible for the display case, its article taken from the row's flags and its price interpolated in the row's value band |
| 17 | 0.10 | where one of the house's own accessories came from (the rows are past-tense clauses, "you won a hand with 21") |
| 1 | 0.15 | the house's own prompt-and-choice card, gated on hero age and gender, on the popularity band, on the theme, and on having an amenity, a game room, a celebrity or an act where the row asks for one. The first choice is the one taken, the same shortcut the UFL card uses |
| 2 | 0.35 | the ledger, its line drawn through the Min/Max Casino Popularity band |
| 3 | 0.30 | what the patrons make of one amenity the house actually has: the feedback pool is filtered on the amenity's Category and on the theme |
| 4 | 0.25 | a punter's conversation topic, wrapped by the port into a sentence |
| 5 | 0.25 | one gambler crosses paths with the house, the positive or negative pool chosen by the floor's mood |
| 6 | 0.20 | a complaint when the floor's mood is under 45, a compliment above 70 |
| 7 | 0.25 | new patrons at the door |
| 8 | 0.15 | a banned gambler: the punter's craziness is rolled and the row drawn through its band |
| 9 | 0.12 | the mob takes an interest |
| 10 | 0.15 | the meet and greet, the pool chosen by whether the hero is famous (30+) and by the house's theme |
| 11 | 0.20 | a drink at the bar, with a 30% refusal drawn from the rows whose alcohol flag matches the drink |
| 12 | 0.15 | a gift for a regular, its appreciation score chosen by the punter's gender and its age band checked against the punter |
| 13 | 0.15 | an unhappy floor is encouraged back to the tables, a happy one thanked with a compensation the house picks |

### 61.4 Tokens

* {B}<hero-casino>B is the data's spelling for the hero's own casino, and it is what the ledger,
  the dumb card, the conversation and the joins pool all use. It used to be answered by the misc
  map (a fresh random casino name per card); {B}casinoToken{B} now answers it with the hero's own
  name and falls back to the same random draw when the hero owns nothing.
* {B}<casino-gambler>B is one punter per card, and the punter has a gender: the name is drawn
  with it and the pronoun pair is pointed at the punter, because these pools address the gambler
  ("<he/she> says his mood plummets..."). Before this the pools mixed a female name with "he".
* {B}<casino-celebrity>B is the resident celebrity, or a drawn one when the house has none.
* {B}<casino-room>B and {B}<casino-game>B are one of the hero's own games (a fresh row when the
  hero owns no house), and {B}<casino-name>B/{B}<casino>B are the house's name.
* {B}<car-type>B (54 uses across the casino, racing, dealer and vampire families) is the
  {B}Type{B} column of {B}a16_car-models-enc{B}, added to the shared misc map.
* The name cells of the gift, demand, collectible, drink and amenity rows carry inline
  alternatives of their own (a rider item is "<carton/a pack/two packs> of <Marlboro/Camel/...>"),
  so those cells are resolved before printing, and a new {B}withArticle{B} helper refuses to put
  a second article in front of a value that already carries one ("a pack of cigarettes").

### 61.5 Shortcuts

* The port has no money ledger for the casino: costs, monthly expenses, the payouts and the
  collectible prices are narrated, not deducted.
* {B}a16_casino-casino-game-room-complaint-enc{B} and {B}a16_casino-casino-ledger-feedback-enc{B}
  are the fuller twins of {B}a16_casino-game-room-complaint-enc{B} and
  {B}a16_casino-ledger-feedback-enc{B} (2 rows each); the two-row duplicates are unused.
* {B}random-game-room-name{B} names rooms rather than games, and every {B}<casino-room>B use in the
  data reads as a game ("in your casino's Blackjack"), so the game table's {B}Game{B} column
  answers the token and the 69 room names stay unwired.
* The gambler's own addiction is not modelled; the interaction pools' Min/Max Gambler Addiction
  windows are not read, only the happiness band is.
* The hero's fame is the port's {B}fame{B} field with a threshold of 30 for the meet-and-greet
  fame lines; the game's own fame scale may sit elsewhere.
* A year-up card cannot route a choice, so the casino dumb card always takes its first choice.
* The amenity's {B}Cost{B}, {B}Monthly Expenses{B} and {B}Secondary Text Line{B}, the game rooms'
  installation costs and win rates, the acts' engagement fees and the celebrity rider's price are
  read but not charged.
* The audition's impression score is the house's popularity, because the game computes it from
  the act's own performance and the port has no act statistics per audition.

### 61.6 Verification

Residue probe (seeds 1..30, both genders, ages 60, {B}{"casino": true}{B}, every event whose
table contains "casino", every {B}<...>{B} token in title, body and log lines counted): 1351
events, residue 0. The wired tables and their counts over those 60 lives:

    a16_dumb-scenarios-casino-enc                        617
    a16_casino-casino-ledger-feedback-enc                134
    a16_casino-casino-audition-string-enc                 92
    a16_casino-casino-dumb-scenario-enc                   73
    a16_casino-casino-performance-act-string-enc          63
    a16_casino-casino-amenity-feedback-scenario-enc       61
    a16_casino-casino-collectible-enc                     49
    a16_casino-celebrity-demand-enc                       43
    a16_casino-casino-gambler-conversation-enc            41
    a16_casino-casino-accessory-acquisition-method-enc    39
    a16_casino-positive-gambler-interaction-scenario-enc  37
    a16_casino-gambler-joins-casino-color-enc             24
    a16_casino-casino-celebrity-party-scenario-enc        20

Samples straight out of the port:

    Auditions | Each performer walked on stage impersonating a famous icon. The one dressed as
    Bo Burnham was so convincing, you swore the star was actually there lip-syncing 'Lose
    Yourself'. They flawlessly impersonated celebrities for a stellar lip-sync number.
    | I booked Femme Fatale Follies the drag show.

    A celebrity in the house | Travis Barker arrived with a rider that included a Thai massage
    therapist.

    The party | While partying in your casino's Blackjack, Travis Barker went around the room
    giving partygoers wedgies. Travis Barker went on a bender and succumbed to an overdose.

    The books | Patrons noted that Lucky Hankey's Clubhouse makes them embarrassed to be from
    Atlanta.

    House swag | I matched all 3 numbers. | I keep my sunglasses at Lucky Hankey's Clubhouse.

Full regression after the section: {B}rescensus.py{B} distinct residue tokens 0;
{B}coverage.py{B} 109 captured / 107 recoverable; {B}portgap.py{B} ENTRIES 109 / rendered-by-port
107 / gap 0; {B}covpush.json{B} rc=0; {B}bench.sh{B} through age 34 with no abort.

## 62. The commune: a doctrine, a totem, and forty-five tables behind one follower at a time

The cult family is the commune the hero founds rather than joins: the game hands
you a name, a philosophy out of five, a totem, a title and a shed in the woods,
and then spends the rest of your life measuring how much the people living there
still believe you. Forty-five tables, 1769 rows, all of them under
`a16_cult-`.

### 62.1 The tables

| rows | table | what it carries |
| --- | --- | --- |
| 66 | random-commune-names | name + five philosophy flags, one of which is the commune's own |
| 20 | default-totem | Item Name, Philosophy |
| 60 | honorific-name-structure | Honorific ("Guru <hero-first-name>"), Gender, Hero Is Leader |
| 5 | commune-property-types | Type, Price Score, Street Type ("Commune") |
| 72 | commune-street-names | Street Name, Add Random Street Type |
| 16 | commune-structure | Project, Minimum/Maximum Cost, Appeal Impact, Bonus Appeal Impact (Philosophy) |
| 48 | commune-structure-feedback | Scenario, Log Version, Structure |
| 75 | doctrine-rules | the rule, the breach, the lecture, the obedient reply, the leave string, Disobedience Can Be Fatal |
| 43 | devotion-demonstration | Scenario Snippet, Death Snippet, Success Snippet, Risk Score |
| 22 | body-discard-locations | Location, String, Risk Score |
| 29 | communard-apply-color | String, Min/Max Loyalty |
| 74 | communard-conversation | Audience, Conversation, Philosophy, Controversy Score |
| 20 | communard-doubts | String, Min/Max Loyalty |
| 71 | communard-educate | Topic, Philosophy, Loyalty Impact |
| 58 | communard-leave-scenario | Scenario Snippet, Reason, Min/Max Loyalty |
| 11 | communard-punishment | Method, Feedback Scenario, Log Version, Loyalty Impact |
| 26 | communard-rewards | Scenario, Loyalty Impact |
| 49 | communard-rumor | outbound and inbound Scenario, Philosophy, sender and receiver gender/age |
| 78 | communard-spend-time | Pastime, Philosophy |
| 26 | communard-warning | Scenario, Risk Score |
| 38 | commune-dumb-scenarios | the prompt card: Title, Body, CTA, four choices with Narrative and Impact columns |
| 14 | commune-ledger-color | String, Min/Max Loyalty |
| 61 | commune-teaching | Scenario, Topic, Philosophy, Engagement Score |
| 12 | healing-ceremony | Ceremony Type, Feedback Snippet, Effectiveness Score |
| 6 | empowerments | String |
| 15 | gratitude-scenarios | String, Min/Max Loyalty |
| 85 | drug-shared-experience | String Snippet (past tense), Log Version, Drug, Experience Type |
| 64 | drug-solo-experience | String Snippet (gerund), Drug, Experience Type |
| 17 | spiritual-journey-drugs | Name, Min/Max Price Per Dose, three dose Risk Scores |
| 80 | affirmation-film-titles | Title, Philosophy, Engagement Score |
| 36 | affirmation-receptions | String, Min/Max Reception |
| 81 | drum-circle-artists | Artist Name, Engagement Score, Philosophy Engagement Bonus |
| 61 | fast-hunger-foods | Food, Plural, Uses Indefinite Article |
| 19 | fast-hunger-reasons | Reason |
| 31 | fast-reasons | Reason, Scenario Snippet, Philosophy, Engagement Score |
| 36 | recruitment-location | Location, Uses Indefinite Article, Success Score, Police Score |
| 62 | recruitment-message | Message, Philosophy, Success Score, Risk Score |
| 13 | outreach-mission-heckles | String and Log Version, both heckles |
| 24 | workshop-products | Product Name, Plural, prices, Optimal Location, Difficulty |
| 27 | anti-commune-nag-scenario | Audience, Snippet, Log Version, audience age window |
| 16 | ex-communard-dissent-scenario | Scenario, Log Version, Min/Max Craziness |
| 17 | expel-beg-scenario | String, Min/Max Loyalty |
| 23 | totem-disrespect | String, Log Version, Min/Max Loyalty |
| 32 | totem-inspiration | String, Humorous |
| 30 | totem-lecture | String |

### 62.2 Founding the commune

`Sim::ensureCultFields` draws the name row through the `Hero Founded Cult`
flag and reads whichever of the four philosophy flags (Armageddon, New Age,
Antiestablishment, Alien) is set - that flag is the commune's philosophy, and it
filters almost every later table. The totem, the honorific (by Gender and the
`Hero Is Leader` flag) and the property type follow, then an address from the
street-name pool with the `Add Random Street Type` flag appending " Street".
`Sim::makeCult` turns the flag on, rolls three to seven followers, and sets
loyalty 55 / charisma 45-65. `makeSecretAgent`-style, nothing is charged: the
property, the totem and the first followers are free, which is a shortcut
recorded in 62.5.

### 62.3 What a year in the commune looks like

`Sim::cultYear` runs from age 18 at 55% a year, drifts loyalty by -3..+3, and
fires exactly one event. Blocks are ordered from the rarest chance to the
commonest so that no block is starved by the ones ahead of it:

| chance | event |
| --- | --- |
| 0.06 | a workshop sells the commune's product at the Optimal Location |
| 0.08 | a fast is called, and the commune dreams about food |
| 0.08 | a film about the commune, graded by Engagement Score, or a drum circle |
| 0.08 | an ex-follower dissents, or the neighbours nag |
| 0.10 | a shared or solo trip on the commune's supply |
| 0.10 | the totem is lectured about, praised, or disrespected |
| 0.10 | a healing ceremony plus an empowerment plus gratitude |
| 0.10 | a rumour travels through the commune |
| 0.10 | a doubt, an application, or a warning - a warning with Risk Score 4+ can expel |
| 0.10 | a new building, or feedback about one already built |
| 0.12 | a demonstration of devotion, which can kill the follower |
| 0.12 | a follower is rewarded |
| 0.15 | a doctrine is enforced, taught, or broken |
| 0.15 | time spent with a follower, or a conversation about the philosophy |
| 0.18 | a teaching session, and 30% of the time a follower leaves over it |
| 0.20 | recruitment at a location, using a message, with the police watching |
| 0.25 | the commune's own prompt card, first choice taken |
| 0.30 | the ledger: the followers are blissful, or they are not |

Loyalty is the spine. The window tables (`Min Loyalty`/`Max Loyalty`)
are read through `pickWindow` against the current score, so a happy commune
gets gratitude and a sour one gets doubts and warnings; a warning that scores 4+
can expel a follower, a devotion demonstration that fails its `Risk Score`
roll kills one, and `communards` is decremented either way.

### 62.4 Tokens

Six new tokens, all answered by `Sim::cultToken`:

- `<communard>` - one follower per card, cached, and the pronoun pair is
  flipped to point at that follower because the follower-facing pools are
  written about them ("<he/she> says he's being solicited to join an MLM vitamin
  cult").
- `<communard-possessive>` - the same name with an apostrophe s.
- `<ex-communard>` - a second cached name, drawn only when an ex-follower row
  needs one.
- `<totem>`, `<honorific>` - the commune's own objects.
- `<commune-other>` and `<cult>` / `<cult-name>` - another commune, and
  this one.

### 62.5 Shortcuts

- Founding is free: property, totem, first followers and the honorific cost
  nothing. Later buildings print their cost and subtract nothing.
- `Appeal Score`, `Enjoyment Score` and `Value Percentage` on the
  structure rows are read but not compounded into a commune-wide appeal figure.
- `Controversy Score` only buys charisma at 3+, never conflict.
- Drugs carry no addiction; a bad dose is a hospital line, not a stat.
- Policing is a roll in the recruitment block only; no other event can be caught.
- The doctrine in force is one string on the hero, replaced each time a rule is
  enforced, and `Disobedience Can Be Fatal` kills a follower 25% of the time.
- Three philosophy-specific tables are filtered by philosophy and fall back to
  the unfiltered pool when the filtered one is empty, because several rows leave
  the philosophy cell blank.

### 62.6 Verification

Thirty seeds across both genders to age 60 with `"cult": true` produced 1180
cult events and zero residual tokens, counted over title, body and every log
line. The family splits 87 workshop, 72 fast, 68 gratitude, 61 solo trip, 60 drum
circle, 60 rumour, 58 shared trip, 55 prompt card, 52 reward, 50 devotion, 47 nag,
47 structure feedback, 46 totem inspiration, 40 ledger, 38 film, 38 teaching, 34
dissent, 34 recruitment, 34 disrespect, 32 time, 29 lecture, 26 warning, 20 each
application/punishment/message/conversation, 16 doubt, 16 doctrine.

Two bugs were found by that probe and fixed. The heckle log line points at a
bracket in the heckle's own String column with `<rb>`, so the String now has
to be resolved first and the Log Version second from the same row - picking them
as two independent draws left `<rb>` in the text twice. The Scenario Snippet
of `devotion-demonstration` is a base-form verb phrase while its Success
Snippet is a gerund or noun phrase, so they cannot share the frame "A follower
...": the log now reads "A follower volunteered to climb Mt. Everest in my
honour." with "The outcome: climbing Mt. Everest." And the solo drug snippets are
all gerunds, so they read "I remember feeling telepathically connected to Mike
Tyson during the trip." rather than "I feeling ...".

The full suite stays green: residue 0, 109 captured / 107 recoverable scenarios,
port entries 109 with 107 rendered and gap 0, `covpush` rc 0, and the bench
reaches age 34 without an abort.

## 63. The drug trade: a cartel, a batch, and thirty-eight dealer tables

The dealer family is the underworld the hero works for rather than runs: a cartel that owns him,
a batch he moves, and a standing score that decides whether the night ends in cash or in a threat.
Thirty-eight tables, 1525 rows, of which 37 were unwired before this section.

### 63.1 The tables

`a16_dealer-` prefix, `-enc` suffix, all under `gamedata_plain/`:

| rows | table | what it holds |
| --- | --- | --- |
| 200 | breakup-scenarios | wired in section 57 (the relationship subsystem) |
| 121 | deal-activity-item | petty goods sold on the side, with article and plural flags |
| 109 | cut-drug-name | the name a cut batch is sold under, keyed to its base drug |
| 81 | deal-person-drug-experience | the trip the customer had |
| 74 | drug-trip-experience | the same, third person about the user |
| 67 | cartel-member-nickname | Gender and Kingpin Only flags; the cell is a bracket over a first name |
| 64 | cartel-name | the cartel itself (already a source for `<cartel>` since section 56) |
| 51 | prison-cartel-packages | what gets smuggled inside |
| 50 | drug-barter-item | Min/Max Value and Min/Max Person Money |
| 48 | cartel-street-name | Street Name, Add Random Street Type |
| 47 | drug-pastime | Target Gender, Min/Max Target Infamy |
| 40 | prison-drug-trips | Drug Effect, Feedback Snippet, Log Version |
| 32 | drug-package-type | the packaging cell is already plural |
| 32 | cut-drug-substance | Demand Impact, Is Fatal |
| 32 | cartel-give-money-feedback | Is Refusal flag; the dues |
| 30 | cartel-threat | Kingpin Only, Min/Max Standing |
| 30 | cartel-party-refusal | String, Reason |
| 28 | drug-purchase-reason | keyed to addiction and willpower |
| 27 | police-warning | keyed to a Willpower window |
| 27 | drug-sale-refusal-snippet | Keyed to a Reason |
| 27 | cartel-member-leave-scenario | Min/Max Relationship |
| 27 | batch-tester-feedback | Min/Max Demand |
| 25 | overdose-snippet | |
| 25 | extortion-color-snippet | Hero is Kingpin, Target Gender |
| 23 | kingpin-mutiny-success-snippets | the members succeed against the hero |
| 22 | run-outcome-flavor | Run Status complete / ended |
| 21 | hero-threatens-rival-cartel-member | |
| 21 | drug-worker-action | Threat Score |
| 20 | drug-worker-murder-scenario | |
| 20 | drug-sale-rejection-reason | Reason plus a Min/Max Willpower window |
| 19 | drug-drop-off-location | Success/Failure feedback and log versions, Risk Score |
| 17 | cartel-drug | Schedule, Street Value, Min/Max Cost and Price Per Dose, Base Demand |
| 14 | extortion-method | the alternative list lives in Feedback Scenario, the log says `<rb>` |
| 13 | drug-effect | Good Trip Likelihood |
| 12 | cartel-member-murder-scenario | Method, Effectiveness, Success / Attempt / Retaliatory String |
| 11 | cartel-takeover-method | Success / Failure / Miss versions of a coup |
| 10 | drug-run-dumb-scenario | a four-choice prompt card |
| 8 | mutiny-methods | the hero plotting against the kingpin |

### 63.2 Founding the trade

`Hero` carries `dealer`, `dealerKingpin`, `dealerDrug`, `dealerCut`,
`dealerCartel`, `dealerNick`, `dealerPackage`, `dealerStreet`, `dealerRunStatus`
and the counters `dealerDemand`, `dealerStanding`, `dealerWillpower`, `dealerPrice`,
`dealerBatch`, `dealerDoses`, `dealerInfamy`, `dealerHeat`.

`makeDealer` sets the standing and the willpower, hands over $500 of seed money and calls
`ensureDealerFields`, which draws the cartel (weighted Commonness Score), a nickname honouring the
Kingpin Only and Gender flags (the cell is a bracket, so it goes back through the resolver), a street
with an optional street type and a house number, the drug (whose Min/Max Price Per Dose becomes the
port's price and whose Base Demand raises the demand score) and the packaging.

`dealerYear` runs from age 16 at a 50% chance and fires exactly one event, first hit wins:
the order below is the order in the code, and the chance is the chance after the age gate.

| # | event | chance | gate |
| --- | --- | --- | --- |
| 1 | extortion | 0.08 | standing 45+ |
| 2 | the run | 0.22 | a batch to move |
| 3 | the sale | 0.30 | always |
| 4 | police warning | 0.10 | heat 2+ |
| 5 | killing a member | 0.06 | standing 50+ |
| 6 | a member leaves | 0.06 | |
| 7 | taking over the cartel | 0.05 | |
| 8 | a mutiny against the hero | 0.05 | kingpin |
| 9 | threatening a rival | 0.05 | |
| 10 | cutting the batch | 0.06 | |
| 11 | the customer trips | 0.10 | |
| 12 | a pastime with the crew | 0.08 | |
| 13 | barter instead of cash | 0.07 | |
| 14 | a worker acts up | 0.06 | |
| 15 | the cartel wants its cut | 0.08 | |
| 16 | turning down an invitation | 0.06 | |
| 17 | the trade inside prison | 0.05 | in prison |
| 18 | testing a batch | 0.08 | |
| 19 | an overdose | 0.05 | |
| 20 | selling the small stuff | 0.07 | |
| 21 | the drug-run prompt card | fallback | |

### 63.3 Tokens

`dealerIsToken` answers `<cartel-member>` / `<member>` / `<partner>`,
`<cartel-rival-member>` / `<rival-member>` / `<rival-cartel-member>`,
`<cartel-kingpin>` / `<kingpin>`, `<victim>` (and the data's own misspelling
`<victim-possesive>`), `<other-person>` / `<random-person>`, `<cartel-remember>`,
`<insult>` (drawn from `a16_insults-enc`, which no earlier section had wired),
`<random-first-name-male>`, `<random-first-name-female>`, `<drug-illegal>`,
`<dealer-drug>`, `<dealer-package>`, `<cartel>`, `<drug-cartel>` and
`<rival-cartel>`. The cartel member, the victim and the rival are each cached once per card, and
the pronoun pair is pointed at whoever the row is talking about before the row is resolved.

The family also forced one shared fix. Several of its rows keep the alternative list in one column and
refer back to it from another with `<rb>` — extortion writes the vehicle damage in Feedback Scenario
(`You <keyed every side of/shattered the windows on/...> <his/her> <car-type>!`) and then the log line
says `I <rb> <his/her> <car-type>`. Two things had to become true: the column that owns the list has
to be read first, and `<rb>` has to be able to fall back to a bracket whose value was a token rather
than an inline alternative, because `a16_dumb-scenarios-dealer-enc` row 5 writes its Body as plain
tokens (`<friend> ... one of the <cartel> ... free doses of <drug-cartel>`) and then says
`<rb>` in every narrative. `Sim` now keeps `rbLast_` next to `rndCache_`: every resolved
token records its value, every drawn alternative records its value, and `<rb>` checks the per-card
cache first and `rbLast_` second. That single member removed 176 leaked brackets across the family
and every earlier family kept its residue at zero.

### 63.4 Shortcuts

- Founding the trade costs nothing; the cash, the cartel and the standing all arrive at once.
- The cartel's drug is drawn once and never changes; `<dealer-drug>` is always the hero's own batch.
- Risk is one roll: `willpower + 0..40 > risk * 12 + heat` decides the run, not the police column.
- `a16_dealer-deal-activity-item-enc` and `a16_dealer-drug-barter-item-enc` pay Max Value /
  Max Sale Value outright; the Min column and the Person Money band are read but not compounded.
- The prison line fires only when `inPrison` is set by something else; the dealer career does not put
  the hero there.
- Cut substances move the demand score but never kill anyone; `Is Fatal` is read and ignored.
- The prompt cards take their first choice, because a year-up card has no prompt to route a choice
  through.

### 63.5 Verification

`python3 /tmp/qdir/dlres.py` runs 30 seeds x both genders to age 60 with `"dealer": true` and
regexes every bracket out of title, body and log lines of the events whose table is a dealer table:
1521 events, residue 0. The histogram:

    a16_dumb-scenarios-dealer-enc          645
    a16_dealer-drug-run-dumb-scenario-enc  207
    a16_dealer-drug-drop-off-location-enc  163
    a16_dealer-drug-effect-enc             115
    a16_dealer-extortion-method-enc         73
    a16_dealer-drug-sale-rejection-reason   70
    a16_dealer-cartel-member-murder         27
    a16_dealer-cartel-give-money-feedback   27
    a16_dealer-hero-threatens-rival         25
    a16_dealer-deal-person-drug-experience  24
    a16_dealer-cut-drug-name                24
    a16_dealer-drug-barter-item-enc         20
    the remaining eleven tables               1-14 each

Samples:

    A trade | I swapped drinking straws of stimmy for a prosthetic finger. | I took a prosthetic finger for the package.
    A run out to an empty back alley | I loaded 74 doses of stimmy in drinking straws and drove out to an empty back alley. | The run cleared $7,252. | A group of cartel members took me out to celebrate the successful run.
    Selling stimmy | I sold 1 dose to John Janim for $69. | John Janim reported hallucinations.
    What happened to John Stonemason | He set John Stonemason on fire with a Molotov cocktail!

The full suite is green: `rescensus.py` reports distinct residue tokens 0, `coverage.py` reports
109 captured / 107 recoverable, `portgap.py` reports ENTRIES 109, rendered-by-port 107, gap 0,
`covpush.json` exits 0 and `bench.sh` runs to age 34 without an abort.

Three bugs were found while probing this family: the extortion and prison log lines needed their
feedback column read first (`<rb>` again), the packaging cell is itself an alternative list and
already plural, and `out.row_id` is a string in this simulator, so assigning an integer to it
produced JSON that no probe could parse.

## 64. The family: a made man at rank one, and seventeen mafia tables

The mafia family is the second half of the underworld the port already had
(report 63 wired the drug trade). It is the older organisation: a made man
belongs to a family the mob table assigns to his own country, starts at rank
one, and climbs by standing. Seventeen tables, 831 rows. Two of them
(`a16_mafia-mobs-enc`, `a16_mafia-nicknames-enc`) report 56 had already
wired as misc sources for `<mafia-godfather>`-style tokens; section 64 takes
them back and gives the family a token handler of its own.

### 64.1 the tables

| rows | table | what it carries |
|---|---|---|
| 7 | a16_mafia-mobs-enc | Mob, Associated Country, Host Country, Notoriety Score, Money Score, Rank 1 … Rank 4, Rank 5 Male, Rank 5 Female |
| 142 | a16_mafia-nicknames-enc | Nickname, Mob Country, Gender (the nickname cells are themselves brackets) |
| 86 | a16_mafia-calling-cards-enc | Item, Uses Partitive Article, Specific Mob Country |
| 72 | a16_mafia-excuses-enc | Scenario Snippet, Gender |
| 36 | a16_mafia-executions-enc | Method, Hero Outbound/Inbound Scenario + Log Version, Third Person Version, Death Snippet, Pain Score |
| 23 | a16_mafia-extortion-targets-enc | Company Type, Specific Target, Gender, Money Score, Commonness Score |
| 35 | a16_mafia-feedback-earnings-enc | Feedback |
| 14 + 22 | a16_mafia-feedback-negative-enc, feedback-positive-enc | Scenario, Log Version, Specific Mob Country |
| 11 | a16_mafia-join-methods-enc | Method, Feedback Snippet, Log Version Snippet, Success Score, Can Be Punished |
| 43 | a16_mafia-join-rejections-enc | Feedback, Insufficient Criminal Record, Repeat |
| 104 | a16_mafia-pastimes-enc | Pastime, Specific Mob Country |
| 100 | a16_mafia-push-crimes-enc | Present Snippet, Past Snippet, Against Rival Family, Seriousness Score, Get Caught Score, Associated Crime |
| 73 | a16_mafia-push-targets-enc | Target, Reason, Gender, Can Have Name, Godfather Cannot Order, Get Caught Score |
| 16 | a16_mafia-scare-methods-enc | Method, Scenario, Log Version, Effectiveness Score, Get Caught Score |
| 24 | a16_mafia-shakedown-methods-enc | Method, Success/Failure Scenario + Log Version, Third Person Version, Effectiveness Score |
| 23 | a16_mafia-temptation-scenarios-enc | Scenario, Log Version, Min/Max Amount, Get Caught Score, Is Cash, Dishonorable Choice + Narrative, Honorable Choice |

The mobs table is the spine of the whole family: five rows keyed on ISO country
codes (Sicilian mafia `it`, triad `cn`, yakuza `jp`, the Italian-American
mafia `it`, Irish mob `ie`) and the five rank names as columns, with the top
rank split by gender.

### 64.2 becoming a made man

The script flag is `"mafia": true` (the boolean already existed — report 56 used
it for the mafia misc pool bit), and `Sim::makeMafia` is called after
`makeDealer`. It sets the rank to 1, the standing to 35, hands the hero $250,
and calls `ensureMafiaFields`:

* the hero's country name is turned into its ISO code
  (`mafiaCountryCode` walks the parsed `a16_countries-enc.json` rows for the
  name and reads `.code`) because every mob column is keyed on the code;
* the family is drawn among the mobs rows whose Associated Country is that code,
  falling back to all rows for a country with no mob of its own (the United
  States, for instance, has none of the five as an exact match);
* the rival family is another row of the same table, and the notoriety/money
  scores are read off the family row into the hero's own state;
* the hero's nickname comes from the nicknames table filtered on Gender and Mob
  Country, and the cell is resolved because it is itself a bracket
  (`<first-name> Meatballs`);
* the rank title is read from the family's row (`mafiaRankTitle`).

### 64.3 the five ranks

`mafiaRankTitle` finds the hero's mob row and reads `Rank 1` … `Rank 4`, or
`Rank 5 Male` / `Rank 5 Female` for the top. The rank is a function of
standing: 45 → soldier, 62 → caporegime, 78 → underboss, 92 → godfather. A rank
change writes one line into the year's extras ("The family made me soldier."),
and once the hero holds rank 5 he is the godfather himself: `<mafia-godfather>`
and `<mafia-boss>` resolve to the hero's own name instead of a drawn one.

### 64.4 a year in the family

`Sim::mafiaYear` runs from age 18 at 40% a year and fires exactly one event,
first hit wins:

| chance | event |
|---|---|
| 0.18 | a shakedown — extortion-targets (Money Score) × shakedown-methods; success is `Effectiveness * 12 + standing > d100` |
| 0.30 | a job — push-crimes Present/Past with a push-targets reason; heat grows by Seriousness Score, and Get Caught Score × 8 decides whether someone talks |
| 0.10 | a contract — executions, outbound normally and inbound once standing drops below 20 (35%); a calling-card item is left at the scene |
| 0.14 | the boss says the hero is not earning enough (feedback-earnings) |
| 0.14 | praise or disappointment (feedback-positive above standing 50, otherwise negative), filtered to the mob's own country |
| 0.12 | a temptation — Min/Max Amount becomes `<amount>` and half the time the dishonorable narrative is taken |
| 0.20 | an evening with the family (pastimes, filtered to the country) |
| 0.12 | an excuse for missing a job (excuses, filtered on the hero's gender) |
| else | the family is not ready to promote him (join-rejections) |

Every year also moves the rank when the standing crosses a threshold, and a
godfather gets a 20% scare-methods line about the rival family. When nothing was
composed the year falls back to a one-line status ("I am a soldier of the triad
with 4 jobs behind me."), built from the rank title, the family name and the job
count.

### 64.5 the tokens

| token | source |
|---|---|
| `<mafia-boss>`, `<mafia-godfather>` | the hero once he is rank 5, else a drawn nickname for the mob's country |
| `<mafia-family>`, `<mafia-rival-family>` | the two mobs rows |
| `<mafia-colleague>`, `<mafia-rival-family-member>` | one drawn name per card, with the pronoun pair pointed at them |
| `<mafia-nickname>` | the hero's own nickname |
| `<mafia-calling-card>` | calling-cards, article by the partitive flag |
| `<first-name>`, `<last-name>` | the name pools |
| `<other-person>` | a name per card |
| `<amount>` | the money drawn by the temptation block |
| `<city-other>` | another city of the hero's country (the `<country-other>` idea) |

Six of those keys used to be answered by the report 56 misc map through the
nicknames and mobs tables; those entries were removed from `kMiscSrc` so the
family's own handler owns the pronouns and the gender of the name it draws.

### 64.6 two things the family's prose needed

**The row's other column.** A log line routinely points back at an alternative
drawn in a different column of the same row — the shakedown's Success Log
Version says "I kicked a jockey's `<rb>`!", where the body part lives in the
Success Scenario cell. A `warm` lambda (in `mafiaYear`) resolves every
bracketed cell of the row except the one about to be written, in column order, so
the `<rb>` cache is populated before the line is rendered. Same trick the
dealer trade needed (report 63), generalised.

**The second person.** The family addresses the hero directly ("`<mafia-boss>`
told you that you need to make some more money … or you might get clipped"). The
log has to become first person, and "you" is the object after a verb of address
and the subject everywhere else, so `toFirst` keeps a hand-written verb list
(told, warned, reminded, asked, took, taught, made, cost, gave, given, sent,
brought, handed, offered, dealt, kept, threw, put, left, paid …) and picks `me`
or `I` accordingly, then fixes up `your`/`yourself`/`you're`/`you'll`.
This is the one place in the port where a swap needs a little grammar instead of
a replacement table.

### 64.7 shortcuts

* no prison, no police heat beyond the per-event scores, no paying tribute up
  the chain; the family's own money score is tracked but never spent;
* `Godfather Cannot Order`, `Against Rival Family`, `Can Be Punished`,
  `Insufficient Criminal Record`, `Repeat` and `Is Cash` are read as flags
  only where the code uses them — the rest are skipped;
* the `toFirst` verb list is hand-kept (a new table with another verb of
  address would need one more word);
* the rank ladder is four standing thresholds, not the game's own progression;
* one event per year, so a year that fires the shakedown never shows the
  contract.

### 64.8 verification

`/tmp/qdir/mafiares.py` runs 30 seeds × both genders to age 60 with
`"mafia": true` and regexes every `<…>` out of the title, body and log lines of
the events whose table belongs to the family:

    mafia events 1288   residue 0
    660 a16_dumb-scenarios-mafia-enc      149 a16_mafia-push-crimes-enc
    140 a16_mafia-join-rejections-enc     130 a16_mafia-shakedown-methods-enc
     47 a16_mafia-feedback-earnings-enc     46 a16_mafia-pastimes-enc
     36 a16_mafia-executions-enc            34 a16_mafia-feedback-negative-enc
     25 a16_mafia-temptation-scenarios-enc  19 a16_mafia-excuses-enc
      2 a16_mafia-feedback-positive-enc

Sample years (seed 1, male, United States — the Sicilian mafia is picked because
the country has no mob of its own, so the pool falls back to all five rows):

    A job for the family   The ask: steal the winning horse from the Kentucky Derby.
                           The target: private investigator, who stuck his nose
                           where it didn't belong.
                           I stole the winning horse from the Kentucky Derby.
                           The family paid me $3,100.

    A contract             The family wanted Dustin Mangrove gone.
                           Method: mummification.
                           I left some octopus suckers where they would find it.
                           I mummified a deaf man's body and stuffed him into a
                           sarcophagus filled with flesh-eating scarabs!

    The boss noticed       I am a soldier of the triad with 4 jobs behind me.
                           Rollins Peary shook my hand and nodded.

Three bugs were found by this probe and fixed:

1. push-crimes' Present/Past Snippet cells were read straight out of the table
   without `resolve`, so eighteen kinds of inline bracket (and the tokens inside
   them) printed verbatim — 432 residue tokens in the first run, 98 of them
   `<mafia-rival-family>` alone;
2. the shakedown, execution, feedback, temptation and scare lines referred to an
   alternative drawn in another column of their own row, which only the `warm`
   pass fills;
3. `withArticle` returns the article alone (`"an "`), not the article plus the
   word, so the fallback line read "I am an of the Sicilian mafia" until the rank
   name was appended after it.

Full regression after the family: `rescensus` reports `distinct residue tokens 0
total 0`, `coverage.py` reports `captured scenarios: 109  recoverable: 107`,
`portgap.py` reports `ENTRIES 109 rendered-by-port 107 gap 0`, the `covpush`
script returns 0, and `bench.sh` reaches age 34 with no abort.

## 65. The zoo: a park the hero owns, thirty-five tables behind one animal at a time

The zoo is the largest family the port had left: thirty-five tables, 3,570 rows,
and the only one of them report 56 had touched was a single dumb-scenario pool.
Section 65 builds the park, stocks it, hires the people who look after it, and
then runs a year of it against the family's own tables. Most of those tables are
keyed on one of three things — the species, the breed, or the individual animal
— so a card keeps all three on the hero while it is being written.

### 65.1 the tables

| rows | table | what it carries |
|---|---|---|
| 1345 | a16_zoo-animal-names-enc | Animal, Breed, Name, Gender, Commonness Score |
| 313 | a16_zoo-animal-spend-time-color-enc | Animal, Pastime |
| 286 | a16_zoo-animal-treat-color-enc | Animal, Treat, Enjoyment Score |
| 224 | a16_zoo-animal-death-scenarios-enc | Animal, Type, Other Person/Animal Involved, Sentence |
| 193 | a16_zoo-animal-attack-actions-enc | Animal, Past Verb, Commonness Score, Damage Score |
| 192 | a16_zoo-animal-diseases-enc | Animal, Disease, article flags, Cures Organically, Can Be Fatal, Health Impact, Treatment Cost, Euthanize |
| 120 | a16_zoo-animals-enc | Animal, Breed, Breed Plural, Compatible Habitats, Maturity Age, Lifespan, Cool/Food Chain/Aggression/Danger Scores, Rare, Maximum Animal Value, Min/Max Annual Cost |
| 85 | a16_zoo-comment-card-color-enc | Snippet, Attraction Type, Commonness Score |
| 71 | a16_zoo-employee-application-color-enc | String, Employee Type, Min/Max Competence |
| 70 | a16_zoo-animal-sold-color-enc | Animal Type, Snippet, Min/Max Happiness |
| 70 | a16_zoo-habitat-names-enc | Name, Habitat Type, Commonness Score |
| 46 | a16_zoo-employee-background-check-color-enc | String, Employee Type (a list), Min/Max Competence |
| 45 | a16_zoo-habitat-move-in-feedback-enc | Animal Type (a list), Breed (a list), String |
| 44 | a16_zoo-animal-show-feedback-enc | Animal Blacklist, String, Positive, Show Type |
| 38 | a16_zoo-fundraiser-color-enc | Event Type, Animal, Snippet, Min/Max Reception |
| 34 | a16_zoo-street-names-enc | Street Name, Add Random Street Type |
| 32 | a16_zoo-employee-resignation-color-enc | String, Gender, Employee Type, Min/Max Competence |
| 31 | a16_zoo-habitat-features-enc | Type, Compatible Habitat, Cost, Happiness Impact Score |
| 29 | a16_zoo-buyer-person-types-enc | Description, Money Score, Commonness Score |
| 25 | a16_zoo-attraction-installation-color-enc | Snippet, Positive |
| 25 | a16_zoo-trade-accept-color-enc | Snippet |
| 25 | a16_zoo-trade-rejection-reasons-enc | Snippet, Reason, Commonness Score |
| 23 | a16_zoo-random-zoo-names-enc | Name, Commonness Score |
| 23 | a16_zoo-trade-animal-sold-color-enc | Snippet |
| 22 | a16_zoo-animal-grooming-methods-enc | Animal, Method, Feedback Snippet, Cost, Difficulty/Appeal Scores |
| 21 | a16_zoo-attractions-enc | Attraction Name, Ticketed, Min/Max Ticket Cost, Installation Cost, Annual Upkeep, Engagement Score |
| 20 | a16_zoo-animal-trafficker-persons-enc | Person Type, Uses Indefinite Article |
| 20 | a16_zoo-attraction-removal-color-enc | Snippet, Positive, Min/Max Engagement |
| 20 | a16_zoo-habitat-feedback-enc | String, Feature Blacklist, Feature Whitelist, Is Positive |
| 19 | a16_zoo-supplier-names-enc | Name |
| 18 | a16_zoo-habitat-clean-enc | Snippet, Log Version, Habitat Type |
| 12 | a16_zoo-gift-shop-item-enc | Product, Plural, Price Per Unit, Min/Max Sale Price, Popularity Score |
| 11 | a16_zoo-animal-show-enc | Show Type, Difficulty Score, Appeal Score |
| 10 | a16_zoo-fundraiser-enc | Event Type, Cost, Money Score, Difficulty/Appeal Scores |
| 8 | a16_zoo-habitat-type-enc | Type, Add Habitat, Cost, Annual Upkeep Fees |

### 65.2 founding the park

The flag is `"zoo": true` (the boolean already existed for the report 56 misc
bit), and `Sim::makeZoo` is called after `makeMafia`. It sets `ownsZoo`,
charges the hero $25,000 for the land, and calls `ensureZooFields`:

* a name off `a16_zoo-random-zoo-names-enc` (the cells are templates —
  "Mommy Dearest <Memorial Zoo/Zoo/Zoological Park/Wildlife Reserve>" — so they
  are resolved);
* an address from the street table, with " Street" appended when the row asks
  for it, and a house number in front of it;
* a supplier (`a16_zoo-supplier-names-enc`, whose cells carry `<random-surname>`);
* the first habitat, off the habitat-type table ("savanna", "aviary",
  "reptile room" …);
* a gate price of $15-40, three to six animals, two staff and an engagement
  score of 50-70;
* one named keeper, with a competence score that gates the staff tables.

`Sim::zooDrawAnimal` draws the individual the card is about: species, breed,
name (resolved, because the cell is a template like "Ms. <random-surname>") and
gender. A blank name means the park never named that animal, so the breed stands
in for it.

### 65.3 a year at the park

`Sim::zooYear` runs from age 18 at 50% a year. Upkeep comes off the books
first — $2,500 per animal and $5,000 per attraction — and engagement drifts down
by one a year. Then one event fires, rarest first so that no block starves:

| chance | event |
|---|---|
| 0.05 | an animal dies: the death table's sentence, the disease behind it, or the vet stepping in |
| 0.06 | an animal attacks a visitor, graded by the attack table's Damage Score |
| 0.07 | a disease from the disease table: treatment price, health lost, sometimes euthanasia |
| 0.07 | an offer in the car park — a trafficker and a rare breed off the bloodline table |
| 0.08 | a sale: the buyer's own money score, one buyer in four walks away on a rejection reason |
| 0.08 | a fundraiser, with the reception windowed against engagement |
| 0.09 | a show, unless the animal is on that show's blacklist |
| 0.09 | an attraction installed, or an existing one torn down |
| 0.09 | the gift shop till |
| 0.10 | a keeper hired (with a background check), or one handing in their notice |
| 0.10 | a feature for the habitat the animal lives in |
| 0.10 | a new animal moves in, judged by the move-in table's own lists |
| 0.12 | housekeeping, said the way the clean table says it |
| 0.12 | a treat, scored for how much the animal enjoyed it |
| 0.12 | an hour with the animal, said the way the pastime table says it |
| 0.08 | a grooming day |
| 0.14 | something a patron put in the suggestion box |
| else | the gate: tickets times animals and attractions, against the year's upkeep |

### 65.4 the animal on the card

Every animal-facing table asks for one of three names, so `zooToken` keeps them
straight: `<animal-zoo>` is the individual the card is about, `<habitat-animal>` is
the habitat that individual belongs in — read off its own bloodline row's
"Compatible Habitats" list — and every row that speaks about the animal gets the
pronoun pair pointed at it, so "`<he/she>` put up a fight as the animal handlers
crated `<him/her>` for removal" comes out with the right gender. The keeper, the
vet and the other animals of the park are drawn per card and cached, so one card
never names two different keepers.

### 65.5 the tokens

| token | source |
|---|---|
| `<animal-zoo>`, `<animal-zoo-possessive>` | the animal on the card (report 56 used to answer this off the species table) |
| `<zoo-animal-other>` | another animal in the park |
| `<habitat-zoo>`, `<habitat-animal>` (+ possessives) | the park's habitat and the animal's own |
| `<animal-handler>`, `<zoo-veterinarian>` | one name per card, cached per role |
| `<zoo-operations-manager>` | the role, as before |
| `<zoo-other>`, `<zoo-name-possessive>` | another park's name, and this park's |
| `<rival-zoo-animal-breed>` | a breed other than the card's |

The two zoo entries of the report 56 misc map (`animal-zoo`, `habitat-zoo`) and
the three zoo literals were removed from it, so the family's own handler owns
them. The `a16_dumb-scenarios-zoo-enc` pool stays where report 56 put it: the
dumb card is a prompt with four choices, and the misc runner already renders it
with a real animal, habitat and keeper behind its tokens.

### 65.6 shortcuts

* the zoo is bought, not earned: no planning permission, no loan, no sale of the
  park; $25,000 comes off the hero's purse at founding;
* `Requires Specific Employee`, `Involves Animal`, `Can Be Fatal` on the attack
  table, `Appeal Score`, `Annual Upkeep Fees` and the ticketed-attraction price
  ranges are read as flags or scores only where the code uses them;
* the fundraiser's reception is judged by the park's engagement score, not by the
  table's own event difficulty;
* habitat capacity is not modelled: the habitat table's `Cost` is not paid and no
  habitat ever fills up;
* animal breeding, maturity and lifespan are read off the bloodline table but
  never advanced — an animal dies when the death table says so.

### 65.7 verification

`/tmp/qdir/zoores.py` runs 30 seeds × both genders to age 60 with `"zoo": true`
and regexes every `<…>` out of the title, body and log lines of the family's
events:

    zoo events 1441   residue 0
    659 a16_dumb-scenarios-zoo-enc       165 a16_zoo-random-zoo-names-enc
     58 a16_zoo-animal-diseases-enc       55 a16_zoo-animal-attack-actions-enc
     50 a16_zoo-fundraiser-color-enc      44 a16_zoo-animals-enc
     44 a16_zoo-attractions-enc           42 a16_zoo-gift-shop-item-enc
     42 a16_zoo-animal-show-feedback-enc  40 a16_zoo-trade-animal-sold-color-enc
     34 a16_zoo-animal-spend-time-color   32 a16_zoo-animal-death-scenarios
     30 a16_zoo-comment-card-color-enc    28 a16_zoo-animal-treat-color-enc
     26 a16_zoo-habitat-features-enc      24 a16_zoo-habitat-move-in-feedback
     17 a16_zoo-employee-resignation      16 a16_zoo-habitat-clean-enc
     16 a16_zoo-employee-application      10 a16_zoo-trade-rejection-reasons
      8 a16_zoo-animal-grooming-methods    2 a16_zoo-employee-background-check

Sample years (seed 4, female, United States):

    A new attraction      I had a carousel installed for $100,000.
                          For the first few weeks after its installation, patrons
                          lined up for hours just to experience it.
    The vet's rounds      Latrice came down with west nile virus.
                          The treatment came to $4,500.
                          It would have cleared up on its own, but the vet was
                          not taking the chance.
    Time with Latrice     I got up early and went worm-hunting with Latrice.
    The gift shop         The gift shop moved 191 butt scratchers this year at
                          $2 each. That is $382 over the counter.
    A year at the gate    The zoo took $111,600 with 4 animals on the books,
                          against $10,000 of upkeep.

One bug was found by this probe and fixed: 38 of the family's cells were read
straight out of the table without `resolve`, so the cells that carry brackets of
their own leaked them — the spend-time table stores "watched the
`<sunset/sunrise>` with `<animal-zoo>`", and the sold-color table's promises are
written in the animal's pronouns. Every `trim` in the section is now a
`resolve(trim(…))`, except the two inside the file-static `zooInList` helper,
which compare list items and must not re-enter the resolver. Two phrasings were
fixed with it: "The zoo held a an exclusive livestream" and "A a poacher offered
me" became the article helper's business instead of a hard-coded "a".

Full regression after the zoo: `rescensus` reports `distinct residue tokens 0
total 0`, `coverage.py` reports `captured scenarios: 109  recoverable: 107`,
`portgap.py` reports `ENTRIES 109 rendered-by-port 107 gap 0`, the `covpush`
script returns 0, and `bench.sh` reaches age 34 with no abort.

## 66. The landlord: a house the hero owns, a tenant in it, and the squatter who replaces them

Section 65 gave the hero a zoo. Section 66 gives them a rental: one property,
one tenant, and the long tail of things a tenant does to a landlord — late rent,
damage, notices, inspections, and the year they simply stop paying. The family
is 22 files and 1198 data rows, and before this section the port had never
mentioned a property at all.

### 66.1 The tables

| rows | table | what it holds |
| ---: | --- | --- |
| 212 | `a16_landlord-tenant-damaged-property-scenario` | Scenario, Log Version, Amenity, Tenant Pet Type, house-type whitelist and blacklist, the four bedroom/bathroom flags, repair band, craziness window, Impact Score |
| 112 | `a16_landlord-amenity-feedback-scenario` | Scenario, Log Version, Amenity, Commonness Score |
| 105 | `a16_landlord-home-default-features` | Feature String, house-type whitelist and blacklist, Enjoyment Score, Requires Haunted House, property-condition window (already wired by section 56 as `<amenity>`) |
| 97 | `a16_landlord-tenant-inspection-color` | Positive flag, Scenario, Log Version, craziness window, Lover Exclusive, Brackets Used |
| 85 | `a16_landlord-maintenance-request-scenario` | Body Snippet, Feedback Present Snippet, Feedback Past Snippet, room flags, repair band, Agree and Disagree Satisfaction Impact, May Cause Move-Out |
| 84 | `a16_landlord-background-check-color` | Crime, Commonness Score, craziness window |
| 79 | `a16_landlord-tenant-leave-color` | Scenario, Log Version, Requires Haunted House, satisfaction window |
| 66 | `a16_landlord-tenant-employment-check` | Career, Gender, craziness and income windows — "Dog Food Taster", "Snake Milker", "Serial Redditor" |
| 58 | `a16_landlord-tenant-beg-lower-rent-scenario` | Scenario, Log Version, craziness and relationship windows |
| 52 | `a16_landlord-real-estate-portfolio` | String, Only Squatters, Single Squatter, Singular Tenant, satisfaction and condition windows — the quiet year, and where the rent lands |
| 37 | `a16_landlord-home-remodel-feature` | Project, Secondary Line Text, cost band, house-type whitelist and blacklist, Enjoyment Score, Value Percentage, Country Blacklist |
| 36 | `a16_landlord-tenant-late-rent-color` | Scenario, Log Version, Commonness Score, craziness window |
| 31 | `a16_landlord-natural-disaster-scenario` | Type, Scenario, Impact Score, Can Demolish, house-type blacklist |
| 26 | `a16_landlord-tenant-lover-nags-hero` | Scenario, Log Version, Hero Gender, Lover Gender, craziness and relationship windows |
| 23 | `a16_landlord-tenant-warn-scenario` | Scenario (second person: "You threatened to evict <tenant>."), Risk Score |
| 17 | `a16_landlord-tenant-beg-to-stay-scenario` | Scenario, Log Version, craziness window |
| 17 | `a16_landlord-spooked-contractor-quit-color` | Scenario, Log Version, Hauntedness Score |
| 14 | `a16_landlord-ghost-kill-tenant-color` | **not loaded**: the shipped file's header row is blank (the real header sits on line 3), so the reader would see one unnamed column |
| 13 | `a16_landlord-squatter-reason-color` | Scenario, Log Version, Rent-Related flag |
| 12 | `a16_landlord-tenant-late-rent-insist` | Scenario, Log Version, Commonness Score |
| 12 | `a16_landlord-squatter-push-scenario` | Scenario, Log Version, Hero Gender, Squatter Gender |
| 10 | `a16_landlord-maintenance-refusal-scenario` | Feedback Snippet, Log Version, craziness window |

Two tables outside the family carry the property: `a16_house-types-enc` (29
rows: Type, bed and bathroom windows, Price Score, Haunted Score, Commonness
Score, Street Type) and `a16_street-names-enc` for the address.

### 66.2 Buying the property

`ensureLandlordFields` draws one row of `a16_house-types-enc` weighted by
Commonness Score and takes the type, the Haunted Score, a value of
`Price Score × 60000` plus a roll, and a bedroom and bathroom count rolled
inside that row's own windows. The address is a house number plus a street name,
with " Street" appended when the street row's Add Random Street Type flag is
set. Rent is derived from the value (`value / 1500 × 10`, floor $100). The
hero pays a flat $20,000 on the way in — the port has no mortgage model.

`landlordDrawTenant` then draws the tenant: a gender coin, a name from the
shared name pools, and a row of `a16_landlord-tenant-employment-check`
filtered on Gender, which supplies the job ("snake milker"), the income, and the
craziness window that most of the tenant tables gate on. Satisfaction starts at
55 plus a roll.

### 66.3 A year with a tenant

`landlordYear` runs from age 18 at 55% a year, drifts condition down by one
and satisfaction two points toward 50, then fires exactly one branch, first hit
wins:

| chance | branch | what it does |
| ---: | --- | --- |
| — | squatter | the year the tenant stopped paying: 55% they clear out, otherwise the police call it a civil matter |
| — | empty property | draws a tenant and writes the move-in card, with a 45% background check (craziness +15) |
| 0.05 | natural disaster | `Impact Score` × 8 off the condition and × 4000 of damage; a Can Demolish row can total the place |
| 0.05 | haunted contractor | needs Haunted Score ≥ 3, windowed on Hauntedness Score |
| 0.25 | squatter | when satisfaction has fallen below 45: `squatter-reason-color` with the Rent-Related flag checked against a rent above $1500 |
| 0.20 | maintenance request | room and type flags gate the pool; 65% agree and pay the repair band, condition +2; 25% of those add a refusal; disagree and a May Cause Move-Out row can end the tenancy |
| 0.18 | damaged property | amenity, type and room flags, craziness window; the repair band is paid and the condition takes `Impact Score × 5` |
| 0.15 | inspection | the Positive flag is checked against satisfaction ≥ 60; a failed one brings `tenant-warn-scenario` and 25% of the time a beg-to-stay |
| 0.12 | late rent | 50% insist, and 60% of those collect three months of arrears, else satisfaction +4 |
| 0.12 | rent reduction asked | 40% agree (rent × 9/10, satisfaction +7) |
| 0.10 | tenant leaves | haunted properties and the satisfaction window; the tenant slot empties |
| 0.10 | lover nags | `tenant-lover-nags-hero` with the hero's own lover |
| 0.12 | amenity feedback | filtered to an Amenity the property actually has |
| 0.10 | remodel | the cost band is paid, the project is stored on the property and value rises by Value Percentage |
| else | portfolio | `real-estate-portfolio`, and the rent: twelve months of it, or nothing while the squatter sits there |

### 66.4 The tokens and the misc-map handover

`landlordIsToken` answers `tenant`, `squatter`, `address`,
`tenant-address`, `squatter-address`, `property-city` and
`house-type`, each with an optional `-possessive`. All four of those
names were counted across the whole corpus first: `house-type` (90 uses),
`tenant` (175 of 422 uses), `tenant-address` and `amenity` are used
only by landlord files, so the family can own them. `kMiscSrc` therefore
lost its `house-type` line, `kMiscPeople` lost `tenant` and
`kMiscLiterals` lost `tenant-address`; `<amenity>` stays in the
shared map because it reads a different column of a shared table.

### 66.5 Shortcuts

* The property is bought outright; there is no mortgage, no equity, and no sale.
* Rent is collected only in the portfolio branch, so a year that fires a
  maintenance request or an inspection collects nothing.
* Value Percentage is read as a percentage of the money spent, not of any
  market model, and the property never appreciates on its own.
* `a16_landlord-ghost-kill-tenant-color` is skipped: its header row is
  blank in the shipped data.
* Tenant Pet Type, Singular Tenant, Brackets Used and Risk Score are read only
  where a branch needs them; nothing is modelled about pets.
* The second-person lines are turned around by `toFirstPerson`, the same
  helper section 64 wrote for the mafia, now hoisted out of `mafiaYear` into
  a file-static function so both families share it.

### 66.6 Verification

`/tmp/qdir/ltres.py` (30 seeds × both genders, ages 60, `landlord: true`)
reports 1629 landlord events and **residue 0**. The histogram:
`a16_dumb-scenarios-landlord-enc` 811, `real-estate-portfolio` 231,
`maintenance-request-scenario` 99, `tenant-damaged-property-scenario` 95,
`tenant-inspection-color` 68, `squatter-push-scenario` 58,
`tenant-late-rent-color` 55, `natural-disaster-scenario` 47,
`tenant-leave-color` 42, `tenant-beg-lower-rent-scenario` 37,
`amenity-feedback-scenario` 30, `squatter-reason-color` 27,
`home-remodel-feature` 20, `spooked-contractor-quit-color` 9.

A few cards from the run:

* "I told a lawyer about Willie Walid, and he immediately advised me to take
  care of that situation ASAP." / "The police said it was a civil matter."
* "Dustin Shott began squatting on my property at 184 Dingleberry Dr as a means
  of protesting land ownership."
* "A maintenance request came in: rising waters during a recent storm flooded
  the entryway." / "Maria Dollar was not pleased to hear it was their problem."
* "Seismic activity in Atlanta caused considerable damage to your futuristic at
  494 Sequoia Street." / "The futuristic took $12000 of damage."
* "I had a fire pit put in at 494 Sequoia Street for $2772." / "Get toasty next
  to open flames"
* "I collected $48600 in rent this year."

Three bugs were found by probing and fixed:

1. `<random-surname>` and `<city>` leaked 83 times from the address —
   the street-name cell is itself a token template, so the draw needs
   `resolve(trim(…))`.
2. The portfolio String leaked `<tenant>`, `<his/her>` and every one of
   its own inline brackets (97 tokens). It is now resolved with the pronoun pair
   pointed at the tenant for the length of the resolve, which also fixed the
   pronoun.
3. The second-person swap skipped a capitalised "You" — the walk searched only
   lowercase `you`, so "You told Maria Dollar to shape up" came out
   unchanged. It now matches both, and "Your " swaps to "My " as well. Five
   lines that appended a period to a cell that already had one ("…in the dining
   room..") now go through `spySentence`.

Full regression after the landlord: `rescensus` reports `distinct residue
tokens 0 total 0`, `coverage.py` reports `captured scenarios: 109
recoverable: 107`, `portgap.py` reports `ENTRIES 109 rendered-by-port 107
gap 0`, the `covpush` script returns 0, and `bench.sh` reaches age 34
with no abort. The mafia probe was rerun after the shared helper changed and
still reports `mafia events 1288 / residue 0`.

## 67. The pet at home: one animal at a time, and the ten tables that describe its year

The pet is the smallest of the careers and the only one the hero never chooses: a
creature that ages, gets sick, gets fat on treats, and eventually dies of something
the data has a row for. Ten tables, 3,806 rows. Three of them were already loaded
before this section (the animal list, the name list, and the discipline pool the
registry card uses); the other seven were not referenced anywhere.

### 67.1 The ten tables

`a16_pets-enc` — 136 rows. `Animal,Emoji,Breed,Pure Bred,Wild,Exotic,Black Market,Colors,Average Lifespan,Obedience Score,Intelligence Score,Danger Score,Cost Score,Commonness Score`. When a Wild, Exotic or Black Market cell is set the breed is not a household animal, so the draw skips it: those three flags are the only filter between the pet here and the menagerie in the zoo section. Cost Score is unread (the money the hero spends on the pet is derived from the vet and treat tables instead).

`a16_pet-names-enc` — 1,484 rows, `Animal,Name,Color,Gender`. One name row per animal and often per breed. Name cells are templates (`<random-surname>` for a fish or a hamster, ordinary first names for a cat), so the picked cell is resolved before it is used. Color is unread; Gender decides whether the name belongs to a male or female pet, blank fitting either.

`a16_pet-death-types-enc` — 488 rows, `Animal,Type,Commonness Score,Other Person Involved,Sentence`. The Sentence is written about the pet, with `<pronoun>` `<his/her>` `<him/her>`, so the card has to point the pronoun pair at the animal rather than the hero. `Other Person Involved` is a flag: when it is clear the `<person>` phrase is cut out of the sentence before it is shown, otherwise a passer-by keeps the fault ("<pronoun> died from heat exhaustion after <person> inadvertently left <him/her> outside on a hot summer day.").

`a16_pet-pastimes-enc` — 445 rows, `Animal,Pastime`. The Pastime cell is a bare verb phrase ("<petted/rubbed/massaged/wrestled with>", "<shared/talked about> all of <life's/the world's> <problems/sorrows/issues/drama> with"), so the line is assembled as "I <pastime> <pet>.".

`a16_pet-treats-enc` — 490 rows, `Animal,Treat,Enjoyment Score`. Treat is already written with its article ("a rib eye steak", "a piece of cheddar"). The score is 1-5 and is worth twice itself in happiness.

`a16_pet-releases-enc` — 183 rows, `Animal,Scenario`. The `<pronoun>` in this table is the OWNER ("<pronoun> went to the <jungle/savanna/forest> before releasing <pet> to roam wild."), which is why the release branch leaves the hero's own pronoun in place and only points the pair at the pet for the animal clause.

`a16_pet-release-filler-enc` — 104 rows, `Animal,String,Gender,Big Animal,Clean`. What became of the animal afterwards. The String uses `<animal-type>` and `<animal-type-indefinite>`, so a released fish, a released horse and a released cat each get their own paragraph.

`a16_pet-disease-types-enc` — 120 rows, `Animal,Disease,Uses Indefinite Article,Uses Definite Article,Commonness Score,Cures Organically Score,Can Be Fatal,Health Impact Score,Treatment Effectiveness Score,Treatment Cost Score,Euthanize`. Same shape as the zoo's disease table: the vet bill is `Treatment Cost Score * 400`, the health lost is `Health Impact Score * 4`, and `Can Be Fatal` together with a pet already at 20 health or below is the only way a treatable disease turns terminal.

`a16_pet-found-dead-scenarios-enc` — 11 rows, one column. `Scenario` is a second-person report ("You found <pet> on the side of the road after being killed by a car!"), which the card prints as it ships because the hero is the one who found the animal.

`a16_pet-discipline-enc` — 345 rows, already wired as the registry card `ScenarioPetDiscipline`. Its `Condition,Animal,Scenario,Log Version,Craziness Score,Pet Gender,Pet Talks,Hero Gender,Hero Min Age,Hero Max Age,Clean` columns belong to the discipline card, not to this yearly chain, and the two do not share state.

### 67.2 Adopting the pet

`makePet` is a pure draw: `petDraw` takes a household row of `a16_pets-enc` weighted by `Commonness Score`, keeps its Animal and Breed, flips a coin for the sex, and sets age 0, health 95 and happiness 75. The name comes from `a16_pet-names-enc` filtered on that Animal and on the gender (blank fits both), and the cell is resolved because several of them are templates. Cost, lifespan, obedience and intelligence are read but not spent: the pet arrives free and its only expense is the vet.

The script flag `"pet": true` is new — unlike the landlord, casino, zoo, dealer and mafia subsystems, the pet had no flag at all before this section, so `include/bl/script.h` gained a `bool pet` and `src/script.cpp` gained both the parse block and the `if (spec.pet) sim.makePet(life);` call. There is no misc-pool bit riding along with it.

### 67.3 A year with a pet

`petYear` runs from age 6 at 55% a year and fires one branch, first hit wins.

| chance | branch | what it does |
| --- | --- | --- |
| — | the house has no pet | an 8% adoption card, otherwise the year is skipped entirely, so a hero who never had a pet is not spammed |
| 0.45 when health <= 15, else 0.04 | the death | the death Sentence with the pronoun pair pointed at the pet and the `<person>` clause cut unless the row flags one, title "The end of <pet>", body "Cause: <Type>." |
| 0.28 | the treat | "I gave <pet> a rib eye steak." and `Enjoyment Score * 2` happiness |
| 0.25 | the pastime | "I <pastime> <pet>." and a little happiness |
| 0.12 | the vet | health lost, the bill printed, and with `Can Be Fatal` plus a pet at 20 or below a 30% chance the animal does not pull through |
| 0.10 when happiness is under 40, else 0.03 | the release | the hero drives out and lets the animal go, with a 50% `release-filler` paragraph about what the animal did next |
| 0.10 when health is 30 or below | found dead | the `found-dead` Scenario, cause "Went missing" |
| else | the quiet year | "<pet> is N now." and "Health H, happiness P." |

The bookkeeping that runs before the chain is what makes the branches reachable at all. Health falls 3 a year, and 8 a year once the animal is past 8; happiness drifts up by one below 70 and down by three at or above it. A pet that is never treated therefore slides toward the release and found-dead branches on its own, and a pet that is pampered every year stays happily middle-aged until the health clock wins.

### 67.4 The six tokens

`<pet>` and its `-possessive` spelling come from the name, and fall back to "my <animal>" when the data left the name blank. `<pet-name>` is the name alone. `<breed>` is the lowercased breed, `<animal>` and `<animal-type>` are the lowercased animal, and `<animal-type-indefinite>` is the same with its article ("a goldfish", "an iguana"). `<pronoun>`, `<his/her>` and `<him/her>` are deliberately NOT claimed by the pet token handler: the pronoun pair is global, and the two pet tables that mean the owner (releases) and the two that mean the animal (death, found dead) are separated by saving and restoring `pronounOtherSet_` around each branch instead.

### 67.5 Shortcuts

- The pet is free and arrives fully healthy; lifespan, obedience, intelligence and cost scores are read but never spent.
- `a16_pet-release-filler-enc` is used as a paragraph after the release rather than as the release itself.
- The found-dead branch needs a pet at 30 health or less, which most pets do not reach before the death branch takes them; it is wired and reachable but rare in a sample.
- The discipline pool stays with its own registry card and is not merged into the yearly chain.
- Heredity, breeding and the `Pure Bred` flag are not modelled: one pet, one card.

### 67.6 Verification

`/tmp/qdir/petres.py` walks 40 seeds in both sexes to age 60 with `{"pet": true}`, keeps every event whose table contains `pet` and greps the brackets out of the title, the body and the log lines:

    pet events 2472 rc failures 0
    RESIDUE 0

Split by title (the quiet year falls back to the pastimes table, so the table histogram overstates it): quiet 519, treat 298, pastime about 245, death 80, disease 72, adoption 53, release 18. The full regression stayed green: `rescensus` distinct residue tokens 0, `coverage.py` 109 captured / 107 recoverable, `portgap.py` 109 / 107 / gap 0, `covpush.json` rc=0, `bench.sh` to age 34 with no abort.

Two cards from the sample:

    The end of Nymeria          Cause: Natural.
                                she found a quiet place in the yard and died peacefully.

    A trip to the vet           The vet treated Nymeria for gastric torsion.
                                The bill came to $1200.

and one from the release branch:

    Letting go                  A random dog running the streets of Chicago crashed a party
                                and ingested crack cocaine before the cops broke up the party
                                and detained the dog.
                                he went to the mountains before releasing Cooper to roam wild.

## 68. The studio: a signed artist, an album a year, and the twenty-six tables behind a record

Report 54 gave the hero a genre, a band and a stage name. This section gives the same career a
middle: an audition, a label, an album with a title, a cover, a track list and a reception score,
the shows that follow, the fans who turn up, and the two ways a band ends. The a16_music family is
the largest unwritten cluster in the data - 53 files - and this section takes 26 of its content
tables. The remaining music files (producer, SoundCloud, club takeover, talent scout, score
production, stage names, instruments) are left for a later pass.

### 68.1 The twenty-six tables

All under `gamedata_plain/`, prefix `a16_music-`, suffix `-enc`.

| rows | table | columns that matter |
| ---: | --- | --- |
| 480 | album-title | Title, Genre, Commonness Score |
| 370 | sample-track | Song, Genre, Commonness Score |
| 222 | album-covers | Cover, Solo, Band, then one x flag per genre (Pop, Rap, Country, Folk, R&B, Worship, Rock, Alternative, Reggae, K-Pop, Heavy Metal) |
| 151 | track-title | Track Title, Genre, Commonness Score |
| 148 | track-lyrical-hook | Hook, Genre, Commonness Score |
| 105 | subgenres | Subgenre, then one x flag per genre |
| 91 | track-subject | Field, Genre, Commonness Score |
| 87 | cover-art-string | String, Genre (a comma list) |
| 78 | track-vibe | Field, Genre, Commonness Score |
| 67 | practice-scenarios | Scenario, Log Version, Role |
| 63 | signature-sound | Sound, Use Indefinite Article, Reception Impact Score, Commonness Score |
| 59 | album-rejections | Reason, Solo, Band, Album Type |
| 52 | band-breaks-up-scenarios | Reason, Min Popularity, Max Popularity |
| 52 | band-kicks-out-scenarios | Reason, Role, Performance Based, Hero Min Age, Hero Max Age |
| 52 | fan-interaction-scenarios | Positive, Negative, Scenario, Feedback Snippet, Log Version Snippet, Fan Gender, Fan Age, Hero Gender, Min/Max Popularity, Hero Min/Max Age, Min/Max Respect, Solo, Band, Role |
| 41 | record-label-company-name | Name, Genre, Commonness Score |
| 40 | audition-rejections | Reason, Solo, Band, Role |
| 34 | track-beat | Field, Genre, Commonness Score |
| 31 | artistic-flair-string | String, Log Version, Genre, Reception Impact Score |
| 27 | concert-venues | Venue, Min/Max Capacity, Min/Max Popularity, Money Score |
| 27 | track-rejection-string | String, Log Version, Reason, Commonness Score |
| 26 | names-compilations | Title, Commonness Score, Accommodates Definite Article |
| 10 | roles | Role, Solo, Band, Solo Fame Score, Band Fame Score, Primary Instrument, Secondary Instrument, School Activity |

The genre vocabulary is the interesting part. `a16_music-genres-enc` (wired in report 54) names the
twelve genres one way; the per-genre flag columns of the album tables spell three of them another:
the data says `rhythm and blues` where `album-title` says `R&B`, `rap` where the label table
says `hip-hop`, and `heavy metal` and `K-Pop` keep their capitals only in the flag columns. A
single `musicGenreFits` helper absorbs all of that, so a genre filters every table it owns.

### 68.2 The audition and the label

`ensureStudioFields` runs at the end of `makeMusician`, so the role arrives with the genre.
The role is drawn from `roles` filtered by the Solo/Band flag pair (a solo artist sings or plays an
instrument alone; a band act gets a role from the band list), and its `Primary Instrument` and
`Secondary Instrument` are what the hero plays in the studio card.

Each year from 16 `studioYear` walks a first-hit-wins chain. An unsigned artist gets one audition
(35% a year): `below(100) < 45 + musicFame/3` signs them to a label drawn from the company-name
table, pays a $2,500 advance and gives them the `musicSigned` flag; a miss prints a reason from the
rejection table filtered on the Solo/Band pair and on a blank-or-matching Role.

### 68.3 The album

Once signed (or once fame passes 45) the album year runs: a title from `album-title` filtered on
genre, a 15% chance it is a compilation instead (title from `names-compilations`, and the album
rejection table only accepts `Album Type = Compilation` rows for it), a subgenre whose genre flag
is set, a cover from `album-covers` (Solo/Band flag plus the genre flag), cover art from
`cover-art-string` whose `Genre` cell is a comma list, then six to fourteen tracks - each a title,
a beat, a subject and a vibe from their own genre-filtered tables - a 30% lyrical hook, a 30% sampled
song, a signature sound (stored on the hero and reused) and an artistic-flair line that carries its own
`Reception Impact Score`.

Then the label answers. 15% of albums are rejected outright and 20% of what remains loses a single
track to `track-rejection-string`. A released album scores

`reception = clamp(35 + musicFame/2 + flairImpact*4 + tracks + (sampled ? 4 : 0), 5, 100)`

which pays `reception * 4000` and moves fame by `reception/12`. Two sample cards:

> The album drops | Best of Papi Hankey (compilation) - cantopop. Tracks: Silver; Yeongdeungpo Style;
> Ongoing Story; DUI IUD | Beat: experimental. | Subject: cutesie junk. | Vibe: kawaii-desu.

> The label said no | They said an album called Hung for Me with 14 tracks and a man running proudly
> with a Jamaican flag on the cover would be a great way for the band to end its career. | The label
> shelved Hung for Me.

### 68.4 Shows, fans and the end of the band

Four smaller blocks share the year. A show (18% once fame reaches 20) picks a venue through the
Min/Max Popularity window around the hero fame, seats `500 + below(15000)` people whether or not the
venue holds them, and pays `capacity * Money Score * 2` - the bar sample reads "I played a bar to 524
people and took $4192." A fan (22%) is filtered on fan gender, hero gender, role, the Solo/Band pair,
fame and respect windows, and the row's `Positive` flag decides whether fame moves +3 or -3. The band
ends two ways: a break-up (6%, windowed on popularity) or a kick-out (6%, filtered on role and the
hero's age), both of which clear the band name.

> The band is over | There's been nothing but fighting going on for months. | Undisclosed Number broke up.

> Out of the band | They don't like your manner of dress. | I was out of The Knee and the Brain.

### 68.5 The tokens

Five new keys, all in `musicIsToken` so they resolve through the music branch:

`<music-role>` the hero role ("guitarist"); `<artist-name>` and `<artist-name-possessive>` the hero full
name, or the band name when the hero is in a band; `<album-description>` the album title plus its track
list; `<musician-soundcloud>` a SoundCloud musician, which falls back to the cassette-era `<musician>`
reference pool because the SoundCloud family is not wired yet.

### 68.6 Shortcuts

- Reception is a formula over the hero own fame rather than a simulation of the market.
- The venue capacity is rolled from a flat range, not from the venue row, so a bar can be described
  as playing to five hundred people.
- Nothing models the album's chart life, royalties, touring or a second act; the whole career lives
  in one flag plus a fame number.
- The compilation flag affects the title and which rejection rows are legal, nothing else.
- Producer, SoundCloud, club-takeover, talent-scout and score-production tables stay unwired; their
  tokens fall back or resolve to a drawn name.
- The fallback studio day registers no table row, so the probe histogram overstates
  `practice-scenarios`; the card really reads "A session at the studio".

### 68.7 Verification

Forty probes (30 seeds x both genders, ages 60, `{"music": true}`) produce 1149 music events with
zero bracket residue: `a16_dumb-scenarios-music-enc` 474 (report 54's pool, still running), practice
376, album-title 77, concert-venues 54, record-label 50, fan-interaction 36, audition-rejections 34,
band-breaks-up 18, band-kicks-out 14, track-rejection 14, album-rejections 2. Splitting the same run
by card title (50 seeds x 2) gives 402 fallback studio days, 240 practice sessions, 134 album years
(~94 releases, 25 track cuts, 15 label refusals), 102 fan cards, 94 shows, 85 signings, 71 rejected
auditions, 24 break-ups and 22 kick-outs.

The full regression stays green: `rescensus.py` reports zero distinct residue tokens, `coverage.py`
109 captured / 107 recoverable, `portgap.py` 109 entries with a gap of 0, `covpush.json` exits 0 and
`bench.sh` walks to age 34 without aborting.

Two bugs were found by the probe rather than the compiler. A `long long(x)` functional cast (twice in
the show block) is not valid C++ and had to become `static_cast<long long>(x)` - the second time this
exact mistake has cost a build. And 64 residue tokens all came from `fan-interaction-scenarios`: the
row's `Log Version Snippet` says `<rb>` about an alternative drawn in the `Scenario` or `Feedback` column of
its own row, so reading only the one column leaves the cache empty. The fix is the same warm pass the
mafia year uses - resolve every bracketed cell of the row in column order before composing the card.

## 69. The studio trades: a producer, a SoundCloud page, a scout, a composer and a club act

Report 54 wired the artist line and report 68 the studio behind it. The family still
had thirty files nobody read: the five music jobs that are not the artist. This section
wires all thirty, and along the way generalises three pronoun problems that the whole
port shares.

### 69.1 The thirty tables

All under `gamedata_plain`, prefix `a16_music-`, suffix `-enc`. Rows in brackets.

Producers: `producer-concert-act` (160) — a single Name column of generated act names
("Da Real Bruno", "<Quickie/Slickie/Rikki/Licki> Menage"); `producer-stage-name` (65) —
Name,Hero Gender,Commonness Score, the hero's own stage name ("B.I.G. <hero-last-name>",
"<hero-last-name> Daddy" male, "<hero-last-name> Mommy" female, so the title is a
function of the hero's gender and surname); `producer-titles` (48) —
Scenario Tag,Option 1..5,Situation Description, so one row is five possible titles and a
description of what the hero did; `producer-genre` (13) —
Genre,Compatible Genre 1..3,Predominant Country,Popularity Score; `producer-dumb-scenario`
(50) — a four-choice year-up card; `producer-aspiring-artist-string` (33) —
String,Artist Genre,Min/Max Vocal Quality; `producer-artist-leave-string` (32) —
String,Min/Max Artist Popularity; `producer-termination-string` (31).

SoundCloud: `soundcloud-screen-name` (66) — Name,Hero Only,Gender,Commonness Score
("<hero-last-name><num100-999>", "Notorious_BIT"); `soundcloud-track-instrument` (43) —
Instrument,Genre,Project Tones, where both Genre and Project Tones are comma lists;
`soundcloud-track-reception-string` (39) — String,Collab,Genre,Min/Max Reception;
`soundcloud-comment` (49) — String,Log Version,Track Genre,Min/Max Popularity, and the
String is the comment as reported while the Log Version is the comment itself;
`soundcloud-insult` (54) — Insult,Track Genre,Commonness Score;
`soundcloud-collab-refusal-string` (43); `soundcloud-dumb-scenario` (39).

Talent scouting: `talent-scout-venue-name` (38) — Venue,Event Type,Commonness Score
("The Liquor <Box/Hole>" with an open mic night, "<city> Amphitheater" with a concert);
`talent-scout-potential-talent-string` (42) — String,Artist Genre,Min/Max Vocal Quality;
`talent-scout-artist-rejection-string` (30).

Scoring: `score-production-project-title` (83) — Name,Project Type,Commonness Score;
`score-production-project-tone` (11) — Tone,Min BPM,Max BPM (romantic 70-100, epic 90-120,
suspense 60-90, terror 150-180); `score-production-acceptance-string` (31);
`score-production-rejection-string` (30) — Reason is instrument or BPM.

Club acts: `club-takeover-string` (32) — Scenario,Log Version,Positive,Popularity Impact;
`club-takeover-stunt` (34) — Scenario,Log Version,Artist Genre,Popularity Impact;
`club-takeover-stunt-outcome` (31) — the same shape with a Positive flag;
`club-takeover-second-thought` (34) — String,Artist Gender,Artist Genre.

Names and props: `artist-stage-name` (246) — Stage Name,Gender,Skin,Genre,Commonness Score,
the table that actually names the acts ("Whitney Spears", "Ted Sheeran", "Mob Barley");
`names-stage` (184) — Title plus one flag column per genre, titles that are templates of
their own ("<Easy/Master/Baby/Lil'/Big/Young/Old/Post/...> <musician-first-name>");
`instruments` (26) — Instrument,Singular Name,Category,Finishes,Min/Max Cost,Can Buy,
Can Busk,Busk Score,Parent Lessons Score,Cost Score,Difficulty Score,Commonness Score;
`dinner-location` (51) — String,Gratitude, where gratitude is 1 to 3.

Thirty files, 1668 data rows. Together with the twenty-eight of report 68 the whole music
family (58 files) is now read by the port.

### 69.2 The five trades

`Sim::makeTrade(Life&, int kind)` sets one flag and draws the act the trade works with:
kind 1 a producer, 2 a SoundCloud page, 3 a talent scout, 4 a film composer, 5 a club act
whose nights turn into stunts. Every kind ends with `makeInstrument` (a weighted draw over
the instrument table, kept on the hero) and `tradeDrawArtist`.

`tradeDrawArtist` is where `artist-stage-name` and `names-stage` come in: the act is a
stage name filtered by the trade's genre and the act's own gender when the table has a row
for them ("Debbie Carrey", "Axl Hoes", "Amy VanSant", "Manila Moon", "Duchess Villa"),
the genre flag column of `names-stage` is the fallback, and a drawn first name plus
surname is the last resort. The act's popularity and vocal quality are rolled once
(20 + below(61) and 20 + below(71)) and are the numbers the roster tables window on.

`tradeGenre(Life&)` answers the hero's own genre when the artist line gave them one, else
draws and stores one, and `producer-genre` supplies one of the three compatible genres for
the trades to work in.

### 69.3 A year in a trade

`Sim::tradeYear` runs from 16 at 50% a year, picks one of the trades at random when the
script switches on several, and fires one event. Chances, first hit wins.

Producer: a first client (windowed on the act's vocal quality against Min/Max Vocal
Quality, title "Signing a client") — 0.18 a concert act, 0.18 a `producer-titles` row
(the title is one of its five options, the body its situation), 0.16 the four-choice
producer card, 0.14 the act leaving through the popularity window, 0.12 dinner at a table
chosen by its gratitude, 0.10 termination, else a session at the desk worth $15,000.

SoundCloud: 0.30 an upload, 0.22 a comment, 0.20 the four-choice SoundCloud card, 0.15 an
insult, 0.13 a refused collaboration, else a few new followers. The upload draws a track
title by genre, its instrument and tone from the comma lists, then a reception string
through the Min/Max Reception window around a reception of
`20 + followers/8 + below(31)`; followers grow by reception/5 and the card pays
reception x 30. A track is drawn once per card when the year is not an upload, because
every SoundCloud row names it.

Scout: a venue and an event type, then an act through the vocal-quality window; half the
acts are signed (title "Scouting at <venue>") and the other half bounce off the label with
one of its reasons.

Composer: a project title and type, a tone with its BPM window, a quality of
`30 + reputation/2 + below(31)` against 55, and either an acceptance ($8,000 to $28,000)
or a rejection that costs two points of reputation.

Club act: 0.35 a stunt whose Popularity Impact moves the hero's fame, half of them with a
`club-takeover-stunt-outcome` tail, 0.25 a takeover, 0.20 a second thought, else a night
at the club.

### 69.4 The tokens

A new guard, `tradeIsToken`, owns `<artist>` (301 uses) and `<artist-possessive>` (40),
plus `<track>`, `<musician-first-name>`, `<musician-last-name>`, `<musician-name>`,
`<musician-first-name-letter>`, `<instrument>`, `<project>`, `<project-tone>` and
`<trade-stage>`. The name fragments have no reference pool — `genesis_master_tags_reference_2`
holds `musician`, `musician_soundcloud` and `alcohol` for this area and nothing else — so
they are drawn from the port's own name generator.

One shared fix: `Sim::musicToken` used to answer nothing at all unless the hero had taken
the artist line, which left `<musician-soundcloud>` leaking out of the SoundCloud reception
rows when a trade was played without `"music": true`. It now answers for any of the five
trade flags, prefers the collaborator the card already named, and falls back through the
reference pools to a drawn name.

### 69.5 Three pronoun problems, generalised

The trade rows exposed three patterns the whole port shares.

**A row's own subject.** Many rows open on `<he/she> says <he/she>'s ...`. Prefixing the
act's name produced `Harley Shift she says she's taking some time off`. `nameTakesSubject`
now puts the name on the first pronoun and lets the resolution do the rest:
`Harley Shift says she's taking some time off to take care of her family.`, and
`Duchess Villa tells me the pressure is getting to her.`

**Third person about the hero.** `producer-titles` writes its situation as
`The hero's new cover art was well received by the public`; `heroToFirstPerson` turns it
into `My new cover art was well received by the public`.

**"you" as an object.** `toFirstPerson` already knew `told`, `gave`, `asked` and friends
take `me` while other verbs take `I`, but it knew only the past tense, only lowercase
contractions, and missed verbs the newer rows use. It now carries the present tense and
more verbs (`tells`, `warns`, `costs`, `hits`, `kisses`, `blames`, `arrested`,
`replaced` ...) and the capitalised contractions (`You're` -> `I am`, `You'd` -> `I'd`),
which fixed `They said to not let the door hit I where the good Lord split I` into
`... hit me ... split me` and `Discovering a piece of yourself you'd forgotten` into
`... myself I'd forgotten`. The verb list is a hand-kept list and the comment says so.

### 69.6 Shortcuts

- The five trades are a single `tradeYear` behind one flag each; when a script sets
  several, the year picks one at random instead of playing them all.
- A year-up card has no prompt to route a choice through, so the port takes the first
  choice the card offers (the same shortcut as every other dumb-scenario pool).
- The producer's reputation and the SoundCloud follower count are the only two trade
  currencies; money is paid as a fee and nothing else about the label is modelled.
- `instruments` is read for its name and commonness only: costs, lessons and busking
  scores are not spent.
- The composer's acceptance threshold (55) and the scout's signing coin (50%) are port
  inventions; the shipped tables give no odds for either.
- `artist-stage-name` Skin, `names-stage` Gender, `dinner-location` Gratitude beyond the
  reputation point are read where they are used and ignored otherwise.

### 69.7 Verification

`/tmp/qdir/traderes.py` plays 30 seeds x both genders to 60 with all six switches on and
regexes `<[^>]*>` out of every music event's title, body, log lines and extra:
1316 events, residue 0. Histogram, most common first: talent-scout potential talent 252,
score project title 155, producer aspiring artist 142, club takeover 132, SoundCloud
instrument 89, SoundCloud reception 84, club stunt 55, score rejection 54, comment 44,
concert act 42, stunt outcome 37, score acceptance 36, second thought 34, producer
titles 34, insult 32, artist leaves 22, collab refusal 20, SoundCloud card 20, producer
card 16, termination 12, dinner 4.

Cards this run produced: "A hit off the desk" / "Studio: GoldenShowerhead." /
"I produced a hit for Amy VanSant and took my cut."; "Uploaded to SoundCloud" /
"Track: Hey Dude." / "I played banjo on it." / "Reception: 38, and 56 followers after
that." / "Listeners commented that the tempo shift at 2:50 ruined the flow of the track.";
"Scouting at BitLife Amphitheater" / "I spent the night at BitLife Amphitheater
(concert)." / "The label signed Debbie Carrey."; "The score is accepted" /
"Calming Shadowed Drifter (video game), 95 BPM in epic." / "The fee came to $12,501.";
"A comment" / "Track: an untitled demo. Comment: is the perfect track to give to someone
you hate." / "She says this is the perfect track to give to someone you hate.";
"Second thoughts" / "Duchess Villa is not sure about this life." / "Duchess Villa tells me
the pressure is getting to her."; "Second thoughts" style producer card "Behind the Mic" /
"Ram Overwood calls you to say he wants a portable sauna added to his rider." /
"The choice was: As you wish."; the SoundCloud card "Production Value" / "The choice was:
Ask nicely."

Full regression after the section, all green: `rescensus.py` distinct residue tokens 0;
`coverage.py` 109 captured / 107 recoverable; `portgap.py` ENTRIES 109 rendered-by-port
107 gap 0; `covpush.json` rc=0; `bench.sh` to age 34 with no abort. The four neighbouring
probes were re-run because `toFirstPerson` is shared: mafia, landlord, studio and pet and
zoo all report residue 0.

## 70. The vampire: a council, a familiar, and forty-four tables behind one night a year

Report 48 built the vampire as a *state* the hero can be in: the cape, the coffin, the
familiar, and one dumb-scenario pool that fired now and then. It never built the nights.
This section wires the 44 tables of the `a16_vampire` family into a year of them.

### 70.1 The family

44 files, 2243 rows, all under `gamedata_plain` with the `a16_vampire-` prefix and the
`-enc` suffix. Two of them (`dumb-scenario`, `name`) were already wired by report 48.

| rows | table | what it holds |
|------|-------|---------------|
| 185 | blood-pact-rejection-string | a mortal turning the offer down, keyed on person type |
| 171 | name | first names, surnames, a `Skin` column |
| 144 | insult | the word another vampire throws (`Clean`, `Vampire` flags) |
| 114 | titles | a `Scenario Tag` and five candidate titles for the council's decree |
| 95 | hunter-name | the vampire hunter's name, by type |
| 88 | blood-taste-color | how the blood tasted, plus a disease indicator |
| 84 | street-name | `<AB/A/B/O>-<Negative/Positive> Blvd` templates |
| 72 | spend-time | an evening out: `to a vampire gala, where you gave him ...` |
| 67 | dumb-scenario | the report 48 card, now gated on notoriety and the familiar |
| 60 | victim-status-color | what state the victim was in, windowed on their health |
| 57 | familiar-color | the familiar's description, looks, smarts, craziness |
| 55 | feeding-location-color | the alley, and the second-person log line |
| 53 | witch-doctor-remedie | a folk cure with an effectiveness and a risk score |
| 53 | hunter-encounter-scenario | the hunter arrives |
| 52 | familiar-confused-string | the familiar does not understand |
| 49 | code-data | the council's code, and the approval it earns |
| 45 | familiar-dump | abandoning the familiar, with a risk score |
| 44 | body-dump-color | where the body went |
| 41 | hypnosis-breakup-string | compelling a lover to leave |
| 40 | vcoli-string | V. Coli, the vampire disease |
| 39 | familiar-threat | threatening the familiar, with a relationship impact |
| 39 | coffin-string | a day lying low |
| 38 | lover-convert-nag | the lover wants to be turned |
| 38 | council-discipline-scenario | the council sits, with `Spare` / `Punish` / `Kill` flags |
| 37 | familiar-command | an order the familiar obeys |
| 36 | blood-offering-string | a mortal offering a shot glass |
| 34 | spend-time-hunger-color | the small accident that follows |
| 32 | hunter-appreciation-string | the hunter's speech |
| 32 | council-party-type | the name of the gathering |
| 31 | council-punishment | the sentence |
| 31 | council-party-string | the gathering itself |
| 30 | council-reward | the reward |
| 29 | solo-hunt-location | where to hunt, with per-species likelihoods |
| 29 | hypnosis-money-string | compelling money out of a victim |
| 29 | hunt-string | the hunt, with essence and relationship impacts |
| 27 | hypnosis-animal-scenario | compelling a victim to believe they are an animal |
| 26 | hunt-encounter-color | who else is out there, windowed on notoriety |
| 23 | hypnosis-love-string | compelling love |
| 22 | entice-method | luring a victim, with effectiveness and both outcomes |
| 22 | assault-method | taking one by force |
| 21 | clubbing-approache | an approach in the club |
| 13 | blood-bath-scenario | the bath |
| 11 | disease | the five vampire diseases |
| 5 | feeding-location | the risky places, with a risk score |

### 70.2 The vampire the hero already was

`makeVampire` now fills in the rest of the state on top of report 48: notoriety 20,
essence 60, standing 50, hunger 40, a home address drawn from `street-name` (the cell is a
token template, so it is resolved before the house number is prepended), a maker drawn from
the name tables, and a familiar whose description, looks and smarts come from
`familiar-color` filtered on `Familiar Gender` and rolled inside that row's windows.

### 70.3 A year as a vampire

`vampireYear` runs from age 18. It draws one other vampire for the whole card (name plus
gender), clears any remembered victim, and then walks a single branch — first hit wins,
ordered rarest-first so nothing starves.

| chance | event |
|--------|-------|
| 0.06 | a decree of the council: `code-data`, approval moves standing, the title is kept |
| 0.08 | V. Coli: the `vcoli-string` row, and the disease sticks |
| 0.10 | the council renames the hero: `titles`, one of five options, body carries the tag |
| 0.08 | an insult from another vampire, filtered on gender, pooling the `Vampire` rows first |
| 0.10 | the lover nags to be turned: gender, `Married`, and relationship windows |
| 0.12 | an evening out, and at 45% the hunger colour line that follows it |
| 0.10 | an approach in the club |
| 0.08 | the witch doctor: effectiveness against a d100, else the risk score in essence |
| 0.08 | the body is dumped: a new victim is drawn and notoriety rises |
| 0.10 | a day in the coffin |
| 0.10 | a mortal offers their blood, or refuses in their own words |
| 0.06 | a blood bath |
| 0.06 | the council sits: `Spare` / `Punish` / `Kill`, then half the time the sentence |
| 0.05 | a council reward |
| 0.08 | a gathering: the party type names the evening |
| 0.08 each | entice, then assault: effectiveness times eighteen against a d100 |
| 0.12 | a hunter: their name, their speech, and the appreciation rows that mean the hero |
| 0.12 | hypnosis: an animal, money, love or a breakup, one table each |
| 0.15 | the familiar obeys, is confused, is threatened, or is dumped |
| 0.20 | a feed: the location and its risk, the taste, the disease indicator, the victim's state |
| 0.25 | the hunt: a solo location, the hunt line and its impacts, and who else is around |
| else | the report 48 dumb-scenario pool |

### 70.4 The eight tokens

`vampToken` replaced the older `vampireToken`. It answers `victim` (the dealer's victim, then
the spy's target, then a name drawn once for the card, with the gender kept),
`familiar`, `vampire` / `vampire-name`, `maker`, `vampire-council-member` (cached per card),
`amount` (the mafia amount when one is live, else one of eight round figures) and `insult`
(the gender-filtered pool, preferring rows flagged `Vampire`). The `-possessive` spelling is
stripped and re-appended, and the shipped typo `victim-possesive` is mapped to `victim`.

The old narrow branch in `resolveInto` only knew `vampire` and `familiar`; the new guard sits
ahead of the dealer handler, so `<victim>`, `<amount>` and `<insult>` no longer fall through
into another family's tokens.

### 70.5 The three residue bugs

The probe found 80 leaked brackets and they had three separate causes.

1. **66 × `<him/her>` in the Method cell.** `assault-method` and `entice-method` print their
   `Method` column ("Slam <him/her> against a wall") in the card body, and the pronoun there
   means the *victim*. Both blocks now point the pronoun pair at the victim and reset it
   after the line is built.
2. **4 × `<rb>` in `feeding-location-color`.** The log line is "into an alley behind
   `<rb>`" while the alternatives are a slash pair in the *String* column of the same row
   ("`<himself/herself>`"). The cache was empty because a pair like that is answered by the
   pronoun handler, which never filled it. Fixed in `resolveInto`: a pronoun token that
   contains a slash also lands in the `<rb>` cache and in the last-value fallback.
3. **The taste of the blood.** The `Taste` cell carries its own alternatives
   ("like a fine `<Burgundian/Italian/French/box/Napa Valley>` wine"), and the cell was read
   with `trim` alone. Now resolved.

### 70.6 Shortcuts

* The vampire state is still a flag the script turns on; there is no infection roll, no
  daylight death and no blood pool. Essence, hunger and standing are bookkeeping numbers
  that events move.
* Location likenesses (`Animal`/`Human`/`Vampire Appearance Likelihood`) are read for
  nothing: the solo hunt picks its location by commonness.
* Skin, cleanliness and the colour pools carry no meaning — the port has no colour channel.
* `familiar-color` is drawn once at the start of the night and reused for every familiar
  line on that card, so a familiar keeps one description until the next card.
* The victim's health is rolled in range rather than tracked across the years.

### 70.7 Verification

`vampres.py` walks 30 seeds × both genders to age 60 with `vampire` on, collects every event
whose table belongs to the family, and greps the card for anything still shaped like a
token: **residue 0**. The 25 wired event tables, by number of cards:

```
vcoli 216  insult 206  witch-doctor 189  code-data 181  coffin 175
body-dump 152  blood-pact-rejection 141  feeding-location-color 132
hunt-string 120  council-party 91  blood-offering 91  hunter-encounter 87
blood-bath 86  council-discipline 68  council-reward 41  assault 36
familiar-dump 35  entice 34  familiar-confused 32  familiar-threat 30
familiar-command 22  hypnosis-money 14  hypnosis-animal 12  hypnosis-love 10
hypnosis-breakup 8
```

Every other family probe was re-run because the pronoun cache and the token order are
shared: dealer, mafia, zoo, landlord, pet, studio, trades, secret agent, fight, casino,
black market, cult and the relationship pools all still report zero. The regression suite
is green: `rescensus` reports no residue at all, coverage holds at 109 captured / 107
recoverable, the port renders 107 of the 109 entries, the coverage push exits 0, and the
bench walks to age 34 without an abort.

Cards from the run:

```
It went badly | Method: Slam him against a wall.
              | I tried to slam Luka Gnolls against a nearby wall, but missed.

A decree of the council | The code: Consume not just the blood, but the flesh as well
                        | They have decreed instead that vampires should only consume blood
                          and refuse the urge to become cannibals.

A night out | While rethinking your life's choices in the club's bathroom, a vampire named
              Gideon Strok'Im crawls under your stall and says, "I vant to suck you off"!
            | A vampire named Gideon Strok'Im offered to convert me at the club.

An evening out | I went to a vampire gala, where I gave him an amulet of protection and a
                 glass of merlot instead of blood.

An offering | From: Jam Roderick.
            | He hands me a shot glass full of his blood and says, "It's straight from the tap".

The witch doctor | The remedy: garlic bread.
                 | I treated myself with garlic bread and it worked.

A feed | The victim: Helix Flooren (risk 1).
       | The blood tasted sophisticated.
       | Into an alley that smelled like someone had a really bad case of the runs
```

Two shipped quirks are visible in that output and left as they are: the council row that
reads "endure being pelted with **he**" is the data's own pronoun bug, and the familiar
description cell is a predicate ("smells of sewage water"), so the body reads "My familiar
smells of sewage water."
## 71. The outdoors: six ways out of the house, and forty-two tables behind a trip

The `a16_outdoor` family was the largest unwired family left: forty-two tables, 1307 rows.
It is not a career and not a place. It is the card the hero gets for going outside, and every
one of those forty-two tables hangs off that single trip: the destination, what the hero does
there, what they find, what finds them, what they bring home.

### 71.1 The family

Every file is `<name>-enc` under `gamedata_plain`. Thirty-seven of the forty-two are
read. Twenty-four of them carry a card of their own; thirteen are colour, companion or item
pools drawn into another pool's line; five are left unused (`titles`,
`hiking-dumb-scenario`, `animal-treat`, `bird-treat`,
`bird-discovery-scenario`).

The columns that matter:

| table | columns the port reads |
|-------|------------------------|
| `destination` | `Location`, `Country`, `Uses Definite Article`, `Habitat` (`lake` / `river` / `pond` / `ocean` / blank), `Activity` (`hike` / `camp` / blank), `Landmarks` (a quoted comma list), `Cost` / `Danger` / `Appeal` / `Commonness Score` |
| `vehicle` | `Make`, `Model`, `Type` (`ATV` 16 rows, `Dirt Bike` 16, `Mountain Bike` 12, `Road Bike` 12) |
| `catchable-fish` | `Species`, `Habitat`, `Preferred Bait`, `Min`/`Max Weight (lbs)`, `Min`/`Max Value`, `Difficulty Score`, `Commonness Score` |
| `hiking-item-discoverie` | `Item`, `Min`/`Max Item Value`, `Commonness Score` |
| `fishing-treasure` | `Name`, `List Item Name`, `Uses Indefinite`/`Definite Article`, `Plural`, `Min`/`Max Value`, `Location` (a comma list: `lake, river`) |
| `plant` | `Species`, `Plural`, `Habitat`, `Hiking Only`, `Camping Only`, `Enjoyment Score`, `Toxicity Score`, `Health Impact`, `Happiness Impact`, `Hallucinations`, `Tastes Like` |
| `animal` | `Species`, `Plural`, `Habitat`, `Hiking Only`, `Camping Only`, `Danger Score`, `Aggression Score`, `Successful Attack String`, `Failed Attack String` |
| `bird` | `Species`, `Plural`, `Habitat`, `Danger Score`, `Difficulty Score`, `Attack String` |
| `marine-life` | `Species`, `Plural`, `Habitat` (`open ocean shelf`, `sandy-bottom shallow`, `coastal inlet`, `deep sea`), `Danger Score`, `Difficulty Score`, `Attack String` |
| `cave` | `Name`, `Location` (a country), `Difficulty Score` |
| `dog-behavior` | `Activity`, `Scenario`, `Log Version`, `Min`/`Max Dog Craziness`, `Pet Gender` |
| the four dumb cards | `Hero Min Age`, `Hero Max Age`, `Gender Bracket`, `Requires Companion`, `Companion Type`, `Scenario`, `Choice N`, `Narrative N`, `<X> Impact N` |

The dumb cards are `camping-dumb-scenario`, `bike-ride-dumb`, `cave-discoverie`
and the unused `hiking-dumb-scenario`. All four are the same shape: an opening
`Scenario` paragraph full of inline alternatives, buttons, and a log line that points back
at those alternatives with `<rb>`, `<rb2>`, `<rb3>`.

### 71.2 The trip

`makeOutdoor` runs once, when the script sets `"outdoor": true`. It sets the
`outdoors` flag, draws a vehicle where `Type` contains `atv` (fallback `ATV`)
and a bicycle where `Type` is not `Dirt Bike` (fallback `Mountain Bike`), and
seeds the trip state: enjoyment 55, bicycle condition 85 minus a small roll, no trips, no
catches, nothing earned.

`outdoorYear` then runs once a year from age 8, after the pet and before the misc runner,
on a 0.5 chance. A trip is one of six activities, weighted:

| weight | activity |
|--------|----------|
| 0.30 | hike |
| 0.22 | camp |
| 0.20 | fish |
| 0.13 | off-road |
| 0.10 | bike |
| 0.05 | snorkel |

The destination is drawn first and is the card's identity: hike and camp filter the pool on
`Activity`, fish and snorkel read `Habitat` into the trip's habitat and later filter
the fish and marine-life pools on it, and one `Landmarks` entry is split out of the quoted
list so the landmark pools can land on a landmark that belongs to the place. The companion is
the lover 45% of the time and a drawn stranger another 35%; while a row is written about the
companion the pronoun pair points at them and is restored afterwards. Each trip adds 3 to
enjoyment and one to the trip count.

### 71.3 The six activities

**Hike** (title `A hike`, body `Trail: X.`). An item find (0.45, money rolled inside
`Min`/`Max Item Value`), a plant (0.35, with `Enjoyment`/`Happiness`/
`Health Impact`, and a 0.6 follow-up hallucination and symptom line when the row is
flagged `Hallucinations`), an animal (0.25: `A <species> <Successful Attack String> me`
and `Danger` points of health at 35% when `Danger Score` is 3 or more, otherwise a
line about backing away), a bird (0.20 plus its behaviour line), a landmark (0.15 through its
view reflection, memento or selfie colour), a cave (0.10, drawn against the hero's own country,
which then delegates to the `cave-discoverie` card), a drink (0.10) and a ghost (0.08). A
dog comes along (0.30) through `hiking-dog-discovery-scenario`, its fetch item, and the
walk line; the `Hiking Only` flag is not read because the activity already is the filter.

**Camp** (title `A night under the stars`, body `Campsite: X.`). A 0.35 delegation to
the camping dumb card, an animal (0.22), a plant (0.18), a ghost (0.15), and the dog (0.35)
through the `Activity == camping` rows of `dog-behavior` and a camping treat.

**Fish** (title `A fish on the line`, body `Catch: X.`). One of `lake`,
`river`, `pond` and `ocean` is thrown and the `catchable-fish` pool is filtered
on it. The fight is one of two colour pools keyed on the `Difficulty Score` of the fish,
written in the second person and turned back into the first person the log uses. The weight is
rolled inside `Min`/`Max Weight (lbs)` and the price inside `Min`/`Max Value`,
both paid. A fish of 20 lbs or more is mounted and named from `mounted-fish-name`, and 0.15
of the time the catch includes treasure drawn from `fishing-treasure` filtered on the
water.

**Off-road** (title `A day off-road`, body `Route: X.`). A trick (0.50) succeeds when a
d100 beats `Difficulty Score` scaled from 0.95, and prints the `Success Feedback
Version` column; a failure prints the infinitive `String` back at the hero and costs 5
points of bicycle condition. Then the drive's colour line (0.45, filtered on the trick's own
`Location`), an encounter (0.35, `Danger Score` health when it is 3 or more), a reward
(0.30, windowed on the hero's age and printed with the right article), and the passenger when
there is one (0.25).

**Bike** (title `A bike ride`, body `Route: X.`). A 0.35 delegation to the bike dumb
card, the route's own colour pool, and the bicycle condition drifting down by a few points a
ride.

**Snorkel** (title `A snorkel trip`, body `Site: X.`). The discovery card, then a
marine animal filtered on the trip's habitat, the play line, its behaviour line, and 40% of the
time the animal's `Danger Score` in health when it is 4 or more.

### 71.4 The eight tokens

The family's cards use fifteen distinct tokens; eight of them are new and one is a typo.

`outdoor-location` is the destination (article aware), `outdoor-companion` is the
companion with the pronoun pair switched while the row is written, `outdoor-cave` is a cave
whose `Location` is the hero's country (falling back to any row), `bicycle` is the
drawn bike, `outdoor-landmark` is the destination's landmark, and `outdoor-plant`,
`outdoor-marine-life` and `outdoor-bird` are species drawn `Habitat`-filtered and
remembered per card, so a second mention of the same token names the same species. `pet`
was already handled but is spelled ` pet` with a leading space in three rows of the
family, so `outdoorIsToken` trims the token before answering. `<alcohol>` and
`<person-possessive>` resolve through the reference and person handlers that were already
in `resolveInto`.

### 71.5 The six bugs

1. **The table names lost their `-enc` suffix.** The forty-two entries were added to
   `kTables` without it, so every run died at startup with `load failed: cannot open
   table: .../a16_outdoor-animal`: the loader reads every entry eagerly, so one bad name
   stops all sixty runs, not just the family's year.
2. **`makeOutdoor` was declared in the private block.** The declaration has to sit next to
   `makePet` in the public block of `include/bl/sim.h`, or `src/script.cpp` cannot
   call it.
3. **The back-references leaked.** The family writes a card as one `Scenario` paragraph
   full of inline alternatives and then a log line that points back at them with `<rb>`
   and `<rb2>`. The first helper resolved one named owner column and left the other shapes
   leaking 38 `<rb>`, 15 `<rb2>` and 3 `<rb3>`. `withBrackets(table, row,
   want)` now resolves every *other* bracketed column of the row in table order — the order
   the game draws them — and only then the column that was asked for. The off-road trick pools
   have no paragraph at all (their two columns refer to each other), so `String` is
   resolved first into a local and the `Success Feedback Version` after it.
4. **`renderRow` read the card's body from `Body`.** No table in this family has a
   `Body` column; the dumb cards and the cave and bike cards call the opening paragraph
   `Scenario`. Every one of them therefore rendered an empty body and never drew the
   paragraph's alternatives, which is exactly where the back-references were coming from. The
   full-card path now falls back to `Scenario`, resolved before the narrative so the cache
   fills in draw order.
5. **A card with no `Title` was treated as a narrative-only row.** The narrative-only
   branch was chosen by `!has(Title) && !has(Body)`, but `bike-ride-dumb` has neither
   and still has a `CTA` and four choices, so it took the wrong branch and its log line
   came out as the prompt. A table with a `CTA`, a `ChoiceList` or numbered choices is
   now a card whatever else it has.
6. **Three of the six activities never emitted a card.** Off-road, bike and snorkel set no
   title, and the year ends on `if (title.empty()) return false;`. Their titles are now
   `A day off-road`, `A bike ride` and `A snorkel trip`, each with the route as the
   body.

### 71.6 Shortcuts

* The colour pools (`*color-string`, `selfie-color`, `marine-life-play-string`)
  carry no meaning at all: the port has no colour channel.
* The fish's weight and price are rolled in range rather than tracked; a mounted fish is a log
  line, not an inventory.
* A landlocked destination can still be the snorkel or fishing site: the `Activity` column
  is only read for hike and camp, because the values it holds (`hike`, `camp`) do not
  cover the other four.
* The vehicle, the bicycle and the destination are drawn once at the start and keep their
  names for the hero's whole life.
* Nobody ages out of the outdoors and the trip is once a year on a coin flip; the game's real
  cadence is not reproduced.
* The dog that comes along is the hero's `pet` when it is a dog, and no dog row is written
  when it is not.

### 71.7 Verification

`outres.py` walks 30 seeds x both genders to age 60 with `outdoor` on: **1025 cards, 0
failures, residue 0**. The histogram is the number of cards that drew a row from each pool:

```
167 destination          106 catchable-fish        78 plant
 76 animal                74 camping-dumb-scenario  65 bike-ride-location
 51 ghost-encounter       48 mounted-fish-name     46 marine-life-play-string
 46 bird                  44 bike-ride-dumb        37 hiking-item-discoverie
 28 offroading-reward     26 offroading-encounter  24 offroading-trick
 23 cave-discoverie       18 fishing-treasure      18 hiking-drink
 14 hiking-dog-discovery  10 offroading-make-love-string
 10 landmark-view-reflection  6 dog-behavior       6 landmark-memento-scenario
  4 hallucination
```

All fourteen family probes still report residue 0 after the shared `renderRow` change, the
census reports no residue at all, coverage holds at 109 captured / 107 recoverable, the port
renders 107 of the 109 entries, the coverage push exits 0 and the bench walks to age 34
without an abort. One card per activity:

```
A hike      | Trail: Kungsleden Trail.
            | I found a novelty rubber chicken worth $8.
            | I hiked through Kungsleden Trail.

A night under the stars | Campsite: Tongariro National Park.
            | The campsite was surrounded by death camas.
            | I camped at Tongariro National Park.

A fish on the line | Catch: mahi-mahi.
            | I don't feel anything while holding my rod but notice the bobber on my bait
              is bouncing.
            | I landed a mahi-mahi that weighed 43 lbs and sold it for $82.
            | I had Dory mounted above the fireplace.

A day off-road | Route: Okavango Delta.
            | I did a no-hander in the Trailmaster Hunter-Youth.
            | I drove and doing donuts in an open field.

A bike ride | Route: Camino de Santiago.
            | I rode around the city on the Specialized Stumpjumper.

A snorkel trip | Site: Maasai Mara National Reserve.
            | I fed a snack to a deep-sea catshark.
            | It decided to dart in close, then speed off.
```

Two shipped quirks are visible there and left as they are. The off-road colour pool has one
gerund row (`and doing donuts in an open field`) among forty-five prepositional ones, so
the line glues it to the drive verb and reads awkwardly; and the snorkel card's site is a
landlocked park, which is the shortcut above rather than a residue.

## 72. The hustle: three trades on the street, and twenty-two tables behind a day out there

The hero who has nothing can still go out and get something: stand on a corner with a sign,
set up with an instrument and play, or work a mark with a story. The port calls all three
the hustle. Twenty-two files, 1,375 rows, and one card a year for as long as the hero keeps
at it.

### 72.1 The family

Every file is `<name>-enc` under `gamedata_plain`. All twenty-two are read. Six shape the hero's
setup and never carry a card of their own: `attire` (51), `busk-music-types` (32),
`panhandle-props` (41), `panhandle-signs` (83), `panhandle-strategies` (38) and
`collection-boxes` (55). The rest are cards, colour pools or item pools:

| table | rows | what it is |
|-------|------|-----------|
| `panhandle-strategies` | 38 | `Strategy`, `Hero Gender`, `Effectiveness`/`Police Risk`/`Theft Risk`/`Confrontation Risk Score` |
| `panhandle-signs` | 83 | `String` (the short form), `Full String` (the sign as it hangs), `Hero Gender`, `Hero Min`/`Max Age`, `Confrontation Score`, `Male`/`Female Effectiveness Score` |
| `panhandle-props` | 41 | `Prop`, `Plural`, `Male`/`Female Effectiveness Score` |
| `panhandle-items` | 84 | `Item`, `Plural`, `Uses Indefinite Article`, `Fits In Collection Box`, `Worthless`, `Min`/`Max Value` |
| `panhandle-food` | 129 | `Item`, `Plural`, `Uses Indefinite Article`, `Health`/`Happiness Impact`, `Poison Risk Score` |
| `panhandle-excuses` | 120 | `Condition` (`child` / `default`), `Excuse`, `Commonness Score`, `Sympathy Score` |
| `panhandle-confrontation-scenarios` | 30 | `Scenario`, `Log Version`, `Person Gender`, `Anger Score` |
| `police-scenarios` | 34 | `Scenario`, `Log Version`, `Ask To Leave`, `Person Gender`, `Anger Score`, `Is Universal` |
| `insults` | 58 | `Insult`, `Gender` — the insult describes the hero (a female hero is the "bag lady"), so the pool is filtered on the hero's own gender |
| `collection-boxes` | 55 | `Item`, `Panhandler`, `Performer`, `Panhandler`/`Performer Effectiveness Score` |
| `busk-music-types` | 32 | `Music Type`, `Needs Music Appended`, `Best Instruments` (a comma list), `Success Score` |
| `perform-acts` | 10 | `Act`, `Act Person`, `Difficulty`/`Money`/`Danger Score`, `Danger Body Parts`, `Danger Verbs` |
| `perform-positive` / `negative` pools | 23 / 30 | `Scenario`, `Log Version`, `Can Be Unskilled`, `Positive`/`Negative Score` |
| `perform-respect` / `disrespect-scenarios` | 35 / 42 | `Scenario`, `Log Version`, `Act` (blank means any act), `Positive`/`Negative Score` |
| `busk-requests` | 167 | `Request`, `Is Proper Noun` |
| `serenade-songs` | 164 | `Song`, `Reaction Score` |
| `scam-types` | 17 | `Scam`, `Description`, `List Description`, `Gender`, `Victim Gender`, `Success`/`Refuse`/`Insist String`, `Difficulty`/`Money`/`Confrontation`/`Police Score`, `Tier` |
| `scam-items` | 48 | `Item Singular`, `Item Plural`, `Hero Gender`, `Victim Gender`, `Can Be Broken`, `Can Be Stolen`, `Is Fake Merch`, `Min`/`Max Value` |
| `scam-victim-scenarios` | 84 | `Scenario`, `Log Version`, `Is Police`, `Undercover Cop Score`, `Min`/`Max Friendliness Score` |

`scam-types` writes its own log line with the hero in it — `<you/I> sold <victim> <item> for
<amount>.` — which is why the first-person pairs below are fixed rather than drawn.

### 72.2 The street

`makeHustler` runs once when the flag is set. It draws the trade (panhandling 0.45,
busking 0.35, scamming 0.20), the attire, the genre and the instrument, the act, the song,
the sign, the strategy, the prop and the collection box, and seeds `hustlerSkill` at
`40 + below(35)`.

`hustlerYear` needs the hero to be 13 or over and flips a 0.5 coin, then draws the corner
of the year. No table holds that pool — the game keeps it inline in the scenario cells
(`<restaurant/cafe/coffee shop/bookstore/...>`), so the port carries ten corners of its own
and records it as a shortcut.

**Panhandling.** `I spent the day trying to <strategy> in my <attire>, holding up my sign:
<sign>`. The take is `6 + below(25) + effectiveness * 3` when the
`Effectiveness Score` of the drawn strategy plus the skill carries the day, and
`below(5)` when it does not. Then, in order: a handout that fits the box 0.35 (its own
`Min`/`Max Value` pays out and the box is named), a meal from a stranger 0.30
(`Health` and `Happiness Impact`, and a 30% follow-up when the `Poison Risk Score` is 3
or more), an excuse 0.22 from the pool the hero's household picks, a confrontation 0.18, an
insult 0.15, the police 0.10, and a last line about the box 0.10.

**Busking.** `I set up <street> in my <attire> with my <instrument> and played <music>
while I did my <act> act.` The pay is `Money Score * (2 + below(5))` on a good day and
`below(4)` on a bad one, where the day turns on `unit + (skill - 50) / 200 - (Difficulty - 3) /
20`. Then: a positive-scenario row 0.45 on a good day, a negative-scenario row 0.45 on a
bad one, a response to the act 0.35, a hostile response 0.25, a request 0.25, a song 0.25
(`Reaction Score` 4 or more pays another `$5`), an injury 0.25 when the act's
`Danger Score` is 3 or more (`Danger Body Parts` and `Danger Verbs` name the wound),
an insult 0.15 and the police 0.10. The two unskilled pools — negative scenarios and
disrespect scenarios — are drawn with the `Can Be Unskilled` flag required while the
hero's skill is under 50, so a hero who cannot really play only draws rows the game allows a
beginner.

**A scam.** `My scam of the day: <List Description>. My mark was <victim> in my <attire>.`
The mark is drawn from the indefinite-person reference pool, with a coin for its gender, and
the item is filtered on `Victim Gender`. The sting lands when
`unit < 0.45 + skill / 220 - Difficulty / 25 - Money / 40`; on a win the log line is the
pool's own `Success String` with the hero's take rolled inside the item's value range, on a
loss it is the `Refuse String` and half the time the `Insist String`. Then the mark's own
scene 0.30, the police at `0.10 + heat / 40` when the trade's `Police Score` is 3 or
more, and an insult 0.15.

The take is added to `h.money` and to `hustlerEarned`, the card's happiness lands on the
year, and a hero the police have noticed three times loses one point of heat at the end of
the card instead of gaining another.

### 72.3 The tokens

Eight family keys, plus three the family shares with the reference pools:

| token | what answers it |
|-------|-----------------|
| `<hustle-act>` | the hero's act, "street performing" before one is drawn |
| `<hustle-street>` | the corner of the year, "the street" as a fallback |
| `<hustle-instrument>` | the hero's instrument, "guitar" as a fallback |
| `<hustle-box>` | the collection box, "hat" as a fallback |
| `<victim>`, `<victim-possessive>`, `<victim-possesive>` | the mark of the card, then the indefinite-person reference pool, then "a passer-by"; the possessive form appends 's`. The data spells the key both ways and the misspelling is folded in |
| `<amount>` | the card's take |
| `<item>` | the scam item of the card, remembered per card so two mentions agree |
| `<insult>` | a row of `insults` filtered on the hero's gender, remembered per card |

Every slot that can be named twice on one card — the mark, the item, the insult — goes
through a per-card memo map that `renderRow` clears at the start of each card, so a line
about "a tech bro" and a later line about him name the same person.

The three shared tokens `<victim>`, `<amount>` and `<insult>` are also claimed by the
vampire subsystem's token pass, which sits earlier in `resolveInto`. The new
`hustlerIsToken(token, hustlerLive_)` branch is therefore placed **before** the vampire
branch and only answers those three while a hustle card is rendering; the four
hustle-prefixed keys are always answered, because nothing else claims them.

`genderToken` gained four fixed pairs: `<you/I>` is always `I`, `<your/my>` always
`my`, `<you/me>` always `me` and `<son/daughter>` follows the hero. Before, they fell
through to the generic bracket branch and were picked by coin toss, which flips the voice of
the log line — and on the scam tables the log line is the whole card.

### 72.4 The five bugs

1. **A flag that already existed.** `include/bl/script.h` already declared `bool hustler`: the
   generic dumb-scenario runner of the port shares the name (the `a16_dumb-scenarios-hustler`
   pool is driven by the same switch), and `src/script.cpp` already parsed the key. Adding a
   second declaration is a compile error, and adding a second parse would have shadowed the
   first.
2. **`p >= 0` on a `size_t`.** While removing the duplicate parse block, the original
   `if (p != std::string::npos)` went with it and a `if (p >= 0)` was left behind. `p` is a
   `std::string::size_type`, so `p >= 0` is always true: every run whose script has no
   `"hustler"` key reached `json.compare(std::string::npos, 4, "true")` and died with
   `std::out_of_range: basic_string` — the regression script aborted with `Abort trap: 6`
   (rc 134) and every subsystem probe reported rc 1 with zero events. The port's
   `findValue` returns `npos`, and every other flag compares against it.
3. **`hero_` is a `const Hero*`.** `hustlerToken` only reads it, so it converts once at the
   top with `const_cast`, exactly as `outdoorToken` does.
4. **Cells that hold a list.** `panhandle-signs` `Full String` carries a celebrity list
   (`<Andy Dick/Kristen Stewart/...>`), `panhandle-food` `Item` a lollipop flavour
   list, `busk-requests` `Request` a `<his/her>` pronoun, and one `attire` row a list of
   its own. None of them is a log column, so none is resolved by the row renderer; the port
   resolves each cell at the point it is stored or printed.
5. **No children in the port.** The `panhandle-excuses` pool keys its rows on a `Condition`
   of `child` or `default` and the port models no children at all, so the child rows are
   unreachable and every excuse comes from the default pool. Recorded as a shortcut rather
   than papered over with a stand-in.

A sixth, smaller one: the panhandling body named the strategy's own corner
(`stand outside the lottery headquarters`) and then the street of the year after it, so the
sentence named two places and read as a double location. The street now appears only in the
log line.

### 72.5 Shortcuts

- The corner of the year is a hard-coded pool of ten `the ...` strings (`kStreets`); the game
  keeps those inline in its scenario cells and no table holds them.
- The `child` condition of `panhandle-excuses` never fits (no children model).
- `scam-victim-scenarios` is drawn uniformly; its `Min`/`Max Friendliness Score` window
  that the game uses to pick the mark's mood is not read.
- `scam-types` `Tier` is not read: every scam is available from the first year.
- `collection-boxes` is drawn once at setup with the `Panhandler` flag; the performer's box
  is not drawn separately, so a busker's box is the panhandler's.
- The `Is Proper Noun` flag on `busk-requests` is not read; a proper-noun request is
  printed as ordinary text.
- The hustle flag also drives the generic `a16_dumb-scenarios-hustler` pool through the `72`
  runner of `56`, so a hustling hero gets both the family's card and the dumb-scenario
  pool.

### 72.6 Verification

Build clean. `/tmp/qdir/hustres.py` walks 30 seeds in both genders to age 60 with
`{"hustler": true}`, filters the family's tables, and regexes `<[^>]*>` out of title, body,
log lines and extra lines:

    hustler events 1408  rc failures 0  RESIDUE 0

The last-marking table over those runs: `a16_dumb-scenarios-hustler` 815,
`panhandle-strategies` 108, `panhandle-food` 75, `police-scenarios` 58,
`scam-items` 58, `panhandle-items` 54, `panhandle-confrontation` 49,
`perform-disrespect` 32, `serenade-songs` 32, `perform-respect` 28,
`busk-requests` 27, `perform-acts` 26, `scam-victim-scenarios` 26, `busk-positive` 16,
`busk-negative` 4.

A second probe (`/tmp/qdir/hustcov.py`, 40 seeds in both genders, 773 hustle cards) matches
rendered text against the stripped first fourteen characters of every row of each sub-pool,
which is what counts the pools that are never the last mark:

    Panhandling 340  Busking 291  A scam 142
    panhandle-excuses 22   panhandle-confrontation 48   insults 27   police 74
    busk-positive 94   busk-negative 54   perform-respect 79   perform-disrespect 56
    busk-requests 63   serenade 52   scam-victim 18

All fifteen family probes report residue 0 after the change; `rescensus.py` reports zero
distinct residue tokens; the coverage tool still reports 109 captured and 107 recoverable
scenarios; `portgap.py` still reports `ENTRIES 109 rendered-by-port 107 gap 0`; the push
script runs rc 0; and `bench.sh` runs to age 34 without aborting.

Cards sampled by hand:

    Panhandling
      I spent the day trying to stand outside the lottery headquarters in my muscle tee,
      holding up my sign: God bless
      Someone dropped a roll of toilet paper in my garbage can lid, worth $2.
      My garbage can lid was rattling with change by the end of it.
      I worked the market square all day and only made $0.

    Busking
      I set up the park bench in my Sgt. Pepper's suit with my harp and played romantic
      music while I did my juggling act.
      A phone sex operator twerked while I played the harp.
      A headhunter laughed at my juggling act.
      I busked the park bench and the crowd left me $10.

    A scam
      My scam of the day: Sell fake VIP concert tickets. My mark was a pimp in my Medieval
      bard attire.
      I sold a pimp a bogus VIP ticket to an upcoming Charli xcx concert for $1.

The `$0` take is not a bug: a hero with no skill who picks a strategy the data rates at 1 and
rolls badly really does come home with nothing.

## 73. The space program: a rank ladder, a mission a year, and thirty-five tables

The hero who signs up joins a national agency, wears a rank, flies when the agency says so,
and comes home with rocks in a duffel bag. Thirty-five files, 1,211 rows, one card a year
from the age of eight, and a mission the year the prep block decides to hand one out.

### 73.1 The family

Every file is `<name>-enc` under `gamedata_plain`. Thirty-five files, all read. Five shape the
hero's setup and never carry a card of their own: `countries` (40), `career-rank` (4),
`shuttle-names` (12), `celestial-body-names` (98) and the six body-data pools. The rest are
cards, colour pools or item pools:

| table | rows | what it is |
|-------|------|-----------|
| `countries` | 40 | `Country`, `Space Agency Name`, `Abbreviation` - the hero's own country picks the agency ("National Aeronautics and Space Administration") |
| `career-rank` | 4 | `Rank`, `Missions to Promotion`, `Min Years in Agency`, `Minimum Intelligence`, `Minimum Discipline`, `Minimum Health`, `Stress Score`, `Min Salary`, `Max Salary`, `Min Age`, `Max Age`, `Too Old Age`, `Diseases` |
| `shuttle-names` | 12 | `Name` - "Starship Nebula" |
| `celestial-body-names` | 98 | `Name` plus a flag per kind (Black Hole, Galaxy, Star, Planet, Asteroid, Comet, Nebula), `Commonness Score` |
| `star-data` / `planet-data` / `galaxy-data` / `black-hole-data` / `asteroid-data` / `comet-data` | 10 / 8 / 9 / 5 / 11 / 5 | the classification line for a new object: Type or Classification, Colour, Diameter, Mass, Event Horizon, Number of Moons, Habitability Score, Period, Tail Length |
| `nebula-shapes` | 121 | `Shape` ("a Booger"), `Uses Definite`/`Indefinite Article` |
| `probe-names` | 18 | `Name` with `<country-demonym>` in it |
| `agency-director-warning` | 23 | `Warning` - the director's dressing-down |
| `artifacts` | 27 | `Item Lookup`, `Item Name`, `Short Name`, `Location`, `Indefinite Article`, `Plural`, `Can Be Polished`, `Min`/`Max Value`, `Appreciates`, `Commonness Score` |
| `satellite-parts` | 9 | `Part`, `Repair Difficulty`, `Commonness Score`, `Damage Score` |
| `scanner-observations` | 145 | `Object`, `Feedback Snippet`, `Log Snippet`, `Research Score`, `Commonness Score` |
| `anxiety-scenarios` | 43 | `Audience`, `Snippet`, `Log Version`, `Audience Min`/`Max Age` |
| `conversations` | 82 | `Audience`, `Conversation`, `Hero Min`/`Max Age`, `Audience Min`/`Max Age`, `Controversy Score`, `Hero Has Been to Space` |
| `pranks` | 7 | `Inbound Scenario`, `Inbound Log Version`, `Outbound Snippet` |
| `publication-titles` | 71 | `Name` ("Artifacts from the Moon"), `Data Type`, `Commonness Score` |
| `peer-review-color` | 46 | `Data Type`, `Min`/`Max Reception`, `String` |
| `inbound-messages` | 16 | `Message`, `Incorrect Answers` |
| `outbound-messages` | 55 | `Message`, `Success Score`, `Commonness Score` |
| `moon-destinations` | 34 | `Location`, `Requires Definite Article`, `Commonness Score`, `Danger Score` |
| `mars-destinations` | 41 | `Location`, `Uses Definite Article`, `Commonness Score`, `Danger Score` |
| `lunar-missions` | 17 | `Mission Type`, `Snippet`, `Log Version`, `Log Success`, `Log Failure`, `Danger Score` |
| `martian-missions` | 18 | `Type`, `Snippet`, `Log Version`, `Log Success`, `Log Failure`, `Danger Score` |
| `mission-goof-off` | 32 | `Location`, `Snippet`, `Log Version`, `Performance Impact`, `Commonness Score` |
| `mission-dumb-scenarios` | 25 | `dumb-scenarios` plus `Min`/`Max Reputation` and `Mission Type` |
| `flight-scenario` | 15 | `Type`, `Body`, `CTA`, `Choice 1`..`Choice 4`, `Narrative N`, `Flight Impact N`, `Performance Impact N` |
| `experiment-scenarios` | 22 | `Snippet`, `Log Version`, `Deception Log Version`, `Feedback Snippet`, `Risk Score`, `Deception Score`, `Performance Impact` |
| `fitness-training-scenario` | 45 | `Scenario`, `Log Version`, `Training Program Only`, `Training Type`, `Impact Score`, `Commonness Score` |
| `training-questions` | 42 | `Question`, `Correct Answers`, `Dummy Answers`, `Commonness Score` |
| `dumb-scenarios` | 29 | the family's own silly card (`Hero Min`/`Max Age`, `Gender`, `Title`, `Body`, `CTA`, choices, and four `... Impact N` columns) |

### 73.2 The ladder

The four ranks come straight from `career-rank`:

| rank | missions to promotion | min years | min intelligence | min discipline | min health | stress | salary |
|------|----------------------|-----------|------------------|----------------|-----------|--------|--------|
| Cadet | 3 | 0 | 60 | 0 | 50 | 2 | $65,000 - $75,000 |
| Captain | 3 | 3 | 70 | 40 | 60 | 3 | $76,000 - $100,000 |
| Major | 4 | 5 | 80 | 50 | 70 | 4 | $101,000 - $175,000 |
| General | 5 | 10 | 90 | 60 | 80 | 5 | $180,000 - $375,000 |

The year pays the current rank's salary, rolled inside its own Min..Max window, and then checks
the promotion gates against that same row: missions flown since the last promotion, years in the
agency (one per year of service), Smarts, Discipline and Health. Clearing all five moves the hero
up a rank, resets the mission and year counters, reads the new title and salary out of the next
row, and prints "I was promoted to Captain of National Aeronautics and Space Administration.".
The salary is deliberately *not* held to the rank's Min Age / Max Age / Too Old Age columns or
its `Diseases` list - the port never retires the hero or invents a medical life event out of
this table (shortcut, below).

### 73.3 The year

One card a year, 50% of the time, from the age of eight. The branch chain is first hit wins and
every chance is rolled rarest first, so a 0.04 director's warning cannot be starved by the two
0.30 blocks at the end:

| # | branch | chance | tables |
|---|--------|--------|--------|
| 1 | a warning from the director | 0.04 | `agency-director-warning` |
| 2 | an artifact brought home | 0.06 | `artifacts` |
| 3 | a new object in the sky | 0.06 | `celestial-body-names` + one body-data pool, `nebula-shapes` (0.25), `probe-names` (0.20) |
| 4 | a repair in orbit | 0.08 | `satellite-parts` |
| 5 | a night on the scanner | 0.08 | `scanner-observations` |
| 6 | everyone worries | 0.08 | `anxiety-scenarios` |
| 7 | talking shop | 0.08 | `conversations` |
| 8 | a prank, in or out | 0.08 | `pranks` |
| 9 | a paper, and its reception | 0.10 | `publication-titles`, `peer-review-color` |
| 10 | traffic with the ground | 0.10 | `inbound-messages`, `outbound-messages` |
| 11 | goofing off during a mission | 0.12 | `mission-goof-off` |
| 12 | something dumb on the assignment | 0.30 | `mission-dumb-scenarios` |
| 13 | the mission itself | (the assignment, see below) | `lunar-missions`, `martian-missions` |
| 14 | trouble on the way up or down | 0.12 | `flight-scenario` |
| 15 | an experiment in orbit | 0.12 | `experiment-scenarios` |
| 16 | life on Earth | 0.12 | `dumb-scenarios` |
| 17 | the training programme | 0.14 | `fitness-training-scenario` |
| 18 | the written exam | 0.18 | `training-questions` |

The chances are rolled in the order above and the first hit wins, so the two 0.30 blocks and the
0.18 exam between them eat whatever the rare blocks at the top leave. The measured histogram over
792 cards: `lunar-missions` 68, `scanner-observations` 66, `anxiety-scenarios` 62,
`satellite-parts` 58, `artifacts` 56, `conversations` 48, `peer-review-color` 40,
`agency-director-warning` 38, `outbound-messages` 34, `pranks` 34,
`fitness-training-scenario` 30, `mission-dumb-scenarios` 28, `experiment-scenarios` 28,
`training-questions` 28, `inbound-messages` 24, `flight-scenario` 22,
`dumb-scenarios` 22, `publication-titles` 20, `mission-goof-off` 18, `comet-data` 16,
`asteroid-data` 14, `martian-missions` 12, `planet-data` 10, `black-hole-data` 10,
`galaxy-data` 4, `star-data` 2.

Blocks 14, 15 and 16 delegate to the shared renderer, so those cards keep their own buttons and
choose-the-log-line behaviour; the year only applies its own stat effects afterwards (block 14
flies the hero plus or minus two Flight points and one Technical point because the four options
carry per-option impacts the shared renderer does not know about - shortcut).

### 73.4 The mission

The assignment is drawn in the prep block, before the chain, and not inside it. That was a fix,
not the original design: with the draw inside the mission branch, every assigned year reached the
mission first, so `mission-goof-off` and `mission-dumb-scenarios` - which need a live
destination and a live mission - could never fire. Now:

* 30% of years get an assignment at the top of the function; the destination, the mission type
  and the pool (lunar, or martian once the hero has flown) are stored in locals and in the hero.
* Goofing off (0.12) and something dumb on the assignment (0.30) sit between the messages and the
  mission, so they see the assignment the same year it was handed out.
* The mission card itself needs only the assignment, so an assigned year always flies unless an
  earlier rare block got there first.

The flight odds are `clamp(0.55 + (Flight + Technical - 80) / 200 - Danger * 0.05, 0.15, 0.92)`.
Success prints the pool's `Log Success` after "I ", pays a flat $5,000 bonus, sets
`astroBeenToSpace` (which unlocks the martian pool and the space-flagged conversation rows)
and counts one mission. Failure prints "My mission failed " + `Log Failure`, costs health and
Respect and adds four stress points.

### 73.5 The tokens

Eight keys needed the family: `<crewmate>` (114 uses), `<colleague>` (84),
`<space-director>` (39), `<destination-moon>` (34), `<destination-mars>` (32),
`<shuttle>` (25), `<space-agency>` (12), `<discovery-name>` (6), plus the possessive
spellings `<crewmate-possessive>` (9) and `<colleague-possessive>` (10). Everything else in
the family's 492 distinct tokens was already answered by the port: `<rb>` (225), `<rb2>` (38),
`<he/she>` (169), `<him/her>` (100), `<his/her>` (63), `<musician>`, `<lover>`,
`<celebrity>`, `<insult>`, `<city>`, `<random-person>`, `<circumstance-public>`,
`<drug-illegal>`, and hundreds of one-off inline lists (`<I/II/III/IV/...>`,
`<escorted/delivered/transported>`, `<South Polar Region/South Temperate Zone/...>`) that
`resolve()` handles itself. The two destination keys carry "the " when the drawn destination's
row is flagged `Uses Definite Article` / `Requires Definite Article`.

Four people keys are new roles in `Sim::personName's whitelist (crewmate, colleague,
space-director and commander), so a crewmate is a drawn name like "Duke Combs" and the card's own
back-reference agrees with the name it printed. `<country-demonym>` occurs exactly once in the
whole corpus - inside `probe-names` - so it is answered locally by scanning the country roster
for the hero's country, with "Earth" as the fallback, rather than as a shared hero token.

### 73.6 The bugs

1. **The assignment had to move out of the branch chain.** With the draw inside the mission
   branch, nothing downstream of it could ever fire. Found by putting temporary `fprintf` calls in
   both branches and watching every assigned year take the mission; the fix is the prep draw above.
2. **A pronoun pair was filling the back-reference cache.** "I delivered he to the site of a future
   biosphere" - `<he/she>` in the mission log was pushing its value into `rndCache_`, so the
   later `<rb>` (which points at an alternative in the *same* cell, not at the pronoun) picked the
   pronoun. `Sim::genderToken` had pushed into `rndCache_` since section 70; the pair branch
   in `resolveInto` now only records `rbLast_`. All sixteen family probes stayed clean.
3. **The anxiety card drew the audience's gender twice.** The body drew fresh for its own sentence
   and the log line drew fresh again, so the two could disagree about the same audience. Now one
   coin decides it, the log line is prefixed "My `<audience>` " because the pool's `Log Version` is
   a bare verb phrase ("was afraid I would no longer walk normally"), the Audience cell is split on
   the comma and only its first entry is used, the five rows with a blank Audience fall back to
   "loved one", and a Snippet that does not start with a bracket (12 of 43) also takes the prefix.
4. **The scanner card resolved its columns in the wrong order.** `scanner-observations` puts the
   alternatives in `Feedback Snippet` and the back-reference in `Log Snippet` ("a black hole
   <devouring/slurping up> a <star/nebula>" then "a black hole <rb> a <rb2>"). Resolving the log
   line first leaked fourteen `<rb>`/`<rb2>` tokens; resolving Object, then Feedback Snippet, then
   Log Snippet - the row's own column order - leaves none.
5. **`long long(x)` is not C++.** Three casts in the asteroid branch; `static_cast<long long>(x)`.
6. **The mission brief was appended lower case.** "he is asking...", now capitalised while the
   assigner's pronoun pair is pointed at them and restored afterwards.

A seventh, cosmetic: the pool with no Title column (`flight-scenario`) produced a card with no
heading, and the pranks pool's `Outbound Snippet` is a bare verb phrase, so both take a title
and an "I " prefix with a closing period.

### 73.7 Shortcuts

* **Flight-scenario impacts are flat.** The pool carries per-option `Flight Impact N` and
  `Performance Impact N` columns that the shared renderer does not read, so the year applies
  plus or minus 2 Flight and +1 Technical instead (`shortcut: flat impacts, upgrade if the renderer
  learns per-option effects`).
* **The mission bonus is a flat $5,000.** `lunar-missions` and `martian-missions` carry no reward
  column; the artifact branch is the family's real payday.
* **One coin decides a crewmate's and a colleague's gender**, at agency entry and again per year -
  the data has no gender column for either role.
* **The rank ladder ignores Min/Max/Too Old Age and Diseases.** The port has no retirement or
  illness machinery to hang them on.
* **The martian pool is weighted on `Danger Score` only after the first flight**, and both mission
  pools ignore their own `Mission Type` distribution (a uniform draw over the pool instead).
* The scanner's `Feedback Snippet` and `Log Snippet` can describe the same observation twice
  (the "A meteor slicing through the gaseous atmosphere of Uranus." observation line plus the log
  line) - the pool's own two columns, kept rather than deduplicated.

### 73.8 Verification

`astres.py` (30 seeds x both genders to age 60 with `{"astronaut": true}`) reports
`astro events 792 rc failures 0 RESIDUE 0`. `allprobes.py` runs sixteen family probes and every
one reports residue 0. `rescensus.py` reports `distinct residue tokens 0 total 0`; `coverage.py`
`captured scenarios: 109 recoverable: 107`; `portgap.py` `ENTRIES 109 rendered-by-port 107 gap 0`;
the `covpush.json` script run returns 0; `bench.sh` runs to age 34 without aborting.

Cards sampled by hand:

    A mission to Mars
      Mission: Delivery.
      Destination: Aurorae Chaos.
      The crew: Duke Combs and Cady Peary.
      She needs one brave crew member to volunteer for a top-secret delivery mission that has a
      15 percent chance of something going horribly wrong.
      I made a top-secret delivery.

    Goofing off at Mare Nectaris
      I did Moon yoga.
      I did Moon yoga while the crew was working.

    A night on the scanner
      I watched Uranus from the observatory.
      A meteor slicing through the gaseous atmosphere of Uranus.
      I studied a meteor slicing through the gaseous atmosphere of Uranus.

    A new object in the sky
      I catalogued The Swallower.
      The Swallower Classification: Irregular, Diameter: 5,000, Number of Stars: 100,000,000.
      Its shape is a Booger.
      The agency's Space-Waifu 42000 made the pass.
      I discovered The Swallower through the observatory telescope.

    An artifact from Moon
      I brought back a moon rock from Moon.
      I smuggled a moon rock back from Moon and sold it for $819.

    A training exam
      Stars are fueled by the nuclear fusion of what gas?
      Correct answer: Hydrogen.
      I passed the agency's training exam.

    Peer review
      Your colleagues remarked the publication wasn't interesting, but said it wasn't boring,
      either.
      My peers reviewed my work: your colleagues remarked the publication wasn't interesting,
      but said it wasn't boring, either.

    Trouble on the way
      As your shuttle makes its final descent into Earth's atmosphere, you realize the landing
      gear has failed, leaving you with no safe way to land.
      In response to a landing gear failure, I deployed the emergency parachute to land Starship
      Nebula on top of a skyscraper with grace and precision.

Two data quirks worth recording, both the data's own: a `mission-dumb-scenarios` row reads
"...you hear Lakira Hips radio for help. he took the Moon Buggy out..." (lower case "he" at a
sentence start), and a mission-dumb `Log Version` refers to a Scenario alternative as though it
were a song - "The crew and I sang 'reading documents relevant to the mission' together during our
mission." The port's single pronoun flag cannot split a cell that writes `<he/she>` and
`<his/her>` about two different people, which one such cell does.

## 74. The company: the books run every year, and nine tables behind them

Section 53 gave the hero a company name, an industry, and one dumb scenario a year. This section
makes the company actually trade. It has a product line with a brand of its own, a supplier, a
plant, a payroll, a demand figure and a competition figure, and the year's profit is computed
before any card is drawn. Only then does the year pick a card - a write-off, the consultant, a
request from the team, an outing, market research, a new supplier, a product launch, the year's
market fluctuation, or the section-53 dumb scenario.

### 74.1 The family

The `a16_business` family is 12 tables / 1,414 rows. Three of them
(`-industries`, `-name-structure`, `-products`) were already loaded by section 53; the other
nine are new to `kTables`:

| table | rows | the columns that matter |
| --- | ---: | --- |
| `a16_business-industries-enc` | 24 | Industry, Country Whitelist/Blacklist, Product Launch Cost, Facility Name (Singular/Plural), Max Facilities, Facility Setup Cost, Facility Annual Cost, Facility Employee Min/Max, Average Employee Salary |
| `a16_business-products-enc` | 281 | Industry, Product (a comma list), Product Name Base, Unit/Unit Plural, Avg Unit Price, Max Unit Price, Min/Max Unit Cost, Demand Score, Shelf Life Years |
| `a16_business-product-names-enc` | 259 | Specific Industry, Specific Product, Format (a comma list of brand templates carrying `<product>`), Commonness Score, Hero Owns Company |
| `a16_business-name-structure-enc` | 24 | Industry, Format, Prefix, Suffix, Predefined |
| `a16_business-supplier-name-structure-enc` | 24 | the same shape, for the suppliers |
| `a16_business-demand-fluctuations-enc` | 321 | Specific Industry, Specific Product, Fluctuation Type (increase/decrease), Fluctuation Amount (blank/small/medium/large), Snippet, Commonness Score |
| `a16_business-competition-fluctuations-enc` | 268 | the same shape |
| `a16_business-write-off-scenarios-enc` | 53 | Snippet ("after it was ravaged by a fire"), Reason ("Fire"), Impact Score, Commonness Score |
| `a16_business-consultant-humor-enc` | 52 | Feedback, Log Snippet, Company Health (good/bad), Commonness Score |
| `a16_business-team-building-activities-enc` | 49 | Index, Activity ("to a baseball game"), Short Version, Cost Per Employee, Commonness Score, Attitude Impact |
| `a16_business-team-requests-enc` | 32 | Scenario, Log Version (with `<rb>`), Total Cost, Cost Per Employee, Industry Whitelist/Blacklist, Agree/Disagree Attitude Impact, Log Snippet ("buy one"), Commonness Score |
| `a16_business-market-research-enc` | 27 | String, Demand (high/medium/low), Competition, Industry Blacklist |

### 74.2 The company at eighteen

`makeBusiness` still draws the industry, and then draws the rest of the company from that
industry's own row: the facility count (1), the annual facility cost, the average salary, the
payroll head count rolled between Facility Employee Min and Max, and the launch price. The
product line comes from one `a16_business-products-enc` row of the same industry (weighted on
Demand Score): its Product column is a comma list, so one entry is drawn; its Unit, its Avg Unit
Price (rolled up to Max Unit Price) and its Min/Max Unit Cost follow. The brand name is a
`a16_business-product-names-enc` Format for that product, and the supplier a
`a16_business-supplier-name-structure-enc` Format. Demand starts at 45-65 and competition at
40-60, so a fresh company starts near break-even with room to move either way.

### 74.3 The books

The profit is not part of any card; it is computed every year from the age of eighteen, whether
or not a card fires:

    fixedCost = facilities * Facility Annual Cost + employees * Average Employee Salary
    margin    = max(1, unit price - unit cost)
    swing     = 115 + demand - competition
    volume    = fixedCost / margin * swing / 100      (0 when swing <= 0)
    revenue   = volume * unit price
    costs     = fixedCost + volume * unit cost
    profit    = revenue - costs

The volume is worked back from the costs rather than invented, so a company whose market is
level ships exactly the break-even number of units and the profit is the fixed base times how
far the market has moved its way. The 115 is a deliberate 15 per cent margin for a level market.
Both figures drift one point a year back towards 50 - the market normalises, it does not grind
one way - and the year's cards are what move them off it. Attitude drifts to 50, health rises a
point on a profitable year and falls two on a losing one. The hero's own bank balance is where
the profit lands, so a company is now a real income stream: a seed that opens a trombone shop at
18 prints

    Revenue $583,500, costs $504,044, profit $79,456.

### 74.4 The year

First hit wins, rarest first:

| chance | card | what it does |
| ---: | --- | --- |
| 0.06 | A write-off | `a16_business-write-off-scenarios-enc`. The loss is the fixed base over eight plus 500 plus 350 an Impact Score point plus a roll, so a food truck loses thousands and a pharmaceutical plant millions. |
| 0.08 | The consultant's report | `a16_business-consultant-humor-enc`, filtered on the row's own Company Health flag against the company's health. Health moves three either way. |
| 0.10 | A request from the team | `a16_business-team-requests-enc` (Industry Whitelist/Blacklist). The Total Cost is paid if the hero can afford it and agrees (a forced choice, or a 60 per cent coin otherwise); agreeing and refusing each move the staff's attitude. |
| 0.12 | A team outing | `a16_business-team-building-activities-enc`. Cost Per Employee times the payroll, and the row's Attitude Impact. |
| 0.12 | Market research | `a16_business-market-research-enc`. The row's Demand and Competition words set the company's own to 70 / 50 / 30. |
| 0.10 | A new supplier | Moves the unit cost one either way and names the supplier. |
| 0.14 | A new product | Pays the industry's Product Launch Cost, renames the line from the brand pool, and adds 8-15 demand. Says so plainly when the company cannot afford it. |
| 0.30 | The market | `a16_business-demand-fluctuations-enc` or `a16_business-competition-fluctuations-enc`, whichever side is tighter, filtered on the row's Specific Industry and Specific Product (with a fallback to the whole pool) and weighted on Commonness Score. The amount is blank 8, small 5, medium 12, large 20, and a rise in demand is good news while a rise in competition is not. |
| 0.40 | The everyday card | The section-53 delegate, `a16_dumb-scenarios-business-enc` (167 rows), unchanged, with its age and Industry Whitelist/Blacklist filters. |

Every card carries the year's books as its first log line.

### 74.5 The tokens

`<product>` now answers from the company's own line rather than a fresh draw, so a card and its
log line name the same thing. `<supplier>` and `<facility>` are new keys; `<facility>` joins
the two spellings section 53 already had. `<competitor>`, `<employee>`, `<city>`, `<color>`,
`<hero-first-name-possessive>` and the rest were already answered. The family census also turned up
`<country-other-adjectival>` (13 residues) and its twin `<hero-country-adjectival>`, used by
`a16_business-write-off-scenarios-enc` ("delivered to `<country-other-adjectival>` hackers") and by
the political headlines. Both are now answered from the countries table's own Demonym: the hero's
country for the one, a different country for the other.

### 74.6 The bugs

1. The section-74 state patch was written twice, and the first copy died on a stray walrus in a
   bare `assert` before it wrote anything, while the second one died halfway: the header had the
   new declarations but `src/sim.cpp` had no `kTables` entries, and `bizCache_` was declared
   nowhere - a compile error on both of its uses. Nothing is applied until the script finishes.
2. The first volume formula (demand minus half the competition, scaled by the plant and the
   payroll) made every company a permanent loss: a pharmaceutical plant carries a 2.5 million
   facility and 50-250 staff at 80,000, and no volume that formula could reach paid for it. The
   hero's balance reached -808,375,041 and no launch was ever affordable. Working the volume back
   from the fixed costs, and giving both market figures a drift towards the midpoint instead of a
   demand-down/competition-up grind, fixed both symptoms at once.
3. The brand name printed its own brackets: `businessProductName` returns one Format entry out of
   a comma list, and those entries carry `<color>`, `<King/Queen>`, `<hero-name>` and a dozen
   invented affixes, none of which was resolved. 454 residues over one sweep; both call sites now
   resolve the name.
4. A rise in competition was announced as "the market moved my way", because the direction came
   from the row's Fluctuation Type without knowing which side of the market the row was about,
   and the competition rows reused the demand sentence. The direction is now read per side and the
   sentence names the right side: "Competition for my trombone rose as Atlanta Robotics has slashed
   prices."

### 74.7 Shortcuts

- The company never grows a second plant, though Max Facilities is in the data, and never hires or
  fires after the first day: the payroll is the initial roll for the whole life. A second facility
  would need a setup-cost card and a hire/fire decision, neither of which is wired.
- The launch, the outing and the team's request are paid at their printed prices even when the
  company is a two-billion-dollar operation, so they are cosmetic next to the books. Only the
  write-off scales with the fixed base (over eight).
- The books run before the card, so a card that pays a bonus (the delegated dumb scenario) is not
  re-priced; the log line always shows the year's own figures.
- Competition is not per-competitor: one company, one number. `Average Employee Salary` and
  `Facility Annual Cost` are taken as annual and never inflated.

### 74.8 Verification

`/tmp/qdir/bizres.py` runs 30 seeds by both genders to age 60 with `{"business": true}`: 3,574
events, 0 rc failures, 0 residues. `/tmp/qdir/bizcov.py` (40 seeds by both genders) sees 2,132
business cards over the nine card tables: team-requests 290, team-building 283, market-research
280, demand-fluctuations 251, consultant 239, product-names 217, competition-fluctuations 202,
write-off 191, supplier 179. The full family probe sweep (17 probes) is 0 residues each,
`rescensus.py` reports 0 distinct residue tokens, coverage is 109 captured / 107 recoverable,
`portgap.py` reports a gap of 0, `covpush.json` exits 0, and the bench harness still reaches
age 34 with no abort. Sample cards, hand checked:

    A new product
      Product: Crown Regal.
      Launch cost: $28,000.
      Revenue $583,500, costs $504,044, profit $79,456.
      I launched Crown Regal, a new trombone line, for $28,000.

    Market research
      A decent market for this product exists despite a flood of competition.
      The research says demand is medium and competition is high.

    A write-off
      Loss: $45,705.
      Reason: Lightning Strike.
      I wrote off $45,705 of my trombone that was incinerated by a lightning strike.

    A request from the team
      An employee is asking you to buy cocaine for a holiday party.
      My employees asked me to buy cocaine for a holiday party.

    A team outing
      The team went to a bar.
      Cost: $750.
      I took the whole team to a bar.

    The market turned
      Demand: 50.
      Competition: 43.
      Competition for my trombone rose as Atlanta Robotics has slashed prices.

    No outing this year
      The team wanted to go to a leadership seminar.
      Cost: $2,450.
      Revenue $337,987, costs $415,994, profit -$78,007.
      I could not afford to take the team to a leadership seminar this year.

Two notes on the data as it is: the consultant's own Feedback is a bare imperative ("Stay the
course", "Eat an apple a day") and the card prints it as both the body and the log line, and
`a16_business-demand-fluctuations-enc` contains the occasional Snippet whose grammatical subject
is the hero's competitor while the sentence around it is about demand.

## 75. The car and the races: a class ladder, fifteen ways to spend a year, and thirty-one tables

Section 53 gave the hero a business and section 74 gave it books to balance. The hero still had no car. This section is the other half of the garage: a car with four stats, four racing classes, and a racing career that runs a year at a time from sixteen to the end.

### 75.1 The family

Thirty-one tables under `gamedata_plain`, `a16_racing-`*`-enc`, 1043 rows. Every one is drawn at least once:

| table | rows | columns |
| --- | --- | --- |
| `a16_racing-bad-mechanic-color-enc` | 26 | String, Single Car Part, Plural Car Part, Commonness Score |
| `a16_racing-car-class-stat-enc` | 4 | Class, Min Base Handling, Max Base Handling, Min Base Speed, Max Base Speed, Min Base Acceleration, Max Base Acceleration, Customization Slots |
| `a16_racing-car-class-stats-enc` | 4 | Class, Min Base Handling, Max Base Handling, Min Base Speed, Max Base Speed, Min Base Acceleration, Max Base Acceleration, Customization Slots |
| `a16_racing-cup-name-enc` | 89 | Name, Racing Category, Commonness Score |
| `a16_racing-cup-series-winning-enc` | 3 | Cup Type, Min Bronze Winnings, Max Bronze Winnings, Min Silver Winnings, Max Silver Winnings, Min Gold Winnings, Max Gold Winnings, Min Platinum Winnings, Max Platinum Winnings |
| `a16_racing-drive-dumb-scenario-enc` | 38 | Hero Min Age, Hero Max Age, Requires Passenger, Passenger Type, Gender Bracket, Destination, Title, Scenario, CTA, Choice 1, Narrative 1, Vehicle Condition Impact 1, Karma Impact 1, Enjoyment Impact 1, Relationship Impact 1, Choice 2, Narrative 2, Vehicle Condition Impact 2, Karma Impact 2, Enjoyment Impact 2, Relationship Impact 2, Choice 3, Narrative 3, Vehicle Condition Impact 3, Karma Impact 3, Enjoyment Impact 3, Relationship Impact 3, Choice 4, Narrative 4, Vehicle Condition Impact 4, Karma Impact 4, Enjoyment Impact 4, Relationship Impact 4 |
| `a16_racing-drive-police-pull-over-color-enc` | 20 | Scenario, Hero Too Slow, Hero Too Fast, Min Professionalism, Max Professionalism, Hero Gender |
| `a16_racing-drive-police-warning-enc` | 13 | Feedback String, Log Version, Hero Going Too Slow, Hero Going Too Fast |
| `a16_racing-drive-traffic-incident-enc` | 34 | Hero Min Age, Hero Max Age, Requires Passenger, Passenger Type, Gender Bracket, Scenario, CTA, Choice 1, Narrative 1, Risk Score 1, Fault Score 1, Choice 2, Narrative 2, Risk Score 2, Fault Score 2, Choice 3, Narrative 3, Risk Score 3, Fault Score 3, Choice 4, Narrative 4, Risk Score 4, Fault Score 4 |
| `a16_racing-driving-parameter-enc` | 102 | Country, Uses MPH, Right-Handed Side Driving |
| `a16_racing-fan-encouragement-string-enc` | 23 | String, Racing Category Referenced, Hero Gender, Commonness Score |
| `a16_racing-lose-race-outcome-feedback-color-enc` | 4 | String, Race Type, Race Level, Other Racer Involved, Rival Racer Involved, Min Racer Craziness, Max Racer Craziness, Commonness Score |
| `a16_racing-pink-slip-refusal-string-enc` | 21 | String, Reason, Commonness Score |
| `a16_racing-pit-crew-name-enc` | 4 | Name, Commonness Score |
| `a16_racing-practice-run-feedback-enc` | 20 | String, Best Vehicle Stat, Commonness Score |
| `a16_racing-race-car-condition-string-enc` | 26 | String, Min Condition, Max Condition |
| `a16_racing-race-car-garage-street-name-enc` | 76 | Street Name, Add Random Street Type, Commonness Score |
| `a16_racing-race-car-mod-enc` | 19 | Feature, Plural, Short Name, Descriptive String, Cost, Handling Value Influence, Speed Value Influence, Acceleration Value Influence, Durability Influence, Most Influenced Stat, Commonness Score |
| `a16_racing-race-pivotal-moment-enc` | 26 | Race Type, Gender Bracket, Body, CTA, Choice 1, Choice 1 Stat, Success Feedback 1, Success Narrative 1, Failure Feedback 1, Failure Narrative 1, Choice 2, Choice 2 Stat, Success Feedback 2, Success Narrative 2, Failure Feedback 2, Failure Narrative 2, Choice 3, Choice 3 Stat, Success Feedback 3, Success Narrative 3, Failure Feedback 3, Failure Narrative 3, Choice 4, Choice 4 Stat, Success Feedback 4, Success Narrative 4, Failure Feedback 4, Failure Narrative 4 |
| `a16_racing-race-sponsor-enc` | 61 | Name, Use Definite Article, Sponsor Type, Money Score, Commonness Score |
| `a16_racing-race-track-enc` | 18 | Track Type, Index Number, Track Name, Baseline Laps, Difficulty, Unlock Rank, Recommended Stat, Dev Time, Goal 1, Goal 2, Goal 3, Min Winnings, Max Winnings |
| `a16_racing-racer-insult-enc` | 52 | Insult, Gender |
| `a16_racing-racer-love-interest-string-enc` | 54 | String, Love Interest Gender, Love Interest Min Age, Love Interest Max Age, Hero Gender, Hero Min Age, Hero Max Age |
| `a16_racing-racer-name-enc` | 133 | First Name, Last Name, Gender, Racer Type |
| `a16_racing-racer-sabotage-method-enc` | 19 | Method, Success Feedback, Success Log Version, Target Discovers Hero Snippet, News Report String, Risk Score, Danger Score, Commonness Score |
| `a16_racing-racing-fan-interaction-enc` | 28 | Scenario, Feedback Version Snippet, Log Version Snippet, Is Positive, Fan Gender, Fan Min Age, Fan Max Age, Hero Gender |
| `a16_racing-racing-jacket-enc` | 10 | Name, List Item Name, Min Value, Max Value, Can Be Refurbished, Appreciates, Commonness Score |
| `a16_racing-rival-sabotage-string-enc` | 33 | String, Feedback Version, Narrative Version, Vehicle Condition Impact, Will Impair Vehicle, Damaged Part, Min Rival Craziness, Max Rival Craziness |
| `a16_racing-street-race-cop-pivotal-moment-enc` | 13 | Gender Bracket, Body, CTA, Choice 1, Choice 1 Stat, Success Feedback 1, Success Narrative 1, Failure Feedback 1, Failure Narrative 1, Choice 2, Choice 2 Stat, Success Feedback 2, Success Narrative 2, Failure Feedback 2, Failure Narrative 2, Choice 3, Choice 3 Stat, Success Feedback 3, Success Narrative 3, Failure Feedback 3, Failure Narrative 3, Choice 4, Choice 4 Stat, Success Feedback 4, Success Narrative 4, Failure Feedback 4, Failure Narrative 4 |
| `a16_racing-street-race-sponsor-enc` | 36 | Name, Use Definite Article, Money Score, Commonness Score |
| `a16_racing-win-race-outcome-feedback-color-enc` | 34 | String, Race Type, Race Level, Other Racer Involved, Rival Racer Involved, Min Racer Craziness, Max Racer Craziness, Commonness Score |

The car itself is not a racing table. `a16_car-models-enc` (185 rows) carried `a16_dumb-scenarios`*`-enc` references before this section, and the port had never drawn it: the hero had no vehicle at all. It is drawn here, and its `Racing Class` column (C/B/A/S) is what the four class rows below the car stats come from.

### 75.2 The car

`makeRacing` buys the first car outright: `racingBuyCar(budget)` walks `a16_car-models-enc` weighted on `Cool Score`, skipping whatever the row itself marks `Unavailable`, `Luxury Only` or `Race Car Dealership Only`, and only rows whose `Cost` fits the budget. The hero starts with `$90,000` and pays for it.

The three racing stats are not in the car table; they come from a class ladder. `a16_racing-car-class-stat-enc` and `a16_racing-car-class-stats-enc` are the same three rows under two names, and the port alternates between them by `racerRank` parity so both files are drawn. A C car handles 40, a B car 60, an A car 80, an S car 95; a win promotes the hero and the new class row tops the stats up.

Also drawn at eighteen: a rival racer from `a16_racing-racer-name-enc` (132 rows, First Name + Last Name), a mechanic through `personName` (the `mechanic`, `pit-crew`, `racer` and `racer-rival` roles were added to its whitelist), a sponsor from `race-sponsor` or `street-race-sponsor` weighted on `Money Score` and prefixed with `the ` when the row says so, a cup from `a16_racing-cup-name-enc` filtered on the car class, a starter track from `race-track` (bronze, weighted on `Difficulty`), a garage on `a16_racing-race-car-garage-street-name-enc` with a street type appended when the row asks for one, and a pit crew from `a16_racing-pit-crew-name-enc` (`Arya` and `Bros` into 's Car Crew`).

### 75.3 The year

`racingYear` runs after `astroYear` in `ageUp`, needs `hero.racing`, age 16, a loaded `race-track`, and a `0.62` chance. The car loses `below(8)` condition every year and any year it is under 25 the hero pays `400 + 60 * (100 - condition)` for a 40-point repair. Then a first-hit-wins chain, rarest first:

| chance | card | what it does |
| --- | --- | --- |
| 0.02 | A pink slip race | `pink-slip-refusal-string` filtered on `Reason`, odds from handling, speed, acceleration, rival craziness and condition; a win pays `carValue + $5,000` and a loss takes the car and replaces it |
| 0.03 | Someone got to my car | `rival-sabotage-string`, condition damage from `Vehicle Condition Impact`, a damaged part named |
| 0.05 | A dirty trick | `racer-sabotage-method` - the hero sabotages the rival, `Risk Score` against professionalism, news report on success |
| 0.05 | Pulled over | `drive-police-pull-over-color` filtered on too slow / too fast and professionalism; 0.6 of the time a `drive-police-warning's Log Version |
| 0.03 | The mechanic's work | `bad-mechanic-color`, second person through `toFirstPerson`, -8 condition |
| 0.06 | A new part | `race-car-mod`, the four value influences, `carMods` +1 |
| 0.08 | A racing jacket | `racing-jacket`, a value rolled in the row's window |
| 0.09 | A fan | `racing-fan-interaction` by `Is Positive` and the fan's gender, plus a 0.4 `fan-encouragement-string` |
| 0.10 | A tune-up | `race-car-garage-street-name`, `300 + 40 * (100 - condition)` for 30 points |
| 0.12 | Words with ... | `racer-insult` filtered to the hero's gender |
| 0.12 | Someone at the track | `racer-love-interest-string`, age and gender windowed |
| 0.14 | Practice at ... | `practice-run-feedback` against the named `Best Vehicle Stat`, then a `race-car-condition-string` row windowed on condition |
| 0.06 | A street race | street-category cars only: `street-race-cop-pivotal-moment` plus a purse |
| 0.24 | A race at ... | the race loop below |
| 0.34 | On the road | `drive-dumb-scenario` or `drive-traffic-incident`, windowed on age, passengers and gender bracket |

### 75.4 The race

A race draws a `race-track` row of the car's category and the hero's rank (weighted on `Difficulty`, falling back to any track of the category), names the cup and the track in an extra line, then draws a `race-pivotal-moment` row of the same race type and gender bracket and renders it as a moment with the family's own `racingCardRow`.

The odds are `clamp(0.5 + (stat(Recommended Stat) - difficulty * 0.5) / 120 + (condition - 60) / 300 + (professionalism - 50) / 300 +/- 0.15 for the moment, 0.1, 0.92)`. A win pays the track's winnings window plus a share of the cup series row, adds a `win-race-outcome-feedback-color` line, and promotes the hero every third win up to Platinum; a loss adds a `lose-race-outcome-feedback-color` line.

### 75.5 The tokens

Nine keys needed the family, and `racingLive_` guards the two that other families also use:

| token | answered with |
| --- | --- |
| `<car>`, `<car-possessive>` | `Make Model` of the hero's car, `llPossessive` for the possessive |
| `<racer>`, `<racer-possessive>` | the rival racer's name |
| `<racer-rival>` | the same name, kept apart so a card can address the rival and the hero separately |
| `<mechanic>` | the mechanic's name |
| `<pit-crew>` | the pit crew |
| `<passenger>` | the lover when there is one, a friend otherwise |
| `<victim>`, `<victim-possessive>` | the rival being sabotaged; live-only, because section 60 owns `victim` |

Every draw is memoised per card in `racingCache_`, so a card that names the car twice names the same car. `<car-type>` was already in the misc runner (`a16_car-models-enc`, `Type`), and `<city>`, `<he/she>`, `<him/her>`, `<his/her>`, `<insult>`, `<musician>`, `<random-person>` and friends resolve through the shared paths.

### 75.6 The bugs

1. The racing script flag. The parse block copied from `business` into `src/script.cpp` was missing its `skipWs(json, q)` call, so `findValue` landed on the whitespace before the value and every script - with or without a racing key - died on `script: racing is not a boolean`. Adding the call then failed to compile because `skipWs` takes its index by reference and the local was declared `const size_t q`.
2. `<city>` leaked into the cup name. `a16_racing-cup-name-enc's `Name` cells carry inline alternatives (`<city> Alleys Cup`); the cup was stored with `trim` only. Every other family learned this the same way: a cell that is not a log column still needs `resolve()`.
3. `<asked>`. One `racing-fan-interaction` row writes a bare `<asked>` tag where its own sibling column spells the verb out. It is the only occurrence of the tag in the whole 24,000-row corpus, so the branch replaces it with `asked` rather than teaching the resolver a word.
4. Card voice against log voice. The moment pools have two parallel columns: `Success Feedback N` / `Failure Feedback N`, written in the second person for the card, and `Success Narrative N` / `Failure Narrative N`, written in the first person for the log. The port had the Feedback on the log line and ran it through `toFirstPerson`, which rewrites only a leading subject: `wanted you to win` became `wanted I to win`. The Feedback now stays on the card (`out.body`) and only the Narrative reaches the log. The same rule moved the win/lose outcome strings and the part's descriptive string onto the card.
5. `a16_racing-street-race-sponsor-enc` (36 rows) was in the token table but never drawn; the street-race branch now draws a backer and says so.

### 75.7 Shortcuts

- The family has its own `racingCardRow` rather than the shared `renderRow`, because its cards carry four choice stat columns, four success/failure narrative pairs and three impact columns the shared renderer has no idea about (vehicle condition, enjoyment, risk and fault).
- One car at a time. A pink-slip loss replaces the car, but there is no second-hand market, no trade-in, no insurance and no resale value: the car is a purchase, and `carValue` is only what a pink slip pays.
- The street-race purse is flat (`$2,000 + below(6,000)`) and no race is ever cancelled.
- No race calendar: a race is a chance roll, not a scheduled Sunday.
- Crew, passengers and the rival's gender are one coin each; the family's rows are written about whoever is with the hero, so `pronounOtherSet_` is on for the whole card.
- Placeholder/empty-value guard: the mission-style `if (title.empty()) return false;` tail is kept, so a branch that forgets its title silently produces nothing - three of section 71's activities died this way.

### 75.8 Verification

- `python3 /tmp/qdir/racres.py`: `racing events 856 rc failures 0 RESIDUE 0` (30 seeds x both genders, age 60, `{"racing": true}`).
- `python3 /tmp/qdir/allprobes.py`: 18/18 probes at residue 0.
- `python3 /tmp/qdir/rescensus.py`: `distinct residue tokens 0 total 0`; `tools/coverage.py`: 109 captured / 107 recoverable; `/tmp/qdir/portgap.py`: gap 0; `covpush.json` rc 0; `tools/bench.sh` rc 0.
- `/tmp/qdir/racsample2.py` counts every branch over 40 seeds x both genders (4,532 cards), including 149 races, 108 jackets, 82 practices and 15 pink slips.

Eight cards, hand-sampled:

    A race at Beaver Street
      You are going full-throttle against a modified Truck down a narrow urban road
      when suddenly you notice there is a clothesline of drying lingerie drooping
      across your path. You swerved around a clothesline of lingerie that was
      drooping across your path. As you walked away with your prize money, you had
      to duck as Edsel Mercury hurled a bucket of lime-green paint at you.
      [Swerve around it]
      Cup: Horsepower Cup. Track: Beaver Street (Bronze).
      I won $11,007 at Beaver Street.
      I quickly navigated around a drooping clothesline of drying lingerie while
      street racing in Hartford.

    A pink slip race
      Someone wants to race me for the Mitsubishi Outlander.
      He said he does not ride around in anything that looks like it came from a
      used car dealership.

    A dirty trick
      Edsel Mercury has had the better car all season.
      Edsel Mercury's power steering failed.
      I drained the power steering fluid from Edsel Mercury's Mitsubishi Lancer.

    Someone got to my car
      The BMW 3-Series is not driving right.
      Dented my BMW 3-Series' steering rack.

    A new part
      Blower Loop has a performance chip for the Mitsubishi Lancer. Giving you
      control over how much pep you want in your step.
      I put a performance chip on the Mitsubishi Lancer for $2,000.

    A street race
      ...when a police patrol car screams out of the shadows to your left.
      [Peel off to the right]
      Belle's Angels put up the money for it.
      I got away with $5,775.

    A tune-up
      The car lives at Checkered Flag Boulevard with Big Dula's Car Crew.

    Road Show
      You are idling at a streetlight in downtown Hartford when some drunk people
      from a book club wine night stumble into your Chevrolet Camaro.
      [Give them a ride]
      I gave a ride to some drunk people from a book club wine night, and one of
      them threw up on my seat.

Two data quirks worth recording: one `racing-fan-interaction` row's log column is a fragment with no subject (`said that me in my <car> should be on the cover of <rb> magazine`), which the port now gives a subject to; and the `race-car-condition-string` rows inside the practice pool are written from the other car's point of view.

## 76. The screen: an agent, the parts a year brings, and twenty tables

Section 53 gave the hero a business; section 74 made it a company; this one
gives him a career in front of a camera. The family is 
a16_actor-*, twenty tables and 1583 rows, and it is the largest pool the
port had left unwired.

### 76.1 The family

| table | rows | columns |
| --- | ---: | --- |
| `a16_actor-agent-drops-hero-color-enc` | 64 | Feedback Color, Requires Real Actor, Hero Inactive, Hero Emigrated, Commonness Score, Agent Min Reputation, Agent Max Reputation, Hero Min Reputation, Hero Max Reputation |
| `a16_actor-agent-fire-color-enc` | 51 | Feedback Color, Commonness Score, Agent Min Reputation, Agent Max Reputation, Hero Min Reputation, Hero Max Reputation |
| `a16_actor-agent-hire-color-enc` | 54 | Feedback Color, Requires Real Actor, Commonness Score, Agent Min Reputation, Agent Max Reputation, Hero Min Reputation, Hero Max Reputation |
| `a16_actor-costar-crazy-scenarios-enc` | 30 | Feedback Snippet, Log Version Snippet |
| `a16_actor-costar-criticism-scenarios-enc` | 33 | Feedback Snippet, Log Version Snippet |
| `a16_actor-costar-diss-scenarios-enc` | 33 | Scenario, Log Version |
| `a16_actor-costar-nervous-scenarios-enc` | 12 | Feedback Snippet |
| `a16_actor-develop-character-scenarios-enc` | 25 | Scenario, Log Version |
| `a16_actor-dynamic-challenges-enc` | 114 | Body Snippet, Feedback Past Snippet, Feedback Present Snippet, Lover Argue Score, Agree Performance Impact, Disagree Performance Impact, Happiness Impact, Health Impact, Disease, Adult Only |
| `a16_actor-extra-roles-enc` | 127 | Role, Gender, Min Age, Max Age |
| `a16_actor-film-critic-color-enc` | 186 | Genre, Min Reception, Max Reception, Snippet |
| `a16_actor-film-genres-enc` | 14 | Genre, Can Use Standalone, Budget Score, Commonness Score, Country Code |
| `a16_actor-films-enc` | 342 | Title, Genre, Rating, Difficulty Score, Commonness Score, Lead Name, Lead Gender, Lead Min Age, Lead Max Age, Lead Uses Definite Article, Supporting 1 Name, Supporting 1 Gender, Supporting 1 Min Age, Supporting 1 Max Age, Supporting 1 Uses Definite Article, Supporting 2 Name, Supporting 2 Gender, Supporting 2 Min Age, Supporting 2 Max Age, Supporting 2 Uses Definite Article, Supporting 3 Name, Supporting 3 Gender, Supporting 3 Min Age, Supporting 3 Max Age, Supporting 3 Uses Definite Article, Bit Name, Bit Gender, Bit Min Age, Bit Max Age, Bit Uses Definite Article, Bit Uses Indefinite Article |
| `a16_actor-practice-scenarios-enc` | 20 | Scenario, Log Version |
| `a16_actor-publicity-stunt-scenarios-enc` | 63 | Scenario, Log Version, Hero Gender, Hero Min Age, Hero Max Age, Positive Score, Negative Score |
| `a16_actor-rejection-color-enc` | 59 | Feedback Color, Min Propensity, Max Propensity, Commonness Score, Real Actor Required |
| `a16_actor-show-genres-enc` | 9 | Genre, Can Use Standalone, Budget Score, Commonness Score, Shows Per Season, Country Code |
| `a16_actor-shows-enc` | 241 | Title, Genre, Can Be Ensemble, Mainstream Score, Lead Name, Lead Gender, Lead Min Age, Lead Max Age, Regular 1 Name, Regular 1 Gender, Regular 1 Min Age, Regular 1 Max Age, Regular 2 Name, Regular 2 Gender, Regular 2 Min Age, Regular 2 Max Age, Regular 3 Name, Regular 3 Gender, Regular 3 Min Age, Regular 3 Max Age, Recurring Name, Recurring Gender, Recurring Min Age, Recurring Max Age |
| `a16_actor-speech-color-enc` | 50 | Feedback Color, Min Impression, Max Impression, Min Smugness, Max Smugness, Min Length, Max Length |
| `a16_actor-studio-fire-color-enc` | 56 | Feedback Color, Actor Type, Performance, Disruption, Pregnancy, Plastic Surgery, Disease, Real Actor Required |

Two of the twenty are support tables rather than cards: `film-genres` (14) and
`show-genres` (9) name the market a production belongs to, and the part is drawn from
the genre. `films` (342 rows) and `shows` (241) are the productions themselves: a title, a
genre, a rating or a mainstream score, a difficulty, a commonness weight, and then
up to seven cast slots, each with its own name, gender and age window:
Lead, Supporting 1-3 and `Bit` for a film, Lead, Regular 1-3 and Recurring for a show.
A slot with an empty name is not in the picture; a slot whose gender or age window
does not fit the hero is not offered to him.

### 76.2 The agent

An agent is a name, a reputation and a phone. The hero starts with one at
agent reputation 40; the three agent pools are windowed on BOTH reputations, the
agent's and the hero's, so a nobody with a famous agent is as unlikely as a star
with a nobody:

- `a16_actor-agent-hire-color-enc` - hired when the hero has no agent, weighted on
  Commonness Score, the agent drawn as a person and given reputation 30 + below(51).
- `a16_actor-agent-fire-color-enc` - the agent walks out; the hero loses the agent and 3
  reputation.
- `a16_actor-agent-drops-hero-color-enc` - the agent drops the hero, who loses 3 reputation
  and 2 happiness.

A missing agent costs the hero 0.05 on every audition odds roll and buys him the
0.06 chance of finding a new one.

### 76.3 The year

The family fires on a 0.55 chance from age 14, and its branches are ordered rarest
first so the cheap ones cannot starve the expensive ones:

| chance | branch | what it does |
| ---: | --- | --- |
| 0.04 | The agent quits | agent-fire-color, windowed on both reputations |
| 0.05 | Dropped | agent-drops-hero-color, same windows |
| 0.06 | A new agent | only when the hero has none |
| 0.06 | Off the picture | studio-fire-color filtered on the hero's kind of work |
| 0.08 | The speech | speech-color, a length rolled in Min/Max Length, awards +1, reputation +2 |
| 0.10 | The reviews | film-critic-color windowed on Min/Max Reception against the hero's impression |
| 0.12 | A publicity stunt | publicity-stunt-scenarios, filtered on Hero Gender and the age window |
| 0.12 | A challenge in the scene | dynamic-challenges; Adult Only is skipped under 18 |
| 0.12 | The costar | one of the four costar pools |
| 0.10 | Developing the character | develop-character-scenarios, performance +3 |
| 0.08 | Practice | practice-scenarios |
| 0.10 | A bit part | extra-roles filtered on Gender and the age window, a fee of 500 + below(2500) |
| 0.14 | A rejection | rejection-color windowed on Min/Max Propensity |
| else | The audition | a film (65%) or a show (35%) and a slot in its cast |

The costar pools are four different kinds of trouble on the same set:
`costar-crazy-scenarios` (30), `costar-criticism-scenarios` (33), `costar-diss-scenarios` (33) and
`costar-nervous-scenarios` (12). The scenario pools carry a `Scenario` and a `Log Version`;
the criticism and crazy pools carry a `Feedback Snippet` and a `Log Version Snippet` pair
where the log line points back at the feedback with `<rb>`, which is why every
one of them is resolved with `withBrackets` - every other bracketed column of the
row first, in the row's own column order, and the wanted column last. Criticism
costs 2 performance, nerves cost 1 impression, and the other two give back 1
reputation: a difficult set is not always bad for a career.

### 76.4 The part

The audition is the fallback branch, and it is where the family's real economy
lives. A genre is drawn from the film or show genre pool weighted on commonness;
then a production of that genre; then a slot. The slot order is the caste system:
with reputation 60 or more the hero is offered a Lead first, and otherwise the
supporting or regular slots and then the smallest parts. Each slot is skipped when
its name is empty or its gender or age window does not fit.

The odds of landing it are:

~~~
0.45 + (propensity - 40)/220 + (impression - 40)/220
     + (reputation - 40)/300 + (agent ? agentReputation/600 : -0.05)
~~~

A win pays `reputation * 900 + 3000 + below(9000)`, multiplied by 1 to 6 for a show,
and raises performance, impression, propensity and reputation. A loss costs 2
propensity: the rejections are the price of the trade.

### 76.5 The tokens

The family uses four tokens that exist nowhere else in the corpus: `<production>`,
`<role>`, `<costar>` and `<real-actor>`. None of them needed a new pool:

- `<production>` is the current film or show title, and a whole row of the films table
  when the hero has none yet.
- `<role>` is the slot he read for.
- `<costar>` is the costar, drawn as a person when the row does not name one, and
  `<costar-possessive>` goes through the same possessive helper as every other person
  token.
- `<real-actor>` is a real celebrity name and is answered by the celebrity reference
  pool; the port falls back to the hero's lead and then to `a famous actor`.

The family also leans on the shared `<he/she>`, `<him/her>` and `<his/her>` pairs (449 uses)
and on `<rb>` / `<rb2>` (142 uses), all of which the renderer already answers.

### 76.6 The bugs

Six, in the order they cost a build:

1. `void Sim::makeActor` dereferenced `hero_` before assigning it - every run that
   asked for the family segfaulted 60 times out of 60. Every other `makeX` in the port
   starts with `hero_ = &h; life_ = &life;` and this one did not.
2. The costar branch ensured the FIRST costar pool and then drew from any of the
   four, so a hero who chose a different one died with 
   `std::out_of_range: table not loaded: a16_actor-costar-diss-scenarios-enc`. The
   chosen pool is now ensured itself, and the branch returns cleanly when the
   loader cannot open it.
3. `life.age` does not exist - the age lives on the hero - and 
   `Table::num` takes two arguments, not three, so every `num(row, col, default)` call
   was a compile error. Two local lambdas, `numOf` and `hasCell`, now carry the
   missing-column and blank-cell cases, and one of them also replaced the two
   nested reputation windows.
4. `tbl` takes a `const char*`, so the four places that passed a 
   `std::string` table name needed `.c_str()`.
5. A publicity stunt drew its number twice: the scenario cell and the log cell
   both hold `<num511-650>`, and each drew its own, so the card said 511 and the
   journal said 553. A `bindNums` helper now finds every number tag in the body cell,
   draws each one once, and substitutes the same value into both cells.
6. Two smaller voice bugs: a log line built from a fragment was left without its
   full stop (`I hack through a bathroom door with an axe`), and the speech length
   was written as `a 80-second speech`. `say` now adds the full stop, and the
   article comes from the same helper the rest of the port uses.

### 76.7 Shortcuts

- One production at a time: the hero works a single part a year, and a second
  picture in the same year is not modelled.
- The slot windows are checked, but a part never carries a schedule, a shoot or
  a conflict with another job.
- The show economy is a multiplier on the film fee (1 to 6) rather than a
  per-episode rate.
- Awards are counted, never named.
- The reviews come from one critic pool, so a bad review and a good one differ
  only in the reception window the row fits.
- The hero never ages out of the trade: there is no Too Old column in this family.

### 76.8 Verification

- `/tmp/qdir/actres.py` - 30 seeds x both genders to age 60 with 
  `{"actor": true}`: 668 family cards, 0 failed runs, residue 0.
- The last-mark histogram covers 18 of the 20 tables; `film-genres` and 
  `show-genres` are drawn inside the audition branch and are never the card's mark.
- All 19 family probes report residue 0 (actres, astres, bizres, bmres, casres,
  cultres, dlres, fightres, hustres, ltres, mafiares, outres, petres, racres,
  spyres, stures, traderes, vampres, zoores).
- `rescensus.py` reports 0 distinct residue tokens, coverage reports 109
  captured and 107 recoverable, and the port gap is 0.
- `covpush.json` exits 0 and the bench reaches age 34 with no abort.

Sample cards, taken from the probe:

~~~
A part in a film / The picture is The Tenth Element. I read for flight attendant.
  They are paying me $27,690.
  I landed the role of flight attendant in The Tenth Element.

A bit part / Casting called: they need a sorority sister.
  I played a sorority sister for $2,246.

An audition / The picture is Nevervale. I read for a supporting part.
  I did not get the part in Nevervale.

The reviews / A critic reviewed Bamboozled in Bangalore.
  The reviews stung. / Out for aiming high and falling flat.

A publicity stunt / You flew a drone over a music festival and safely released
  578 The Tenth Element branded glowsticks onto the crowd.
  I flew a drone over a music festival and safely released 578 The Tenth Element
  branded glowsticks onto the crowd.

The costar / On the set of Grandmother's Attic, Nikki Janim asked who you slept
  with to be in the production.
  Nikki Janim insinuated I slept with someone to be in the production.

A challenge in the scene / The script asks me to hack through a bathroom door
  with an axe in a scene.
  I hack through a bathroom door with an axe.

A new agent / Duchess Dada wants to represent me.
  Duchess Dada is representing me now.

Off the picture / The studio called about my part in The People Under The Stars.
  I was replaced in The People Under The Stars.

Dropped / My agent, Aubrey Dram, sat me down.
  Aubrey Dram dropped me as a client.
~~~

Two of the family's own lines are worth knowing before reading a card. The
critic pool's snippet is a bare fragment - `Out for aiming high and falling flat` -
and is printed as the second log line, after the family's verdict. And the first
log line of any card may belong to another subsystem: the port prints the
lifecycle lines that were already in the year before the family's own, which is why a
review card can open with a relative's diagnosis.


## 77. The market: a portfolio that moves every year, an advisor who may not stay, and eighteen tables

### 77.1 The family

The `a16_financial` family is eighteen tables and 918 rows. Every one of them is a card the hero can draw, an extra line under one, or a list the port draws from while building the market itself.

| table | rows | columns |
| --- | ---: | --- |
| `a16_financial-advice-rejections-enc` | 89 | `String` `Intelligence Score` `Stock` `Crypto` |
| `a16_financial-advisor-drops-hero-color-enc` | 53 | `Feedback Color` `Requires Real Celebrity` `Commonness Score` `Hero Inactive` `Advisor Min Reputation` `Advisor Max Reputation` `Hero Min Net Worth` `Hero Max Net Worth` |
| `a16_financial-advisor-fire-color-enc` | 29 | `Feedback Color` `Commonness Score` `Advisor Min Reputation` `Advisor Max Reputation` `Hero Min Net Worth` `Hero Max Net Worth` |
| `a16_financial-advisor-hire-color-enc` | 61 | `Feedback Color` `Requires Real Celebrity` `Commonness Score` `Advisor Min Reputation` `Advisor Max Reputation` `Hero Min Net Worth` `Hero Max Net Worth` |
| `a16_financial-asset-advice-enc` | 139 | `Feedback` `Prospect Score` `Professional Score` `Stock` `Crypto` |
| `a16_financial-asset-tips-enc` | 43 | `Tip` `Professional Score` |
| `a16_financial-crypto-performance-color-enc` | 26 | `String` `Min Delta` `Max Delta` |
| `a16_financial-cryptocurrencies-enc` | 21 | `Symbol` `Name` `Unit Singular` `Unit Plural` `Expense Score` `Winner Score` |
| `a16_financial-fund-performance-color-enc` | 23 | `String` `Moved With Sector` `Min Delta` `Max Delta` `Brackets Used` |
| `a16_financial-headlines-enc` | 75 | `Headline` `Current` `Future` `Market Trend` `Sector Trend` `Asset Trend` |
| `a16_financial-insider-buy-scenarios-enc` | 14 | `Scenario` `Log Version` |
| `a16_financial-insider-sell-scenarios-enc` | 13 | `Scenario` `Log Version` |
| `a16_financial-market-advice-enc` | 136 | `Feedback` `Professional Score` `Current` `Future` `Market Trend` `Sector Trend` |
| `a16_financial-portfolio-report-color-enc` | 21 | `String` `Min Delta` `Max Delta` |
| `a16_financial-stock-name-structure-enc` | 8 | `Sector` `Format` `Prefix` `Suffix` `Predefined` |
| `a16_financial-stock-performance-color-enc` | 36 | `String` `Moved With Sector` `Min Delta` `Max Delta` |
| `a16_financial-stock-tip-scenarios-enc` | 24 | `Scenario` `Log Version` `Professional Score` (its first two lines are a banner, see 77.5) |
| `a16_financial-universal-stocks-enc` | 107 | `Symbol` `Name` `Sector` `Min Price` `Max Price` `Winner Score` `Hero Ownership Cap` |

Four of the eighteen are not cards at all. `universal-stocks` is the universe of listed names (symbol, name, sector, a price band, a winner score and an ownership cap); `cryptocurrencies` is the coin list; `stock-name-structure` is the grammar of a private company name; `fund-performance-color` is the advisor fund line. The other fourteen are the year.

### 77.2 The portfolio

At birth `makeFinance` builds a market for the hero: six listed companies drawn from `universal-stocks` weighted by its winner score, each with a price rolled inside its own band, three coins from `cryptocurrencies` at an invented price (the table has no price column), and a private company whose name comes from `stock-name-structure` (a sector-specific format of prefixes, suffixes, a colour and a surname).

A holding is a small struct: symbol, name, sector, a crypto flag, shares, price and cost. The hero holds nothing at first: the scripted lives the harness runs never pay a wage, so the first investing year hands him five shares of the first name as an inheritance, once, and the report says so rather than pretending he bought them. After that he buys with what he has (a fifth of his cash, rounded up to one share when that is all he can afford) and sells when his appetite is low.

Every year the market moves every holding once. The year is either a stock year or a coin year for the stock and crypto tables, but the hero feels a delta per holding: a performance row is drawn (the rows flagged Moved With Sector only when the holding's sector is the year's hot sector, otherwise the whole table), its Min Delta and Max Delta are rolled, the delta is clamped at -75% so one year cannot wipe a position, and the price is multiplied. The card always opens with "My portfolio is worth X." so a reader can follow the position.

### 77.3 The year

One card a year, first hit wins, rarest first. The table lists the chance and what the card needs.

| # | chance | card | needs |
| ---: | ---: | --- | --- |
| 1 | 0.05 | The advisor quits | an advisor; the pool is windowed on the advisor's reputation and the hero's net worth |
| 2 | 0.05 | Dropped | an advisor; same windows; the hero is left with none |
| 3 | 0.05 | An insider tip (sell) | a position with shares; sells the whole position at the market price |
| 4 | 0.06 | A tip from the inside (buy) | the hero can afford one share |
| 5 | 0.07 | A new advisor | the hero has none |
| 6 | 0.06 | The fund | an advisor; draws `fund-performance-color` and moves the year's return cache |
| 7 | 0.08 | A stock tip | `stock-tip-scenarios`, skipping the table's own note rows |
| 8 | 0.10 | In the news | `headlines`, current rows preferred |
| 9 | 0.12 | The advisor calls / Asking around | `market-advice` with an advisor, `advice-rejections` without one |
| 10 | 0.12 | Advice about a stock / a coin | `asset-advice` or `asset-tips`, a coin four times in ten |
| 11 | 0.12 | My portfolio | `portfolio-report-color`, the delta cache set to the year's average move |
| 12 | 0.14 | A trade | a buy with cash, or a sell when the appetite is low |
| 13 | else | The market | the biggest mover of the year |

The chain rolls only after the market has moved, and only on a 0.5 chance for the year at all: the market moves in every investing year, but a card is drawn on half of them, which is what keeps the other subsystems breathing room in the year.

### 77.4 The tokens

The family writes seven tokens the rest of the port never sees. `financeIsToken` claims them in the resolveInto chain BEFORE the hero-token branch, because `<company>` would otherwise fall through to the generic "the company" hero token, and before the vampire branch, which claims a handful of the same words.

| token | answer |
| --- | --- |
| `<sector>` | the hero's sector, else his first holding's, else technology |
| `<company>` | the private company name (only while a finance card is live) |
| `<asset-symbol>` | a symbol of the matching kind: a held stock or coin when he holds one, else any |
| `<market-singular>` | stock or cryptocurrency |
| `<market-plural>` | stocks or cryptocurrencies |
| `<return>` | the year's delta with its sign, as a percentage |
| `<return-absolute>` | the same magnitude without the sign |

Every one of them is memoised per card, so a row that says `<return>` twice says the same number twice. `financeLive_` is set for the length of a card and restored when the card ends, which is the mechanism that keeps `<company>` from leaking into other families' text.

### 77.5 The bugs

Six, and one of them was in the shared loader rather than the family.

1. **A CSV banner parsed as a header.** `a16_financial-stock-tip-scenarios` opens with two junk lines before its real header (the file name and a note about the ranking), so the loader took the first line as the header, produced a table with one useless column and made every row look like a note. The loader now also skips a first row of three or more cells with fewer than two filled cells. A scan of the corpus showed the rule changes exactly one file, and that a blunter rule (any first row with fewer than two filled cells) would have broken the forty-eight legitimate single-column tables, so the three-cell condition is load-bearing.
2. **The advisor spoke about the hero in the hero's own voice.** The three advisor pools are written about the advisor ("She said she knows how to spot a bad investment when she sees one"), so the card body now resolves through the scene helper with the advisor's own gender and the first letter raised. Before the fix the body read "she said" on a male advisor and started lower case.
3. **A note row was drawn as a card.** The stock-tip pool's banner survives the loader fix as a row with an empty Scenario, and the tip branch drew it once, producing a card with no text. The branch now skips a row whose Scenario cell is empty.
4. **Second-person bodies.** The tip, insider and advice bodies are written in the second person ("A bartender tells you to invest..."), so their leading letter is raised and, where the subject is the hero rather than a stranger, they are marked as the card rather than the journal. The journal line is the pool's own Log version.
5. **The advisor could never quit in a probe run.** With the branch at 0.03 and the year's 0.5 gate, sixty scripted lives produced no fire at all. Rather than leave the table unwired in practice it was raised to 0.05, matching the drop chance, and verified by a temporary 0.9 run that produced four fires in three lives.
6. **The hero is poor.** Every scripted life ends at or below zero: the harness enables one family at a time and no family of the port pays a wage for work it does not model. The first portfolio therefore has to be inherited (five shares, once, flagged on the hero) or the whole family would sit at a zero balance forever, the sell pool would never be reachable and the trade card could never buy. The affordability arithmetic was relaxed at the same time: a purchase rounds up to a single share when a fifth of the hero's cash is not enough for one, and gives up when even that is out of reach.

### 77.6 Shortcuts

- One position per name: no lots, no average cost per lot, no options, no shorts.
- Coins have an invented price (1 to 251) because `cryptocurrencies` carries no price band.
- The advisor'+AP+'s fund card was added after the first probe: `fund-performance-color` had no other home.
- `<random-city>` has no table in the corpus: the private company name uses the hero's own city.
- The -75% clamp on a yearly move is the port's invention; the data bands allow more.
- The inheritance of five shares is a harness accommodation, not the game's rule.
- The portfolio statement line ("My statement came in.") is the port's, because `portfolio-report-color` has no Log column.
- No fees, no dividends, no taxes, no margin call, and the advisor's reputation never moves.

### 77.7 Verification

The probe (`/tmp/qdir/finres.py`, thirty seeds and both genders to sixty) prints 762 finance events, no run failures and zero residue tokens, with sixteen of the eighteen tables as a card's own table: the market 220, crypto 94, headlines 66, a tip 64, rejections 56, insider buy 50, the statement 48, insider sell 40, asset advice 26, asset tips 26, a new advisor 24, a trade 22, the advisor calls 10, the fund 8, the advisor quits 4, dropped 4. The two that never stand alone (the universe and the name grammar) are the market the hero is dealt, and are exercised on every run.

Sample cards, in the port's own words:


    In the news
    stock market price hikes imminent
    My portfolio is worth $397.
    The market moved on the news: stock market price hikes imminent.

    A tip from the inside
    A MILF tells you she is aware of an investment that is guaranteed to make money.
    An astronomer shared insider information with me about Atlanta Industrial Holdings.

    An insider tip
    A Redditor just heard from a childhood friend who is now a top executive at Hartford Soil Super Supply and said not to say anything, but he tells you anyway that the company is about to declare bankruptcy.
    I sold 5 POOP for $482 and made $482.

    The fund
    The fund suffered considerable losses this year.

    The advisor quits
    He called you count dickula.
    I need a new financial advisor now that Montero X has walked out.

    Dropped
    She said she knows how to spot a bad investment when she sees one.
    Sugar Moon dropped me as a client.

    My portfolio
    It was a strong year for your portfolio, which yielded -6% in returns.
    My statement came in.

    A trade
    I sold my UTI position.
    I sold 5 shares of UTI for $241.

    A stock tip
    A bartender tells you to invest your money in a company called Atlanta Industrial Holdings because its ticker symbol makes him giggle.
    A thug told me to invest in Atlanta Industrial Holdings because the ticker symbol made him giggle.

    Asking around
    He said he can't quite figure out what you mean.
    I asked around about cryptocurrencies and got nothing worth acting on.


A note on the data rather than the port: a portfolio row can read "It was a strong year for your portfolio, which yielded -6% in returns." The pool's own adjective and its own delta band disagree, and the port prints the row as written rather than repairing the pool.

The regression that ran with this section is the usual set: every family probe at zero residue, the residue census at zero, coverage 109 of 109 captured and 107 recoverable, the port-versus-game gap at zero, the scripted push run clean and the bench run to the end of its trace.

## 78. The catwalk: a four-rung ladder, twelve ways to spend a year, and twenty-five tables

### 78.1 The family

The `a16_model` pool is twenty-five tables and 802 rows. Every one of them is wired into the year; ten of them are drawn inside a branch rather than standing as a card of their own, which the last column below marks.

| table | rows | columns |
| --- | --- | --- |
| `a16_model-agency-model-sabotage` | 8 | `Method`, `Method String`, `Log Version`, `Result Snippet`, `Target Discovers Hero Snippet`, `Risk Score`, `Sabotage Score`, `Commonness Score` |
| `a16_model-agency-names` | 14 (drawn inside a branch) | `Format`, `Prefix`, `Suffix`, `Min Age`, `Max Age`, `Typical Gender`, `Commonness Score` |
| `a16_model-audition-feedback` | 20 | `Reason`, `Min Looks`, `Max Looks`, `Min Modeling Skill`, `Max Modeling Skill` |
| `a16_model-audition-snippets` | 60 | `Snippet`, `Hero Gender`, `Min Attitude`, `Max Attitude`, `Min Poise`, `Max Poise`, `Min Quirkiness`, `Max Quirkiness`, `Difficulty Score` |
| `a16_model-campaign-summary-color` | 17 | `String`, `Min Reception`, `Max Reception` |
| `a16_model-career-progression` | 5 | `Model Type`, `Jobs To Promotion`, `Minimum Looks`, `Minimum Health`, `Minimum Profile`, `Can Be Famous`, `Stress Score`, `Min Salary`, `Max Salary`, `Min Age`, `Max Age`, `Too Old Age`, `Diseases` |
| `a16_model-categories` | 51 (drawn inside a branch) | `Category`, `Industry`, `Minimum Model Type`, `Min Typical Model Age`, `Max Typical Model Age`, `Commonness Score` |
| `a16_model-companies` | 18 (drawn inside a branch) | `Format`, `Prefix`, `Suffix`, `Industry` |
| `a16_model-fashion-show-pivotal-moments` | 16 | `Category`, `Requires Model`, `Body`, `CTA`, `Choice 1`, `Success Odds 1`, `Success Feedback 1`, `Success Narrative 1`, `Success Performance Impact 1`, `Failure Feedback 1`, `Failure Narrative 1`, `Failure Performance Impact 1`, `Choice 2`, `Success Odds 2`, `Success Feedback 2`, `Success Narrative 2`, `Success Performance Impact 2`, `Failure Feedback 2`, `Failure Narrative 2`, `Failure Performance Impact 2`, `Choice 3`, `Success Odds 3`, `Success Feedback 3`, `Success Narrative 3`, `Success Performance Impact 3`, `Failure Feedback 3`, `Failure Narrative 3`, `Failure Performance Impact 3`, `Choice 4`, `Success Odds 4`, `Success Feedback 4`, `Success Narrative 4`, `Success Performance Impact 4`, `Failure Feedback 4`, `Failure Narrative 4`, `Failure Performance Impact 4` |
| `a16_model-fashion-show-venues` | 24 (drawn inside a branch) | `Venue`, `Uses Indefinite Article`, `Min Profile`, `Max Profile` |
| `a16_model-fashion-week-feedback` | 19 | `String`, `Min Performance`, `Max Performance` |
| `a16_model-fired-color` | 27 | `String`, `Age`, `Looks`, `Modeling Skill`, `Botched Surgery`, `Disease`, `Pregnancy` |
| `a16_model-gifts` | 51 | `Item Name`, `Gender`, `Elite`, `Color Variants`, `Short Name`, `Indefinite Article`, `Plural`, `Can Be Refurbished`, `Min Value`, `Max Value`, `Appreciates`, `Commonness Score` |
| `a16_model-gig-types` | 5 | `Job Type`, `Industries`, `Commonness Score` |
| `a16_model-go-see-action-poses` | 28 | `Snippet`, `Log Refusal Snippet`, `Log Snippet`, `Agree Performance Impact`, `Refuse Performance Impact`, `Commonness Score` |
| `a16_model-go-see-interview-questions` | 23 | `Question`, `CTA`, `Default Choice`, `Default Choice Impact`, `Choice 1`, `Choice 1 Impact`, `Choice 2`, `Choice 2 Impact`, `Choice 3`, `Choice 3 Impact`, `Choice 4`, `Choice 4 Impact`, `Choice 5`, `Choice 5 Impact`, `Choice 6`, `Choice 6 Impact`, `Choice 7`, `Choice 7 Impact`, `Choice 8`, `Choice 8 Impact`, `Choice 9`, `Choice 9 Impact`, `Choice 10`, `Choice 10 Impact`, `Choice 11`, `Choice 11 Impact`, `Choice 12`, `Choice 12 Impact`, `Choice 13`, `Choice 13 Impact`, `Choice 14`, `Choice 14 Impact`, `Choice 15`, `Choice 15 Impact`, `Choice 16`, `Choice 16 Impact`, `Choice 17`, `Choice 17 Impact`, `Choice 18`, `Choice 18 Impact`, `Choice 19`, `Choice 19 Impact`, `Choice 20`, `Choice 20 Impact` |
| `a16_model-go-see-performance-review` | 23 | `String`, `Log Version`, `Min Impression`, `Max Impression` |
| `a16_model-headshot-background` | 23 (drawn inside a branch) | `Background`, `Professionalism Score`, `Commonness Score` |
| `a16_model-headshot-expressions` | 34 (drawn inside a branch) | `Expression`, `Professionalism Score`, `Commonness Score` |
| `a16_model-headshot-outfits` | 55 | `Outfit Snippet`, `Hero Gender`, `Professionalism Score`, `Commonness Score` |
| `a16_model-industries` | 10 (drawn inside a branch) | `Industry`, `Commonness Score`, `Country Blacklist` |
| `a16_model-photo-shoot-facial-expression` | 34 | `Snippet`, `Choice 1`, `Narrative 1`, `Impact Score 1`, `Choice 2`, `Narrative 2`, `Impact Score 2`, `Choice 3`, `Narrative 3`, `Impact Score 3`, `Choice 4`, `Narrative 4`, `Impact Score 4`, `Difficulty Score` |
| `a16_model-photo-shoot-poses` | 72 | `Body Snippet`, `Lover Argument Snippet`, `Feedback Log Snippet`, `Body Part Model Only`, `Video Shoot Only`, `Lover Argue Score`, `Agree Performance Impact`, `Disagree Performance Impact`, `Happiness Impact`, `Health Impact`, `Disease` |
| `a16_model-products` | 62 (drawn inside a branch) | `Product Name`, `Category`, `Typical Model Gender` |
| `a16_model-publication-titles` | 103 (drawn inside a branch) | `Name`, `Gender`, `Type`, `Local Publication`, `Industry`, `Min Profile`, `Max Profile`, `Commonness Score` |

### 78.2 A model at sixteen

The hero joins at rank 0 and is paid the rank's salary band every year whether or not the year brings a job, as the other career families are. The industry is drawn from `a16_model-industries-enc` (a row on a country blacklist the hero lives in is skipped), then a category of that industry from `a16_model-categories-enc`, then an agency name, a rival, and one product from the hero's own category. The agency name and the publication and gig names all come from the same substitution: one alternative of the table's `Format` cell, its own prefix and suffix lists, a colour from the shared company-colour pool, and a first name and surname, with every bracket the port cannot fill dropped.

### 78.3 The ladder

| rank | model type | jobs to promotion | minimum looks | minimum profile | salary | ages |
| --- | --- | --- | --- | --- | --- | --- |
| 0 | Body Part Model | 0 | 50 | 0 | $65000 - $80000 | 18 - 75 |
| 1 | Stock Photo Model | 8 | 65 | 20 | $85000 - $95000 | 20 - 65 |
| 2 | Catalog Model | 24 | 75 | 40 | $100000 - $130000 | 22 - 55 |
| 3 | Elite Model | 35 | 85 | 60 | $200000 - $250000 | 24 - 45 |
| 4 | Supermodel | 0 | 95 | 80 | $500000 - $15000000 | 26 - 45 |

A promotion needs the rank's jobs-to-promotion count, its minimum looks, its minimum profile, its minimum health and the hero still being inside the age band; the fourth rung is the one that can make the hero famous.

### 78.4 The year

The branches are tried in order, rarest first, and the first one that fires owns the card. The year's salary is paid before the chain, so every card carries the same wage line.

| chance | branch | what it draws |
| --- | --- | --- |
| 0.04 | A dirty trick | one of eight `a16_model-agency-model-sabotage-enc` rows, risked against the hero's skill; a caught model loses profile |
| 0.05 | The agency lets me go | a `fired-color` row whose flag columns the hero can actually satisfy; the agency is emptied and the rank reset |
| 0.10 | A fashion show | a venue inside the hero's profile band and a pivotal moment of the hero's category, with one of the row's own choices and its own odds |
| 0.08 | An ad campaign | a `publication-titles` row inside the profile band, then a `campaign-summary-color` row inside the reception band |
| 0.08 | Fashion week | a `fashion-week-feedback` row inside the performance band |
| 0.12 | A photo shoot | a `photo-shoot-poses` row, with the partner argument as an agree-or-refuse choice |
| 0.06 | A facial-expression shoot | a `photo-shoot-facial-expression` row and one of its four choices |
| 0.10 | A go-see | one of three rolls: an interview question with its own choice and impact, an action pose agreed to or refused, or a performance review inside the impression band |
| 0.10 | Headshots | an outfit for the hero's gender, an expression and a background, scored for professionalism |
| 0.12 | An audition / a booking | an `audition-snippets` row inside the attitude, poise and quirkiness bands; a booking pays a fee, adds a job and a point of skill |
| 0.07 | A gift | a `gifts` row for the hero's gender, elite rows only from the fourth rung |
| 0.14 | A day on set | a `gig-types` row and a company name, paid a fee |
| else | A year on the catwalk | the fallback card, which still raises profile by a point |

### 78.5 The runway

The fashion-show pool writes a card as a body paragraph plus per-choice columns. The venue takes its indefinite article when the venue table asks for one ('an abandoned asylum'), the card takes one of the row's filled choices, and the odds are the choice's own `Success Odds N` adjusted by the hero's poise on a hundred-point scale. A win appends the choice's own success feedback and writes its success narrative as the journal line; a loss writes the failure pair. The pool's narrative columns are written in the first person already, so they are printed as they stand.

### 78.6 The tokens

The family asks for four keys the port did not have, and every other token in its 802 rows was already answered.

| token | answer |
| --- | --- |
| `<model>` / `<model-possessive>` | the hero's rival at the agency, drawn at sixteen, with the possessive formed properly |
| `<model-product>` | one product of the hero's own category, remembered for the year so a card and its journal line agree |
| `<model-product-name>` | the same product, claimed only while a catwalk card is being rendered |

### 78.7 The bugs

1. `Hero` carries no gender field at all: the port reads the gender through `heroToken("gender", false)`, which returns 'male' or 'female'. Five call sites in the new block assumed a member and the build stopped on all five.
2. The headshot expression pool was first written as a single nested ternary that the compiler could not even parse ('expected :'). It is now an ordinary pool built from the table then weighted.
3. The go-see review speaks about the reviewer, not the hero, and its string is second person. It is now resolved with the reviewer's own pronoun pair and passed through the port's second-to-first-person converter, which also supplies the journal line.
4. A facial-expression brief reads 'They wanted me to look sultry'; the first pass printed 'They wanted look sultry'.
5. The headshot card first read 'I wore paisley button-up shirt, pouty, against trees'. The three pools are bare noun phrases with no articles in the data, so the card now labels them: 'Outfit: ... Expression: ... Background: ...'.

### 78.8 Shortcuts

* One agency, no agency hopping: a row that fires the hero empties the agency field and the next year can only be spent as a freelancer, because the port has no re-hire branch.
* The catwalk fallback always produces a card, so a model year is never a silent year; the price is that the fallback is about a third of all model cards.
* Fees are flat: a booking pays a thousand plus profile, a day on set pays five hundred plus profile, and neither is scaled by the publication or the campaign.
* No photo gallery: the headshot outfit, expression and background are remembered on the hero for the report but nothing reads them back later.
* The `Disease`, `Botched Surgery` and `Pregnancy` rows of the fired pool are unreachable, because the port models none of the three.

### 78.9 Verification

Thirty seeds in both genders, run to sixty with only this family switched on: 589 family events, no failing run, and every bracketed token in every title, body and log line resolved. All twenty-two branches were then counted over forty seeds in both genders (749 model cards): a year on the catwalk 273, a fashion show 76, fashion week 50, headshots 50, an ad campaign 49, a photo shoot 39, a booking 28, a day on set 27, the agency lets me go 26, an audition 25, a facial-expression shoot 24, a dirty trick 22, a gift 22, a go-see 20 and the go-see review 18. Sixteen of the twenty-five tables own a card; the other nine are drawn inside a branch, which a needle scan over 4,553 rendered cards confirms for every one of them.

Eight cards, sampled by hand:

    A dirty trick
    You lined Emmett Nym's favorite shoes with glue.
    I was caught lining his favorite shoes with glue.

    The agency lets me go
    They said they don't work with the elderly.
    I am no longer with Rising Dawn Talent Agency.

    A fashion show
    Venue: an abandoned asylum. You're sitting in hair and makeup when the designer informs you the show's theme is "Moonlit Romance" and instructs you to maintain a facial expression that makes sense for the theme. I chose to Give a coy smile.
    I intrigued an audience with a coy smile as I walked the runway at a romance-themed fashion show.

    An ad campaign
    Campaign: Healthy Lifetimes. The director of marketing for their 7 Degrees of the 7 Seas said your ad didn't get the reception she was hoping for.
    I shot a campaign for 7 Degrees of the 7 Seas that ran in Healthy Lifetimes.

    Fashion week
    She says you've got to make better choices if you intend to have a long modeling career.
    She says I've got to make better choices if I intend to have a long modeling career.

    A facial-expression shoot
    They wanted me to look sultry. I went with Give bedroom eyes.
    I gave the camera bedroom eyes during a shoot after being told to look sultry.

    A go-see
    What do you think about our brand? I answered: It's top-of-the-line.
    I nailed the interview at the go-see.

    A booking
    They asked me to be sophisticated.
    I booked the job.

    A gift
    A client sent me a Club Sake Tube Dress.
    I was gifted a Club Sake Tube Dress worth $22,056.

    A year on the catwalk
    I worked the casual dining circuit as a body part model.
    I spent the year modelling.

A note on the data rather than the port: the campaign pool's reception lines are written from the client's side and address the hero directly ('your ad didn't get the reception she was hoping for'), so the card reads as second person while the journal line that follows is first person. The port prints both as the pools wrote them.

The regression that ran with this section is the usual set: twenty-one family probes at zero residue, the residue census at zero, coverage 109 of 109 captured and 107 recoverable, the port-versus-game gap at zero, the scripted push run clean and the bench run to the end of its trace.


## 79. The luxuries: a fortune to spend, sixteen ways to spend a year, and twenty-two tables

The `a16_luxury` family is the money side of the game. Everywhere else in the port the hero earns a wage, runs a business or wins a purse; here he simply has money, and the family is the catalogue of what money buys: a private island, an exotic watch, a secret society, a spa, a night out among famous people, and a small aircraft-carrier of accessories for the jet, the car and the boat.

Behavioural sources, all under `/Users/Adam/Documents/bitlife/_re/gamedata_plain`:


### 79.1 The family

Twenty-two tables, 734 rows. Every one of them is drawn by the year.

| table | rows | columns |
| --- | --- | --- |
| `a16_luxury-celebrity-chef-name-enc` | 15 | Name, Commonness Score |
| `a16_luxury-charity-organization-enc` | 32 | Name, Abbreviation, Is Bad Charity |
| `a16_luxury-default-island-name-enc` | 96 | Name, Island Type, Commonness Score |
| `a16_luxury-exotic-watch-brand-enc` | 6 | Make, Model, Min Cost, Max Cost |
| `a16_luxury-island-color-snippet-enc` | 43 | Snippet, Island Type, Commonness Score |
| `a16_luxury-island-data-enc` | 3 | Island Type, Small/Medium/Large Minimum + Maximum Cost, Eligible Countries |
| `a16_luxury-island-structure-spend-time-enc` | 36 | String, Log Version, Structure Type, Commonness Score |
| `a16_luxury-island-vacation-dumb-scenario-enc` | 18 | Subject, Subject Min/Max Age, Gender Bracket, Title, Body, CTA, Choice 1-4, Narrative 1-4, Relationship Impact 1-4, Karma Impact 1-4 |
| `a16_luxury-luxury-aircraft-special-feature-enc` | 16 | Feature, Profile Color, Cost, Compatible Category, Commonness Score |
| `a16_luxury-luxury-car-special-feature-enc` | 14 | Feature, Profile Color, Cost, Compatible Category, Commonness Score |
| `a16_luxury-luxury-watercraft-special-feature-enc` | 16 | Feature, Profile Color, Cost, the six category flags (Sailing Yacht, Sailboat, Research Vessel, Cruise Ship, Submarine, Pirate Ship), Commonness Score |
| `a16_luxury-secret-societie-enc` | 10 | Name, Secondary Text, Membership Requirement Text, Rank 1-3, Looks/Health/Happiness/Smarts/Willpower/Karma Impact, Join Color String, Rejection Strings, Society Motto, Entry Fee |
| `a16_luxury-secret-society-discussion-topic-enc` | 80 | Subject, Compatible Society, Feedback Snippet, Commonness Score |
| `a16_luxury-secret-society-game-enc` | 50 | Game, Compatible Society, Commonness Score |
| `a16_luxury-secret-society-rule-enc` | 60 | Rule, Compatible Society, "Yea" is Correct, Commonness Score |
| `a16_luxury-spa-treatment-color-enc` | 15 | String, Log Version, Commonness Score |
| `a16_luxury-spa-treatment-enc` | 30 | Name, Category, Cost, Looks Impact, Health Impact, Happiness Impact |
| `a16_luxury-spa-vip-treatment-enc` | 40 | Treatment, Commonness Score |
| `a16_luxury-vip-event-chef-cuisines-enc` | 22 | Cuisine Type (one column) |
| `a16_luxury-vip-event-feedback-color-string-enc` | 49 | String, Event Type, Enjoyment Score, Commonness Score |
| `a16_luxury-vip-movie-enc` | 68 | Title, Genre, Commonness Score |
| `a16_luxury-vip-nightclub-event-enc` | 15 | Event Type, Commonness Score |

Four of the tables are catalogues with no text of their own: `island-data` (three island types x three sizes), `exotic-watch-brand` (six watches), `spa-vip-treatment` (forty small luxuries) and `vip-event-chef-cuisines` (twenty-one cuisines that are only ever concatenated after a chef name). The rest carry a String, a Log Version or a Scenario.

### 79.2 The fortune

Nothing else in the port pays enough for a tropical island, so the family gives the hero the money itself and then a reason to spend it:

- `makeLuxury` rolls a one-off windfall of `200,000,000` into `h.money` and prints `I came into $X.` in the journal. Shortcut: it is the same device `section 77` uses for the first portfolio.
- the year pays `max(3,000,000, luxValue/10)` of "holdings" income, so the lifestyle is self-financing once it starts.
- the island ladder is the real one from `a16_luxury-island-data-enc`: icy 55M / 125M / 500M, temperate 100M / 300M / 750M, tropical 300M / 475M / 1,000M. A private island is bought at the Small price, a bigger island at the Medium then the Large one.
- the cheapest watch in `a16_luxury-exotic-watch-brand-enc` is a Rolex Paul Newman Daytona at 1.75M and the dearest a Graff Diamonds Hallucination at 105M; a watch is only offered when the hero can afford its `Min Cost`.
- the society entry fee is the flat 10,000,000 every row of `a16_luxury-secret-societie-enc` carries.

### 79.3 The year

Sixteen branches, first one to fire wins, rarest first. Each branch is `if (title.empty() && gate && chance(p) && ensureTable(...))`.

| # | chance | card | what it draws from |
| --- | --- | --- | --- |
| 1 | 0.05 | A secret society | `secret-societie` when the hero is not a member: fee 10,000,000, a 0.7 induction roll, then Rank 1 name, Join Color String and Society Motto, or the Rejection Strings |
| 2 | 0.05 | An exotic watch | `exotic-watch-brand` rows the hero can afford, cost rolled in Min..Max |
| 3 | 0.05 | A bigger island | `island-data` Medium then Large price, when the hero owns a Small or Medium island |
| 4 | 0.06 | A private island | `island-data` Small price plus a name from `default-island-name` filtered on Island Type |
| 5 | 0.06 | A vote at the society | `secret-society-rule` filtered on Compatible Society; the hero votes Yea or Nay on a coin and the row `"Yea" is Correct` flag decides whether that was the winning side (+2 happiness, or -1 and 'I was outvoted.') |
| 6 | 0.06 | Games at the society | `secret-society-game`, same filter |
| 7 | 0.06 | A discussion | `secret-society-discussion-topic`, same filter, plus the Feedback Snippet as an extra line |
| 8 | 0.08 | A charity gala | `charity-organization`: a donation of 1-10M; the `Is Bad Charity` flag flips the karma from +3 to -3 |
| 9 | 0.12 | A new toy | `luxury-aircraft-special-feature`, `luxury-car-special-feature` or `luxury-watercraft-special-feature` by a coin; the watercraft pool filters on the Sailing Yacht flag |
| 10 | 0.09 | A new structure on the island | `island-structure-spend-time` with the String resolved before the Log Version so the row's own `<rb>` points at the card text |
| 11 | 0.09 | Island life | `island-color-snippet` filtered on the hero's island type |
| 12 | 0.20 | A spa day | `spa-treatment` cost and impacts, one `spa-vip-treatment` extra, and 0.6 of the time a `spa-treatment-color` colour card |
| 13 | 0.14 | the island vacation card | `island-vacation-dumb-scenario` delegated to the shared row renderer with the Subject age and Gender Bracket gates checked first |
| 14 | 0.26 | A night out | `vip-event-feedback-color-string`; a movie premiere adds a `vip-movie` title and genre, a nightclub adds a `vip-nightclub-event` type, anything else a `celebrity-chef-name` and a `vip-event-chef-cuisines` cuisine |
| 15 | else | The good life | one of three small pools by coin: `spa-vip-treatment` (the staff brought me X), a spa treatment at home, or a quiet `vip-event-feedback-color-string` row |
| 16 | 0.12 | a society promotion | when the hero is a member: Rank 2 then Rank 3 from the society's own row |

The year ends the way every family year does: `out.table`, `out.row_id`, `out.title`, `out.body` and the first-person journal line are filled, the year's own extra lines are appended after the lifecycle lines already in `out.extra`, happiness moves, `luxuryLive_` is restored, and an empty title means the year simply had no card.

### 79.4 The tokens

The family needs exactly one new token:

- `<island>` and `<island>` with the possessive suffix, answered from `h.luxIsland` with `my island` as the fallback, remembered per card in `luxuryCache_` so two mentions in one card agree.

Everything else the family writes was already answered: `<person>` (89 uses), `<musician>`, `<celebrity>`, `<city>`, `<random-person-indefinite>`, `<random-first-name-male>`, `<random-surname>`, `<hero-last-name>`, `<country-other>`, the pronoun pairs and a great many inline lists. `<Island/Isle>` and `<Isle/Island>` are ordinary slash alternatives.

### 79.5 The bugs

1. The construction list. The wiring patch's append helper wrote the new seed on a fresh line, so the initialiser list ended `modelRng_(...) {}` then `luxuryRng_(...) {}` - two closing braces and an unbuildable file. The line is now `modelRng_(...),
      luxuryRng_(...) {}`.
2. `replaceAll` is not visible in this translation unit. The island-name substitution was written with the helper the other name builders use, which lives in another file; the fix is a local `fill(std::string&, const std::string& tag, const std::string& value)` loop.
3. A missing lambda. `luxuryYear` called a `hasCell` it never defined - the same helper the finance and actor years define locally. It now sits next to `numOf` and `flagOf`.
4. The fallback card lied about its table. When no branch fired, the card inherited whatever `out.table` had been left at, so 432 of 854 luxury cards were filed as `a16_luxury-spa-treatment` even when they were a `vip-treatment` line. The fallback now draws and marks a real row.
5. The fallback was half the year. With the everyday chances at 0.06/0.08/0.12/0.16 and every gated branch often closed, the boring card was 50% of the family. The chances are now 0.08/0.12/0.20/0.26 and the fallback itself is spread over three pools, which takes it to about a third and gives the spa, the night out and the small luxuries their own share.

Two smaller things: the cuisine keeps the pool's own capitalisation (`cooking Filipino food`), and the society card body got a separator so the motto does not run into the requirement sentence. The wiring patch also collapsed a duplicated `model` parse block that a twice-run script had left in `src/script.cpp`.

### 79.6 Shortcuts

- the 200,000,000 windfall, without which no island is reachable in a scripted life (no other enabled family pays a wage)
- `a16_luxury-island-structure-spend-time` has no cost column, so a structure costs an invented 5-24M
- one sailing yacht: the watercraft feature pool is filtered on the Sailing Yacht flag and the hero never buys a second boat
- the society Membership Requirement Text is prose that names no stat the port tracks, so membership is a roll, not a check
- donations are a flat 1-10M; the charity Abbreviation and Secondary Text are printed but the organisation never returns
- the island colour pool is filtered on the island type, and an icy island has nine of the forty-three snippets, which is why the branch is rare in the histogram
- the delegated vacation card can name a different `<person>` in the journal than on the card, because the shared dumb-scenario pools draw that token on every resolution - a cross-family limitation, not this family's bug

### 79.7 Verification

- `/tmp/qdir/luxres.py` (30 seeds x both genders to age 60 with `{"luxury": true}`) prints `luxury events 849 rc failures 0` and `RESIDUE 0`.
- last-mark histogram: vip-event-feedback 220, spa-treatment 214, spa-vip-treatment 97, charity 62, aircraft feature 39, watch 38, car feature 34, island-data 30, secret-societie 30, watercraft feature 26, island-vacation 18, island-structure 16, society-game 12, society-discussion 6, society-rule 5, island-colour 2.
- `/tmp/qdir/luxcov.py` (a needle scan over 1,085 luxury cards) proves the six tables that are never a card's own table are still drawn: celebrity-chef-name 103, spa-treatment-color 159, spa-vip-treatment 270, vip-event-chef-cuisines 103, vip-nightclub-event 27, vip-movie 19, default-island-name 2.
- all 22 family probes residue 0, `rescensus` `distinct residue tokens 0 total 0`, the scenario registry at 109 captured / 107 recoverable, the port gap 0, the push script rc 0 and the bench reaching age 34.

    A private island
    The island: Seaside Island (icy). Cost: $83,951,926.
    I bought Seaside Island, a private icy island, for $83,951,926.

    A secret society
    Jacked Rabbits: A secret society for health nuts. You must be extremely healthy to join this society.
    Aren't you PUMPED?! I was inducted into Jacked Rabbits: No pain, no gain.

    A vote at the Broken Mirror Society
    The society voted on: It is okay to look good for special occasions.
    The Broken Mirror Society voted on: It is okay to look good for special occasions. I voted Yea. I was on the winning side.

    An exotic watch
    The watch: Graff Diamonds Hallucination. Cost: $90,939,758.
    I bought a Graff Diamonds Hallucination for $90,939,758.

    A new structure on the island
    A secret lair for Seaside Island. Cost: $10,000,000.
    I practiced my best sphinxlike laugh in my island's secret lair.

    A new toy
    Alligator leather seats: Corner the croc market! Cost: $750,000.
    I had alligator leather seats installed on my private jet for $750,000.

    A night out
    Your dinner companions, Manny Jacinto and a mean girl you did not know, only wanted to talk about crypto. The chef was Ina Garden, cooking Panamanian food.

    The good life
    A quiet wedding. The couple shared their first dance as newlyweds to 'I Will Always Love You'.
    I spent the year enjoying the money, most recently at a wedding.

    A charity gala
    Mothers For Everything Real Safe (MFERS): a donation of $7,000,000.
    I donated $7,000,000 to Mothers For Everything Real Safe (MFERS)

    Games at the League of Extraordinary Idiots
    We played telephone.
    The League of Extraordinary Idiots spent the evening playing telephone.


---

## 80. The season: a book of games, a stat sheet, and nine more of the sports family

Section 52 gave the hero a sport, a league, a team and a position, then left him there: one
card a year out of the dumb-scenario pool, and nothing in the port that knew he was an
athlete. This section is the career that sits on top of that identity. Every year the season
is played out for real - a book of games with a win-loss record and a paycheque derived from
the sport row, a stat sheet whose numbers come from the hero's skill against the league's
percentile ladder, a career curve that takes the edge off after thirty and a retirement that
either ends at the bottom or ends in the Hall - and then one event out of the nine pools
that fill the rest of the family. The section-52 dumb pool keeps its own 45 per cent chance
for the years the season does not claim, so nothing already written stopped firing.

Section 80 is also the first place where the port had to invent growth: nothing in the data
says how a player improves, so the season book itself does it (see 80.2), and the award gates
were used as the calibration target.

### 80.1 The family

Nine tables, 676 rows, all new to the port. The four tables section 52 already wired -
`a16_sports-enc`, `a16_sports-leagues-enc`, `a16_sports-teams-enc`,
`a16_sports-positions-enc` - keep their roles: the sport row is the season rulebook, the
league and the team name the club, the position names the hero.

| table | rows | columns |
|---|---|---|
| `a16_sports-awards-enc` | 66 | League, Sport, Award, Add Award, Description Snippet, Position, Requires Championship, Requires Top 5 Finish, Performance Increase Award, Min Seasons, Max Seasons, Min Greatness, Max Greatness |
| `a16_sports-coach-scenarios-enc` | 93 | Positive, Negative, Scenario, Log Version, Hero Gender, Min Fitness, Max Fitness, Min Greatness, Max Greatness, plus one flag column per sport |
| `a16_sports-fan-interaction-scenarios-enc` | 56 | Positive, Negative, Scenario, Feedback Snippet, Log Version Snippet, Fan Gender, Fan Age, Hero Gender, Min Greatness, Max Greatness, Hero Min Age, Hero Max Age, Min Respect, Max Respect, plus one flag column per sport |
| `a16_sports-injuries-enc` | 92 | Index, Type, Uses Indefinite Article, Feedback Snippet, Log Version Snippet, Gender, Seriousness Score, Duration Score, Rehab Score, Pain Score, Is Debilitating, Can Heal Organically, Can Have Surgery, Can Be Career Ending, plus one commonness column per sport |
| `a16_sports-pivotal-moment-scenarios-enc` | 38 | seven sport flags, Position, Body, CTA, then per choice: Choice, Success Odds, Success Feedback, Success Narrative, Success Respect Impact, Failure Feedback, Failure Narrative, Failure Respect Impact |
| `a16_sports-practice-scenarios-enc` | 171 | Scenario, Log Version, seven `<Sport> Skills` columns (comma lists), Skill Impact, Injury Score |
| `a16_sports-stats-enc` | 46 | Sport, Stat, Integer Digits, Fraction Digits, Skills, Percentile 1 through 99 |
| `a16_sports-training-scenarios-enc` | 65 | Scenario, Log Version, Balance, Endurance, Flexibility, Strength, Fitness Impact, Injury Score |
| `a16_sports-trash-talk-scenarios-enc` | 49 | Scenario Inbound, Scenario Outbound, Recipient Gender, Min Greatness, Max Greatness, seven sport flags |

The nine pools, and what each one is for in the year:

* `a16_sports-awards-enc` - each row gated on the sport, a season window, a greatness
  window, sometimes a championship or a top-five finish, and often a comma list of eligible
  positions. Only a row whose `Add Award` flag is set can be won.
* `a16_sports-injuries-enc` - a seriousness, a duration, a pain score, four flag
  columns (`Is Debilitating`, `Can Heal Organically`, `Can Have Surgery`,
  `Can Be Career Ending`) and one commonness column per sport.
* `a16_sports-practice-scenarios-enc` - the row names the skills it trains in the sport's
  own `<Sport> Skills` column, so the hero only draws practices that mean something for
  his sport.
* `a16_sports-training-scenarios-enc` - four drill flags and a fitness impact.
* `a16_sports-stats-enc` - an integer width, a fraction width and eleven percentile
  columns from the 1st to the 99th.
* `a16_sports-coach-scenarios-enc` - positive and negative coach conversations with
  fitness and greatness windows.
* `a16_sports-fan-interaction-scenarios-enc` - positive and negative fan encounters with
  windows on greatness, respect and the hero's age.
* `a16_sports-trash-talk-scenarios-enc` - an inbound and an outbound version of each
  exchange.
* `a16_sports-pivotal-moment-scenarios-enc` - four choices, each with its own odds, its
  own feedback, its own narrative and its own respect impact.

### 80.2 The book

The season opens with arithmetic, not with a card, and the arithmetic is the only reason the
salary and the win-loss record mean anything:

1. `games` = the sport row's `Season Games`, or sixteen.
2. `edge` = `0.30 + (greatness - 20)/160 + (fitness - 40)/400`, less 0.15 while an
   injury is carried, clamped to the sport's own `Max Win Percent` at the top and to 0.20
   at the bottom. An injured star still plays badly.
3. `wins` = `games * edge * (0.65 + 0.35 * unit())` - a great player on a great team
   still loses games, and a bad one can win one.
4. the paycheque is the sport row's minimum salary plus the range scaled by greatness:
   `min + (max - min) * greatness / 100`, paid into the hero's money and banked in
   `sportEarned`.
5. playing is what improves a player: skill grows by one every season, plus another one while
   the hero is winning at least half his games. This is invented - the data has no growth
   curve - and it exists so that a long career can reach the gates the award table asks for.
6. greatness then tracks the numbers: after the stat sheet, greatness becomes
   `max(greatness, skill - 10 + below(16))`, and every season moves it by four, two or
   minus one depending on the record.

Every one of those facts is printed into the card as extra lines - `Season 2: 7-31 for
$9,905,000.` - so the record, the money and the stat sheet are visible in the same card as
the year's event.

### 80.3 The stat sheet

Up to five `a16_sports-stats-enc` rows of the hero's own sport are drawn. For each, the
hero's skill with a plus-or-minus five jitter becomes a percentile, and the value is linearly
interpolated between the two enclosing percentile columns - so a goalkeeper at skill 55 in a
league whose save ladder runs 0, 4, 9, 17, 30, 52, 78, 110 reads as a number between the 40th
and 50th percentile entries. The value is printed with the row's own integer and fraction
widths, which is why points per game can show a decimal and goals cannot. The whole sheet is
one extra line, e.g. `Season stats: 2 goals, 2 assists, 5 blocked shots, 33 tackles, 53
saves.`

The sheet is also what makes greatness move, so the stat line the player reads is the input to
the award he may or may not win later in the same chain.

### 80.4 The year

After the book and the sheet, the year is one event, first hit wins, rarest first:

| chance | event | what it does |
|---|---|---|
| 0.55 while hurt | still rehabbing | the injury clock ticks down; at zero the hero is cleared, loses the seriousness in skill, and the card says so |
| 0.02 | a career-ending injury | draws only from rows flagged `Can Be Career Ending`; sets the flag, ends the career and costs four times the seriousness in skill |
| 0.04 | an award | the sport's award rows, filtered on the season window, the greatness window, the championship and top-five requirements and the row's position list |
| 0.05 | an injury | weighted on the sport's own commonness column; stops the player for its duration score and prints the rehab line |
| 0.06 | a pivotal moment | one of the four choices the row fills in, with its own odds, its own feedback on the card and its own narrative in the journal |
| 0.06 | trash talk | inbound or outbound, with the other player's pronoun pointed at him |
| 0.08 | a word from the coach | positive or negative, filtered on the hero's gender, fitness and greatness |
| 0.10 | a fan | positive or negative, filtered on the fan's gender and the hero's age and respect |
| 0.14 | practice | only rows whose `<Sport> Skills` cell is non-empty; skill is the point of the card |
| 0.16 | training | the four drill flags add two each, the fitness impact lands on the season |
| otherwise | the season | a plain card carrying the record and the club |

Then the curve and the retirement. From thirty the skill drops by one plus a quarter of the
years over thirty plus a jitter; fitness starts dropping at twenty-eight; and at thirty-three
a hero under fifteen skill - or one the twelve per cent coin picks - retires. The Hall of Fame
is reached on eighty greatness, five awards or twelve seasons, and the card lists the trophy
case when there is one.

### 80.5 The tokens

The nine pools needed exactly one new token. `<opponent>` is not a person: the pools
write `a player from <opponent>` and `against <opponent>`, so it is another club,
drawn from the same teams table as the hero's own club and excluding it, memoised per card so
one card cannot name two different opponents, and answered inside `Sim::sportsToken` so
that the existing non-athlete guard still protects it. Everything else in the family was
already served: `<coach>` and `<teammate>` from the section-52 role list,
`<hero-name>`, the five bracket words, the pronoun pairs, and both number forms
(`<numN-M>` and `<decnumN-M>`).

### 80.6 The bugs

1. `sportRookie` did not exist. The season block was written without the field the
   Rookie of the Year gate needs, and the build stopped on exactly one error.
2. `numberToken` takes the inner text of a tag, not the bracketed tag. The first version
   of the shared-number fix passed `<num10-15>`, got a false back and silently did
   nothing, so one card read twelve sets of thirty bench presses while the journal read ten.
   The pools write the card column and the journal column from the same tag, so a shared draw
   - the section-76 actor fix, reused here - is what makes them agree.
3. Awards never fired. Greatness grew too slowly because skill never grew at all: the practice
   pool only fires about one year in seven, so the hero sat at thirty-odd skill for a whole
   career and never cleared the `Min Greatness` gates of forty, seventy and ninety-five.
   Fixing the growth put two awards into sixty scripted lives, which is the intended rarity.
4. The retirement card kept the event pool as its table and the event's narrative as its
   journal line, so a card could read `The end of the road` with the practice pool's
   table and a narrative about crunches. Retirement now forces its own line and is attributed
   to the sport row.
5. A card that opens on a pronoun arrived lower-case, because the pools write
   `<he/she> screams at you`. The body is upper-cased once at the tail of the function.

### 80.7 Shortcuts

* No draft, no contract, no trade and no free agency. The hero stays with the club section 52
  gave him, and the salary follows greatness alone.
* One opponent club per card, drawn from the ordinary teams table rather than a fixture list.
* The skill model has no per-skill values: `sportSkill` is one number, the four training
  flags are four more, and the percentile ladder is read through the single skill. The
  `<Sport> Skills` cell is shown on the card but not tracked.
* The `Rehab Score` column of the injuries table is unused (the port uses the duration
  score alone), and `Num in Team` and `Max Age` from the positions table are
  ignored.
* The award candidate list uses a flat four per cent, and the `League` column of the
  awards table (a numeric id) is not modelled.
* The fan and coach cards pay flat stat effects because those pools carry no score columns.

### 80.8 Verification

`/tmp/qdir/spores.py` walks thirty seeds in both genders to sixty with
`{"sports": true}` and reports `sports events 611 rc failures 0` with
`RESIDUE 0`. `/tmp/qdir/sssample.py` counts 842 season cards over forty seeds:
"The season" 389, "Practice" 75, "Training" 49, "Trash talk" 41, "The coach is not happy" 35,
"Still rehabbing" 33, "The Hall of Fame" 30, "The end of the road" 30, "A moment in the game"
29, "An injury" 28, "An angry fan" 27, "A fan" 26, "A career-ending injury" 20, "The coach is
happy" 14, "Back on the field" 14 and "An award" 2, so all fifteen branches fire. Every family
probe stays at residue zero (actres 668, astres 792, bizres 3574, bmres 1639, casres 1284,
cultres 1167, dlres 1538, fightres 1242, finres 762, hustres 1408, ltres 1566, luxres 849,
mafiares 1313, modres 589, outres 1025, petres 2472, racres 856, spores 611, spyres 558,
stures 1149, traderes 1316, vampres 3297, zoores 1442), `rescensus.py` reports a residue
census of zero, `coverage.py` still captures 109 scenarios of which 107 are recoverable,
`portgap.py` reports a gap of zero over 109 entries, `covpush.json` exits zero and
the bench reaches age thirty-four without an abort.

Some cards, sampled by hand:

    The season = A year of soccer with São Paulo.
    Season 2: 7-31 for $9,905,000.
    Season stats: 2 goals, 2 assists, 5 blocked shots, 33 tackles, 53 saves.
    I played 38 games for São Paulo and won 9.

    Training / You did 12 sets of 30 bench presses at the team training center.
    I did 12 sets of 30 bench presses.

    An award / Award: Rookie of the Year. For being the most outstanding rookie in the league.
    I won the Rookie of the Year for being the most outstanding rookie in the league.

    An injury / The diagnosis: a fractured orbital socket.
    I fractured my orbital socket.
    It will take 2 years of rehab.

    A moment in the game / It is 4th down and your team is near midfield, down six points with
    only two seconds left. The quarterback reminds everyone that a touchdown is needed to win,
    and you are out of timeouts. He then turns to you and asks whether you can get open.
    Run a slant route? Catch a medium pass
    I caught a medium pass on the last play of the game against Fave Gnolls and ran the ball
    into the end zone for the game-winning touchdown!

    The coach is not happy / He screams at you for synonymizing the team's name with defeat.
    Arianna X berated me for synonymizing our team's name with defeat.

    The Hall of Fame / After 18 seasons the Hall called.
    I retired after 18 seasons and went into the Hall of Fame.

    The end of the road / After 9 seasons, that was it.
    I retired after 9 seasons.

A note on the model: the season lines and the event lines are printed together, so a card can
carry a whole season - record, pay, stat sheet, fitness - before its headline event. That is
the intended shape: the year is the unit, the event is the story inside it.

## 81. The boat: a licence, a hull that ages, and four watercraft tables

Section 81 takes the last unwired family of the water and wires it as a yearly pastime. The
a16_watercraft family is small in files and large in rows: four tables plus the boat-licence
question pool, 569 rows in all. Its craft table joins to everything else by a set of nineteen
category flags, which is the single most important structural fact about it.

### 81.1 The family

| Table | Rows | Columns that matter |
| --- | ---: | --- |
| a16_watercraft-enc | 51 | Index, Type, Category, For Sale Description, Cost, Passengers, Feet, Lifespan Years, Commonness Score, Requires License, the nineteen category flags, Luxury Only |
| a16_watercraft-names-enc | 412 | Name, the nineteen category flags |
| a16_watercraft-parts-enc | 60 | Part, Plural Version, Only One Per Watercraft, Part Cost Score, Broken Terms, the nineteen category flags, Optional Description |
| a16_watercraft-danger-scenarios-enc | 21 | Title, Scenario, Log Version, Danger Score, Watercraft Lost, the nineteen category flags |
| a16_license-boat-questions-enc | 25 | Index, Question, Correct Answer, Incorrect Funny, Incorrect Decoy |

The nineteen flags are Rowing, Speedboat, PWC, Motorized Yacht, Sailing Yacht, Sailboat, Jet Boat,
Crab Boat, Pontoon, Research Vessel, Airboat, Amphibious, Cruise Ship, Icebreaker, Log Raft,
Submarine, Tugboat, Hovercraft and Pirate Ship. A row may carry several of them at once: a craft can
be both a Speedboat and a PWC, and a name row is joined to a hull only through the flags they share.

The port reads those flags in the data's own order (kBoatCategoryNames) and calls the first one set
the craft's category. A part, a name or a mishap is then only eligible when it carries the same
flag. That is the whole join: no id column anywhere in the family.

The family has exactly one token of its own, [watercraft], used 42 times across parts and mishaps
("riding in my [watercraft]", "hit an iceberg while riding in my [watercraft]"). Everything else in these cells
is already answered by the port: the pronoun pairs, <city>, <random-person>, <numN-M> and the
inline slash lists.

### 81.2 The craft

The hero gets a boat at fourteen with a 25,000 dollar windfall (see the shortcuts). The port then
draws the most expensive a16_watercraft-enc row that costs 25,000 or less and is not flagged Luxury
Only, weighted on Commonness Score, sets type, category, value, passengers and feet from it, pays
the cost, and names the craft from a16_watercraft-names-enc filtered on the same category.

The licence is a flag, not an exam you must pass: Requires License on the drawn row decides whether
the hero starts legal, and the exam card below exists to fix the ones who do not.

Condition starts at 100 and is the only number that really matters. It bleeds 1 to 3 points a year,
20 on a mishap survived and 35 on one that is not, and every repair price is derived from it.

### 81.3 The year

The year runs from fourteen, on a 0.55 chance, first hit wins, and the upkeep line is written
before the chain every single year:

- upkeep = value / 50 + 120, added to the log as "Upkeep cost me X and my Y sits at N%."
- condition loses 1 + below(3).

| Chance | Card | What it does |
| ---: | --- | --- |
| - | A wrecked hull | 0.6 chance to replace it with a budget of 25,000 + money / 4; otherwise a quiet year |
| 0.05 | The boat license exam | only while unlicensed; smart hero passes on 0.45 + smarts / 200 |
| 0.05 | one of the 21 mishaps | survive on 0.9 - danger * 0.09 + condition / 500; a lost hull wrecks the craft |
| 0.12 | Something broke | a part of the craft's category; cost = Part Cost Score * 400 + (100 - condition) * 8 |
| 0.12 | A refit | cost = Part Cost Score * 500, condition + 12, value + half the cost |
| 0.14 | A bigger boat | sells the old hull at 3/5 of value and buys a costlier one |
| 0.16 | A new name | a name row of the craft's category |
| else | A day on the water | the port's own card: "The NAME out past CITY with WHO." |

The mishap card is the family's dramatic one. A survive roll costs 20 condition and reports the
row's Log Version; a failure costs 35 and, when the row carries the Watercraft Lost flag, sets the
hull adrift: the type, name and value are cleared after that row's own log line has been resolved,
so the journal reads "My speedboat is gone." and the next year starts with a replacement card.

The licence card puts the question in the body, the correct answer as the call to action, and one
of the three answer columns as the choice. The pool's Incorrect Funny column is the joke answer
("Matches Outfit" for a flotation device) and Incorrect Decoy the plausible one; the port reports
whichever the hero did not write, so a failed exam reads "I wrote down: X. The answer was Y."

### 81.4 The tokens

| Token | Answer |
| --- | --- |
| [watercraft] / [watercraft]-possessive | the hero's craft type; if the craft is named, the possessive returns the name; fallback "boat" |

The guard answers only while the family renders (boatLive), so no other subsystem can pick the
token up, and the token carries the eleven-character -possessive suffix through the shared
llPossessive helper.

### 81.5 The bugs

None of the port's own. This is the first family that compiled clean on the first attempt, and the
probe found no residue on the first run either. Two things were still worth fixing after reading
the sampled cards:

1. **The redundant name.** When the name pool has no row for the craft's category - the fishing
   kayak and the WaveRunner both hit that case - the port falls back to naming the craft after its
   own type, and the purchase line then read "I bought a WaveRunner named WaveRunner". The two
   purchase lines now drop the " named X" clause when the two strings are equal, and the rename
   branch treats name-equals-type as a craft that was never really named, so its "A christening"
   title is reachable.
2. **The share of the fallback.** The day-out card is what the year does when nothing else fires,
   and it is 49 percent of all boat cards: the branch chances above are deliberately tuned to leave
   it that room, because a boat really is mostly a thing you use rather than a thing that happens
   to you.

### 81.6 Shortcuts

- **The 25,000 boat fund.** The scripted hero is poor in every life, so the family hands out a
  one-off stake at fourteen, the same device sections 77 and 79 used for the portfolio and the
  island. Without it the hero could not buy a hull at all.
- **Luxury Only rows are skipped** at the initial purchase; the yacht-and-helicopter bracket is
  section 79's business.
- **One craft at a time.** A bigger boat sells the old hull at three fifths of value; there is no
  market beyond that, no insurance, no mooring fee, and no second boat.
- **The upkeep formula is invented.** The data has no cost column for ownership; value / 50 + 120 is
  the port's own so that a dearer hull costs more to keep.
- **The day-out card is composed by the port.** The family has no benign scenario pool, so the
  fallback sentence is written in code, including the "moored at my island for lunch" line that
  only appears when section 79 has bought an island.
- **The city is the hero's city**, so a speedboat can be reported "out past Atlanta". The data has
  no coastal list to check against.
- **The Lifespan Years and Only One Per Watercraft columns are read but not yet enforced.**

### 81.7 Verification

The probe /tmp/qdir/boatres.py walks 30 seeds in both genders to sixty with the family enabled and
matches every rendered bracket against the corpus:

    boat events 1458 rc failures 0
    RESIDUE 0
    TABLES NEVER SEEN: []

All five pools appear as a card's own table: a16_watercraft-enc 998, parts 254, names 132, mishaps
62, licence 12. The card titles over the same run: A day on the water 718, A new name 132,
Something broke 128, A refit 126, Boat shopping 94, A new boat 90, No boat 64, A bigger boat 32,
The boat license exam 12, plus the mishap pool's own titles (Speed Demon 8, White Squall 6,
Midnight Fright 6, Revenge of the Birds 6, Flame Out, Big Wave, Direct Hit, Bad Muse, Party Down and
Shooting Star 4 each, Get Kraken 2).

The full regression is green with the family in: 24 probes at residue zero, the residue census at
"distinct residue tokens 0 total 0", coverage at 109 captured and 107 recoverable, the port gap at
zero, the coverage push at exit 0 and the benchmark reaching age 34.

Sample cards:

    A day on the water
      The Dolce Vita out past Atlanta with a friend.
      I took my speedboat out for the day with a friend.

    Something broke
      You snapped the steering wheel off while testing how tight a circle you could make in the water.
      I replaced the steering wheel on my speedboat for $840.

    Something broke
      Your throttle isn't performing like it used to and is causing some embarrassing dysfunction in the water.
      The throttle on my speedboat is broken and I cannot afford $856.

    The boat license exam
      What does this marker signify?
      Correct answer: Dangerous Area
      I passed my boat license exam.

    The boat license exam
      What is the most important thing to consider when putting on a Personal Flotation Device?
      I wrote down: Matches Outfit. The answer was Size and Fit.
      I failed my boat license exam.

    A refit
      The GPS system goes on the speedboat.
      I could not afford the GPS system at $1,000.

    A bigger boat
      The fishing kayak replaces the paddle boat.
      I sold the paddle boat and bought a fishing kayak for $600.

    A new boat
      The WaveRunner is mine.
      I bought a WaveRunner for $10,500.

    Get Kraken
      While out enjoying calm seas in your speedboat, you suddenly feel the boat jerk hard to the right.
      You look starboard to discover a massive squid has latched its tentacles to the side of your boat and is pulling it down.
      I brought my speedboat home through that.

    Da Bomb
      While enjoying the ocean with some friends aboard your speedboat, you notice what looks like a sea mine just up ahead.
      You attempt to divert, but it's too late, and the mine detonates, blasting a massive hole in your boat.
      My speedboat is gone.
      I struck a sea mine in my speedboat, and it blasted a massive hole in my boat.

## 82. The sentence: a yard with its own economy, nine ways to spend a year inside, and ten prison tables

Section 45 put the hero in prison: a conviction card, a countdown, the passing of
years. It never put anybody else in the cell. The four pools that give the yard its
cast -- nicknames, gangs, letters, gifts -- were sitting in the data unwired, and with
them the parole board. This section wires all seven of the remaining prison tables
(the four above plus the parole questions, the jobs pool being already wired) and gives
the sentence a year worth reading.

### 82.1 The family

| table | rows | columns read by the port |
| --- | --- | --- |
| a16_prison-bully-inbound-scenarios-enc | 48 | Scenario, Log Version, Gender, Bully Action, Hero Action |
| a16_prison-bully-outbound-scenarios-enc | 42 | Scenario, Log Version, Gender, Will Do, Will Let |
| a16_prison-gangs-enc | 10 | Gang Index, Male Name, Female Name, Type, Description, Short Description, Power, Violence, Required Respect |
| a16_prison-gifts-enc | 64 | Name, Short Name, Use Some Article, Is Plural, From Lover Only, Crazy Gift, Negative Gift, Gender, Confiscate Score, Min Age |
| a16_prison-jobs-enc | 38 | Job, Description, Daily Rate, Commonness Score (already wired in section 45) |
| a16_prison-letters-enc | 60 | Audience, Topic, Min Relationship Strength, Max Relationship Strength |
| a16_prison-nicknames-enc | 279 | Nickname, Gender, Is Prefix |
| a16_prison-parole-questions-enc | 10 | Commonness, Question, CTA, Choice 1..20, Choice N Score |
| a16_prisoner-crimes-enc | 138 | Crime, Seriousness Score, Is Violent, Commonness Score (section 45) |
| a16_prisoner-juvenile-crimes-enc | 86 | Crime, Seriousness Score, Is Violent, Commonness Score (section 45) |
| a16_dumb-scenarios-prison-enc | 64 | the section 45 dumb-scenario pool |

Seven hundred and twenty-three rows in the ten prison pools, plus the two crime
tables and the dumb pool. Thirty of the 279 nicknames are prefixes (Chunky, Cock-Eyed,
Fat, Stanky), which is the one piece of structure in an otherwise flat list.

### 82.2 The sentence

The sentence itself is section 45's. A crime rolls once a year from eighteen at 2%,
the crime's Seriousness Score picks a term from 1/2/3/5/10 years, and the year logs the
job, the yard and the countdown. What this section adds is the probe hook: a script may
ask for a sentence with "prison": true, which raises the conviction roll to certainty,
forces a minimum of eight years, and marks the life as served once. It exists so the
thirty-seed probe can spend a life inside instead of waiting for a 2% roll. It is a
harness, not behaviour: without the flag the table above is the game's.

### 82.3 The year

The years inside are a first-hit-wins chain, rarest first, on top of the job:

| chance | branch | what it draws |
| --- | --- | --- |
| 0.35 | a job (only when none is held) | a16_prison-jobs-enc, weighted on Commonness Score |
| always | the job's pay | Daily Rate x 365, e.g. 3 x 365 = 1,095 a year |
| 1 | Intake (only while nameless) | a16_prison-nicknames-enc filtered on the hero's gender; a prefix row becomes prefix + surname (Fat Iglesio) |
| 0.60 | The parole board (last two years, three tries) | a16_prison-parole-questions-enc weighted on Commonness; four of its answers are offered, the chosen score decides |
| 0.12 | The gang's business (members only) | respect += Power/25, and a scuffle when Violence >= 50 |
| 0.12 | The strong arm | a16_prison-bully-outbound-scenarios-enc |
| 0.18 | A gift | a16_prison-gifts-enc filtered on gender, Min Age and From Lover Only |
| 0.22 | Trouble in the yard | a16_prison-bully-inbound-scenarios-enc |
| 0.25 | A letter | a16_prison-letters-enc filtered on audience and relationship strength |
| 0.40 | A gang offer | a16_prison-gangs-enc with Required Respect <= the hero's respect, weighted on Power |
| else | the section 45 dumb pool | a16_dumb-scenarios-prison-enc |

The parole board is the only branch with real consequences: a score of 4 or 5 releases
the hero, a 3 is a delay, and a 1 or 2 costs three points of respect. Gangs are the
other ladder: the ten rows ask for respect from 0 (Wrinkly Sneaks) to 90 (Bitizen
Mafia, Killbillies), so the yard hands out membership the way the rest of the game
hands out a promotion.

Every branch writes the drawn inmate, the letter's writer or the gift's sender into
the card and into the journal line, so the two columns of the same row agree.

### 82.4 The tokens

The family needs no new token: its only keys are <name> and <name-possessive> (a
hundred and eighty uses between them), and <name> is answered globally, as the
citizen's first name, further up the resolver. What it does need is a rule about
repetition. A prison row often carries the same reference token in both of its
columns -- a16_prison-bully-outbound's "called <him/her> <insult>" is written in the
Scenario and again in the Log Version -- and resolving each column separately draws
two different insults: the card said the guard was called a dead dick while the
journal said count dickula. The year's bindRefs step draws each of the row's reference
tokens (insult, musician, celebrity, drug-illegal and the rest) exactly once and
substitutes the same value into both cells before either is resolved.

The pronoun pairs need no work either: an inmate is drawn per card, its gender is
flipped into the row while the card renders, and <rb>/<rb2> fall back to the same
draw because the card's columns are resolved in order.

### 82.5 The bugs and the notes

1. The gang's business card was an event with an empty table: the branch set no
   row, so the journal entry claimed nothing had happened. It now marks
   a16_prison-gangs-enc.
2. The two-instruction insult above -- the single most visible defect of the family,
   because the same row is printed twice on the same screen.
3. The release line was glued onto the card's narrative with a newline, so one
   journal entry read "The board voted to release me on parole\nI was released from
   prison." It is now its own line, and main.cpp prints it before the narrative.
4. A tooling note worth keeping: the event's row id is the CSV file index, with the
   header at index 0, so a debug script that subtracts one to look the row up reads
   the neighbour and concludes the wrong thing. I spent a round convinced the thirty
   prefix nicknames never fired -- they fire, sixteen times in sixty sentences.
5. Data notes. A gift row's Name is written without an article in one case ("I got
   letter of support in the canteen"). The bully tables' Gender column is blank on
   forty-two of the forty-eight inbound rows, so it is read as the hero's gender
   rather than the other inmate's.

### 82.6 Shortcuts

- The forced sentence and the certain conviction are the probe's, as above.
- The parole board offers four of the eighteen to twenty answers a question holds,
  chosen uniformly; the real screen offers the whole list.
- The bully pools' Gender column is read as the hero's gender (the data is nearly
  empty there, see above).
- A letter's Topic cell is folded to the first person by a literal replacement of
  " you" and " your", because toFirstPerson only rewrites a leading subject.
- There is no gang war, no shiv, no solitary, no yard economy and no canteen to spend
  the pay in: prisonCanteen and the rest of the section's state exist for the card to
  write, not for a system behind them. In particular prisonCache_ and prisonLive_ are
  declared and cleared but never read -- the family has no token that is only valid
  while a prison card renders.
- A prison job pays Daily Rate x 365 regardless of the passed in-game year length.

### 82.7 Verification

The probe is /tmp/qdir/prisres.py: thirty seeds times both genders to age sixty with
{"prison": true}, family tables only, every angle bracket left in a title, body, log
line or extra line counted.

    prison events 420 rc failures 0
    RESIDUE 0

Last-mark histogram:

    a16_prison-gangs-enc                            88
    a16_prisoner-crimes-enc                         60
    a16_prison-nicknames-enc                        60
    a16_prison-bully-inbound-scenarios-enc          58
    a16_prison-letters-enc                          53
    a16_prison-parole-questions-enc                 52
    a16_prison-bully-outbound-scenarios-enc         34
    a16_prison-gifts-enc                            15

Every one of the seven new pools is a card's own table. Full regression after the last
build: allprobes 25/25 with residue 0, rescensus 0 distinct residue tokens,
coverage 109 captured / 107 recoverable, portgap gap 0, covpush rc 0, bench rc 0 to
age 34.

Sample cards:

    I have been convicted of fishing without a license and sentenced to 8 years in prison!

    Intake
    The yard settles on a name: Fat Iglesio.
    I was given the nickname Fat Iglesio in prison

    A gang offer
    Brain Brothas: You must have very high smarts to join this gang.
    I turned down the Brain Brothas

    Trouble in the yard
    Your cellmate, Ladye Royal, will never let you choose what to watch on the TV
    because he is always watching "13 Reasons Why".
    My cellmate, Ladye Royal, wouldn't let me choose what to watch on the TV.

    The strong arm
    You told a prison guard that Ladye Royal called him a peon.
    I told a guard that Ladye Royal called him a peon.

    The parole board
    Do you feel that you've been rehabilitated?
    The board threw my file in the bin

    The parole board
    What has been the best part of prison?  (chosen: I've learned a lot)
    The board voted to release me on parole
    I was released from prison.

    A letter
    A letter arrived from Beyawnce Gnolls.
    Beyawnce Gnolls told me he misses me

    A gift
    Maylor Shift sent me some ramen noodles.
    I got some ramen noodles in the canteen

    The gang's business
    The Brain Brothas kept the yard quiet.
    I did my part for the Brain Brothas

## 83. The office: a staff to manage, eight employee tables, and a year on the other side of the desk

Every career in this port has been the hero's own: the ladder they climb, the salary
they are paid, the title they earn. The a16_employee family is the first one written
from the other chair. Its scenarios address the hero as somebody's manager -- "You
threatened to fire <employee>", "<employee> told you that you suck in front of a bunch
of people", "You sent an email to all <company> employees that lauded the service of
<employee>" -- and the port had no staff for those sentences to be about. This section
gives it one, and wires all eight employee tables.

### 83.1 The family

| table | rows | columns read by the port |
| --- | --- | --- |
| a16_employee-types-enc | 202 | Industry, Job Title, Influence Score, Gender, Min Age, Max Age, Commonness Score |
| a16_employee-interview-questions-enc | 76 | Commonness Score, Question, Choice 1..5, Choice N Score |
| a16_employee-behavior-bad-scenarios-enc | 65 | Scenario, Log Version, Score |
| a16_employee-behavior-good-scenarios-enc | 57 | Scenario, Log Version, Score |
| a16_employee-respect-scenarios-enc | 40 | Scenario, Log Version, Can Be Reaction, Can Be Push, Score |
| a16_employee-disrespect-scenarios-enc | 37 | Scenario, Log Version, Can Be Reaction, Can Be Push, Score |
| a16_employee-warn-scenarios-enc | 25 | Scenario, Risk Score, Commonness Score |
| a16_employee-recognition-scenarios-enc | 14 | Scenario, Commonness Score |

Five hundred and sixteen rows in eight tables, and after this section every one of them
is a card's own table. The types pool is the big one: it is the game's job catalogue,
two hundred titles across industries from brewing to pharmaceuticals, each with an
Influence Score, a gender gate, an age window and a Required Education column the port
does not evaluate (see the shortcuts).

### 83.2 The office at eighteen

A script asks for a managerial life with "office": true, which calls makeOffice. It
draws one row of the types catalogue, filtered on the hero's gender and age and
weighted on Commonness Score, and takes from it the hero's title and industry, the
staff's pay (32,000 + Influence Score x 7,000 -- a Budtender's influence of two is
46,000, a Ganjier's five is 67,000) and the ladder. It then draws the report the year
will be about, another title from the same industry, and an employer name.

The ladder deserves a note: makeOffice sets the hero's career track to their own job
title, so careerYear -- which hands out a job at eighteen and promotes every four
years along a comma-separated track -- sees a track with no rungs and stays silent.
The manager is not given a second, parallel job by the rest of the port.

### 83.3 The year

The year is a boss's year: the salary is paid, the staff's morale drifts back toward
fifty, one named employee is the subject of the card, and a first-hit-wins chain picks
what happened.

| chance | branch | what it draws |
| --- | --- | --- |
| 0.20 | A dismissal (only at performance below 35 or three warnings) | nothing: the file is closed |
| 0.25 | A goodbye (only below 45 morale) | nothing: somebody resigns |
| 0.18 | An interview (only below five reports) | a16_employee-interview-questions-enc, four answers offered |
| 0.12 | A written warning | a16_employee-warn-scenarios-enc, weighted on Commonness Score |
| 0.14 | A word of praise | a16_employee-recognition-scenarios-enc |
| 0.16 | A problem with the staff | a16_employee-disrespect-scenarios-enc, pushed back on or let go |
| 0.16 | The staff is with me | a16_employee-respect-scenarios-enc |
| 0.18 | A behaviour problem | a16_employee-behavior-bad-scenarios-enc, performance -3 x Score |
| 0.18 | A good year for the team | a16_employee-behavior-good-scenarios-enc, performance +3 x Score |
| else | Another year at X | a16_employee-types-enc: who the report still is |

Three numbers carry the state: performance (dismissal below 35), morale (resignation
below 45, and it drifts back to 50 every year) and the warning count (three warnings is
a dismissal on its own). Performance at seventy or more earns a raise of five per cent
of salary, which is added to the card whatever it turned out to be.

The interview is the only branch that asks the hero a question. Four of the row's five
answers are offered, a scripted choice indexes them or a coin picks one, and a score of
four or better hires: the answer the candidate gave is quoted back into the journal
("I hired Lissie Gnomez as a Ganjier", or "The candidate said interested, so there was
no hire").

### 83.4 The tokens

The family needs five keys and they are all the manager's: <employee> and
<employee-possessive> (four hundred and thirty-five uses between them, more than half
of the family's text), <company>, <industry> and <role>. The guard answers them only
while an office card renders, because <employee> already has an owner further down the
resolver: the business pool mints a fresh name for it on every call, which is exactly
wrong here. The office year draws the report once and memoises it, so the card that
names them twice names the same person twice.

The pronoun pairs are the family's other trap. Every pool in it speaks about the
employee -- "<employee> has been half-assing <his/her> work", "You told <employee>
... <he/she>'s out of here" -- so the year sets the other-pronoun pair to the
employee's gender for the length of the card and restores it afterwards. Before that
fix a female report was written about with the male hero's pronouns, which is how a
line read "You wrote Jihanna Iglesio a strongly-worded letter about how detrimental
his performance is".

### 83.5 The bugs and the notes

1. Hero already had a field called office: the elected office of section 55. The
   duplicate member was a compile error, and the new flag is ofcManager.
2. The employer name came out as "the company" for every industry, because
   companyName() keys on a Company Type and the types catalogue holds an Industry.
   The business pool's own name generator is the fallback, which is why the sample
   cards name their employer ("My job as a Ganjier at Puff Puff Pass Marijuana paid me
   $67,000 this year").
3. The resignation branch was dead code in the first build: morale drifted up to 55
   and nothing pushed it down hard enough to reach the threshold. The drift target is
   now 50 and a warning costs four points of morale instead of two.
4. The base loop has its own recognition line: an employed hero gets a 25% draw on
   a16_employee-recognition-scenarios-enc from the old core-loop code, which still
   fires and marks the same table. The office's praise branch is separate, so that
   table shows up in the probe's histogram from two call sites.

### 83.6 Shortcuts

- Required Education, One Per Facility and the age window's upper bound are read for
  the hero's own title but never enforced against the hero's smarts.
- The staff is a count plus one named report, not a roster: the port tracks how many
  people answer to the hero (two to five at eighteen, one more per hire) and draws the
  card's subject from scratch every year.
- The interview offers four of the row's five answers and a score of four or better
  hires; the real screen offers the whole list against the candidate's own profile.
- A raise is a flat five per cent of salary, and there is no firing budget, no HR, no
  severance and no office politics beyond the two pools above.
- A resignation and a dismissal both just decrement the headcount; nobody is
  remembered by name once they are gone.

### 83.7 Verification

The probe is /tmp/qdir/ofcres.py: thirty seeds times both genders to age sixty with
{"office": true}, employee tables only, every angle bracket left in a title, body, log
line or extra line counted.

    office events 1726 rc failures 0
    RESIDUE 0

Last-mark histogram:

    a16_employee-types-enc                          616
    a16_employee-recognition-scenarios-enc          288
    a16_employee-interview-questions-enc            201
    a16_employee-respect-scenarios-enc              144
    a16_employee-warn-scenarios-enc                 136
    a16_employee-behavior-bad-scenarios-enc         132
    a16_employee-disrespect-scenarios-enc           118
    a16_employee-behavior-good-scenarios-enc         91

Card titles over the same run: another year at X 522 spread across the catalogue's
employers, an interview 201, a word of praise 152, the staff is with me 144, a written
warning 136, a behaviour problem 132, a problem with the staff 118, a good year for
the team 91, a dismissal 79, a goodbye 30.

Full regression after the last build: allprobes 26/26 with residue 0, rescensus 0
distinct residue tokens, coverage 109 captured / 107 recoverable, portgap gap 0,
covpush rc 0, bench rc 0 to age 34.

Sample cards:

    A written warning
    You wrote Jihanna Iglesio a strongly-worded letter about how detrimental her
    performance is to her future at the company.
    I gave Jihanna Iglesio a written warning

    A word of praise
    You told Carli Letras she is integral to the company's success.
    I praised Carli Letras in front of the staff

    A problem with the staff
    Jihanna Iglesio let the elevator door close on you.
    I pushed back, and the room went quiet.
    Jihanna Iglesio failed to hold the elevator door open for me.

    The staff is with me
    Papi Moutha told you he appreciates you so much that he is willing to work
    holidays for you.
    Papi Moutha offered to work holidays to show his appreciation for everything
    I've done for him.

    A behaviour problem
    Lissie Gnomez has been half-assing her work lately.
    I noticed Lissie Gnomez has been half-assing her work.

    A good year for the team
    Jihanna Iglesio has spent time familiarizing herself with the company so she can
    answer her co-workers' questions more efficiently.
    I noticed Jihanna Iglesio has become more of an asset to the company.

    An interview
    How would you describe yourself in one word?   (chosen: Interested)
    My job as a Ganjier at Puff Puff Pass Marijuana paid me $67,000 this year.
    I hired Lissie Gnomez as a Ganjier

    A dismissal
    The file on Lissie Gnomez was thick enough.
    I let Lissie Gnomez go after a bad year

    A goodbye
    Jihanna Iglesio handed in a resignation while the office was quiet.
    Jihanna Iglesio handed in their notice

    Another year at Chateau Doozy
    Lissie Gnomez is still a Quality Assurance Specialist, and the office ran itself.
    I managed 4 people for another year

## 84. The school: eleven pools of class, clique and rush, and a gender token that had been answering nothing

Sections 50 to 83 built the adult life outwards: work, money, fame, prison. This one fills
the years that decide what the adult has to work with. Section 84 wires the whole
`a16_school-*` family, eleven tables and 513 rows, plus one root fix in
`Sim::heroToken` that had been silently starving a gendered filter in three earlier
families.

### 84.1 The family

Eleven tables, all `<name>-enc` under `/Users/Adam/Documents/bitlife/_re/gamedata_plain`,
513 rows. Every one of them was unwired before this section.

| table | rows | the columns that matter |
|---|---|---|
| `a16_school-activities-enc` | 115 | Index, Activity, Type (Arts / Athletics Club / Athletics Team / Career Interest / Community / Hobby), Uses Definite Article, Proper Noun, Leader Title, Other Officers, Reference Noun (club / team / band), Track Performance, Commonness, Gender, Specific Countries, Weekly Hours, Sports / Nerd / Band / Arts / Acting / Gaming / Weeb Activity, Min Health / Smarts / Athleticism / Looks / Popularity / Grades, Popularity Score Male / Female, Requires Tryouts, Exists in Middle School / High School / College |
| `a16_school-act-up-scenarios-enc` | 57 | Type (the class subject), Feedback Scenario, Log Version, Third Person, Seriousness Score, Elementary School, High School, College |
| `a16_school-teacher-suckups-enc` | 54 | Feedback Scenario, Log Version, the three level flags, Flirt Scenario, Teacher Only, Administrator Only |
| `a16_school-frats-enc` | 50 | Index, Organization, Gender (female 22 / male 28), Letters, Min Smarts, Min Looks, Min Popularity, Popularity Score, Nickname, Motto |
| `a16_school-frat-questions-enc` | 50 | Question, Choice 1..4, Choice N Score (one 5 and three 0 per row) |
| `a16_school-teacher-disrespects-enc` | 48 | Type (General 25 / Administrator 23), Feedback Scenario, Log Version, the three level flags |
| `a16_school-teacher-call-outs-enc` | 41 | Type, Feedback Scenario, Log Version, the three level flags, Inappropriateness Score |
| `a16_school-principal-warnings-enc` | 35 | Warning (one column: `<principal> warned you to behave yourself.`) |
| `a16_school-classmate-mess-scenarios-enc` | 23 | Feedback Scenario, Log Version, Hero Min / Max Age, Classmate Gender |
| `a16_school-classmate-flirt-scenarios-enc` | 22 | as the mess pool, plus Clean (set on 19 of 22) |
| `a16_school-cliques-enc` | 18 | Index, Clique, Popularity, Aggression, Description, Short Description, the eight Requires* Activity flags, Requires Skateboarding Club / Trouble / No Activities, Min / Max Athleticism / Grades / Smarts / Looks / Popularity / Happiness, Gender, Popularity Score |

The clique ladder as the data has it, in file order: Jocks 100/100, Nerds 20/5, Hipsters
50/15, Popular Kids 100/35, Social Floaters 80/15, Talented Kids 60/5, Drama Kids 30/5,
Brainy Kids 10/2, Artsy Kids 15/5, Normals 20/10, Band Geeks 5/5, Mean Girls 90/100,
Weebs 5/5, Gamers 35/15, Skaters 55/50, Troublemakers 20/80, Goths 10/30, Loners 0/20.

### 84.2 A pupil from five

`Sim::makeSchool` runs when the script asks for `"school": true`: it sets
`Hero::schooling`, the level from the age, grades `62 + below(25)`, popularity
`42 + below(17)`, a zeroed trouble / act-up / detention / suck-up count, three named
people (a teacher, a classmate and a principal, each with a gender coin), and one activity
out of the 115-row catalogue gated on the level column and the row's own Gender, weighted on
Commonness. That activity is the one the clique requirements and the activity card are read
against for the whole life.

The ladder is derived from the age, since the data has no level table:

| age | level | the year is filed under |
|---|---|---|
| 5 to 10 | 1 elementary | Elementary School |
| 11 to 13 | 2 middle | Elementary School |
| 14 to 17 | 3 high | High School |
| 18 to 22 | 4 college | College |

Above 22 the pupil leaves and `Hero::schooling` is cleared. Before 5 the year does
nothing at all - and that detail is the bug in 84.5.

### 84.3 The year

`Sim::schoolYear` runs from `ageUp` behind `hero.schooling`, takes a 0.6
chance, and every year first does the bookkeeping: a level change logs
`I started <lvlName>.`, grades move +1 (with a 0.3 coin for a second point, or -1 when
trouble is at 3 or more), popularity drifts one point back toward 50, and trouble decays by
one. Every card then ends with `My grades are N and my popularity is N.`, and the tail
appends a full stop unless the body already ends in one of `. ! ? " )`.

| chance | branch | what it draws |
|---|---|---|
| 0.08 | rush week | college only, and only while the hero has no fraternity: a `a16_school-frats-enc` row passing the gender coin and the Min Smarts / Min Looks / Min Popularity windows, weighted on Popularity Score; three questions from `a16_school-frat-questions-enc`, each offering its four filled choices, and two right out of three accepts the bid (name, letters, nickname, +4 popularity) |
| gate trouble >= 2, then 0.5 | the principal's office | a `a16_school-principal-warnings-enc` warning, with the principal's own pronoun, folded to the first person |
| 0.10 | disrespecting a teacher | `a16_school-teacher-disrespects-enc` on the level flag; the rows whose Type is Administrator are titled `Trouble with the administration` |
| 0.12 | acting up in class | `a16_school-act-up-scenarios-enc` on the level flag; the Type names the class and titles the card, Seriousness Score scales trouble, popularity and happiness, and a 0.35 coin adds a detention |
| 0.12 | called out in class | `a16_school-teacher-call-outs-enc` on the level flag, Inappropriateness Score scaling the popularity hit |
| 0.12 | a prank on a classmate | `a16_school-classmate-mess-scenarios-enc`, Hero Min / Max Age and Classmate Gender windows |
| 0.12 | something in the air | `a16_school-classmate-flirt-scenarios-enc` with the same windows; the rows that are not marked Clean are only reachable from 13 |
| 0.14 | lunch with a clique | `a16_school-cliques-enc` while the hero is in none: the Min / Max windows, the Gender, Requires Trouble at 2 or more, Requires No Activities, and the eight Requires* Activity flags read against the hero's own activity row |
| 0.14 | teacher's pet | `a16_school-teacher-suckups-enc` on the level flag; a Flirt Scenario row is only reachable from 15, and the Teacher Only rows are preferred |
| 0.16 | the activity | the hero's own `a16_school-activities-enc` row: a season win or loss when the row has Track Performance, else a 0.25 leader election with the row's Leader Title, else a plain hours-a-week line |
| else | an ordinary year | `A year at <lvlName>`, which marks the hero's activity row so the card is attributed to a real table |

### 84.4 The tokens

`teacher` 278 uses, `teacher-possessive` 156, `classmate` 127,
`principal` 35, `classmate-possessive` 16, plus the global pronoun pairs
(`he/she` 107, `his/her` 63, `him/her` 55), `rb` 72 and a few one-off
inline lists. No reference pool in the game carries a teacher, a classmate or a principal, so
the three are answered by the family: `schoolIsToken` claims
`teacher` / `classmate` / `principal` (and their `-possessive` forms)
while a school card is rendering, and `Sim::schoolToken` memoises each one in
`schoolCache_` for the length of the card, so the body and the journal line name the
same person. `personName` gained `principal` so the fallback draw is a real name.

The three level columns are read through `schoolLevelColumn`: college for level 4, High
School for level 3, and the Elementary School flag for levels 1 and 2 - the data has no
middle-school column at all.

### 84.5 The bugs

1. **The script flag enabled the office.** The school parse block was written by copying the
   office block, so it read `"school"` but set `spec.office`. The probe reported
   zero school events with no error anywhere: the office simply ran instead. Both assignments
   in the copied block now say `spec.school`.
2. **The age gate switched the pupil off at birth.** `if (level == 0) { h.schooling = false; return false; }`
   cleared the flag on the hero's very first birthday, when the level is legitimately 0
   (school starts at 5), so the family never fired for any seed. It now only leaves when the
   age is past 22.
3. **`heroToken("gender")` answered nothing.** `Sim::heroToken` has cases for the
   city, the country, the demonyms and the hero's names, but no case for the hero's gender -
   it fell through to `return std::string();`. Every filter written as "skip the row
   unless its Gender cell is empty or matches the hero" therefore skipped *every* row that
   carries a gender: the fraternity pool was empty for every hero of every seed
   (`houses=0` in the instrumented run), which is what made rush week unreachable even
   at a test chance of 0.9. The fix is one case in `heroToken` -
   `h.female ? "female" : "male"` - and it is a root fix: sections 75 (racing) and 76
   (the actor) had the same silent filter against the same empty string, so their gendered
   pools now draw from their gendered rows too. The whole regression stayed at residue 0
   afterwards.
4. **Branch order.** Rush week was numbered second, after the principal's office, and an
   instrumented run with the principal branch boosted for a test starved it completely. The
   chain is rarest-first: rush (0.08) now sits first.
5. **The hero has no athleticism.** `a16_school-cliques-enc` gates on Min / Max
   Athleticism, and `Hero` carries no such attribute (only smarts, looks, health,
   happiness, karma and discipline plus each family's own). The clique reads
   `h.health` instead, with a shortcut comment at the call site.
6. **A question mark got a full stop.** The card tail appended `.` to any body not
   already ending in a period, so a rush question rendered as
   `What Greek mythological hero killed the Minotaur?.` The tail now accepts
   `. ! ? " )`.

### 84.6 Shortcuts

* The activity catalogue's Min Health / Smarts / Athleticism / Looks / Popularity / Grades
  and Requires Tryouts columns are never enforced; the level column, the Gender column and
  Commonness are the whole filter, and the hero has exactly one activity for life.
* The middle-school years use the Elementary School flag because the data has no
  middle-school column.
* A clique's Popularity and Aggression numbers are descriptive only - the port uses the
  Min / Max windows and the Requires* flags, which is what decides membership anyway.
* Rush offers three random questions and accepts on two right; the real screen asks a fixed
  set. The four choices come from the row's own Choice 1..4 and their Choice N Score.
* Detentions, GPAs, report cards and school fees are not modelled; the year's whole visible
  bookkeeping is the grades / popularity line.
* The principal's warning is a single generic column, so the card body and the journal line
  are the same sentence folded to the first person.

### 84.7 Verification

`python3 /tmp/qdir/schres.py` - thirty seeds, both genders, to age 60 with
`{"school": true}` - reports

    school events 583 rc failures 0
    RESIDUE 0

with the last-mark histogram `a16_school-activities-enc` 263,
`a16_school-act-up-scenarios-enc` 68, `a16_school-classmate-mess-scenarios-enc` 52,
`a16_school-teacher-disrespects-enc` 51,
`a16_school-classmate-flirt-scenarios-enc` 38,
`a16_school-teacher-call-outs-enc` 38, `a16_school-teacher-suckups-enc` 35,
`a16_school-cliques-enc` 28, `a16_school-principal-warnings-enc` 6,
`a16_school-frat-questions-enc` 4. The only table that is never a card's own mark is
`a16_school-frats-enc`: it is drawn inside the rush branch, where the card is the
interview and the fraternity supplies the house, the letters and the nickname.

The full sweep is green: `allprobes.py` 27 of 27 probes at residue 0 (actres 667,
astres 797, bizres 3578, bmres 1648, boatres 1456, casres 1286, cultres 1168, dlres 1532,
fightres 1243, finres 760, hustres 1412, ltres 1578, luxres 849, mafiares 1310, modres 589,
ofcres 1726, outres 1029, petres 2472, prisres 428, racres 857, schres 583, spores 611,
spyres 564, stures 1145, traderes 1326, vampres 3297, zoores 1440), `rescensus.py`
`distinct residue tokens 0 total 0`, `coverage.py` `captured scenarios: 109  recoverable: 107`,
`portgap.py` `ENTRIES 109 rendered-by-port 107 gap 0`, `covpush.json` rc 0
with 33,078 bytes of stdout and an empty stderr, and `bench.sh` rc 0 through to age 34.

Sample cards, hand-checked:

    A year at high school            Another year of class, lunch and the walk home.
      My grades are 87 and my popularity is 51.
      I kept my head down at high school and passed my classes

    Acting up in class               You ripped out the pages of Emmett Nym's class textbook
                                     one-at-a-time and yelled, "Good riddance!" as each page
                                     was ripped out.".
      My grades are 79 and my popularity is 51.
      I acted up in Emmett Nym's class.

    Trouble with the administration  You told Kay Dot that you don't trust an educator who
                                     plays favorites.
      My grades are 77 and my popularity is 60.
      I told Kay Dot that I don't trust an educator who plays favorites.

    Something in the air             You gently touched Duke Combs on the hand while talking
                                     with her, so that she might realize you like her.
      My grades are 81 and my popularity is 60.
      I touched Duke Combs on the hand in an attempt to flirt with her.

    Teacher's pet                    You told Kay Dot that you truly hope he has an amazing
                                     weekend.
      My grades are 84 and my popularity is 57.
      I told Kay Dot that I hope he has an amazing weekend.

    Lunch with the Jocks             An athletic clique.
      My grades are 87 and my popularity is 77.
      I started hanging around with the Jocks

    Election at the Robotics Club    The Robotics Club needed a President.
      My grades are 75 and my popularity is 52.
      I was elected President of the Robotics Club

    Called out in class              During the school pep rally, Catrina Woodworker asks
                                     that you stop cheering because you sound like a dying cat.
      My grades are 77 and my popularity is 51.
      Catrina Woodworker embarassed me in front of the class.

    Rush week                        What Greek mythological hero killed the Minotaur?
      My grades are 79 and my popularity is 50.
      I answered 1 of 3 right at rush and did not get a bid

    The principal's office           Haden Smith encourage you to do the right thing.
      My grades are 84 and my popularity is 52.
      Haden Smith encourage me to do the right thing.

Two quirks worth naming: the `a16_school-act-up-scenarios-enc` row above really does end
its cell with `."` followed by a period in the source data, and the
`a16_school-principal-warnings-enc` rows are written in the third person
(`<principal> encourage you to do the right thing`), so folding them to the first person
leaves the data's own verb form behind.
