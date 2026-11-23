package com.capobuf.dubsiren

import android.media.AudioManager
import android.os.Bundle
import android.view.WindowManager
import io.flutter.embedding.android.FlutterActivity

// Window settings only: the siren lives in native/, the panel in lib/.
class MainActivity : FlutterActivity() {
    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        window.addFlags(WindowManager.LayoutParams.FLAG_KEEP_SCREEN_ON)
        // Volume keys drive the media stream the siren plays on.
        volumeControlStream = AudioManager.STREAM_MUSIC
    }
}
