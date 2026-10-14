# BitLife 3.25.1 mod APK — 逆向分析报告

目标: /Users/Adam/Documents/bitlife/BitLife-Life-Simulator-v3.25.1-mod_sdk.apk
SHA-256: c2111dee01a5d28f0a5f6cfb61bcf2d513a53d8f8ef0f530265da9240f215c1d
大小: 317,793,590 B / zip 2786 entries / 解压 435,325,355 B
包名 com.candywriter.bitlife · versionCode 1063 · versionName 3.25.1 · minSdk 28 · targetSdk/compileSdk 35 · installLocation preferExternal
分析工具: jadx 1.5.6、radare2/rabin2、REA MCP (Ghidra 12.1.2 provider)、python3；工作目录 /Users/Adam/Documents/bitlife/_re

---

## 一句话结论

这不是"改过数值的破解版游戏本体"，而是**原版 Unity IL2CPP 游戏 3.25.1 + 第三方外挂菜单模板 v5.3（LiteApks / 9MOD 渠道）+ PairIP 壳 + 签名校验绕过 + 遥测上报**的拼接产物。游戏逻辑本身**没有发现静态改动**；功能开关由外挂菜单在运行时用 native hook 注入。

三类标记: 【观察】= 直接读到的字节/反编译结果；【推断】= 由观察支撑的结论；【未知】= 尚未验证。

---

## 1. 启动链（谁先跑，跑了什么）

1. 【观察】Manifest application 类 = com.pairip.application.Application（AndroidManifest.xml:153-159）。
2. 【观察】该类源码（dex/classes2.dex）：
   ```java
   public class Application extends killsign {
       static { StartupLauncher.launch(); }
       protected void attachBaseContext(Context context) {
           VMRunner.setContext(context);
           SignatureCheck.verifyIntegrity(context);   // PairIP 完整性校验
           super.attachBaseContext(context);
       }
   }
   ```
   即壳类**继承**了破解者塞进来的 com.android.support.killsign（ApkSignatureKillerEx）。
3. 【观察】Manifest launcher activity = com.unity3d.player.UnityPlayerActivity（79 个 activity 之一）。
4. 【观察】classes10.dex 里 UnityPlayerActivity.onCreate 被改写（唯一调用点，路径 /Users/Adam/Documents/bitlife/_re/jadx_c10/sources/com/unity3d/player/UnityPlayerActivity.java）：
   ```java
   protected void onCreate(Bundle bundle) {
       ModPlayerprefs.Modded(this);                 // ① 加载外挂
       UtLeJu.VpvSv.invoke(null, this, bundle);     // ② 原始 Unity 初始化被挪进反射
   }
   ```
   其余生命周期方法（onStart/onResume/onPause/onStop/onDestroy）同样被改成对混淆持有者（pABlfjK.Kdx、DBtzQpFDSfhAJx.TcCz / .SbRMKBHTsfVw、UtLeJu.VpvSv）的 MethodHandle 反射调用，用来藏 hook。
5. 【观察】加载器本身（classes13.dex，com/ModPlayerprefs.java）：
   ```java
   public class ModPlayerprefs {
       private static final String Located_Playerprefs = "MainActivity";
       public static void Modded(Context context) { System.loadLibrary("menu"); }
   }
   ```
6. 【观察】libmenu.so 的 JNI_OnLoad（vaddr 0x1f6b64，文件偏移 1010532，函数体 36 字节）→ FUN_001f6370（0x1f6370，1960 字节）完成 RegisterNatives，并用循环
   `*pbVar20 = (byte)uVar7 | (byte)(uVar6 << 4);` 把内嵌的 ASCII-hex 解密成真正的 mod 菜单 DEX。

---

## 2. 外挂菜单本体（拿到源码了）

【观察】libmenu.so 里有两段明文 ASCII-hex：文件偏移 0x48305 起 319,496 字符（到 0x9630D）、0x98464 起 4,768 字符。

- 第一段解码 = 159,748 字节的**合法 DEX**：magic `dex\n035`，file_size 0x00027004 = 159,748（与实际长度一致），string_ids 1441，type_ids 237，proto_ids 285，method_ids 717，**class_defs 63**。jadx 反编译出 7 个源文件：
  /tmp/lm_dex/sources/com/android/support/{MainActivity,Menu,Main,Preferences,DialogHelper,CrashHandler,BuildConfig}.java
- 第二段解码 = 2,384 字节 DEX（7 个 class_defs，只反编译出 R.java 资源 ID 桩）。

### 2.1 这是公开的 Mod Menu v5.3 模板

【观察】BuildConfig.java: APPLICATION_ID = "com.android.support"，BUILD_TYPE = "debug"，DEBUG = true，VERSION_CODE = 1，**VERSION_NAME = "5.3"**。

【观察】MainActivity.java:
```java
public String GameActivity = "com.unity3d.player.UnityPlayerActivity";
public boolean hasLaunched = false;
// onCreate: guard → System.loadLibrary("menu") → startActivity(new Intent(this, Class.forName(GameActivity)))
// catch ClassNotFoundException → tag "Mod_menu" / "Error. Game's main activity does not exist"
```

【观察】Main.java: REQUIRED_PACKAGE = "com.adb.apkinstaller"，REQUIRED_PACKAGE_URL = "https://liteapks.com/9adb-installer.html"；Start() 弹不可取消的 "App Required" 对话框，点 Install 跳该网址后 `System.exit(0)`；Toast 文案 "LiteApks.Com & 9MOD.COM"。

【观察】Menu.java（2078 行）= WindowManager 悬浮菜单：MENU_WIDTH 290 / MENU_HEIGHT 210 / MENU_CORNER 35.0f / ICON_SIZE 45 / ICON_ALPHA 0.7f，配色 ACCENT_COLOR #7C4DFF、MENU_BG_COLOR #141821、ToggleON #4CAF50、BtnOFF #B3261E 等；控件构造器 Button / ButtonOnOff / ButtonLink / Category / CheckBox / CheckBoxFeature / Collapse / InputLNum / InputNum / InputText / RadioButton / SeekBar / Spinner / DonateImg；里面出现 `$$Nest$mfeatureList`、`val$featName`、`SettingsList`。

【观察】Menu.java 内嵌作者侧配置：
- TG_TOKEN = "8992321759:AAEvrB-hjuIHdhLHYmRg_XQujQfedLOEUUs"，TG_CHAT_ID = "-1004319045612"，反馈走 `/sendMessage` API（Telegram Bot）
- FEEDBACK_COOLDOWN_MS = 30000，FEEDBACK_DAILY_LIMIT = 3，弹窗 "All reports are anonymous" / "Internet connection is required to send feedback"
- 捐赠图与开发者信息为内嵌 base64 PNG，文案 "Donate To Me" / "Join Telegram" / "LITEAPKS.COM" / "9MOD.COM"

【观察】Preferences.java = 开关持久化层：
```java
sharedPreferences = context.getApplicationContext()
        .getSharedPreferences(context.getPackageName() + "_preferences", 0);
public static void changeFeatureBool(String str, int i, boolean z) { with(context).writeBoolean(i, z); }
// 同形: changeFeatureInt / changeFeatureLong / changeFeatureString
```
**关键推断**：菜单 UI 只是"显示层"，开关按索引 i 写进 SharedPreferences；真正功能逻辑在 libmenu.so 里读这些值（`MENU_VERSION`、`_length` 等键在两边都出现）。所以"这个 mod 有哪些功能"的清单存在于 native 侧的加密字符串表里。

---

## 3. 签名校验绕过链（PairIP vs killsign）

【观察】com.pairip.SignatureCheck 常量：
- ALLOWLISTED_SIG = "Vn3kj4pUblROi2S+QfRRL9nhsaO2uoHQg6+dpEtxdTE="
- expectedSignature = expectedLegacyUpgradedSignature = expectedTestSignature = "13Gtz+XU0d5zweeer/N1aoPBO89tvA3T4ZndjymXIxg="
- verifyIntegrity() 对 pi.signatures[0].toByteArray() 做 SHA-256，Base64.encodeToString(digest, 2)，不等就抛 SignatureCheck$SignatureTamperedException

【观察】实际签名者 = META-INF/TESTKEY.RSA，即 AOSP 公开 testkey（subject/issuer C=US, ST=California, L=Mountain View, O=Android, OU=Android, CN=Android, emailAddress=android@android.com）。证书 DER 的 SHA-256、Base64 = `pA2oClnRcMqpUM8VwYxFTUejmyaYnYtkDs10W6cb9dw=`（openssl 指纹 A4:0D:A8:0A:...:1B:F5:DC）。
【推断】该值与 PairIP 期望的 "13Gtz+..." 不一致，因此校验能过**只可能**是因为 killsign 伪造了返回给 PairIP 的签名。

【观察】/Users/Adam/Documents/bitlife/_re/jadx_small/classes12/sources/com/android/support/killsign.java（102,419 B，内部类 killsign$a … killsign$l）里 class l 实现 Parcelable.Creator，构造参数 (Parcelable.Creator a, String b, Signature c)：
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
即劫持 PackageInfo 的 Parcel 反序列化，把签名替换成"原始签名"，让任何校验方（PairIP、PackageManager、GameGuardian 类检测）看到旧签名。同族成员还有 killPM、killOpen、EnvironmentData、getApkContentsSigners、`VMRuntime.setHiddenApiExemptions`、反射访问 `"sPackageInfoCache"`、sun.misc.Unsafe + MethodHandles。源码里保留了出处字符串 `https://github.com/L-JINBIN/ApkSignatureKillerEx`。

【观察】混淆手法（值得单独记一条）：killsign 有约 40 个与功能无关的 SDK 诱饵 import（com.moloco.*、io.bidmachine.*、com.mbridge.*、kotlinx.coroutines.*、com.iab.omid.*、com.fyber.*、androidx.collection.internal.ycs.bRImUgx …），并且它的 static 块**往这些外部类的静态字段里写字符串常量**（bRImUgx.ACF="WebvttCueParser"、bRImUgx.GOkzoRuQIqY="\u001f"、bRImUgx.Gxlqxn="Step is zero." …），纯粹制造分析噪声。

注意：com.android.support.MainActivity（外挂菜单 Activity）在 APK 的任何 dex 里都**不存在**，只存在于 libmenu.so 内嵌 DEX 中、运行时加载 —— 静态扫描工具看不到它。

---

## 4. native 侧的"功能目录"：静态读不出，运行期可 dump（见 §13）

【观察】libmenu.so（sha256 ab7e98cd5c9d12178e07fc0c0cee48b1bff209a1970134680c477229fd537239，1,671,720 B，DT_NEEDED liblog/libandroid/libEGL/libGLESv2/libc/libm/libdl —— 链 EGL/GLESv2 说明是在游戏画面上叠 OpenGL 悬浮层）。
- .rodata 0x9658D–0xA2200（48,243 B）里有 1,958 个 NUL 结尾字符串：614 个正常可读（含 `/proc/self/maps`、`/sbin/magisk`、`/system/xbin/su`、`/dev/qemu_pipe`、`frida-gadget`、`libreveny_hook.so`、`substrate_hook_init`、`zygisk_inject`、`dlsym_hook_detected`、`libil2cpp_hook_guard`、`AES_GCM_IV_NONCE_96BIT`、`AES256_ENCRYPT_KEY_ROTATION_SALT`、`AQAB_RSA_PUBKEY_MODULUS_N`、`https://api.protection.internal/v3/verify`），另外 946 个是随机字节 —— 逐字符串选择性加密。
- 实测排除的加密方案：单字节 XOR（946 个里只有 12 个碰巧过 0.85 可读率，且都是短串）；基于地址的 XOR（k=(off+i)、(off>>8)+i、i、off−i，0 命中）；重复密钥 XOR（key 长 1..32 时相邻块归一化 Hamming 距离恒 ~3.9 bits/byte，无下降点）；同密钥流复用（等长串两两 XOR 后低位比例 0.284–0.300，随机基线 0.25，若为 ASCII 明文复用应 >0.4）。
- 【推断】**native 字符串表是用带密钥的分组/流密码加密、密钥运行时派生**（与同二进制里的 AES-GCM、密钥轮换常量一致）。
- 【更正·动态阶段】"静态还原不了"≠"拿不到"。这些字符串在**运行期是明文**：把进程里 libmenu.so 的模块镜像整块 dump 出来，与磁盘文件做可打印字符串集合差，直接得到 907 条明文（含全部 hook 目标名与 JNI 签名），**不需要逆向它的解密例程**。详见 §13

【观察】libmenu.so 只有 167 个导入，含 dlopen/dlsym/dl_iterate_phdr/mprotect/prctl/syscall/pthread_create —— 典型的 inline hook 引擎特征。

---

## 5. 游戏本体有没有被静态改过？

【观察】libil2cpp.so 104,046,736 B、global-metadata.dat 18,847,828 B 里 grep 不到 `liteapks` / `9mod` / `ModPlayerprefs` / `mod_menu` / `unlimited` / `godmode` 等任何 mod 痕迹（0 命中）。
【推断】数值/逻辑修改不在文件里，而是运行时由 libmenu.so 读 SharedPreferences 开关后 hook IL2CPP 函数完成。
【已解·动态阶段】59 个 hook 点全部落在 libil2cpp.so，已逐个反查成 IL2CPP 方法名：1 个 `BitLife.UI.Containers.UIDarklyPaywallExpansionPack::CultBulletPoints`，其余是 `Game.GameVars::*` 与约 50 个 `Game.UIIAPManager::*Purchased` 授权点 + 一批 `PurchaseXxx` 发起函数，外加付费墙按钮 `BitLife.UI.Containers.UIDarklyPaywallGenericIAP::BuyButtonPress`。完整清单见 §13。

---

## 6. 遥测、分发与广告堆

【观察】分发/变现：Main.java 强制安装外部 installer（com.adb.apkinstaller ← https://liteapks.com/9adb-installer.html），菜单内嵌 Telegram Bot 与捐赠入口，模板版本 5.3、包名占位 com.android.support —— 属于 LiteApks / 9MOD 的公开外挂菜单渠道。

【观察】第三方遥测被塞进 Manifest 作为 ContentProvider（AndroidManifest.xml:1301-1309）：
- android:name = com.bamen.report.ReportProvider，authorities = com.candywriter.bitlife.bamen.report.provider，exported = false，initOrder = 100
- meta-data: bamen.report.statisticsNo = 9，bamen.report.debugLog = false
- ReportProvider.onCreate() → Reporter.start(context)；Reporter 端点：
  `https://api.gamekillerapp.com/api/overseas/v1/single-game/online-log` 与 `.../launch-log`
- 采集内容：Settings.Secure 的 android_id（失败则写 SharedPreferences "bamen_report" 生成 UUID）、包名、应用名、versionCode、客户端时间 yyyy-MM-dd HH:mm:ss、会话；OnlineTracker 注册 ActivityLifecycleCallbacks，用 "bamen-online-report" 守护线程等前台再上报。
【推断】这是给"游戏修改工具/分发面板"做安装与在线统计的埋点，跟游戏无关，装这个 mod 版等于额外接受一路第三方上报。

【观察】壳/保护相关：PairIP（com.pairip.application.Application、SignatureCheck、VMRunner、StartupLauncher）+ 12 个 `\x00IAP` 魔数资产（assets/3M7BEoMfrOJfBpEL 159,500B、ahQQhuwI7ZiFgGHs 280,034、b2GBqOK2U4MiE4qo 371,660、bSPdtT1PTPyaldHv 161,053、bsWs4yCUsnR4HPiO 159,200、cD9YCYNSWv55leZ3 159,095、dohrLT3fRgBSZXtd、FJs7BXdFi5IEAKbf、FNS1OaeRn7pXyrv0、fPD0Vc6q1J8biTN7、IhUP9onrobUq4vZ6）＝ PairIP 的 IAP V2 载荷。
【观察】classes14.dex 的 od/ccc/d/f/a/dd/d.java：静态字段 ApplicationInfo a / DexClassLoader b / Method h，字符串常量用 base64 分段 + f.a(b.a(...)) 还原（"wDA=\n"+"9gTSLrK3How=\n" 等），并读取 applicationInfo.publicSourceDir、处理 ZipInputStream → 第二段 DEX 加载器 / 反篡改桩；同类包里还塞了错位包名桩 androidx/work/job/DataSyncWorker、android/content/res/AppComponentFactoryImpl、com/google/firebase/analytics/FirebaseMessageServiceImpl（借包名混淆）。
【观察】广告/分析 SDK 堆：com/inmobi/media(1769 类引用)、com/applovin/impl(742)、com/explorestack/protobuf(728)、com/google/protobuf(631)、com/flurry/sdk(524)、com/appsflyer/internal(456)、vungle/unity-ads/moloco/facebook-ads/ironsource/chartboost/mintegral/bidmachine/pubnative/gadsme/gameanalytics/didomi consent/OMID 等；libquack.so + libquickjs.so 是 koushikdutta 的 QuickJS 绑定（无 Java 侧引用）。

---

## 7. 运行/动态验证（本节静态阶段的"做不到"已被 MuMu 实测推翻，见第二部分 §10–§15）

【观察】唯一可用设备（REA adb）：serial 192.168.1.105:5555，TCL Smart_TV / G13_2K_GB，Android 14 (SDK 34)，安全补丁 2025-03-01，ro.debuggable = 0，**ro.product.cpu.abilist = "armeabi-v7a,armeabi"（纯 32 位 Google TV 盒子）**；本 APK 只带 arm64-v8a（无 armeabi-v7a 目录）→ 安装会 INSTALL_FAILED_NO_MATCHING_ABIS。com.candywriter.bitlife 未安装。
【观察】/Users/Adam/Documents/bitlife 只有这一个 APK，没有原版可比对 → 无法做版本差分。
【更正·动态阶段】上述设备确实装不了（32 位 TV 盒子），但改用宿主机的 **MuMu Player 安卓模拟器**（Android 12 / arm64-v8a / 已 root）+ frida 16.7.19 完成了全部动态验证：安装启动成功、绕过 mod 门禁、枚举出菜单全部条目、抓到 59 个 hook 并解析成方法名、解密了全部内容库。细节见第二部分。

---

## 8. 剩余开放项（静态阶段的 1–3 已在动态阶段解决，见 §13）

1. ~~逆向 libmenu.so 的字符串解密例程~~ → **已解决**：不解密也能拿全，dump 运行期模块镜像即得 907 条明文（§13）。
2. ~~反编译读 SharedPreferences 的函数~~ → **已解决**：菜单条目是从 native 的 `SettingsList()`/`sub2025s()` 直接枚举出来的，无需逆 SharedPreferences（§12）。
3. ~~在 arm64 环境跑一次~~ → **已解决**：MuMu + frida 全流程（§10–§13）。
4. 【仍未解】od/ccc 第二段 DEX 加载器读取的是哪个 zip 条目（d.java 只处理 publicSourceDir + ZipInputStream，看着像自校验/再解密，未见投递落地的载荷）。
5. 【仍未解】13 个 BinaryFormatter 资产里 `female0-age13` / `male1-age13` 两个存盘样本，`nrbf` 解析时 RecursionError（对象图过深或含环）。
6. 【仍未解】写一个能直接跑起来的现代 BitLife 复刻需要的美术/音频资源没有导出（datapack 里 8133 张 Texture2D 未导出，本阶段只导了内容表）。

---

## 9. 复现命令（关键几步）

```bash
# 1) 清单与 dex
jadx --no-src -d _re/jadx_res BitLife-Life-Simulator-v3.25.1-mod_sdk.apk
# 2) 抽取 libmenu.so 内嵌 hex DEX（偏移 0x48305，长度 319496）
python3 -c "d=open('_re/lib/arm64-v8a/libmenu.so','rb').read();
open('/tmp/lm_embedded.dex','wb').write(bytes.fromhex(d[0x48305:0x48305+319496].decode()))"
jadx -d /tmp/lm_dex --no-res --no-debug-info /tmp/lm_embedded.dex
# 3) 段表 / JNI_OnLoad
rabin2 -S _re/lib/arm64-v8a/libmenu.so
# 4) 签名者
keytool -printcert -file _re/META-INF/TESTKEY.RSA
```

---

## 10. 速查表

| 项 | 值 |
|---|---|
| APK SHA-256 | c2111dee01a5d28f0a5f6cfb61bcf2d513a53d8f8ef0f530265da9240f215c1d |
| 签名者 | AOSP testkey（TESTKEY.RSA），cert DER SHA-256 b64 pA2oClnRcMqpUM8VwYxFTUejmyaYnYtkDs10W6cb9dw= |
| PairIP 期望签名 | 13Gtz+XU0d5zweeer/N1aoPBO89tvA3T4ZndjymXIxg= |
| 签名绕过 | com.android.support.killsign（ApkSignatureKillerEx，github L-JINBIN），劫持 PackageInfo Parcel |
| 外挂入口 | UnityPlayerActivity.onCreate → ModPlayerprefs.Modded → System.loadLibrary("menu") |
| 外挂本体 | libmenu.so（sha256 ab7e98cd5c9d12178e07fc0c0cee48b1bff209a1970134680c477229fd537239）+ 内嵌 DEX（159,748 B，7 个源文件） |
| 菜单模板 | Mod Menu v5.3，包名 com.android.support，LiteApks / 9MOD 渠道 |
| 开关存储 | SharedPreferences "<包名>_preferences"（Preferences.changeFeatureBool/Int/Long/String(index, value)） |
| 遥测 | com.bamen.report → api.gamekillerapp.com/api/overseas/v1/single-game/{online,launch}-log（android_id + 包名 + 版本） |
| 功能清单 | 【未知】，native 字符串表加密，静态不可还原 |
| 游戏逻辑静态改动 | 【未发现】libil2cpp.so / global-metadata.dat 无 mod 痕迹 |
---

# 第二部分：动态阶段（MuMu 模拟器实测）

> 第一部分（静态）的结论里，有两处被动态证据推翻，已在上文就地更正：
> 1. 「native 功能目录读不出来」——错。名称表在运行期是明文，dump 进程镜像即可拿到（见 11）。
> 2. 「本轮无法动态验证」——错。MuMu Player 上已完成安装、启动、绕过门禁、抓取 hook、解密全量内容（见 10-13）。

## 10. 动态环境（[obs]）

- 宿主：Apple M1 / macOS 15.4。模拟器 /Applications/MuMuPlayer.app，VM 名「我的安卓」，Android 12（SDK 32），arm64-v8a，机型伪装 SM-S9180。
- adb 端点：`/opt/homebrew/bin/adb -s 127.0.0.1:16384`（冷启动后端口由 5555 变为 vm.json 里的 adbPort=16384）。
- root：改 `~/Library/Application Support/com.netease.mumu.nemux/bundles/mumu_0_*/setting/vm.json` 的 `vmRootEnable: false -> true`，重启 VM 后 `adb shell id` = uid=0(root)。该开关在关机后不回滚。
- frida：**必须用 16.7.19**（frida 17 起不再内置 Java bridge，脚本会报 `ReferenceError: 'Java' is not defined`）。frida-server 16.7.19 android-arm64 放 `/data/local/tmp/fsrv16` 以 root 运行；客户端 frida 16.7.19 + frida-tools 13.7.1。
- 注意：`frida.get_usb_device()` 会误连同一网段的一台 TCL 电视（192.168.1.105:5555），必须显式 `frida.get_device("127.0.0.1:16384")`；`dev.attach("com.candywriter.bitlife")` 会 ProcessNotFoundError，要先用 `ps -A` 取 pid 再 attach。

## 11. 门禁绕过与外挂装载链（[obs]）

- 首次启动卡在 mod 自带弹窗「App Required / Please install 9ADB Installer from our website to continue.」，对应 `com.android.support.Main`：
  - `REQUIRED_PACKAGE = com.adb.apkinstaller`
  - `REQUIRED_PACKAGE_URL = https://liteapks.com/9adb-installer.html`
  - 方法集：`Start`, `StartWithoutPermission`, `ShowRequiredAppDialog`, `isPackageInstalled`, `getTopActivity` + 三个 `-$$Nest$sm*`。
- 绕过（不需要装任何东西）：frida spawn，用 `Java.enumerateClassLoaders` 找到 loader，`loadClass("com.android.support.Main")`，把 `isPackageInstalled` 的所有重载实现改为 `return true`，再 resume。
- 此时可见 loader 类型：
  `dalvik.system.InMemoryDexClassLoader[DexPathList[[dex file "InMemoryDexFile[cookie=[-5476376643880247920, -5476376644419490032, -5476376644419483984]]"], ...]]`
  ⇒ 外挂的 Java 半身是 **3 个纯内存 dex**，磁盘上不存在任何 dex 文件（mod 会把 dex 落到 `/data/data/com.candywriter.bitlife/files/<ts>.dex` 加载后 unlink，只有已删除 inode 的映射可见）。
- 从 /proc/`<pid>`/maps 的 `[anon:dalvik-DEX data]` 区域直接 dd 出这三个 region，剪掉尾部填充后得到：
  - mod_a 5,237,600 B（`dex\n035`，Facebook/Instagram/Kotlin 依赖，2822 个 java 文件）
  - **mod_b 159,748 B（`dex\n035`）——就是外挂菜单本体**：`com/android/support/{Main,Menu,MainActivity,Preferences,DialogHelper,CrashHandler,BuildConfig}`
  - mod_c 1,334,156 B（`dex\n038`，org.xbill.DNS + com.google.android.gms）

## 12. 外挂的全部功能（动态枚举，[obs] 逐字）

菜单表由 native 提供，运行时在 `com.android.support.Menu` 上调用即得（`native String[] SettingsList()` / `native String[] sub2025s()`）：

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
其余为 Branding/反馈模块：`⏩ LITEAPKS.COM ⏪`、`⏩ 9MOD.COM ⏪`、https://liteapks.com、https://9mod.com、https://t.me/liteapks、https://t.me/newbiexabo、PayPal 打赏、Quick Feedback/Report Bug（Telegram 机器人 `8992321759:AAEvrB-hjuIHdhLHYmRg_XQujQfedLOEUUs`，chat_id `-1004319045612`，每日 3 条/30 秒冷却）。

结论：**这个外挂只有一个作弊功能——「Free In-App Purchase」**，外加两个菜单设置开关。其余全是分发方的引流与反馈。

## 13. 「Free In-App Purchase」的实现（[obs]，全链路）

1. `libmenu.so` 在 `JNI_OnLoad` 里用 ShadowHook 2.0.1 做 inline hook，logcat tag `Mod_Menu`/`shadowhook_tag` 会打印每一条：
   `shadowhook version 2.0.1: shadowhook init(default_mode: UNIQUE, debuggable: false), init-errno: 0, real-init: yes` + **59 条 `shadowhook HOOK(addr) OK: 0x...`**。
2. 用 /proc/<pid>/maps 的模块基址换算，59 个 hook 全部落在 **libil2cpp.so**：1 个在 0x2fcf8a8，其余 58 个密集分布在 0x31ddad0–0x31f5040（约 1.4 MB 区间）。
3. 用 frida 遍历 IL2CPP（128 assemblies / 150,007 methods）取每个 MethodInfo 的入口指针，反查落点 ⇒ 得到方法名：
   - 0x2fcf8a8 → `BitLife.UI.Containers.UIDarklyPaywallExpansionPack::CultBulletPoints`（唯一与内购无关的 hook）
   - 0x31ddad0 起 → `Game.GameVars::{SaveGameVars, LoadDefaults, SecondsSinceLastInterstitial, ...}`
   - 0x31e19c8 起的主体 → `Game.UIIAPManager::*`（约 50 个 `XxxPurchased` 授权点 + 一批 `PurchaseXxx`）
4. libmenu.so 里没有这些偏移的字面量表（按 8 字节和 4 字节 LE 全量扫描 59 个值：0 命中）⇒ 它是**按名字**在运行时通过 IL2CPP API 解析方法再 hook 的，所以磁盘上的名字必须是密文。
5. 把 libmenu.so 在进程里的模块镜像（base 0x7404c03000, 0x1a2000 字节）整块 dump 出来，与磁盘文件做「可打印字符串集合差」：新增 1053 条、过滤噪声后 **907 条明文**，其中：
   - 授权点（授予）：`Game.UIIAPManager.AssassinsBladePurchased`, `AuraRingPurchased`, `BattlePassPurchased`, `BitizenshipPurchased`, `BitizenshipAndGodModePurchased`, `BossModePurchased`, `Bundle*`, `Career*`, `ChallengeVaultPurchased`, `ExpansionPack{BlackMarket,Casino,Cult,Investor,Landlord,Racing,SecretAgent,Vampire,Zoo}Purchased`, `FighterModePurchased`, `GetOutJailCardPurchased`, `GodModePurchased`, `GoldThemePurchased`, `Golden{Diploma,NekoStatue,Pacifier,Passport,PiggyBank,Resume,Wrench}Purchased`, `HollywoodStarPurchased`, `LuxuryAccessPurchased`, `MovieDirectorPurchased`, `OutdoorLifestylePurchased`, `PromiscuityPotionPurchased`, `RemoveAdsPurchased`, `SpikedKnucklesPurchased`, `StreakSaverPurchased`, `TimeMachinePurchased`, `UnlimitedTimeMachinePurchased` …
   - 发起（`PurchaseXxx`，同上列表一一对应）
   - **`BitLife.UI.Containers.UIDarklyPaywallGenericIAP.BuyButtonPress`** —— 真正的付费墙按钮，外挂直接拦它
   - Java 侧目标：`com.android.support.Main` / `Menu` / `Preferences`，以及 JNI 签名 `(Landroid/content/Context;ILjava/lang/String;IJZLjava/lang/String;)V`（= `Preferences.sub2025`）、`([Ljava/nio/ByteBuffer;Ljava/lang/ClassLoader;)V`（= InMemoryDexClassLoader 构造器）
   - 它自己调用的 IL2CPP API：`il2cpp_domain_get_assemblies`, `il2cpp_method_get_name`；反分析探测：`/proc/self/maps`, `/sbin/magisk`, `/system/xbin/su`, `/dev/qemu_pipe`, `frida-gadget`, `zygisk_inject`, `libreveny_hook.so`
   - 内嵌压缩：`/system/lib64/liblzma.so` + `XzUnpacker_*`（XZ SDK 静态链接）⇒ 3 个内存 dex 就是它解压后喂给 InMemoryDexClassLoader 的
6. 语义：点「Free In-App Purchase」→ Java 侧 `Preferences.sub2025(ctx, 3, "Free In-App Purchase", ...)` → native 侧对上述 `PurchaseXxx`/`*Purchased` 全部返回「已购」，并直接放行真正的 `BuyButtonPress` ⇒ **不发起任何 Google Play 交易，直接把权益标记为已拥有**。
7. 交叉验证（[obs]）：把 59 个 hook 点逐个 `Interceptor.attach` 后再触发购买，未捕捉到调用；直接 `il2cpp_runtime_invoke` `BitizenshipPurchased(bool)` 会打进 IL2CPP 断言（`Error: breakpoint triggered`），传 NULL 参数则 `access violation accessing 0x0`（**原因是缺参数，不是线程问题**：`RemoveAdsPurchased(true)`、`GoldenPacifierPurchased(true)` 都能干净返回）。结论：这些授权点的调用依赖真实 IAP 状态机，复刻时以「按钮 → 记录 → 授权」这条链为准，不要直接调内部函数。

## 14. 内容数据库：全量解出（[obs]）

游戏把整个内容库作为 Unity TextAsset 打包：

1. `assets/bin/Data/datapack.unity3d`（121,483,836 B）。UnityPy 1.25.4 枚举：8133 Texture2D / 8094 Sprite / 7653 MonoBehaviour / 3988 GameObject / **1086 TextAsset** / 327 Canvas；`data.unity3d` 只有 UI（0 个 TextAsset）。
2. 导出 1086 个 TextAsset → 1084 个唯一文件（19.3 MB），落 `_re/gamedata/`。
3. 其中 **967 个 `*-enc` 是 base64 文本**，由游戏自己的 `EncryptionManager` 加密。运行时取 `EncryptionManager.DefaultCipherKey()` = **`com.wtfapps.apollo16`**（20 字符），`DecodeAndDecryptString(text, key)` 返回明文（`DecryptString` 会抛异常，别用）。
4. 用 frida RPC 跑完全量：**966 个解密成功**；余下 118 个逐类处理 —— 105 个本来就是明文（引擎未加密，如 `a16_voltron_componentFilenames`、`BillingMode`、`IAPProductCatalog`），**13 个是 .NET BinaryFormatter 流**（`00 01 00 00 00 FF FF FF FF` + `Assembly-CSharp, Version=0.0.0.0`），其中 11 个已用 `nrbf` 解析为 JSON（`a16_countries-enc` = 102 个国家、键名是真实 backing-field，如 `<Name>k__BackingField`、`<Cities>k__BackingField`、`<Currency>k__BackingField`；另有 8 个 age4/age13 存盘样本，各 297 个字段）。
5. 结果：`_re/gamedata_plain/` **1071 个文件 / 13.3 MB 明文**（939 个 `a16_*` + 77 个 `genesis*` + 若干），索引 `_re/gamedata_plain_index.json`、`_re/gamedata_plain_INDEX` 说明见 `gamedata_INDEX.md`；BF 解析结果在 `_re/gamedata_json/`。
6. [unk] `female0-age13` 与 `male1-age13` 两个存盘样本在 `nrbf` 里 RecursionError（对象图过深/含环），未解析。
7. 典型表：`genesis_master_tags_reference_2`（1.3 MB，列 `tag_id,tag_value,weight,categorization_a…`）、`genesis_movie_director`（90 万字节的剧本表）、`a16_dumb-scenarios-calendar`（日历剧情）、`a16_dumb-scenarios-career`（职业剧情）、`genesis_lookup_emoji_visuals`（4488 行表情表）、`a16_countries-enc`（102 国）等。

## 15. 结论修订

- [obs] 外挂 = 通用 LGL Mod Menu（Java 模板）+ 定制 native libmenu.so（ShadowHook + IL2CPP 名称解析 + LZMA）。它**不改游戏文件**：APK 内的 libil2cpp.so / global-metadata.dat 干净，全部效果发生在运行时。它的全部玩法作弊只有一个开关：免费内购。
- [obs] 门禁与反馈模块说明这是分发方（liteapks/9mod）的引流构建，而非针对本机的恶意软件；遥测组件 `com.bamen.report` 会向 `api.gamekillerapp.com` 上报安装/在线。
- [inf] 对移植最有价值的部分不是外挂，而是**内容库 + IL2CPP 方法清单**：两者一起足以在别的平台上重写这款游戏（见第三部分）。



## 16. hook 现场复核与更正（[obs]，动态阶段二次验证）

1. 【观察】正确寻址方式：libil2cpp.so 的段在进程里的映射**不是** base+文件偏移的简单平移（0x1000–0x4000 的对齐漂移）。必须用 `/proc/<pid>/maps` 的段表（start/end/file_offset）把「文件偏移」换算成进程地址，再读字节；直接 base+offset 会读到错位页面（这正是 §13 早期误判的来源）。
2. 【观察】按段表换算后，对 §13 记录的 59 个 hook 点逐点读 16 字节，与磁盘 ELF 同偏移字节比对：**59/59 全部不同**，且差异**只在第 1 条指令（4 字节）**，其后紧跟的序言字节与文件完全一致（例如 `fe5fbda9 f65701a9 f44f02a9` 之后原样保留）。
3. 【观察】这 4 字节的取值都是 `0x17xxxxxx`：高 6 位 = `0b000101` = ARM64 **B（无条件跳转）**，imm26 取符号扩展后为**负值**（约 −15 至 −19 MB）⇒ 从被 hook 点向前跳到 shadow 区域。
4. 【观察】这 59 个点所在页面在 maps 里权限是 **rwxp**（如 `73db5e3000-73db5e7000 rwxp off 0x31dd000`、`73db5e7000-73db5ef000 rwxp off 0x31e1000`、`73db5f1000-73db5f5000 rwxp off 0x31eb000`），同库其余代码页是 r-xp ⇒ 外挂对这些页做过 mprotect 提权，确认就是 ShadowHook 的 inline hook 现场。
5. 【更正】§13 旧说法「58 个点仍是原始序言、只有 0x31ddad0 有 adrp 补丁」**错误**：那是错位寻址造成的误读。正确结论是 **59 个 hook 点全部已就地打补丁**。
6. 【观察】被 hook 的地址**不是方法入口**：例如 `Game.UIIAPManager::GodModePurchased` 的 methodPointer 落在文件偏移 0x31e24e8，其入口 16 字节（`fe0f1bf8 fa6701a9 f85f02a9 f65703a9`）与文件一致、未被打补丁；同类还有 `BitizenshipPurchased`(0x31e1b54)、`CareerActorPurchased`(0x31e66e8)、`GoldenPacifierPurchased`(0x31e9078)、`RestorePurchases`(0x31f9bbc)、`get_Inst`(0x31e19e0)。⇒ 补丁打在方法体内的中间点（0x31ddad0–0x31f5040 区间），与「按名解析方法 + 命中内部检查点」的实现一致。
7. 【未知】跳转目标（trampoline）的确切位置未定位：按 B 的相对偏移解出的目标地址落在只读段/段间隙，未能读出可执行桩代码，故 trampoline 布局仍记为未知（不影响"已 hook"这一结论，字节差异与 RWX 页是直接证据）。


## 17. 第三部分：原生 C++ 移植（动态阶段产出）

交付目录 `/Users/Adam/Documents/bitlife/_re/port/`（C++17 + CMake，**无第三方依赖**，11 个文件约 1,590 行）：

| 文件 | 行数 | 作用 |
|---|---|---|
| CMakeLists.txt | 19 | 静态库 blport + 可执行 blsim |
| include/bl/table.h · src/table.cpp | 63 · 153 | 直接读 gamedata_plain 的真实 CSV/TSV（引号字段、# 注释、按列名取值、Commonness Score 加权抽取） |
| include/bl/game.h · src/game.cpp | 100 · 125 | 按 il2cpp_dump.txt 的 Game.GameVars / MonetizationVars 真实字段移植的状态与 60 条权益表 |
| include/bl/sim.h · src/sim.cpp | 83 · 439 | 用真实 a16_* 剧情表驱动的人生推进循环（年龄、五维属性、karma/discipline、占位符替换、死亡） |
| include/bl/mod.h · src/mod.cpp | 92 · 228 | mod 模型：grant(productId) + 两个真实开关 + LGL 菜单语法 |
| src/main.cpp | 157 | 真实数据 demo |
| README.md | 131 | 发现 → 模块映射表 + 未移植清单 |

**构建与运行（本次已独立复验：cmake 4.4.3 / Apple clang 17.0.0 arm64，configure+build exit 0，`./build/blsim` exit 0，183 行输出）**

```
cd /Users/Adam/Documents/bitlife/_re/port
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j4
./build/blsim
```

**已接通的真实数据（观察）**：`a16_dumb-scenarios-universal-enc`（258 行 / 42 列：Index, Hero Min/Max Age, Student, Job, Vampire, Both Parents Alive, Hero Gender, Title, Body, CTA, Choice 1..4, Narrative 1..4, Karma/Discipline/Happiness/Intelligence/Appearance Impact 1..4）、`a16_employee-recognition-scenarios-enc`（14 行，提供 Commonness Score 加权路径）、`a16_casino-casino-celebrity-name-enc`（106 行姓名池）、`a16_hero-filler-enc`（1132 行）、`a16_death-types-enc`（110 行）、`gamedata_json/a16_countries-enc.json`（102 国）。占位符 `<a/b/c>`、`<num2-5>` 与 `<friend> <teacher> <classmate> <coworker> <parent> <lover> <company> <employee>` 已替换，解不出的原样保留、不猜。

**注意（核对过的细节）**：demo 里打印的 `row N` 是**载入序号**，不是表里的 Index 列。例：Teething 那条载入序号 117，而它的 Index 列值是 143。内容本身已与解密表逐字核对一致。

**mod 侧移植**：`kEntitlements[60]` 由 Game.UIIAPManager 的 60 个 `*Purchased` 授权点 + 58 个 `Purchase*` 发起点生成，每行回指其 IAPManager 的 product-id 字段；`grant()` 接受四种真实寻址形式（授权方法名 / 短名 / IAPManager 字段 / Purchase* 发起点）；`grantAll()` 一次授予 60 个产品。菜单树逐字复刻：NOTE / RichTextView（Google Play Services 提示）/ MOD FEATURES / Button Free In-App Purchase / CONTACT US / Settings（Toggle Save Feature、Toggle Auto Menu Size）/ Game Info。

**未移植（诚实清单，README 有全文）**：职业/教育/关系/金钱/房产系统（a16_careers-enc 33 列可载入但未用）、小游戏、传家宝/器物、Darkly 付费墙、时间机器、Battle Pass、每日任务、通知与广告、整个 UI 层（菜单几何已知：MENU_WIDTH 290 / HEIGHT 210 / CORNER 35.0f / ICON_SIZE 45 / ACCENT #7C4DFF，未复刻）、美术与音频（8133 张 Texture2D 未导出）、11 个 BinaryFormatter 表（含两个 age13 存盘、Nrbf RecursionError）、`<rb>` 等未解占位符、死亡模型为移植侧简化规则。

