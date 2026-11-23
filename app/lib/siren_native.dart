// dart:ffi surface of the shared siren core (native/siren_core.h).
import 'dart:ffi';
import 'dart:io';

import 'package:ffi/ffi.dart';

final DynamicLibrary _lib =
    DynamicLibrary.open(Platform.isWindows ? 'siren_core.dll' : 'libsiren_core.so');

/// Pot channels as wired to the 74HC4051: 0..5 = POT1..POT6, 6 = rear Master.
const int masterChannel = 6;

final bool Function(Pointer<Utf8>) _init =
    _lib.lookupFunction<Bool Function(Pointer<Utf8>), bool Function(Pointer<Utf8>)>('siren_init');
final bool Function() startAudio =
    _lib.lookupFunction<Bool Function(), bool Function()>('siren_start_audio');
final void Function() stopAudio =
    _lib.lookupFunction<Void Function(), void Function()>('siren_stop_audio');

/// Wiper travel 0..1; quantized natively like the ADC path in PanelInputs.
final void Function(int, double) setPot =
    _lib.lookupFunction<Void Function(Int32, Float), void Function(int, double)>('siren_set_pot');
final void Function(int, bool) setButton =
    _lib.lookupFunction<Void Function(Int32, Bool), void Function(int, bool)>('siren_set_button');
final void Function(bool) setEncoderPressed =
    _lib.lookupFunction<Void Function(Bool), void Function(bool)>('siren_set_encoder_pressed');
final void Function(int) rotateEncoder =
    _lib.lookupFunction<Void Function(Int32), void Function(int)>('siren_rotate_encoder');

/// Firmware loop pass; returns the framebuffer counter.
final int Function() tick = _lib.lookupFunction<Int32 Function(), int Function()>('siren_tick');
final void Function(Pointer<Uint32>) copyDisplay =
    _lib.lookupFunction<Void Function(Pointer<Uint32>), void Function(Pointer<Uint32>)>(
        'siren_copy_display');
final int displayWidth =
    _lib.lookupFunction<Int32 Function(), int Function()>('siren_display_width')();
final int displayHeight =
    _lib.lookupFunction<Int32 Function(), int Function()>('siren_display_height')();

final void Function(Pointer<Utf8>, int) _audioStatus =
    _lib.lookupFunction<Void Function(Pointer<Utf8>, Size), void Function(Pointer<Utf8>, int)>(
        'siren_audio_status');

bool init(String settingsPath) {
  final path = settingsPath.toNativeUtf8();
  try {
    return _init(path);
  } finally {
    malloc.free(path);
  }
}

String audioStatus() {
  final text = malloc<Uint8>(160).cast<Utf8>();
  try {
    _audioStatus(text, 160);
    return text.toDartString();
  } finally {
    malloc.free(text);
  }
}
