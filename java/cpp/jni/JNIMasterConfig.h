//
//  _   _         ______    _ _ _   _             _ _ _
// | \ | |       |  ____|  | (_) | (_)           | | | |
// |  \| | ___   | |__   __| |_| |_ _ _ __   __ _| | | |
// | . ` |/ _ \  |  __| / _` | | __| | '_ \ / _` | | | |
// | |\  | (_) | | |___| (_| | | |_| | | | | (_| |_|_|_|
// |_| \_|\___/  |______\__,_|_|\__|_|_| |_|\__, (_|_|_)
//                                           __/ |
//                                          |___/
// 
// This file is auto-generated. Do not edit manually
// 
// Copyright 2016 Automatak LLC
// 
// Automatak LLC (www.automatak.com) licenses this file
// to you under the the Apache License Version 2.0 (the "License"):
// 
// http://www.apache.org/licenses/LICENSE-2.0.html
//

#ifndef OPENDNP3JAVA_JNIMASTERCONFIG_H
#define OPENDNP3JAVA_JNIMASTERCONFIG_H

#include <jni.h>

namespace jni
{
    struct JCache;

    namespace cache
    {
        class MasterConfig
        {
            friend struct JCache;

            bool init(JNIEnv* env);
            void cleanup(JNIEnv* env);

            public:

            // field getter methods
            jobject getresponseTimeout(JNIEnv* env, jobject instance);
            jobject gettimeSyncMode(JNIEnv* env, jobject instance);
            jboolean getdisableUnsolOnStartup(JNIEnv* env, jobject instance);
            jboolean getignoreRestartIIN(JNIEnv* env, jobject instance);
            jobject getunsolClassMask(JNIEnv* env, jobject instance);
            jobject getstartupIntegrityClassMask(JNIEnv* env, jobject instance);
            jboolean getintegrityOnEventOverflowIIN(JNIEnv* env, jobject instance);
            jobject geteventScanOnEventsAvailableClassMask(JNIEnv* env, jobject instance);
            jobject gettaskRetryPeriod(JNIEnv* env, jobject instance);
            jobject gettaskStartTimeout(JNIEnv* env, jobject instance);
            jint getmaxTxFragSize(JNIEnv* env, jobject instance);
            jint getmaxRxFragSize(JNIEnv* env, jobject instance);

            private:

            jclass clazz = nullptr;

            // field ids
            jfieldID responseTimeoutField = nullptr;
            jfieldID timeSyncModeField = nullptr;
            jfieldID disableUnsolOnStartupField = nullptr;
            jfieldID ignoreRestartIINField = nullptr;
            jfieldID unsolClassMaskField = nullptr;
            jfieldID startupIntegrityClassMaskField = nullptr;
            jfieldID integrityOnEventOverflowIINField = nullptr;
            jfieldID eventScanOnEventsAvailableClassMaskField = nullptr;
            jfieldID taskRetryPeriodField = nullptr;
            jfieldID taskStartTimeoutField = nullptr;
            jfieldID maxTxFragSizeField = nullptr;
            jfieldID maxRxFragSizeField = nullptr;
        };
    }
}

#endif
