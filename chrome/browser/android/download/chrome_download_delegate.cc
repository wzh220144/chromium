// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/android/download/chrome_download_delegate.h"

#include <jni.h>

#include <string>
#include <type_traits>

#include "base/android/jni_android.h"
#include "base/android/jni_string.h"
#include "base/android/scoped_java_ref.h"
#include "base/bind.h"
#include "base/callback.h"
#include "base/files/file_path.h"
#include "chrome/browser/android/download/download_controller_base.h"
#include "chrome/browser/android/tab_android.h"
#include "chrome/browser/infobars/infobar_service.h"
#include "chrome/common/safe_browsing/file_type_policies.h"
#include "chrome/grit/generated_resources.h"
#include "jni/ChromeDownloadDelegate_jni.h"
#include "ui/base/l10n/l10n_util.h"

using base::android::ConvertUTF8ToJavaString;
using base::android::JavaParamRef;
using base::android::ScopedJavaLocalRef;
using content::WebContents;

// Gets the download warning text for the given file name.
static ScopedJavaLocalRef<jstring> GetDownloadWarningText(
    JNIEnv* env,
    const JavaParamRef<jclass>& clazz,
    const JavaParamRef<jstring>& filename) {
  return base::android::ConvertUTF8ToJavaString(
      env, l10n_util::GetStringFUTF8(
               IDS_PROMPT_DANGEROUS_DOWNLOAD,
               base::android::ConvertJavaStringToUTF16(env, filename)));
}

// static
bool ChromeDownloadDelegate::EnqueueDownloadManagerRequest(
    jobject chrome_download_delegate,
    bool overwrite,
    jobject download_info) {
  JNIEnv* env = base::android::AttachCurrentThread();

  return Java_ChromeDownloadDelegate_enqueueDownloadManagerRequestFromNative(
      env, chrome_download_delegate, overwrite, download_info);
}

ChromeDownloadDelegate::ChromeDownloadDelegate(
    WebContents* web_contents) {}

ChromeDownloadDelegate::~ChromeDownloadDelegate() {
   JNIEnv* env = base::android::AttachCurrentThread();
   env->DeleteGlobalRef(java_ref_);
}

void ChromeDownloadDelegate::SetJavaRef(JNIEnv* env, jobject jobj) {
  java_ref_ = env->NewGlobalRef(jobj);
}

void Init(JNIEnv* env,
          const JavaParamRef<jobject>& obj,
          const JavaParamRef<jobject>& jweb_contents) {
  auto* web_contents = WebContents::FromJavaWebContents(jweb_contents);
  ChromeDownloadDelegate::CreateForWebContents(web_contents);
  ChromeDownloadDelegate::FromWebContents(web_contents)->SetJavaRef(env, obj);
}

bool RegisterChromeDownloadDelegate(JNIEnv* env) {
  return RegisterNativesImpl(env);
}

DEFINE_WEB_CONTENTS_USER_DATA_KEY(ChromeDownloadDelegate);
