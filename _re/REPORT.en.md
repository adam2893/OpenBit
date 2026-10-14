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
6. [obs] The hooked addresses are **not** method entries: `Game.UIIAPManager::GodModePurchased`'s methodPointer sits at file offset 0x31e24e8 and its first 16 bytes (`fe0f1bf8 fa6701a9 f85f02a9 f65703a9`) match the file, unpatched; likewise `BitizenshipPurchased`(0x31e1b54), `CareerActorPurchased`(0x31e66e8), `GoldenPacifierPurchased`(0x31e9078), `RestorePurchases`(0x31f9bbc), `get_Inst`(0x31e19e0). => The patches sit at interior check points (0x31ddad0-0x31f5040), consistent with name-based method resolution plus interior interception.
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
| #9389 | 523412 | `'<person> has been diagnosed with <disease-new>.'` |
| #9390 | 523459 | `'<person> has been promoted to <job-title>.'` |
| #9385 | 523216 | `'<person> graduated from <high-school>.'` |
| #24360 | 941449 | `'I have been diagnosed with {0}.'` |
| #2617 | 360098 | `' is suffering from '` |

Corroborating negatives: `grep` over all of `gamedata_plain/` finds none of "started middle school",
"graduated from high school", "has been promoted" or "suffering from the"; "middle school" appears only
as the UI action `'Go to middle school'` (#21921) — the sentence itself is assembled from fragments at
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
game feeds through `string.Format` (e.g. #23450 `'I am suffering from {0}.'`, #50732
`'Your {0}, {1}, is no longer suffering from {2}.'`, #50446 `'Your parents just had a baby {0} named
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
  `'<color> <suffix>, <city> <suffix>'` — two comma-separated *alternatives*, with
  `<suffix>` and `<color>` drawn from that same row's Suffix column and from
  `a16_company-name-color-enc`. Substituting a whole row's Format verbatim produces
  the literal `'<color> <suffix>'` in the line; the observed live company names are
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
(`ScenarioIAPConfiguredPush`'s five offers, `ScenarioBase`'s film data, the
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
| literal #70722 '`{0}I have been sent to an orphanage for the remainder of {1} childhood.`' | the hero's own entry line, first person (its second-person twin is #47060 'With no surviving parents, you{0} have been sent to an orphanage ...') |
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
