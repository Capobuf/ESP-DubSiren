package com.capobuf.dubsiren

import android.app.Activity
import android.media.AudioManager
import android.os.Build
import android.os.Bundle
import android.os.SystemClock
import android.util.Log
import android.view.Choreographer
import android.view.View
import android.view.WindowInsets
import android.view.WindowInsetsController
import android.view.WindowManager
import java.io.File

class MainActivity : Activity(), Choreographer.FrameCallback {
    private lateinit var panel: PanelView
    private lateinit var pixels: IntArray
    private var frame = Int.MIN_VALUE
    private var lastStatusMs = 0L
    private var running = false

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        window.addFlags(WindowManager.LayoutParams.FLAG_KEEP_SCREEN_ON)
        // Volume keys drive the media stream the siren plays on.
        volumeControlStream = AudioManager.STREAM_MUSIC
        NativeSiren.nativeInit(File(filesDir, "panel_settings.bin").absolutePath)
        pixels = IntArray(NativeSiren.nativeDisplayWidth() * NativeSiren.nativeDisplayHeight())
        panel = PanelView(this, getSharedPreferences("panel", MODE_PRIVATE))
        setContentView(panel)
        // Like the first ADC scan after boot: every pot reports where it sits.
        panel.sendAllPositions()
        // No rear Master on screen: hold it at full travel, level = tablet volume.
        NativeSiren.nativeSetPot(NativeSiren.MASTER_CHANNEL, 1f)
    }

    override fun onStart() {
        super.onStart()
        NativeSiren.nativeStartAudio()
        running = true
        Choreographer.getInstance().postFrameCallback(this)
    }

    override fun onStop() {
        running = false
        Choreographer.getInstance().removeFrameCallback(this)
        panel.releaseAll()
        panel.savePositions()
        NativeSiren.nativeStopAudio()
        super.onStop()
    }

    override fun onWindowFocusChanged(hasFocus: Boolean) {
        super.onWindowFocusChanged(hasFocus)
        if (hasFocus) hideSystemBars() else panel.releaseAll()
    }

    override fun doFrame(frameTimeNanos: Long) {
        if (!running) return
        val next = NativeSiren.nativeTick(pixels, frame)
        if (next != frame) {
            frame = next
            panel.updateDisplay(pixels)
        }
        // Audio diagnostics go to logcat only: the panel shows no app text.
        val now = SystemClock.uptimeMillis()
        if (now - lastStatusMs >= 5000) {
            lastStatusMs = now
            Log.i("DubSiren", NativeSiren.nativeAudioStatus())
        }
        Choreographer.getInstance().postFrameCallback(this)
    }

    @Suppress("DEPRECATION")
    private fun hideSystemBars() {
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.R) {
            window.insetsController?.let {
                it.hide(WindowInsets.Type.systemBars())
                it.systemBarsBehavior = WindowInsetsController.BEHAVIOR_SHOW_TRANSIENT_BARS_BY_SWIPE
            }
        } else {
            window.decorView.systemUiVisibility = View.SYSTEM_UI_FLAG_IMMERSIVE_STICKY or
                View.SYSTEM_UI_FLAG_FULLSCREEN or View.SYSTEM_UI_FLAG_HIDE_NAVIGATION or
                View.SYSTEM_UI_FLAG_LAYOUT_STABLE or View.SYSTEM_UI_FLAG_LAYOUT_HIDE_NAVIGATION or
                View.SYSTEM_UI_FLAG_LAYOUT_FULLSCREEN
        }
    }
}
