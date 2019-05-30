#include "JNICommon.h"
#include "RTSPClientJNI.h"
#include <string.h>

#include "RTSPClient.h"
#include "RTSPCommonEnv.h"

static RTSPClient*	g_rtspClient[MAX_CHANNELS] = {NULL};
static JavaVM*		g_vm;
static jobject		g_obj[MAX_CHANNELS];
static jmethodID	g_method_frameHandler[MAX_CHANNELS];

static void frameHandlerFunc(void *arg, RTP_FRAME_TYPE frame_type, int64_t timestamp, uint8_t *buf, int len)
{
	int idx = (long)arg;
	//DPRINTF("[%d] %d, %d, %d\n", idx, frame_type, timestamp, len);

	JNIEnv *env;

	int getEnvStat = g_vm->GetEnv((void **)&env, JNI_VERSION_1_6);
	if (getEnvStat == JNI_EDETACHED) {
		//DPRINTF("GetEnv: not attached\n");
		if (g_vm->AttachCurrentThread(&env, NULL) != 0) {
			DPRINTF("Failed to attach\n");
		}
	} else if (getEnvStat == JNI_OK) {
		//
	} else if (getEnvStat == JNI_EVERSION) {
		DPRINTF("GetEnv: version not supported\n");
	}

	jbyteArray jarr;
	jarr = env->NewByteArray(len);

	jboolean isCopy;
	jbyte* jbytes = env->GetByteArrayElements(jarr, &isCopy);

	memcpy(jbytes, buf, len);
	env->SetByteArrayRegion(jarr, 0, len, jbytes);

	env->CallVoidMethod(g_obj[idx], g_method_frameHandler[idx], frame_type, timestamp, jarr);

	env->ReleaseByteArrayElements(jarr, jbytes, JNI_ABORT);

	if (env->ExceptionCheck()) {
		env->ExceptionDescribe();
	}

	g_vm->DetachCurrentThread();
}

static bool JNI_Hanlder_Register(JNIEnv *env, jobject obj, jint idx)
{
	bool returnValue = true;
	// convert local to global reference 
	// (local will die after this method call)
	g_obj[idx] = env->NewGlobalRef(obj);

	// save refs for callback
	jclass cls = env->GetObjectClass(g_obj[idx]);
	if (cls == NULL) {
		DPRINTF("Failed to find class\n");
	}

	g_method_frameHandler[idx] = env->GetMethodID(cls, "frameHandler", "(II[B)V");
	if (g_method_frameHandler[idx] == NULL) {
		DPRINTF("Unable to get method ref\n");
	}

	env->GetJavaVM(&g_vm);

	return returnValue;
}

JNIEXPORT jint JNICALL Java_com_example_rtspclienttestapp_RTSPClient_openURL(JNIEnv *env, jobject obj, jint idx, jstring strURL, jint streamType)
{
	const char *url = env->GetStringUTFChars(strURL, 0);
	DPRINTF("openURL %d %s %d\n", idx, url, streamType);

	jint ret = 0;

	if (g_rtspClient[idx]) {
		DPRINTF("failed to openURL, rtspclient[%d] already exist\n", idx);
		ret = -1;
		goto exit;
	}

	if (!JNI_Hanlder_Register(env, obj, idx)) {
		DPRINTF("failed to openURL, cannot register jni handler functions\n");
		ret = -1;
		goto exit;
	}

	g_rtspClient[idx] = new RTSPClient();

	if (g_rtspClient[idx]->openURL(url, streamType, 3) < 0) {
		DPRINTF("failed to openURL, cannot connect to rtsp server\n");
		ret = -1;
		goto exit;
	}

exit:
	env->ReleaseStringUTFChars(strURL, url);
	return ret;
}

JNIEXPORT jint JNICALL Java_com_example_rtspclienttestapp_RTSPClient_playURL(JNIEnv *env, jobject obj, jint idx)
{
	if (!g_rtspClient[idx]) {
		DPRINTF("failed to playURL, rtspclient[%d] not opened\n", idx);
		return -1;
	}

	return g_rtspClient[idx]->playURL(frameHandlerFunc, (void *)idx, NULL, NULL);
}


JNIEXPORT void JNICALL Java_com_example_rtspclienttestapp_RTSPClient_closeURL(JNIEnv *env, jobject obj, jint idx)
{
	if (g_rtspClient[idx]) {
		g_rtspClient[idx]->closeURL();
		delete g_rtspClient[idx];
		g_rtspClient[idx] = NULL;

		env->DeleteGlobalRef(g_obj[idx]);
	}
	DPRINTF("closeURL\n");
}
