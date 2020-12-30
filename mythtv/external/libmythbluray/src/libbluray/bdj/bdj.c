/*
 * This file is part of libbluray
 * Copyright (C) 2010  William Hahne
 * Copyright (C) 2012-2019  Petri Hintukainen <phintuka@users.sourceforge.net>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.s
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library. If not, see
 * <http://www.gnu.org/licenses/>.
 */

#if HAVE_CONFIG_H
#include "config.h"
#endif

#include "bdj.h"

#include "native/register_native.h"

#include "file/file.h"
#include "file/dirs.h"
#include "file/dl.h"
#include "util/strutl.h"
#include "util/macro.h"
#include "util/logging.h"


#include <jni.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef __APPLE__
#include <sys/types.h>
#include <sys/wait.h>
#include <limits.h>
#include <unistd.h>
#endif

#ifdef _WIN32
#include <windows.h>
#include <winreg.h>
#endif

#ifdef HAVE_BDJ_J2ME
#define BDJ_JARFILE "libmythbluray-j2me-" VERSION ".jar"
#else
#define BDJ_JARFILE "libmythbluray-j2se-" VERSION ".jar"
#endif

struct bdjava_s {
#if defined(__APPLE__) && !defined(HAVE_BDJ_J2ME)
    void   *h_libjli;
#endif
    void   *h_libjvm;
    JavaVM *jvm;
};

typedef jint (JNICALL * fptr_JNI_CreateJavaVM) (JavaVM **pvm, void **penv,void *args);
typedef jint (JNICALL * fptr_JNI_GetCreatedJavaVMs) (JavaVM **vmBuf, jsize bufLen, jsize *nVMs);


#if defined(_WIN32) && !defined(HAVE_BDJ_J2ME)
static void *_load_dll(const wchar_t *lib_path, const wchar_t *dll_search_path)
{
    void *result;

    typedef PVOID(WINAPI *AddDllDirectoryF)  (PCWSTR);
    typedef BOOL(WINAPI *RemoveDllDirectoryF)(PVOID);
    AddDllDirectoryF pAddDllDirectory;
    RemoveDllDirectoryF pRemoveDllDirectory;
    pAddDllDirectory = (AddDllDirectoryF)GetProcAddress(GetModuleHandle(TEXT("kernel32.dll")), "AddDllDirectory");
    pRemoveDllDirectory = (RemoveDllDirectoryF)GetProcAddress(GetModuleHandle(TEXT("kernel32.dll")), "RemoveDllDirectory");

    if (pAddDllDirectory && pRemoveDllDirectory) {

        result = LoadLibraryExW(lib_path, NULL,
                               LOAD_LIBRARY_SEARCH_SYSTEM32);

        if (!result) {
            PVOID cookie = pAddDllDirectory(dll_search_path);
            result = LoadLibraryExW(lib_path, NULL,
                                    LOAD_LIBRARY_SEARCH_SYSTEM32 |
                                    LOAD_LIBRARY_SEARCH_USER_DIRS);
            pRemoveDllDirectory(cookie);
        }
    } else {
        result = LoadLibraryW(lib_path);
        if (!result) {
            SetDllDirectoryW(dll_search_path);
            result = LoadLibraryW(lib_path);
            SetDllDirectoryW(L"");
        }
    }

    return result;
}
#endif

#if defined(_WIN32) && !defined(HAVE_BDJ_J2ME)
static void *_load_jvm_win32(const char **p_java_home)
{
    static char java_home[256] = "";

    wchar_t buf_loc[4096] = L"SOFTWARE\\JavaSoft\\Java Runtime Environment\\";
    wchar_t buf_vers[128];
    wchar_t java_path[4096] = L"";
    char strbuf[256];

    LONG r;
    DWORD lType;
    DWORD dSize = sizeof(buf_vers);
    HKEY hkey;

    r = RegOpenKeyExW(HKEY_LOCAL_MACHINE, buf_loc, 0, KEY_READ, &hkey);
# ifndef NO_JAVA9_SUPPORT
    if (r != ERROR_SUCCESS) {
        /* Try Java 9 */
        wcscpy(buf_loc, L"SOFTWARE\\JavaSoft\\JRE\\");
        r = RegOpenKeyExW(HKEY_LOCAL_MACHINE, buf_loc, 0, KEY_READ, &hkey);
    }
# endif
    if (r != ERROR_SUCCESS) {
        BD_DEBUG(DBG_BDJ | DBG_CRIT, "Error opening registry key SOFTWARE\\JavaSoft\\Java Runtime Environment\\\n");
        return NULL;
    }

    r = RegQueryValueExW(hkey, L"CurrentVersion", NULL, &lType, (LPBYTE)buf_vers, &dSize);
    RegCloseKey(hkey);
    if (r != ERROR_SUCCESS) {
        BD_DEBUG(DBG_BDJ | DBG_CRIT, "CurrentVersion registry value not found\n");
        return NULL;
    }

    if (debug_mask & DBG_BDJ) {
        if (!WideCharToMultiByte(CP_UTF8, 0, buf_vers, -1, strbuf, sizeof(strbuf), NULL, NULL)) {
            strbuf[0] = 0;
        }
        BD_DEBUG(DBG_BDJ, "JRE version: %s\n", strbuf);
    }
    wcscat(buf_loc, buf_vers);

    dSize = sizeof(buf_loc);
    r = RegOpenKeyExW(HKEY_LOCAL_MACHINE, buf_loc, 0, KEY_READ, &hkey);
    if (r != ERROR_SUCCESS) {
        BD_DEBUG(DBG_BDJ | DBG_CRIT, "Error opening JRE version-specific registry key\n");
        return NULL;
    }

    r = RegQueryValueExW(hkey, L"JavaHome", NULL, &lType, (LPBYTE)buf_loc, &dSize);

    if (r == ERROR_SUCCESS) {
        /* do not fail even if not found */
        if (WideCharToMultiByte(CP_UTF8, 0, buf_loc, -1, java_home, sizeof(java_home), NULL, NULL)) {
            *p_java_home = java_home;
        }
        BD_DEBUG(DBG_BDJ, "JavaHome: %s\n", java_home);

        wcscat(java_path, buf_loc);
        wcscat(java_path, L"\\bin");
    }

    dSize = sizeof(buf_loc);
    r = RegQueryValueExW(hkey, L"RuntimeLib", NULL, &lType, (LPBYTE)buf_loc, &dSize);
    RegCloseKey(hkey);

    if (r != ERROR_SUCCESS) {
        BD_DEBUG(DBG_BDJ | DBG_CRIT, "RuntimeLib registry value not found\n");
        return NULL;
    }


    void *result = _load_dll(buf_loc, java_path);

    if (!WideCharToMultiByte(CP_UTF8, 0, buf_loc, -1, strbuf, sizeof(strbuf), NULL, NULL)) {
        strbuf[0] = 0;
    }
    if (!result) {
        BD_DEBUG(DBG_BDJ | DBG_CRIT, "can't open library '%s'\n", strbuf);
    } else {
        BD_DEBUG(DBG_BDJ, "Using JRE library %s\n", strbuf);
    }

    return result;
}
#endif

#ifdef _WIN32
static inline char *_utf8_to_cp(const char *utf8)
{
    int wlen = MultiByteToWideChar(CP_UTF8, 0, utf8, -1, NULL, 0);
    if (wlen <= 0) {
        return NULL;
    }

    wchar_t *wide = (wchar_t *)malloc(wlen * sizeof(wchar_t));
    if (!wide) {
        return NULL;
    }
    if (!MultiByteToWideChar(CP_UTF8, 0, utf8, -1, wide, wlen)) {
        X_FREE(wide);
        return NULL;
    }

    size_t len = WideCharToMultiByte(CP_ACP, 0, wide, -1, NULL, 0, NULL, NULL);
    if (len <= 0) {
        X_FREE(wide);
        return NULL;
    }

    char *out = (char *)malloc(len);
    if (out != NULL) {
        if (!WideCharToMultiByte(CP_ACP, 0, wide, -1, out, len, NULL, NULL)) {
            X_FREE(out);
        }
    }
    X_FREE(wide);
    return out;
}
#endif

#ifdef __APPLE__
// The current official JRE is installed by Oracle's Java Applet internet plugin:
#define MACOS_JRE_HOME "/Library/Internet Plug-Ins/JavaAppletPlugin.plugin/Contents/Home"
static const char *jre_plugin_path = MACOS_JRE_HOME;
#endif

#if defined(__APPLE__) && !defined(HAVE_BDJ_J2ME)

#define MACOS_JAVA_HOME "/usr/libexec/java_home"
static char *_java_home_macos()
{
    static char result[PATH_MAX] = "";

    if (result[0])
        return result;

    pid_t java_home_pid;
    int fd[2], exitcode;

    if (pipe(fd)) {
        BD_DEBUG(DBG_BDJ | DBG_CRIT, "unable to set up pipes\n");
        return NULL;
    }

    switch (java_home_pid = vfork())
    {
        case -1:
            BD_DEBUG(DBG_BDJ | DBG_CRIT, "vfork failed\n");
            return NULL;

        case 0:
            if (dup2(fd[1], STDOUT_FILENO) == -1) {
                _exit(-1);
            }

            close(fd[1]);
            close(fd[0]);

            execl(MACOS_JAVA_HOME, MACOS_JAVA_HOME);

            _exit(-1);

        default:
            close(fd[1]);

            for (int len = 0; ;) {
                int n = read(fd[0], result + len, sizeof result - len);
                if (n <= 0)
                    break;

                len += n;
                result[len-1] = '\0';
            }

            waitpid(java_home_pid, &exitcode, 0);
    }

    if (result[0] == '\0' || exitcode) {
        BD_DEBUG(DBG_BDJ | DBG_CRIT,
                 "Unable to read path from " MACOS_JAVA_HOME "\n");
        result[0] = '\0';
        return NULL;
    }

    BD_DEBUG(DBG_BDJ, "macos java home: '%s'\n", result );
    return result;
}
#undef MACOS_JAVA_HOME

#endif

static void *_jvm_dlopen(const char *java_home, const char *jvm_dir, const char *jvm_lib)
{
    if (java_home) {
        char *path = str_printf("%s" DIR_SEP "%s" DIR_SEP "%s", java_home, jvm_dir, jvm_lib);
        if (!path) {
            BD_DEBUG(DBG_CRIT, "out of memory\n");
            return NULL;
        }
        BD_DEBUG(DBG_BDJ, "Opening %s ...\n", path);
        void *h = dl_dlopen(path, NULL);
# ifdef NO_JAVA9_SUPPORT
        /* ignore Java 9+ */
        if (h && dl_dlsym(h, "JVM_DefineModule")) {
            BD_DEBUG(DBG_CRIT | DBG_BDJ, "Ignoring JVM %s: looks like Java 9 or later\n", path);
            dl_dlclose(h);
            h = NULL;
        }
# endif
        X_FREE(path);
        return h;
    } else {
        BD_DEBUG(DBG_BDJ, "Opening %s ...\n", jvm_lib);
        return dl_dlopen(jvm_lib, NULL);
    }
}

static void *_jvm_dlopen_a(const char *java_home,
                           const char * const *jvm_dir, unsigned num_jvm_dir,
                           const char *jvm_lib)
{
  unsigned ii;
  void *dll = NULL;

  for (ii = 0; !dll && ii < num_jvm_dir; ii++) {
      dll = _jvm_dlopen(java_home, jvm_dir[ii], jvm_lib);
  }

  return dll;
}

#if defined(__APPLE__) && !defined(HAVE_BDJ_J2ME)
static void *_load_jli_macos()
{
    const char *java_home = NULL;
    static const char * const jli_dir[]  = {
        "jre/lib/jli", "lib/jli",
    };
    const unsigned num_jli_dir  = sizeof(jli_dir)  / sizeof(jli_dir[0]);

    static const char jli_lib[] = "libjli";
    void *handle;

    /* JAVA_HOME set, use it */
    java_home = getenv("JAVA_HOME");
    if (java_home) {
        return _jvm_dlopen_a(java_home, jli_dir, num_jli_dir, jli_lib);
    }

    java_home = _java_home_macos();
    if (java_home) {
        handle = _jvm_dlopen_a(java_home, jli_dir, num_jli_dir, jli_lib);
        if (handle) {
            return handle;
        }
    }
    // check if the JRE is installed:
    return _jvm_dlopen(jre_plugin_path, "lib/jli", jli_lib);
}
#endif

static void *_load_jvm(const char **p_java_home)
{
#ifdef HAVE_BDJ_J2ME
# ifdef _WIN32
    static const char * const jvm_path[] = {NULL, JDK_HOME};
    static const char * const jvm_dir[]  = {"bin"};
    static const char         jvm_lib[]  = "cvmi";
# else
    static const char * const jvm_path[] = {NULL, JDK_HOME, "/opt/PhoneME"};
    static const char * const jvm_dir[]  = {"bin"};
    static const char         jvm_lib[]  = "libcvm";
# endif
#else /* HAVE_BDJ_J2ME */
# ifdef _WIN32
    static const char * const jvm_path[] = {NULL, JDK_HOME};
    static const char * const jvm_dir[]  = {"jre\\bin\\server",
                                            "bin\\server",
                                            "jre\\bin\\client",
                                            "bin\\client",
    };
    static const char         jvm_lib[]  = "jvm";
# else
#  ifdef __APPLE__
    static const char * const jvm_path[] = {NULL, JDK_HOME, MACOS_JRE_HOME};
    static const char * const jvm_dir[]  = {"jre/lib/server",
                                            "lib/server"};
#  else
    static const char * const jvm_path[] = {NULL,
                                            JDK_HOME,
                                            "/usr/lib/jvm/default-java",
                                            "/usr/lib/jvm/default",
                                            "/usr/lib/jvm/",
                                            "/etc/java-config-2/current-system-vm",
                                            "/usr/lib/jvm/java-7-openjdk",
                                            "/usr/lib/jvm/java-7-openjdk-" JAVA_ARCH,
                                            "/usr/lib/jvm/java-8-openjdk",
                                            "/usr/lib/jvm/java-8-openjdk-" JAVA_ARCH,
                                            "/usr/lib/jvm/java-6-openjdk",
    };
    static const char * const jvm_dir[]  = {"jre/lib/" JAVA_ARCH "/server",
                                            "lib/server",
                                            "lib/client",
    };
#  endif
    static const char         jvm_lib[]  = "libjvm";
# endif
#endif
    const unsigned num_jvm_dir  = sizeof(jvm_dir)  / sizeof(jvm_dir[0]);
    const unsigned num_jvm_path = sizeof(jvm_path) / sizeof(jvm_path[0]);

    const char *java_home = NULL;
    unsigned    path_ind;
    void       *handle = NULL;

    /* JAVA_HOME set, use it */
    java_home = getenv("JAVA_HOME");
    if (java_home) {
        *p_java_home = java_home;
        return _jvm_dlopen_a(java_home, jvm_dir, num_jvm_dir, jvm_lib);
    }

#if defined(_WIN32) && !defined(HAVE_BDJ_J2ME)
    handle = _load_jvm_win32(p_java_home);
    if (handle) {
        return handle;
    }
#endif

#if defined(__APPLE__) && !defined(HAVE_BDJ_J2ME)
    java_home = _java_home_macos();
    if (java_home) {
        handle = _jvm_dlopen_a(java_home, jvm_dir, num_jvm_dir, jvm_lib);
        if (handle) {
            *p_java_home = java_home;
            return handle;
        }
    }
    // check if the JRE is installed:
    handle = _jvm_dlopen(jre_plugin_path, "lib/server", jvm_lib);
    if (handle) {
        *p_java_home = jre_plugin_path;
        return handle;
    }
#endif

    BD_DEBUG(DBG_BDJ, "JAVA_HOME not set, trying default locations\n");

    /* try our pre-defined locations */
    for (path_ind = 0; !handle && path_ind < num_jvm_path; path_ind++) {
        *p_java_home = jvm_path[path_ind];
        handle = _jvm_dlopen_a(jvm_path[path_ind], jvm_dir, num_jvm_dir, jvm_lib);
    }

    if (!*p_java_home) {
        *p_java_home = dl_get_path();
    }

    return handle;
}

static int _can_read_file(const char *fn)
{
    BD_FILE_H *fp;

    if (!fn) {
        return 0;
    }

    fp = file_open(fn, "rb");
    if (fp) {
        uint8_t b;
        int result = (int)file_read(fp, &b, 1);
        file_close(fp);
        if (result == 1) {
            return 1;
        }
        BD_DEBUG(DBG_BDJ | DBG_CRIT, "Error reading %s\n", fn);
    }
    return 0;
}

void bdj_config_cleanup(BDJ_CONFIG *p)
{
    X_FREE(p->cache_root);
    X_FREE(p->persistent_root);
    X_FREE(p->classpath[0]);
    X_FREE(p->classpath[1]);
}

static char *_find_libbluray_jar0()
{
    // pre-defined search paths for libbluray.jar
    static const char * const jar_paths[] = {
#ifndef _WIN32
        "/usr/share/java/" BDJ_JARFILE,
        "/usr/share/libbluray/lib/" BDJ_JARFILE,
#endif
        BDJ_JARFILE,
    };

    unsigned i;

    // check if overriding the classpath
    const char *classpath = getenv("LIBBLURAY_CP");
    if (classpath) {
        size_t cp_len = strlen(classpath);
        char *jar;

        // directory or file ?
        if (cp_len > 0 && (classpath[cp_len - 1] == '/' || classpath[cp_len - 1] == '\\')) {
            jar = str_printf("%s%s", classpath, BDJ_JARFILE);
        } else {
            jar = str_dup(classpath);
        }

        if (!jar) {
            BD_DEBUG(DBG_CRIT, "out of memory\n");
            return NULL;
        }

        if (_can_read_file(jar)) {
            return jar;
        }

        X_FREE(jar);
        BD_DEBUG(DBG_BDJ | DBG_CRIT, "invalid LIBBLURAY_CP %s\n", classpath);
        return NULL;
    }

    BD_DEBUG(DBG_BDJ, "LIBBLURAY_CP not set, searching for "BDJ_JARFILE" ...\n");

    // check directory where libbluray.so was loaded from
    const char *lib_path = dl_get_path();
    if (lib_path) {
        char *cp = str_printf("%s" BDJ_JARFILE, lib_path);
        if (!cp) {
            BD_DEBUG(DBG_CRIT, "out of memory\n");
            return NULL;
        }

        BD_DEBUG(DBG_BDJ, "Checking %s ...\n", cp);
        if (_can_read_file(cp)) {
            BD_DEBUG(DBG_BDJ, "using %s\n", cp);
            return cp;
        }
        X_FREE(cp);
    }

    // check pre-defined directories
    for (i = 0; i < sizeof(jar_paths) / sizeof(jar_paths[0]); i++) {
        BD_DEBUG(DBG_BDJ, "Checking %s ...\n", jar_paths[i]);
        if (_can_read_file(jar_paths[i])) {
            BD_DEBUG(DBG_BDJ, "using %s\n", jar_paths[i]);
            return str_dup(jar_paths[i]);
        }
    }

    BD_DEBUG(DBG_BDJ | DBG_CRIT, BDJ_JARFILE" not found.\n");
    return NULL;
}

static char *_find_libbluray_jar1(const char *jar0)
{
    char *jar1;
    int   cut;

    cut = (int)strlen(jar0) - (int)strlen(VERSION) - 9;
    if (cut <= 0)
        return NULL;

    jar1 = str_printf("%.*sawt-%s", cut, jar0, jar0 + cut);
    if (!jar1)
        return NULL;

    if (!_can_read_file(jar1)) {
        BD_DEBUG(DBG_BDJ | DBG_CRIT, "Cant access AWT jar file %s\n", jar1);
        X_FREE(jar1);
    }

    return jar1;
}

static int _find_libbluray_jar(BDJ_CONFIG *storage)
{
    if (!storage->classpath[0]) {
        storage->classpath[0] = _find_libbluray_jar0();
        X_FREE(storage->classpath[1]);
        if (!storage->classpath[0])
            return 0;
    }

    if (!storage->classpath[1]) {
        storage->classpath[1] = _find_libbluray_jar1(storage->classpath[0]);
        if (!storage->classpath[1]) {
            X_FREE(storage->classpath[0]);
            X_FREE(storage->classpath[1]);
        }
    }

    return !!storage->classpath[0];
}

static const char *_bdj_persistent_root(BDJ_CONFIG *storage)
{
    const char *root;
    char       *data_home;

    if (storage->no_persistent_storage) {
        return NULL;
    }

    if (!storage->persistent_root) {

        root = getenv("LIBBLURAY_PERSISTENT_ROOT");
        if (root) {
            return root;
        }

        data_home = file_get_data_home();
        if (data_home) {
            storage->persistent_root = str_printf("%s" DIR_SEP "bluray" DIR_SEP "dvb.persistent.root" DIR_SEP, data_home);
            X_FREE(data_home);
            BD_DEBUG(DBG_BDJ, "LIBBLURAY_PERSISTENT_ROOT not set, using %s\n", storage->persistent_root);
        }

        if (!storage->persistent_root) {
            BD_DEBUG(DBG_BDJ | DBG_CRIT, "WARNING: BD-J persistent root not set\n");
        }
    }

    return storage->persistent_root;
}

static const char *_bdj_buda_root(BDJ_CONFIG *storage)
{
    const char *root;
    char       *cache_home;

    if (storage->no_persistent_storage) {
        return NULL;
    }

    if (!storage->cache_root) {

        root = getenv("LIBBLURAY_CACHE_ROOT");
        if (root) {
            return root;
        }

        cache_home = file_get_cache_home();
        if (cache_home) {
            storage->cache_root = str_printf("%s" DIR_SEP "bluray" DIR_SEP "bluray.bindingunit.root" DIR_SEP, cache_home);
            X_FREE(cache_home);
            BD_DEBUG(DBG_BDJ, "LIBBLURAY_CACHE_ROOT not set, using %s\n", storage->cache_root);
        }

        if (!storage->cache_root) {
            BD_DEBUG(DBG_BDJ | DBG_CRIT, "WARNING: BD-J cache root not set\n");
        }
    }

    return storage->cache_root;
}

static int _get_method(JNIEnv *env, jclass *cls, jmethodID *method_id,
                       const char *class_name, const char *method_name, const char *method_sig)
{
    *method_id = NULL;
    *cls = (*env)->FindClass(env, class_name);
    if (!*cls) {
        (*env)->ExceptionDescribe(env);
        BD_DEBUG(DBG_BDJ | DBG_CRIT, "Failed to locate class %s\n", class_name);
        (*env)->ExceptionClear(env);
        return 0;
    }

    *method_id = (*env)->GetStaticMethodID(env, *cls, method_name, method_sig);
    if (!*method_id) {
        (*env)->ExceptionDescribe(env);
        BD_DEBUG(DBG_BDJ | DBG_CRIT, "Failed to locate class %s method %s %s\n",
                 class_name, method_name, method_sig);
        (*env)->DeleteLocalRef(env, *cls);
        *cls = NULL;
        (*env)->ExceptionClear(env);
        return 0;
    }

    return 1;
}

static int _bdj_init(JNIEnv *env, struct bluray *bd, const char *disc_root, const char *bdj_disc_id,
                     BDJ_CONFIG *storage)
{
    if (!bdj_register_native_methods(env)) {
        BD_DEBUG(DBG_BDJ | DBG_CRIT, "Couldn't register native methods.\n");
    }

    // initialize class org.videolan.Libbluray
    jclass init_class;
    jmethodID init_id;
    if (!_get_method(env, &init_class, &init_id,
                     "org/videolan/Libbluray", "init",
                     "(JLjava/lang/String;Ljava/lang/String;Ljava/lang/String;Ljava/lang/String;)V")) {
        return 0;
    }

    const char *disc_id = (bdj_disc_id && bdj_disc_id[0]) ? bdj_disc_id : "00000000000000000000000000000000";
    jlong param_bdjava_ptr = (jlong)(intptr_t) bd;
    jstring param_disc_id = (*env)->NewStringUTF(env, disc_id);
    jstring param_disc_root = (*env)->NewStringUTF(env, disc_root);
    jstring param_persistent_root = (*env)->NewStringUTF(env, _bdj_persistent_root(storage));
    jstring param_buda_root = (*env)->NewStringUTF(env, _bdj_buda_root(storage));

    (*env)->CallStaticVoidMethod(env, init_class, init_id,
                                 param_bdjava_ptr, param_disc_id, param_disc_root,
                                 param_persistent_root, param_buda_root);

    (*env)->DeleteLocalRef(env, init_class);
    (*env)->DeleteLocalRef(env, param_disc_id);
    (*env)->DeleteLocalRef(env, param_disc_root);
    (*env)->DeleteLocalRef(env, param_persistent_root);
    (*env)->DeleteLocalRef(env, param_buda_root);

    if ((*env)->ExceptionOccurred(env)) {
        (*env)->ExceptionDescribe(env);
        BD_DEBUG(DBG_BDJ | DBG_CRIT, "Failed to initialize BD-J (uncaught exception)\n");
        (*env)->ExceptionClear(env);
        return 0;
    }

    return 1;
}

int bdj_jvm_available(BDJ_CONFIG *storage)
{
    const char *java_home;
    void* jvm_lib = _load_jvm(&java_home);
    if (!jvm_lib) {
        BD_DEBUG(DBG_BDJ | DBG_CRIT, "BD-J check: Failed to load JVM library\n");
        return BDJ_CHECK_NO_JVM;
    }
    dl_dlclose(jvm_lib);

    if (!_find_libbluray_jar(storage)) {
        BD_DEBUG(DBG_BDJ | DBG_CRIT, "BD-J check: Failed to load libbluray.jar\n");
        return BDJ_CHECK_NO_JAR;
    }

    BD_DEBUG(DBG_BDJ, "BD-J check: OK\n");

    return BDJ_CHECK_OK;
}

static int _find_jvm(void *jvm_lib, JNIEnv **env, JavaVM **jvm)
{
    fptr_JNI_GetCreatedJavaVMs JNI_GetCreatedJavaVMs_fp;

    *(void **)(&JNI_GetCreatedJavaVMs_fp) = dl_dlsym(jvm_lib, "JNI_GetCreatedJavaVMs");
    if (JNI_GetCreatedJavaVMs_fp == NULL) {
        BD_DEBUG(DBG_BDJ | DBG_CRIT, "Couldn't find symbol JNI_GetCreatedJavaVMs.\n");
        return 0;
    }

    jsize nVMs = 0;
    JavaVM* javavm = NULL;

    int result = JNI_GetCreatedJavaVMs_fp(&javavm, 1, &nVMs);
    if (result == JNI_OK && nVMs > 0) {
      *jvm = javavm;
      (**jvm)->AttachCurrentThread(*jvm, (void**)env, NULL);
      return 1;
    }

    return 0;
}

/* Export packages for Xlets */
static const char * const java_base_exports[] = {
        "javax.media" ,
        "javax.media.protocol",
        "javax.tv.graphics",
        "javax.tv.service",
        "javax.tv.service.guide",
        "javax.tv.service.selection",
        "javax.tv.service.transport",
        "javax.tv.service.navigation",
        "javax.tv.net",
        "javax.tv.locator",
        "javax.tv.util",
        "javax.tv.media",
        "javax.tv.xlet",
        "javax.microedition.xlet",
        "org.davic.resources",
        "org.davic.net",
        "org.davic.media",
        "org.davic.mpeg",
        "org.dvb.user",
        "org.dvb.dsmcc",
        "org.dvb.application",
        "org.dvb.ui",
        "org.dvb.test",
        "org.dvb.lang",
        "org.dvb.event",
        "org.dvb.io.ixc",
        "org.dvb.io.persistent",
        "org.dvb.media",
        "org.havi.ui",
        "org.havi.ui.event",
        "org.bluray.application",
        "org.bluray.ui",
        "org.bluray.ui.event",
        "org.bluray.net",
        "org.bluray.storage",
        "org.bluray.vfs",
        "org.bluray.bdplus",
        "org.bluray.system",
        "org.bluray.media",
        "org.bluray.ti",
        "org.bluray.ti.selection",
        "org.blurayx.s3d.ui",
        "org.blurayx.s3d.system",
        "org.blurayx.s3d.media",
        "org.blurayx.s3d.ti",
        "org.blurayx.uhd.ui",
        "org.blurayx.uhd.system",
        "org.blurayx.uhd.ti",
        "com.aacsla.bluray.online",
        "com.aacsla.bluray.mc",
        "com.aacsla.bluray.mt",
        "org.videolan.backdoor", /* entry for injected Xlet / runtime fixes */
};
static const size_t num_java_base_exports = sizeof(java_base_exports) / sizeof(java_base_exports[0]);

static int _create_jvm(void *jvm_lib, const char *java_home, const char *jar_file[2],
                       JNIEnv **env, JavaVM **jvm)
{
    (void)java_home;  /* used only with J2ME */

    fptr_JNI_CreateJavaVM JNI_CreateJavaVM_fp;
    JavaVMOption option[96];
    int n = 0, result, java_9;
    JavaVMInitArgs args;

    *(void **)(&JNI_CreateJavaVM_fp) = dl_dlsym(jvm_lib, "JNI_CreateJavaVM");
    if (JNI_CreateJavaVM_fp == NULL) {
        BD_DEBUG(DBG_BDJ | DBG_CRIT, "Couldn't find symbol JNI_CreateJavaVM.\n");
        return 0;
    }

#ifdef HAVE_BDJ_J2ME
    java_9 = 0;
#else
    java_9 = !!dl_dlsym(jvm_lib, "JVM_DefineModule");
    if (java_9) {
        BD_DEBUG(DBG_BDJ, "Detected Java 9 or later JVM\n");
    }
#endif

    memset(option, 0, sizeof(option));

    option[n++].optionString = str_dup   ("-Dawt.toolkit=java.awt.BDToolkit");
    option[n++].optionString = str_dup   ("-Djava.awt.graphicsenv=java.awt.BDGraphicsEnvironment");
    option[n++].optionString = str_dup   ("-Xms256M");
    option[n++].optionString = str_dup   ("-Xmx256M");
    option[n++].optionString = str_dup   ("-Xss2048k");
#ifdef HAVE_BDJ_J2ME
    option[n++].optionString = str_printf("-Djava.home=%s", java_home);
    option[n++].optionString = str_printf("-Xbootclasspath/a:%s/lib/xmlparser.jar", java_home);
    option[n++].optionString = str_dup   ("-XfullShutdown");
#endif

#ifdef _WIN32
# define CLASSPATH_FORMAT_P "%s;%s"
#else
# define CLASSPATH_FORMAT_P "%s:%s"
#endif

    if (!java_9) {
      option[n++].optionString = str_dup   ("-Djavax.accessibility.assistive_technologies= ");
      option[n++].optionString = str_printf("-Xbootclasspath/p:" CLASSPATH_FORMAT_P, jar_file[0], jar_file[1]);
    } else {
      option[n++].optionString = str_printf("--patch-module=java.base=%s", jar_file[0]);
      option[n++].optionString = str_printf("--patch-module=java.desktop=%s", jar_file[1]);

      /* Fix module graph */

      option[n++].optionString = str_dup("--add-reads=java.base=java.desktop");
      /* org.videolan.IxcRegistryImpl -> java.rmi.Remote */
      option[n++].optionString = str_dup("--add-reads=java.base=java.rmi");
      /* org.videolan.FontIndex -> java.xml. */
      option[n++].optionString = str_dup("--add-reads=java.base=java.xml");
      /* AWT needs to access logger and Xlet context */
      option[n++].optionString = str_dup("--add-opens=java.base/org.videolan=java.desktop");
      /* AWT needs to acess DVBGraphics */
      option[n++].optionString = str_dup("--add-exports=java.base/org.dvb.ui=java.desktop");
      /* org.havi.ui.HBackgroundImage needs to access sun.awt.image.FileImageSource */
      option[n++].optionString = str_dup("--add-exports=java.desktop/sun.awt.image=java.base");

      /* Export BluRay packages to Xlets */
      for (size_t idx = 0; idx < num_java_base_exports; idx++) {
          option[n++].optionString = str_printf("--add-exports=java.base/%s=ALL-UNNAMED", java_base_exports[idx]);
      }
    }

    /* JVM debug options */

    if (getenv("BDJ_JVM_DISABLE_JIT")) {
        BD_DEBUG(DBG_CRIT | DBG_BDJ, "Disabling BD-J JIT\n");
        option[n++].optionString = str_dup("-Xint");
    }
    if (getenv("BDJ_JVM_DEBUG")) {
        BD_DEBUG(DBG_CRIT | DBG_BDJ, "Enabling BD-J debug mode\n");
        option[n++].optionString = str_dup("-ea");
        //option[n++].optionString = str_dup("-verbose");
        //option[n++].optionString = str_dup("-verbose:class,gc,jni");
        option[n++].optionString = str_dup("-Xdebug");
        option[n++].optionString = str_dup("-Xrunjdwp:transport=dt_socket,address=8000,server=y,suspend=n");
    }

#ifdef HAVE_BDJ_J2ME
    /*
      see: http://docs.oracle.com/javame/config/cdc/cdc-opt-impl/ojmeec/1.0/runtime/html/cvm.htm#CACBHBJB
      trace method execution: BDJ_JVM_TRACE=0x0002
      trace exceptions:       BDJ_JVM_TRACE=0x4000
    */
    if (getenv("BDJ_JVM_TRACE")) {
        option[n++].optionString = str_printf("-Xtrace:%s", getenv("BDJ_JVM_TRACE"));
    }
#endif

    args.version = JNI_VERSION_1_4;
    args.nOptions = n;
    args.options = option;
    args.ignoreUnrecognized = JNI_FALSE; // don't ignore unrecognized options

#ifdef _WIN32
    /* ... in windows, JVM options are not UTF8 but current system code page ... */
    /* luckily, most BD-J options can be passed in as java strings later. But, not class path. */
    int ii;
    for (ii = 0; ii < n; ii++) {
        char *tmp = _utf8_to_cp(option[ii].optionString);
        if (tmp) {
            X_FREE(option[ii].optionString);
            option[ii].optionString = tmp;
        } else {
            BD_DEBUG(DBG_BDJ | DBG_CRIT, "Failed to convert %s\n", option[ii].optionString);
        }
    }
#endif

    result = JNI_CreateJavaVM_fp(jvm, (void**) env, &args);

    while (--n >= 0) {
        X_FREE(option[n].optionString);
    }

    if (result != JNI_OK || !*env) {
        BD_DEBUG(DBG_BDJ | DBG_CRIT, "Failed to create new Java VM. JNI_CreateJavaVM result: %d\n", result);
        return 0;
    }

    return 1;
}

BDJAVA* bdj_open(const char *path, struct bluray *bd,
                 const char *bdj_disc_id, BDJ_CONFIG *cfg)
{
    BD_DEBUG(DBG_BDJ, "bdj_open()\n");

    if (!_find_libbluray_jar(cfg)) {
        BD_DEBUG(DBG_BDJ | DBG_CRIT, "BD-J start failed: " BDJ_JARFILE " not found.\n");
        return NULL;
    }

#if defined(__APPLE__) && !defined(HAVE_BDJ_J2ME)
    /* On macOS we need to load libjli to workaround a bug where the wrong
     * version would be used: https://bugs.openjdk.java.net/browse/JDK-7131356
     */
    void* jli_lib = _load_jli_macos();
    if (!jli_lib) {
        BD_DEBUG(DBG_BDJ, "Wasn't able to load JLI\n");
    }
#endif

    // first load the jvm using dlopen
    const char *java_home = NULL;
    void* jvm_lib = _load_jvm(&java_home);

    if (!jvm_lib) {
        BD_DEBUG(DBG_BDJ | DBG_CRIT, "Wasn't able to load JVM\n");
        return 0;
    }

    BDJAVA* bdjava = calloc(1, sizeof(BDJAVA));
    if (!bdjava) {
        dl_dlclose(jvm_lib);
        return NULL;
    }

    JNIEnv* env = NULL;
    JavaVM *jvm = NULL;
    const char *jar[2] = { cfg->classpath[0], cfg->classpath[1] };
    if (!_find_jvm(jvm_lib, &env, &jvm) &&
        !_create_jvm(jvm_lib, java_home, jar, &env, &jvm)) {

        X_FREE(bdjava);
        dl_dlclose(jvm_lib);
        return NULL;
    }

#if defined(__APPLE__) && !defined(HAVE_BDJ_J2ME)
    bdjava->h_libjli = jli_lib;
#endif
    bdjava->h_libjvm = jvm_lib;
    bdjava->jvm = jvm;

    if (debug_mask & DBG_JNI) {
        int version = (int)(*env)->GetVersion(env);
        BD_DEBUG(DBG_BDJ, "Java version: %d.%d\n", version >> 16, version & 0xffff);
    }

    if (!_bdj_init(env, bd, path, bdj_disc_id, cfg)) {
        bdj_close(bdjava);
        return NULL;
    }

    /* detach java main thread (CreateJavaVM attachs calling thread to JVM) */
    (*bdjava->jvm)->DetachCurrentThread(bdjava->jvm);

    return bdjava;
}

void bdj_close(BDJAVA *bdjava)
{
    JNIEnv *env;
    int attach = 0;
    jclass shutdown_class;
    jmethodID shutdown_id;

    if (!bdjava) {
        return;
    }

    BD_DEBUG(DBG_BDJ, "bdj_close()\n");

    if (bdjava->jvm) {
        if ((*bdjava->jvm)->GetEnv(bdjava->jvm, (void**)&env, JNI_VERSION_1_4) != JNI_OK) {
            (*bdjava->jvm)->AttachCurrentThread(bdjava->jvm, (void**)&env, NULL);
            attach = 1;
        }

        if (_get_method(env, &shutdown_class, &shutdown_id,
                           "org/videolan/Libbluray", "shutdown", "()V")) {
            (*env)->CallStaticVoidMethod(env, shutdown_class, shutdown_id);

            if ((*env)->ExceptionOccurred(env)) {
                (*env)->ExceptionDescribe(env);
                BD_DEBUG(DBG_BDJ | DBG_CRIT, "Failed to shutdown BD-J (uncaught exception)\n");
                (*env)->ExceptionClear(env);
            }

            (*env)->DeleteLocalRef(env, shutdown_class);
        }

        bdj_unregister_native_methods(env);

        if (attach) {
            (*bdjava->jvm)->DetachCurrentThread(bdjava->jvm);
        }
    }

    if (bdjava->h_libjvm) {
        dl_dlclose(bdjava->h_libjvm);
    }

#if defined(__APPLE__) && !defined(HAVE_BDJ_J2ME)
    if (bdjava->h_libjli) {
        dl_dlclose(bdjava->h_libjli);
    }
#endif

    X_FREE(bdjava);
}

int bdj_process_event(BDJAVA *bdjava, unsigned ev, unsigned param)
{
    static const char * const ev_name[] = {
        /*  0 */ "NONE",

        /*  1 */ "START",
        /*  2 */ "STOP",
        /*  3 */ "PSR102",

        /*  4 */ "PLAYLIST",
        /*  5 */ "PLAYITEM",
        /*  6 */ "CHAPTER",
        /*  7 */ "MARK",
        /*  8 */ "PTS",
        /*  9 */ "END_OF_PLAYLIST",

        /* 10 */ "SEEK",
        /* 11 */ "RATE",

        /* 12 */ "ANGLE",
        /* 13 */ "AUDIO_STREAM",
        /* 14 */ "SUBTITLE",
        /* 15 */ "SECONDARY_STREAM",

        /* 16 */ "VK_KEY",
        /* 17 */ "UO_MASKED",
        /* 18 */ "MOUSE",
    };

    JNIEnv* env;
    int attach = 0;
    jclass event_class;
    jmethodID event_id;
    int result = -1;

    if (!bdjava) {
        return -1;
    }

    if (ev > BDJ_EVENT_LAST) {
        BD_DEBUG(DBG_BDJ | DBG_CRIT, "bdj_process_event(%d,%d): unknown event\n", ev, param);
    }
    // Disable too verbose logging (PTS)
    else if (ev != BDJ_EVENT_PTS) {
        BD_DEBUG(DBG_BDJ, "bdj_process_event(%s,%d)\n", ev_name[ev], param);
    }

    if ((*bdjava->jvm)->GetEnv(bdjava->jvm, (void**)&env, JNI_VERSION_1_4) != JNI_OK) {
        (*bdjava->jvm)->AttachCurrentThread(bdjava->jvm, (void**)&env, NULL);
        attach = 1;
    }

    if (_get_method(env, &event_class, &event_id,
                       "org/videolan/Libbluray", "processEvent", "(II)Z")) {
        if ((*env)->CallStaticBooleanMethod(env, event_class, event_id, (jint)ev, (jint)param)) {
            result = 0;
        }

        if ((*env)->ExceptionOccurred(env)) {
            (*env)->ExceptionDescribe(env);
            BD_DEBUG(DBG_BDJ | DBG_CRIT, "bdj_process_event(%u,%u) failed (uncaught exception)\n", ev, param);
            (*env)->ExceptionClear(env);
        }

        (*env)->DeleteLocalRef(env, event_class);
    }

    if (attach) {
        (*bdjava->jvm)->DetachCurrentThread(bdjava->jvm);
    }

    return result;
}
