package com.capobuf.dubsiren

/** JNI surface of libdubsiren (src/main/cpp/siren_bridge.cpp over app/native/siren_core.h). */
object NativeSiren {
    init {
        System.loadLibrary("dubsiren")
    }

    /** Pot channels as wired to the 74HC4051: 0..5 = POT1..POT6, 6 = rear Master. */
    const val MASTER_CHANNEL = 6

    external fun nativeInit(settingsPath: String): Boolean
    external fun nativeStartAudio(): Boolean
    external fun nativeStopAudio()

    /** Wiper travel 0..1; quantized natively like the ADC path in PanelInputs. */
    external fun nativeSetPot(channel: Int, position: Float)
    external fun nativeSetButton(button: Int, down: Boolean)
    external fun nativeSetEncoderPressed(down: Boolean)
    external fun nativeRotateEncoder(direction: Int)

    /** Firmware loop pass; returns the framebuffer counter, copying pixels if it changed. */
    external fun nativeTick(pixels: IntArray, since: Int): Int
    external fun nativeDisplayWidth(): Int
    external fun nativeDisplayHeight(): Int
    external fun nativeAudioStatus(): String
}
