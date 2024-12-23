/*
 * Copyright (c) 2009, 2015, Oracle and/or its affiliates. All rights reserved.
 * DO NOT ALTER OR REMOVE COPYRIGHT NOTICES OR THIS FILE HEADER.
 *
 * This code is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License version 2 only, as
 * published by the Free Software Foundation.  Oracle designates this
 * particular file as subject to the "Classpath" exception as provided
 * by Oracle in the LICENSE file that accompanied this code.
 *
 * This code is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License
 * version 2 for more details (a copy is included in the LICENSE file that
 * accompanied this code).
 *
 * You should have received a copy of the GNU General Public License version
 * 2 along with this work; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301 USA.
 *
 * Please contact Oracle, 500 Oracle Parkway, Redwood Shores, CA 94065 USA
 * or visit www.oracle.com if you need additional information or have any
 * questions.
 */

#ifdef WIN32

#include <windows.h>
#include <stdio.h>
#include <stddef.h>
#include <stdlib.h>

#include <jni.h>
#include <com_sun_javafx_font_PrismFontFactory.h>

JNIEXPORT jstring JNICALL
Java_com_sun_javafx_font_PrismFontFactory_getFontPath(JNIEnv *env, jobject thiz)
{
    const wchar_t* const FONTS_DIR = L"\\Fonts";
    const UINT FONTS_DIR_STRLEN = (UINT)wcslen(FONTS_DIR);
    errno_t err = 0;
    wchar_t* windir = NULL;
    UINT windirBufSize = 0; // total buffer size in wchar_t elements (including NULL terminator)
    UINT windirFilledLen = 0; // amount of wchar_t elements filled (excluding NULL terminator)
    wchar_t* sysdir = NULL;
    UINT sysdirBufSize = 0;
    UINT sysdirFilledLen = 0;
    wchar_t* fontpath = NULL;
    UINT fontpathBufSize = 0;
    UINT fontpathFilledLen = 0;
    wchar_t *end = NULL;
    jsize pathLen = 0;
    jstring stringObj = NULL;

    /* Preallocate dummy 1-char buffers in case WinAPI wants to write something despite count being 0.
     *
     * Windows API doesn't explicitly say that calling GetSystem/GetWindowsDirectory(NULL, 0)
     * will NOT attempt writing to NULL, so we want to provide something to prevent access
     * violation if they do
     */
    sysdir = calloc(1, sizeof(wchar_t));
    windir = calloc(1, sizeof(wchar_t));
    if (!sysdir || !windir) goto finish;

    /* Locate fonts directories relative to the Windows System directory.
     * If Windows System location is different than the user's window
     * directory location, as in a shared Windows installation,
     * return both locations as potential font directories
     */
    UINT ret = GetSystemDirectoryW(sysdir, 0);
    if (ret == 0) {
        goto finish;
    } else {
        /* if buffer is too small, GetSystemDirectory will return the length in wchar_t-s
         * that's needed to allocate the buffer, including terminating null character.
         * `*Len` variables keep amount of elements without the NULL-terminator, so appropriate
         * corrections have to be applied here.
         */
        free(sysdir);
        sysdirBufSize = ret + FONTS_DIR_STRLEN;
        sysdir = calloc(sysdirBufSize, sizeof(wchar_t));
        if (!sysdir) goto finish;
        ret = GetSystemDirectoryW(sysdir, sysdirBufSize - FONTS_DIR_STRLEN);
        if (ret == 0 || ret >= (sysdirBufSize - FONTS_DIR_STRLEN)) {
            goto finish;
        }
        // write was successful so now ret contains only characters written (without NULL-terminator)
        sysdirFilledLen = ret;
    }

    /* Figure out fonts location based on acquired System directory - commonly it is one level up
     * from sysdir and then in "Fonts" dir
     */
    end = wcsrchr(sysdir, '\\');
    if (end) {
        const wchar_t* const SYSTEM_DIR = L"\\System";
        const wchar_t* const SYSTEM32_DIR = L"\\System32";

        UINT sysdirToEnd = (UINT)(end - sysdir);
        UINT endLen = sysdirFilledLen - sysdirToEnd;

        // If the last directory in sysdir is "System" or "System32", strip that and replace it with "\Fonts"
        if ((_wcsnicmp(end, SYSTEM_DIR, endLen) == 0) || (_wcsnicmp(end, SYSTEM32_DIR, endLen) == 0)) {
            *end = 0;
            // no length re-check here, both \System and \System32 are longer than \Fonts string
            sysdirFilledLen -= endLen;
            err = wcsncat_s(sysdir, sysdirBufSize, FONTS_DIR, FONTS_DIR_STRLEN);
            if (err != 0) goto finish;
            sysdirFilledLen += FONTS_DIR_STRLEN;
        }
    }

    // Acquire Windows' directory - follows same assumptions as sysdir above
    ret = GetWindowsDirectoryW(windir, 0);
    if (ret == 0) {
        goto finish;
    } else {
        free(windir);
        windirBufSize = ret + FONTS_DIR_STRLEN;
        windir = calloc(windirBufSize, sizeof(wchar_t));
        if (!windir) goto finish;
        ret = GetWindowsDirectoryW(windir, windirBufSize - FONTS_DIR_STRLEN);
        if (ret == 0 || ret >= (windirBufSize - FONTS_DIR_STRLEN)) {
            goto finish;
        }
        windirFilledLen = ret;
    }

    // "Fonts" directory should be placed right inside Windows directory, so just append it to the path
    err = wcsncat_s(windir, windirBufSize, FONTS_DIR, FONTS_DIR_STRLEN);
    if (err != 0) goto finish;
    windirFilledLen += FONTS_DIR_STRLEN;

    // Copy sysdir to final string that we'll return to JVM
    fontpathBufSize = windirFilledLen + sysdirFilledLen + 2; // add semicolon and zero-terminator
    fontpath = calloc(fontpathBufSize, sizeof(wchar_t));
    if (!fontpath) goto finish;
    err = wcsncpy_s(fontpath, fontpathBufSize, sysdir, sysdirFilledLen);
    if (err != 0) goto finish;
    fontpathFilledLen = sysdirFilledLen;

    /* JFX expects either one path, or two separated by a semicolon.
     * If sysdir and windir are different, form a complete path "list" by
     * joining them in "<sysdir>;<windir>" pattern. JVM side will unpack it.
     */
    if (_wcsnicmp(sysdir, windir, min(sysdirFilledLen, windirFilledLen))) {
        err = wcsncat_s(fontpath, fontpathBufSize, L";", 1);
        if (err != 0) goto finish;
        err = wcsncat_s(fontpath, fontpathBufSize, windir, windirFilledLen);
        if (err != 0) goto finish;
        fontpathFilledLen += windirFilledLen + 1;
    }

    pathLen = (jsize)wcsnlen_s(fontpath, fontpathBufSize);

    // Lastly, return what we just created to JVM as a String
    stringObj = (*env)->NewString(env, fontpath, fontpathFilledLen);

finish:
    if (sysdir != NULL) {
        free(sysdir);
    }
    if (windir != NULL) {
        free(windir);
    }
    if (fontpath != NULL) {
        free(fontpath);
    }

    return stringObj;
}

/* The code below is used to obtain information from the windows font APIS
 * and registry on which fonts are available and what font files hold those
 * fonts. The results are used to speed font lookup.
 */

typedef struct GdiFontMapInfo {
    JNIEnv *env;
    jstring family;
    jobject fontToFamilyMap;
    jobject familyToFontListMap;
    jobject list;
    jmethodID putMID;
    jmethodID containsKeyMID;
    jclass arrayListClass;
    jmethodID arrayListCtr;
    jmethodID addMID;
    jmethodID toLowerCaseMID;
    jobject locale;
    HDC screenDC;
} GdiFontMapInfo;

/* NT is W2K & XP, Vista, Win 7 etc. ie anything later than win9x */
static const char FONTKEY_NT[] =
    "Software\\Microsoft\\Windows NT\\CurrentVersion\\Fonts";


typedef struct CheckFamilyInfo {
  wchar_t *family;
  wchar_t* fullName;
  int isDifferent;
} CheckFamilyInfo;

static int CALLBACK CheckFontFamilyProcW(
  ENUMLOGFONTEXW *lpelfe,
  NEWTEXTMETRICEX *lpntme,
  int FontType,
  LPARAM lParam)
{
    CheckFamilyInfo *info = (CheckFamilyInfo*)lParam;
    info->isDifferent = wcscmp(lpelfe->elfLogFont.lfFaceName, info->family);

/*     if (!info->isDifferent) { */
/*         wprintf(LFor font %s expected family=%s instead got %s\n", */
/*                 lpelfe->elfFullName, */
/*                 info->family, */
/*                 lpelfe->elfLogFont.lfFaceName); */
/*         fflush(stdout); */
/*     } */
    return 0;
}

static int DifferentFamily(wchar_t *family, wchar_t* fullName,
                           GdiFontMapInfo *fmi) {
    LOGFONTW lfw;
    CheckFamilyInfo info;

    /* If fullName can't be stored in the struct, assume correct family */
    if (wcslen((LPWSTR)fullName) >= LF_FACESIZE) {
        return 0;
    }

    memset(&info, 0, sizeof(CheckFamilyInfo));
    info.family = family;
    info.fullName = fullName;
    info.isDifferent = 0;

    memset(&lfw, 0, sizeof(lfw));
    wcscpy(lfw.lfFaceName, fullName);
    lfw.lfCharSet = DEFAULT_CHARSET;
    EnumFontFamiliesExW(fmi->screenDC, &lfw,
                        (FONTENUMPROCW)CheckFontFamilyProcW,
                        (LPARAM)(&info), 0L);

    return info.isDifferent;
}

/* Callback for call to EnumFontFamiliesEx in the EnumFamilyNames function.
 * Expects to be called once for each face name in the family specified
 * in the call. We extract the full name for the font which is expected
 * to be in the "system encoding" and create canonical and lower case
 * Java strings for the name which are added to the maps. The lower case
 * name is used as key to the family name value in the font to family map,
 * the canonical name is one of the"list" of members of the family.
 */
static int CALLBACK EnumFontFacesInFamilyProcW(
  ENUMLOGFONTEXW *lpelfe,
  NEWTEXTMETRICEX *lpntme,
  int FontType,
  LPARAM lParam)
{
    GdiFontMapInfo *fmi = (GdiFontMapInfo*)lParam;
    JNIEnv *env = fmi->env;
    jstring fullname, fullnameLC;

    /* Both Vista and XP return DEVICE_FONTTYPE for OTF fonts */
    if (FontType != TRUETYPE_FONTTYPE && FontType != DEVICE_FONTTYPE) {
        return 1;
    }

    /* Windows has font aliases and so may enumerate fonts from
     * the aliased family if any actual font of that family is installed.
     * To protect against it ignore fonts which aren't enumerated under
     * their true family.
     */
    if (DifferentFamily(lpelfe->elfLogFont.lfFaceName,
                        lpelfe->elfFullName, fmi))  {
      return 1;
    }

    fullname = (*env)->NewString(env, lpelfe->elfFullName,
                                 wcslen((LPWSTR)lpelfe->elfFullName));
    if (fullname == NULL) {
        (*env)->ExceptionClear(env);
        return 1;
    }

    fullnameLC = (*env)->CallObjectMethod(env, fullname,
                                          fmi->toLowerCaseMID, fmi->locale);
    (*env)->CallBooleanMethod(env, fmi->list, fmi->addMID, fullname);
    (*env)->CallObjectMethod(env, fmi->fontToFamilyMap,
                             fmi->putMID, fullnameLC, fmi->family);
    return 1;
}

/* Callback for EnumFontFamiliesEx in populateFontFileNameMap.
 * Expects to be called for every charset of every font family.
 * If this is the first time we have been called for this family,
 * add a new mapping to the familyToFontListMap from this family to a
 * list of its members. To populate that list, further enumerate all faces
 * in this family for the matched charset. This assumes that all fonts
 * in a family support the same charset, which is a fairly safe assumption
 * and saves time as the call we make here to EnumFontFamiliesEx will
 * enumerate the members of this family just once each.
 * Because we set fmi->list to be the newly created list the call back
 * can safely add to that list without a search.
 */
static int CALLBACK EnumFamilyNamesW(
  ENUMLOGFONTEXW *lpelfe,    /* pointer to logical-font data */
  NEWTEXTMETRICEX *lpntme,  /* pointer to physical-font data */
  int FontType,             /* type of font */
  LPARAM lParam )           /* application-defined data */
{
    GdiFontMapInfo *fmi = (GdiFontMapInfo*)lParam;
    JNIEnv *env = fmi->env;
    jstring familyLC;
    int slen;
    LOGFONTW lfw;

    /* Both Vista and XP return DEVICE_FONTTYPE for OTF fonts */
    if (FontType != TRUETYPE_FONTTYPE && FontType != DEVICE_FONTTYPE) {
        return 1;
    }
/*     wprintf(L"FAMILY=%s charset=%d FULL=%s\n", */
/*          lpelfe->elfLogFont.lfFaceName, */
/*          lpelfe->elfLogFont.lfCharSet, */
/*          lpelfe->elfFullName); */
/*     fflush(stdout); */

    /* Windows lists fonts which have a vmtx (vertical metrics) table twice.
     * Once using their normal name, and again preceded by '@'. These appear
     * in font lists in some windows apps, such as wordpad. We don't want
     * these so we skip any font where the first character is '@'
     */
    if (lpelfe->elfLogFont.lfFaceName[0] == L'@') {
            return 1;
    }
    slen = wcslen(lpelfe->elfLogFont.lfFaceName);
    fmi->family = (*env)->NewString(env,lpelfe->elfLogFont.lfFaceName, slen);
    if (fmi->family == NULL) {
        (*env)->ExceptionClear(env);
        return 1;
    }
    familyLC = (*env)->CallObjectMethod(env, fmi->family,
                                        fmi->toLowerCaseMID, fmi->locale);
    /* check if already seen this family with a different charset */
    if ((*env)->CallBooleanMethod(env,fmi->familyToFontListMap,
                                  fmi->containsKeyMID, familyLC)) {
        return 1;
    }
    fmi->list = (*env)->NewObject(env,
                                  fmi->arrayListClass, fmi->arrayListCtr, 4);
    if (fmi->list == NULL) {
        (*env)->ExceptionClear(env);
        return 1;
    }
    (*env)->CallObjectMethod(env, fmi->familyToFontListMap,
                             fmi->putMID, familyLC, fmi->list);

    memset(&lfw, 0, sizeof(lfw));
    wcscpy(lfw.lfFaceName, lpelfe->elfLogFont.lfFaceName);
    lfw.lfCharSet = lpelfe->elfLogFont.lfCharSet;
    EnumFontFamiliesExW(fmi->screenDC, &lfw,
                        (FONTENUMPROCW)EnumFontFacesInFamilyProcW,
                        lParam, 0L);
    return 1;
}


/* It looks like TrueType fonts have " (TrueType)" tacked on the end of their
 * name, so we can try to use that to distinguish TT from other fonts.
 * However if a program "installed" a font in the registry the key may
 * not include that. We could also try to "pass" fonts which have no "(..)"
 * at the end. But that turns out to pass a few .FON files that MS supply.
 * If there's no parenthesised type string, we could next try to infer
 * the file type from the file name extension. Since the MS entries that
 * have no type string are very few, and have odd names like "MS-DOS CP 437"
 * and would never return a Java Font anyway its currently OK to put these
 * in the font map, although clearly the returned names must never percolate
 * up into a list of available fonts returned to the application.
 * Additionally for TTC font files the key looks like
 * Font 1 & Font 2 (TrueType)
 * or sometimes even :
 * Font 1 & Font 2 & Font 3 (TrueType)
 * Also if a Font has a name for this locale that name also
 * exists in the registry using the appropriate platform encoding.
 * What do we do then?
 *
 * Note: OpenType fonts seems to have " (TrueType)" suffix on Vista
 *   but " (OpenType)" on XP.
 */
static BOOL RegistryToBaseTTNameW(LPWSTR name) {
    static const wchar_t TTSUFFIX[] = L" (TrueType)";
    static const wchar_t OTSUFFIX[] = L" (OpenType)";
    int TTSLEN = wcslen(TTSUFFIX);
    wchar_t *suffix;

    int len = wcslen(name);
    if (len == 0) {
        return FALSE;
    }
    if (name[len-1] != L')') {
        return FALSE;
    }
    if (len <= TTSLEN) {
        return FALSE;
    }
    /* suffix length is the same for truetype and opentype fonts */
    suffix = name + (len - TTSLEN);
    // REMIND : renable OpenType (.otf) some day.
    if (wcscmp(suffix, TTSUFFIX) == 0 /*|| wcscmp(suffix, OTSUFFIX) == 0*/) {
        suffix[0] = L'\0'; /* truncate name */
        return TRUE;
    }
    return FALSE;
}

static void registerFontW(GdiFontMapInfo *fmi, jobject fontToFileMap,
                          LPWSTR name, LPWSTR data) {

    wchar_t *ptr1, *ptr2;
    jstring fontStr;
    JNIEnv *env = fmi->env;
    int dslen = wcslen(data);
    jstring fileStr = (*env)->NewString(env, data, dslen);
    if (fileStr == NULL) {
        (*env)->ExceptionClear(env);
        return;
    }

    /* TTC or ttc means it may be a collection. Need to parse out
     * multiple font face names separated by " & "
     * By only doing this for fonts which look like collections based on
     * file name we are adhering to MS recommendations for font file names
     * so it seems that we can be sure that this identifies precisely
     * the MS-supplied truetype collections.
     * This avoids any potential issues if a TTF file happens to have
     * a & in the font name (I can't find anything which prohibits this)
     * and also means we only parse the key in cases we know to be
     * worthwhile.
     */

    if ((data[dslen-1] == L'C' || data[dslen-1] == L'c') &&
        (ptr1 = wcsstr(name, L" & ")) != NULL) {
        ptr1+=3;
        while (ptr1 >= name) { /* marginally safer than while (true) */
            while ((ptr2 = wcsstr(ptr1, L" & ")) != NULL) {
                ptr1 = ptr2+3;
            }
            fontStr = (*env)->NewString(env, ptr1, wcslen(ptr1));
            if (fontStr == NULL) {
                (*env)->ExceptionClear(env);
                return;
            }
            fontStr = (*env)->CallObjectMethod(env, fontStr,
                                               fmi->toLowerCaseMID,
                                               fmi->locale);
            (*env)->CallObjectMethod(env, fontToFileMap, fmi->putMID,
                                     fontStr, fileStr);
            if (ptr1 == name) {
                break;
            } else {
                *(ptr1-3) = L'\0';
                ptr1 = name;
            }
        }
    } else {
        fontStr = (*env)->NewString(env, name, wcslen(name));
        if (fontStr == NULL) {
            (*env)->ExceptionClear(env);
            return;
        }
        fontStr = (*env)->CallObjectMethod(env, fontStr,
                                           fmi->toLowerCaseMID, fmi->locale);
        (*env)->CallObjectMethod(env, fontToFileMap, fmi->putMID,
                                 fontStr, fileStr);
    }
}

/* Obtain all the fontname -> filename mappings.
 * This is called once and the results returned to Java code which can
 * use it for lookups to reduce or avoid the need to search font files.
 */
JNIEXPORT void JNICALL
Java_com_sun_javafx_font_PrismFontFactory_populateFontFileNameMap
(JNIEnv *env, jclass obj, jobject fontToFileMap,
 jobject fontToFamilyMap, jobject familyToFontListMap, jobject locale)
{
#define MAX_BUFFER (FILENAME_MAX+1)
    const wchar_t wname[MAX_BUFFER];
    const char data[MAX_BUFFER];

    DWORD type;
    LONG ret;
    HKEY hkeyFonts;
    DWORD dwNameSize;
    DWORD dwDataValueSize;
    DWORD nval;
    LPCSTR fontKeyName;
    DWORD dwNumValues, dwMaxValueNameLen, dwMaxValueDataLen;
    DWORD numValues = 0;
    jclass classID;
    jmethodID putMID;
    GdiFontMapInfo fmi;
    LOGFONTW lfw;

    /* Check we were passed all the maps we need, and do lookup of
     * methods for JNI up-calls
     */
    if (fontToFileMap == NULL ||
        fontToFamilyMap == NULL ||
        familyToFontListMap == NULL) {
        return;
    }
    classID = (*env)->FindClass(env, "java/util/HashMap");
    if (classID == NULL) {
        return;
    }
    putMID = (*env)->GetMethodID(env, classID, "put",
                 "(Ljava/lang/Object;Ljava/lang/Object;)Ljava/lang/Object;");
    if (putMID == NULL) {
        return;
    }

    fmi.env = env;
    fmi.fontToFamilyMap = fontToFamilyMap;
    fmi.familyToFontListMap = familyToFontListMap;
    fmi.putMID = putMID;
    fmi.locale = locale;
    fmi.containsKeyMID = (*env)->GetMethodID(env, classID, "containsKey",
                                             "(Ljava/lang/Object;)Z");
    if (fmi.containsKeyMID == NULL) {
        return;
    }

    fmi.arrayListClass = (*env)->FindClass(env, "java/util/ArrayList");
    if (fmi.arrayListClass == NULL) {
        return;
    }
    fmi.arrayListCtr = (*env)->GetMethodID(env, fmi.arrayListClass,
                                              "<init>", "(I)V");
    if (fmi.arrayListCtr == NULL) {
        return;
    }
    fmi.addMID = (*env)->GetMethodID(env, fmi.arrayListClass,
                                     "add", "(Ljava/lang/Object;)Z");
    if (fmi.addMID == NULL) {
        return;
    }
    classID = (*env)->FindClass(env, "java/lang/String");
    if (classID == NULL) {
        return;
    }
    fmi.toLowerCaseMID =
        (*env)->GetMethodID(env, classID, "toLowerCase",
                            "(Ljava/util/Locale;)Ljava/lang/String;");
    if (fmi.toLowerCaseMID == NULL) {
        return;
    }

    /* This HDC is initialised and released in this populate family map
     * JNI entry point, and used within the call which would otherwise
     * create many DCs.
     */
    fmi.screenDC = GetDC(NULL);
    if (fmi.screenDC == NULL) {
        return;
    }

    /* Enumerate fonts via GDI to build maps of fonts and families */
    memset(&lfw, 0, sizeof(lfw));
    lfw.lfCharSet = DEFAULT_CHARSET;  /* all charsets */
    wcscpy(lfw.lfFaceName, L"");      /* one face per family (CHECK) */
    EnumFontFamiliesExW(fmi.screenDC, &lfw,
                        (FONTENUMPROCW)EnumFamilyNamesW,
                        (LPARAM)(&fmi), 0L);

    /* Use the windows registry to map font names to files */
    fontKeyName =  FONTKEY_NT;
    ret = RegOpenKeyEx(HKEY_LOCAL_MACHINE,
                       fontKeyName, 0L, KEY_READ, &hkeyFonts);
    if (ret != ERROR_SUCCESS) {
        ReleaseDC(NULL, fmi.screenDC);
        fmi.screenDC = NULL;
        return;
    }

    ret = RegQueryInfoKeyW(hkeyFonts, NULL, NULL, NULL, NULL, NULL, NULL,
                           &dwNumValues, &dwMaxValueNameLen,
                           &dwMaxValueDataLen, NULL, NULL);

    if (ret != ERROR_SUCCESS ||
        dwMaxValueNameLen >= MAX_BUFFER ||
        dwMaxValueDataLen >= MAX_BUFFER) {
        RegCloseKey(hkeyFonts);
        ReleaseDC(NULL, fmi.screenDC);
        fmi.screenDC = NULL;
        return;
    }
    for (nval = 0; nval < dwNumValues; nval++ ) {
        dwNameSize = MAX_BUFFER;
        dwDataValueSize = MAX_BUFFER;
            ret = RegEnumValueW(hkeyFonts, nval, (LPWSTR)wname, &dwNameSize,
                                NULL, &type, (LPBYTE)data, &dwDataValueSize);
        if (ret != ERROR_SUCCESS) {
            break;
        }
        if (type != REG_SZ) { /* REG_SZ means a null-terminated string */
            continue;
        }
            if (!RegistryToBaseTTNameW((LPWSTR)wname) ) {
                /* If the filename ends with ".ttf" or ".otf" also accept it.
                 * REMIND : in fact not accepting .otf's for now as the
                 * upstream code isn't expecting them.
                 * Not expecting to need to do this for .ttc files.
                 * Also note this code is not mirrored in the "A" (win9x) path.
                 */
                LPWSTR dot = wcsrchr((LPWSTR)data, L'.');
                if (dot == NULL || ((wcsicmp(dot, L".ttf") != 0)
                                    /* && (wcsicmp(dot, L".otf") != 0) */)) {
                    continue;  /* not a TT font... */
                }
            }
            registerFontW(&fmi, fontToFileMap, (LPWSTR)wname, (LPWSTR)data);
    }
    RegCloseKey(hkeyFonts);
    ReleaseDC(NULL, fmi.screenDC);
    fmi.screenDC = NULL;
}

JNIEXPORT jstring JNICALL
Java_com_sun_javafx_font_PrismFontFactory_regReadFontLink(JNIEnv *env, jclass obj, jstring lpFontName)
{
    LONG lResult;
    BYTE* buf;
    DWORD dwBufSize = sizeof(buf);
    DWORD dwType = REG_MULTI_SZ;
    HKEY hKey;
    LPCWSTR fontpath = NULL;
    jstring linkStr;

    LPWSTR lpSubKey = L"SOFTWARE\\Microsoft\\Windows NT\\CurrentVersion\\FontLink\\SystemLink";
    lResult = RegOpenKeyExW (HKEY_LOCAL_MACHINE, lpSubKey, 0, KEY_READ, &hKey);
    if (lResult != ERROR_SUCCESS)
    {
        return (jstring)NULL;
    }

    fontpath = (*env)->GetStringChars(env, lpFontName, (jboolean*) NULL);

    //get the buffer size
    lResult = RegQueryValueExW(hKey, fontpath, 0, &dwType, NULL, &dwBufSize);
    if ((lResult == ERROR_SUCCESS) && (dwBufSize > 0)) {
        buf = malloc( dwBufSize );
        if (buf == NULL) {
            (*env)->ReleaseStringChars(env, lpFontName, fontpath);
            RegCloseKey (hKey);
            return (jstring)NULL;
        }
        lResult = RegQueryValueExW(hKey, fontpath, 0, &dwType, (BYTE*)buf,
                                   &dwBufSize);
        (*env)->ReleaseStringChars(env, lpFontName, fontpath);
        RegCloseKey (hKey);

        if (lResult != ERROR_SUCCESS) {
            free(buf);
            return (jstring)NULL;
        }
    } else {
        return (jstring)NULL;
    }

    linkStr = (*env)->NewString(env, (LPWSTR)buf, dwBufSize/sizeof(WCHAR));
    free(buf);
    return linkStr;
}


typedef  unsigned short  LANGID;


#define LANGID_JA_JP   0x411
#define LANGID_ZH_CN   0x0804
#define LANGID_ZH_SG   0x1004
#define LANGID_ZH_TW   0x0404
#define LANGID_ZH_HK   0x0c04
#define LANGID_ZH_MO   0x1404
#define LANGID_KO_KR   0x0412
#define LANGID_US      0x409

static const wchar_t EUDCKEY_JA_JP[] = L"EUDC\\932";
static const wchar_t EUDCKEY_ZH_CN[] = L"EUDC\\936";
static const wchar_t EUDCKEY_ZH_TW[] = L"EUDC\\950";
static const wchar_t EUDCKEY_KO_KR[] = L"EUDC\\949";
static const wchar_t EUDCKEY_DEFAULT[] = L"EUDC\\1252";


JNIEXPORT jstring JNICALL
Java_com_sun_javafx_font_PrismFontFactory_getEUDCFontFile(JNIEnv *env, jclass cl) {
    int    rc;
    HKEY   key;
    DWORD  type;
    WCHAR  fontPathBuf[MAX_PATH + 1];
    DWORD  fontPathLen = MAX_PATH + 1;
    WCHAR  tmpPath[MAX_PATH + 1];
    LPWSTR fontPath = fontPathBuf;
    LPWSTR eudcKey = NULL;

    LANGID langID = GetSystemDefaultLangID();

    //lookup for encoding ID, EUDC only supported in
    //codepage 932, 936, 949, 950 (and unicode)
    if (langID == LANGID_JA_JP) {
        eudcKey = EUDCKEY_JA_JP;
    } else if (langID == LANGID_ZH_CN || langID == LANGID_ZH_SG) {
        eudcKey = EUDCKEY_ZH_CN;
    } else if (langID == LANGID_ZH_HK || langID == LANGID_ZH_TW ||
               langID == LANGID_ZH_MO) {
        eudcKey = EUDCKEY_ZH_TW;
    } else if (langID == LANGID_KO_KR) {
        eudcKey = EUDCKEY_KO_KR;
    } else if (langID == LANGID_US) {
        eudcKey = EUDCKEY_DEFAULT;
    } else {
        return NULL;
    }

    rc = RegOpenKeyExW(HKEY_CURRENT_USER, eudcKey, 0, KEY_READ, &key);
    if (rc != ERROR_SUCCESS) {
        return NULL;
    }
    rc = RegQueryValueExW(key,
                         L"SystemDefaultEUDCFont",
                         0,
                         &type,
                         (LPBYTE)fontPath,
                         &fontPathLen);
    RegCloseKey(key);
    fontPathLen /= sizeof(WCHAR);
    if (rc != ERROR_SUCCESS || type != REG_SZ ||
        (fontPathLen > MAX_PATH)) {
        return NULL;
    }

    fontPath[fontPathLen] = L'\0';
    if (wcsstr(fontPath, L"%SystemRoot%") == fontPath) {
        //if the fontPath includes %SystemRoot%
        LPWSTR systemRoot = _wgetenv(L"SystemRoot");
        // Subtract 12, being the length of "SystemRoot".
        if ((systemRoot == NULL) ||
           (fontPathLen-12 +wcslen(systemRoot) > MAX_PATH)) {
                return NULL;
        }
        wcscpy(tmpPath, systemRoot);
        wcscat(tmpPath, (wchar_t *)(fontPath+12));
        fontPath = tmpPath;
        fontPathLen = wcslen(tmpPath);

    } else if (wcscmp(fontPath, L"EUDC.TTE") == 0) {
        //else to see if it only inludes "EUDC.TTE"
        WCHAR systemRoot[MAX_PATH];
        UINT ret = GetWindowsDirectoryW(systemRoot, MAX_PATH);
        if ( ret != 0) {
            if (ret + 16 > MAX_PATH) {
                return NULL;
            }
            wcscpy(fontPath, systemRoot);
            wcscat(fontPath, L"\\FONTS\\EUDC.TTE");
            fontPathLen = wcslen(fontPath);
        }
        else {
            return NULL;
        }
    }
    return (*env)->NewString(env, (LPWSTR)fontPath, fontPathLen);
}

static BOOL getSysParams(NONCLIENTMETRICSW* ncmetrics) {

    OSVERSIONINFOEX osvi;
    int cbsize;

    ZeroMemory(&osvi, sizeof(OSVERSIONINFOEX));
    osvi.dwOSVersionInfoSize = sizeof(OSVERSIONINFOEX);
    if (!(GetVersionEx((OSVERSIONINFO *)&osvi))) {
        return FALSE;
    }

    // See JDK bug 6944516: specify correct size for ncmetrics on Windows XP
    // Microsoft recommend to subtract the size of the 'iPaddedBorderWidth'
    // field when running on XP. Yuck.
    if (osvi.dwMajorVersion < 6) { // 5 is XP, 6 is Vista.
        cbsize = offsetof(NONCLIENTMETRICSW, iPaddedBorderWidth);
    } else {
        cbsize = sizeof(*ncmetrics);
    }
    ZeroMemory(ncmetrics, cbsize);
    ncmetrics->cbSize = cbsize;

    return SystemParametersInfoW(SPI_GETNONCLIENTMETRICS,
                                 ncmetrics->cbSize, ncmetrics, FALSE);
}


/*
 * Class:     Java_com_sun_javafx_font_PrismFontFactory
 * Method:    getLCDContrastWin32
 * Signature: ()I
 */
JNIEXPORT jint JNICALL Java_com_sun_javafx_font_PrismFontFactory_getLCDContrastWin32
  (JNIEnv *env, jobject klass) {

    unsigned int fontSmoothingContrast;
    static const int fontSmoothingContrastDefault = 1300;

    return SystemParametersInfo(SPI_GETFONTSMOOTHINGCONTRAST, 0,
        &fontSmoothingContrast, 0) ? fontSmoothingContrast : fontSmoothingContrastDefault;
}

JNIEXPORT jint JNICALL
Java_com_sun_javafx_font_PrismFontFactory_getSystemFontSizeNative(JNIEnv *env, jclass cl)
{
    NONCLIENTMETRICSW ncmetrics;

    if (getSysParams(&ncmetrics)) {
        return -ncmetrics.lfMessageFont.lfHeight;
    } else {
        return 12;
    }
}

JNIEXPORT jstring JNICALL
Java_com_sun_javafx_font_PrismFontFactory_getSystemFontNative(JNIEnv *env, jclass cl) {

    NONCLIENTMETRICSW ncmetrics;

    if (getSysParams(&ncmetrics)) {
        int len = wcslen(ncmetrics.lfMessageFont.lfFaceName);
        return (*env)->NewString(env, ncmetrics.lfMessageFont.lfFaceName, len);
    } else {
        return NULL;
    }
}


JNIEXPORT jshort JNICALL
Java_com_sun_javafx_font_PrismFontFactory_getSystemLCID(JNIEnv *env, jclass cl)
{
    LCID lcid = GetSystemDefaultLCID();
    DWORD value;

    int ret = GetLocaleInfoW(lcid,
                             LOCALE_ILANGUAGE | LOCALE_RETURN_NUMBER,
                             (LPTSTR)&value,
                             sizeof(value) / sizeof(TCHAR));
    return (jshort)value;
}

#endif /* WIN32 */
