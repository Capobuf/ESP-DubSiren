#pragma once

// C surface of the shared DubSiren host core, used by the Android JNI bridge
// and by the Flutter FFI bindings. Threads mirror the firmware split:
//   - UI thread ~ firmware loop(): every function below except the audio
//     callback, which only calls siren_render_mono().
//   - Audio callback ~ audioTask(): siren_render_mono().
// Only ControlStore is shared between them, guarded by its own mutex.

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#if defined(_WIN32)
#define SIREN_API __declspec(dllexport)
#else
#define SIREN_API __attribute__((visibility("default")))
#endif

#ifdef __cplusplus
extern "C" {
#endif

// Creates the siren once per process; later calls return true and keep it.
SIREN_API bool siren_init(const char *settings_path);
SIREN_API bool siren_start_audio(void);
// Stops audio and saves pending panel settings at once: no more ticks follow.
SIREN_API void siren_stop_audio(void);

// channel 0..5 = POT1..6, 6 = rear Master; position = wiper travel 0..1.
SIREN_API void siren_set_pot(int32_t channel, float position);
SIREN_API void siren_set_button(int32_t button, bool down);
SIREN_API void siren_set_encoder_pressed(bool down);
SIREN_API void siren_rotate_encoder(int32_t direction);

// One loop() pass: latency tuning, settings persistence, display update.
// Returns the frame counter; it changes whenever the firmware sends a frame.
SIREN_API int32_t siren_tick(void);
// Copies the last frame as 0xAARRGGBB words (BGRA bytes on little endian),
// siren_display_width() x siren_display_height() of them.
SIREN_API void siren_copy_display(uint32_t *argb);
SIREN_API int32_t siren_display_width(void);
SIREN_API int32_t siren_display_height(void);

// Audio thread: fills `frames` mono 48 kHz samples, rendering 480-sample blocks.
SIREN_API void siren_render_mono(int16_t *out, int32_t frames);

// One diagnostic line: firmware version, stream state, buffer, xruns, render time.
SIREN_API void siren_audio_status(char *text, size_t size);

#ifdef __cplusplus
}
#endif
