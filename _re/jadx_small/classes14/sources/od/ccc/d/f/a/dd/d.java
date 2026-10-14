package od.ccc.d.f.a.dd;

import android.app.ActivityManager;
import android.app.Application;
import android.content.Context;
import android.content.pm.ApplicationInfo;
import android.os.Process;
import android.text.TextUtils;
import dalvik.system.DexClassLoader;
import java.io.File;
import java.io.FileInputStream;
import java.io.FileOutputStream;
import java.io.IOException;
import java.io.InputStream;
import java.lang.reflect.Field;
import java.lang.reflect.Method;
import java.nio.charset.StandardCharsets;
import java.nio.file.Files;
import java.nio.file.OpenOption;
import java.util.HashMap;
import java.util.HashSet;
import java.util.Objects;
import java.util.zip.ZipEntry;
import java.util.zip.ZipInputStream;

/* JADX INFO: loaded from: /Users/Adam/Documents/bitlife/_re/dex/classes14.dex */
public abstract class d {
    public static ApplicationInfo a;
    public static DexClassLoader b;
    public static String c;
    public static Method h;
    public static final String d = f.a(b.a("wDA=\n"), b.a("9gTSLrK3How=\n"));
    public static final String e = f.a(b.a("Dw6bjkua\n"), b.a("IGLy7H2uC9o=\n"));
    public static final String f = f.a(b.a("zJToGA==\n"), b.a("4/iBemLBR4M=\n"));
    public static final String g = f.a(b.a("++O2\n"), b.a("1ZDZ6NhUF4Q=\n"));
    public static final HashMap i = new HashMap();
    public static final HashSet j = new HashSet();

    public static Class a(String str) throws ClassNotFoundException {
        if (j.contains(str)) {
            throw new ClassNotFoundException(str);
        }
        if (h == null) {
            Method methodA = a(b.getClass(), f.a(b.a("j2eqQJXj50ea\n"), b.a("6Q7EJNaPhjQ=\n")), String.class);
            h = methodA;
            methodA.setAccessible(true);
        }
        HashMap map = i;
        if (map.containsKey(str)) {
            return (Class) map.get(str);
        }
        try {
            map.put(str, (Class) h.invoke(b, str));
            return (Class) map.get(str);
        } catch (Throwable unused) {
            j.add(str);
            throw new ClassNotFoundException(str);
        }
    }

    public static String a(Context context) {
        try {
            return Application.getProcessName();
        } catch (Throwable unused) {
            byte[] bArrA = b.a("tbm2j4eDWPM=\n");
            byte[] bArrA2 = b.a("1NrC5vHqLIo=\n");
            int length = bArrA.length;
            int length2 = bArrA2.length;
            int i2 = 0;
            int i3 = 0;
            while (i2 < length) {
                if (i3 >= length2) {
                    i3 = 0;
                }
                bArrA[i2] = (byte) (bArrA[i2] ^ bArrA2[i3]);
                i2++;
                i3++;
            }
            for (ActivityManager.RunningAppProcessInfo runningAppProcessInfo : ((ActivityManager) context.getSystemService(new String(bArrA, StandardCharsets.UTF_8))).getRunningAppProcesses()) {
                if (runningAppProcessInfo.pid == Process.myPid()) {
                    return runningAppProcessInfo.processName;
                }
            }
            StringBuilder sb = new StringBuilder();
            byte[] bArrA3 = b.a("4P75yybRq/bXserOLJXl6dH+78IxguX3wvzphySet7nT+OiH\n");
            byte[] bArrA4 = b.a("o5GMp0LxxZk=\n");
            int length3 = bArrA3.length;
            int length4 = bArrA4.length;
            int i4 = 0;
            int i5 = 0;
            while (i4 < length3) {
                if (i5 >= length4) {
                    i5 = 0;
                }
                bArrA3[i4] = (byte) (bArrA3[i4] ^ bArrA4[i5]);
                i4++;
                i5++;
            }
            sb.append(new String(bArrA3, StandardCharsets.UTF_8));
            sb.append(Process.myPid());
            throw new RuntimeException(sb.toString());
        }
    }

    public static Method a(Class cls, String str, Class... clsArr) {
        try {
            return cls.getDeclaredMethod(str, clsArr);
        } catch (NoSuchMethodException | SecurityException unused) {
            if (cls.getSuperclass() != null) {
                return a(cls.getSuperclass(), str, clsArr);
            }
            return null;
        }
    }

    public static void a(Application application) {
        try {
            Method methodA = a(a(f.a(b.a("rlGKBVuJ5p69UoIFX4H3n6Rdhl9XnundoEeGW06d7pCsSo5EUMOpsr1O\n"), b.a("zT7nKz7xh/M=\n"))), f.a(b.a("vd2sag==\n"), b.a("1LPFHvZsPb8=\n")), Context.class);
            methodA.setAccessible(true);
            c cVar = new c(application);
            cVar.onCreate();
            methodA.invoke(null, cVar);
        } catch (Throwable th) {
            th.printStackTrace();
        }
    }

    public static void a(ApplicationInfo applicationInfo) {
        StringBuilder sb;
        String str;
        if (b == null) {
            if (applicationInfo == null && (applicationInfo = a) == null) {
                try {
                    Method method = Class.forName(f.a(b.a("UbVpBOe5761Rq31YybP/6kayeQ/cuPnmUb8=\n"), b.a("MNsNdojQi4M=\n"))).getMethod(f.a(b.a("Cxps+d2eFiQLG3f90YQbMQAde+rc\n"), b.a("aG8ei7jwYmU=\n")), null);
                    method.setAccessible(true);
                    Object objInvoke = method.invoke(null, null);
                    Field declaredField = objInvoke.getClass().getDeclaredField(f.a(b.a("nt7rstFz9siD8O2k3mPe150=\n"), b.a("85yEx78Xt7g=\n")));
                    declaredField.setAccessible(true);
                    Object obj = declaredField.get(objInvoke);
                    Field declaredField2 = obj.getClass().getDeclaredField(f.a(b.a("lqsI95e24Q==\n"), b.a("99t4vvnQjpQ=\n")));
                    declaredField2.setAccessible(true);
                    applicationInfo = (ApplicationInfo) declaredField2.get(obj);
                    a = applicationInfo;
                } catch (Throwable th) {
                    throw new RuntimeException(th);
                }
            }
            ClassLoader classLoader = Context.class.getClassLoader();
            c = applicationInfo.dataDir + f.a(b.a("PCpnVfFBN0Z3JyFJ+Ed/XH0=\n"), b.a("E0wOOZQyGDU=\n"));
            File file = new File(c);
            if (!file.exists()) {
                file.mkdirs();
            }
            File file2 = new File(c, f.a(b.a("UA==\n"), b.a("MQoeqfOSwmc=\n")));
            try {
                File file3 = new File(applicationInfo.publicSourceDir);
                if (!file2.exists()) {
                    InputStream inputStreamNewInputStream = Files.newInputStream(file3.toPath(), new OpenOption[0]);
                    ZipInputStream zipInputStream = new ZipInputStream(inputStreamNewInputStream);
                    String strA = f.a(b.a("C3Rjnz79+w==\n"), b.a("eBAIsVyUldQ=\n"));
                    while (true) {
                        ZipEntry nextEntry = zipInputStream.getNextEntry();
                        if (nextEntry == null) {
                            break;
                        }
                        String name = nextEntry.getName();
                        if (!TextUtils.isEmpty(name) && name.endsWith(strA)) {
                            a(zipInputStream, file2.getAbsolutePath());
                            a(new ZipInputStream(new FileInputStream(file2)));
                            break;
                        }
                    }
                    inputStreamNewInputStream.close();
                } else if (file3.lastModified() > file2.lastModified()) {
                    InputStream inputStreamNewInputStream2 = Files.newInputStream(file3.toPath(), new OpenOption[0]);
                    ZipInputStream zipInputStream2 = new ZipInputStream(inputStreamNewInputStream2);
                    String strA2 = f.a(b.a("1Ly31ASdUQ==\n"), b.a("p9jc+mb0Py8=\n"));
                    while (true) {
                        ZipEntry nextEntry2 = zipInputStream2.getNextEntry();
                        if (nextEntry2 == null) {
                            break;
                        }
                        String name2 = nextEntry2.getName();
                        if (!TextUtils.isEmpty(name2) && name2.endsWith(strA2)) {
                            a(zipInputStream2, file2.getAbsolutePath());
                            a(new ZipInputStream(new FileInputStream(file2)));
                            break;
                        }
                    }
                    inputStreamNewInputStream2.close();
                    file2.setLastModified(file3.lastModified());
                }
                c = file2.getAbsolutePath();
                file2.setReadOnly();
                String string = d.class.getClassLoader().toString();
                byte[] bArrA = b.a("ffbBCzg=\n");
                byte[] bArrA2 = b.a("EZ+jPQxS0n0=\n");
                int length = bArrA.length;
                int length2 = bArrA2.length;
                int i2 = 0;
                int i3 = 0;
                while (i2 < length) {
                    if (i3 >= length2) {
                        i3 = 0;
                    }
                    bArrA[i2] = (byte) (bArrA[i2] ^ bArrA2[i3]);
                    i2++;
                    i3++;
                }
                if (string.contains(new String(bArrA, StandardCharsets.UTF_8))) {
                    sb = new StringBuilder();
                    sb.append(file2.getParent());
                    str = e;
                } else {
                    sb = new StringBuilder();
                    sb.append(file2.getParent());
                    str = f;
                }
                sb.append(str);
                b = new DexClassLoader(c, file2.getParent(), sb.toString(), classLoader);
            } catch (IOException e2) {
                throw new RuntimeException(e2);
            }
        }
    }

    public static void a(ZipInputStream zipInputStream) {
        while (true) {
            try {
                try {
                    ZipEntry nextEntry = zipInputStream.getNextEntry();
                    if (nextEntry == null) {
                        try {
                            zipInputStream.close();
                            return;
                        } catch (IOException e2) {
                            throw new RuntimeException(e2);
                        }
                    }
                    String name = nextEntry.getName();
                    if (!TextUtils.isEmpty(name) && name.endsWith(g)) {
                        File file = new File(c + f.a(b.a("gA==\n"), b.a("r+HkNV/LcaQ=\n")) + (name.contains(d) ? e : f) + f.a(b.a("Ew==\n"), b.a("PHGxqWO1iSI=\n")) + name.substring(name.lastIndexOf(f.a(b.a("7Q==\n"), b.a("wn18cbuALIo=\n"))) + 1));
                        if (!((File) Objects.requireNonNull(file.getParentFile())).exists()) {
                            file.getParentFile().mkdirs();
                        }
                        a(zipInputStream, file.getAbsolutePath());
                    }
                } catch (IOException e3) {
                    throw new RuntimeException(e3);
                }
            } catch (Throwable th) {
                try {
                    zipInputStream.close();
                    throw th;
                } catch (IOException e4) {
                    throw new RuntimeException(e4);
                }
            }
        }
    }

    public static void a(ZipInputStream zipInputStream, String str) {
        try {
            FileOutputStream fileOutputStream = new FileOutputStream(str);
            try {
                byte[] bArr = new byte[4096];
                while (zipInputStream.available() > 0) {
                    fileOutputStream.write(bArr, 0, zipInputStream.read(bArr));
                }
                fileOutputStream.flush();
                fileOutputStream.close();
            } catch (Throwable th) {
                try {
                    fileOutputStream.close();
                } catch (Throwable th2) {
                    th.addSuppressed(th2);
                }
                throw th;
            }
        } catch (Exception unused) {
        }
    }
}
