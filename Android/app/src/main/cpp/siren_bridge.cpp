// JNI surface of the Kotlin app over the shared host core (app/native).

#include <jni.h>

#include <string>

#include "siren_core.h"

namespace {

std::string jstringToString(JNIEnv *env, jstring value) {
    if (value == nullptr) return {};
    const char *chars = env->GetStringUTFChars(value, nullptr);
    std::string result(chars ? chars : "");
    if (chars) env->ReleaseStringUTFChars(value, chars);
    return result;
}

}  // namespace

#define JNI_FN(name) Java_com_capobuf_dubsiren_NativeSiren_##name

extern "C" {

JNIEXPORT jboolean JNICALL JNI_FN(nativeInit)(JNIEnv *env, jobject, jstring path) {
    return siren_init(jstringToString(env, path).c_str()) ? JNI_TRUE : JNI_FALSE;
}

JNIEXPORT jboolean JNICALL JNI_FN(nativeStartAudio)(JNIEnv *, jobject) {
    return siren_start_audio() ? JNI_TRUE : JNI_FALSE;
}

JNIEXPORT void JNICALL JNI_FN(nativeStopAudio)(JNIEnv *, jobject) {
    siren_stop_audio();
}

JNIEXPORT void JNICALL JNI_FN(nativeSetPot)(JNIEnv *, jobject, jint channel, jfloat position) {
    siren_set_pot(channel, position);
}

JNIEXPORT void JNICALL JNI_FN(nativeSetButton)(JNIEnv *, jobject, jint button, jboolean down) {
    siren_set_button(button, down == JNI_TRUE);
}

JNIEXPORT void JNICALL JNI_FN(nativeSetEncoderPressed)(JNIEnv *, jobject, jboolean down) {
    siren_set_encoder_pressed(down == JNI_TRUE);
}

JNIEXPORT void JNICALL JNI_FN(nativeRotateEncoder)(JNIEnv *, jobject, jint direction) {
    siren_rotate_encoder(direction);
}

// One loop() pass. Copies the framebuffer into `pixels` (ARGB) and returns
// the new frame counter when it changed since `since`.
JNIEXPORT jint JNICALL JNI_FN(nativeTick)(JNIEnv *env, jobject, jintArray pixels, jint since) {
    const jint frame = siren_tick();
    if (frame == since) return since;
    if (env->GetArrayLength(pixels) < siren_display_width() * siren_display_height()) return since;
    jint *out = env->GetIntArrayElements(pixels, nullptr);
    siren_copy_display(reinterpret_cast<uint32_t *>(out));
    env->ReleaseIntArrayElements(pixels, out, 0);
    return frame;
}

JNIEXPORT jint JNICALL JNI_FN(nativeDisplayWidth)(JNIEnv *, jobject) {
    return siren_display_width();
}

JNIEXPORT jint JNICALL JNI_FN(nativeDisplayHeight)(JNIEnv *, jobject) {
    return siren_display_height();
}

JNIEXPORT jstring JNICALL JNI_FN(nativeAudioStatus)(JNIEnv *env, jobject) {
    char text[160];
    siren_audio_status(text, sizeof(text));
    return env->NewStringUTF(text);
}

}  // extern "C"
