// DubSiren simulator: the shared firmware core (native/) behind the panel.
// A periodic timer plays the firmware loop(); the native audio callback plays
// audioTask(). Dart never enters the audio path.
import 'dart:async';
import 'dart:ffi';
import 'dart:io';
import 'dart:ui' as ui;

import 'package:ffi/ffi.dart';
import 'package:flutter/services.dart';
import 'package:flutter/widgets.dart';
import 'package:path_provider/path_provider.dart';
import 'package:shared_preferences/shared_preferences.dart';

import 'panel.dart';
import 'pcb_art.dart';
import 'siren_native.dart' as siren;

Future<void> main() async {
  WidgetsFlutterBinding.ensureInitialized();
  // Full screen like the panel; system bars come back with a swipe.
  if (Platform.isAndroid) await SystemChrome.setEnabledSystemUIMode(SystemUiMode.immersiveSticky);
  // Android: filesDir, the same file the Kotlin app saved.
  final directory = await getApplicationSupportDirectory();
  // shortcut: fopen() takes the path in the ANSI code page on Windows; fine
  // for an ASCII profile path, switch to wide paths if a user's is not.
  siren.init('${directory.path}/panel_settings.bin');
  final controller = PanelController(await SharedPreferences.getInstance(), await PcbArt.load());
  // Like the first ADC scan after boot: every pot reports where it sits.
  controller.sendAllPositions();
  // No rear Master on screen: hold it at full travel, level = system volume.
  siren.setPot(siren.masterChannel, 1);
  runApp(SirenApp(controller: controller));
}

class SirenApp extends StatefulWidget {
  const SirenApp({super.key, required this.controller});
  final PanelController controller;

  @override
  State<SirenApp> createState() => _SirenAppState();
}

class _SirenAppState extends State<SirenApp> {
  // The firmware loop() runs on a timer, not on vsync: a Ticker would make
  // Flutter render a frame every vsync even when nothing changed.
  Timer? _loop;
  final Stopwatch _clock = Stopwatch();
  late final AppLifecycleListener _lifecycle;
  final Pointer<Uint32> _pixels = malloc<Uint32>(siren.displayWidth * siren.displayHeight);
  int _frame = -1;
  bool _decoding = false;
  bool _running = false;
  Duration _lastStatus = Duration.zero;
  // About one vsync at 120 Hz, the cadence of the Kotlin app's Choreographer loop.
  static const Duration _loopPeriod = Duration(milliseconds: 8);

  @override
  void initState() {
    super.initState();
    _lifecycle = AppLifecycleListener(
        onShow: _start,
        onHide: _stop,
        // Focus lost (system dialog, other window): no key stays held.
        onInactive: widget.controller.releaseAll,
        onExitRequested: () async {
      _stop();
      return ui.AppExitResponse.exit;
    });
    _start();
  }

  void _start() {
    if (_running) return;
    _running = true;
    _lastStatus = Duration.zero;
    _clock
      ..reset()
      ..start();
    siren.startAudio();
    _loop = Timer.periodic(_loopPeriod, (_) => _onTick(_clock.elapsed));
  }

  void _stop() {
    if (!_running) return;
    _running = false;
    _loop?.cancel();
    _clock.stop();
    widget.controller.releaseAll();
    widget.controller.savePositions();
    siren.stopAudio();
  }

  void _onTick(Duration elapsed) {
    final frame = siren.tick();
    // One decode at a time; a frame sent meanwhile is picked up next tick.
    if (frame != _frame && !_decoding) {
      _frame = frame;
      _decoding = true;
      siren.copyDisplay(_pixels);
      // 0xAARRGGBB words are BGRA bytes on little-endian hosts.
      final bytes = Uint8List.fromList(_pixels.cast<Uint8>().asTypedList(siren.displayWidth * siren.displayHeight * 4));
      ui.decodeImageFromPixels(bytes, siren.displayWidth, siren.displayHeight, ui.PixelFormat.bgra8888, (image) {
        _decoding = false;
        if (mounted) {
          widget.controller.updateDisplay(image);
        } else {
          image.dispose();
        }
      });
    }
    // Audio diagnostics go to the console only: the panel shows no app text.
    if (elapsed - _lastStatus >= const Duration(seconds: 5)) {
      _lastStatus = elapsed;
      debugPrint('DubSiren ${siren.audioStatus()}');
    }
  }

  @override
  void dispose() {
    _stop();
    _lifecycle.dispose();
    malloc.free(_pixels);
    super.dispose();
  }

  @override
  Widget build(BuildContext context) {
    return Directionality(
      textDirection: TextDirection.ltr,
      child: PanelView(controller: widget.controller),
    );
  }
}
