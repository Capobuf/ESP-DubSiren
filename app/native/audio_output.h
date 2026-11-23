#pragma once

// Platform audio output used by siren_core.cpp: audio_aaudio.cpp on Android,
// audio_miniaudio.cpp on Windows.
// The output callback pulls samples with siren_render_mono().

#include <stdint.h>

struct AudioOutputInfo {
    bool open = false;
    int32_t sampleRate = 0;
    int32_t bufferFrames = 0;
    int32_t xruns = 0;
};

// sampleRate: the DSP rate (Config::kSampleRate). Kept out of Config.h here:
// its Arduino stub clashes with windows.h (INPUT) under miniaudio.
bool audioOutputStart(uint32_t sampleRate);
void audioOutputStop();
// UI thread, once per tick: underrun handling.
void audioOutputTune();
AudioOutputInfo audioOutputInfo();
