// Front panel render and input. Port of PanelView.kt: positions come from
// hardware/kicad/ESP-DubSiren.kicad_pcb (8 Nov 2026); the board itself is
// drawn by PcbArt. Only physical gestures are offered: six pots, four
// momentary keys, encoder turn (detented) and push. The rear Master is held
// at full scale; the level is the system volume.
//
// Without a touch screen (one mouse pointer) the same controls also answer to
// the keyboard and the wheel: keys 1-4 held = MX keys, space = encoder push,
// wheel over a pot or the encoder = rotation.
//
// Depth: light from the top left, camera slightly in front of the panel, so
// tall parts show their top shifted up and their side towards the viewer.
import 'dart:async';
import 'dart:io';
import 'dart:math' as math;
import 'dart:ui' as ui;

import 'package:flutter/gestures.dart';
import 'package:flutter/services.dart';
import 'package:flutter/widgets.dart';
import 'package:shared_preferences/shared_preferences.dart';

import 'pcb_art.dart';
import 'siren_native.dart' as siren;

const double _boardW = 86;
const double _boardTop = -6;
const double _boardBottom = 120;

const List<double> _potX = [15, 43, 71, 15, 43, 71]; // RV1..RV6
const List<double> _potY = [18, 18, 18, 76, 76, 76];
const double _potSkirtR = 19.84 / 2; // Davies 1100 skirt 25/32"
const double _potBodyR = 16.67 / 2; // body 21/32"
const double _potTravel = 300; // Bourns PTV111 mechanical angle

const double _encX = 75; // SW7 PEC11R-4120F-S0018
const double _encY = 46.5;
const double _encR = 7; // SAK-023: visual radius, not dimensioned in the repo
const double _encPushR = 3.6;
const double _detentDeg = 20; // 18 detents per turn (PEC11R datasheet)

// U5 Adafruit 5393 (official Eagle geometry in the footprint F.Fab).
const double _modX = 44.016;
const double _modY = 46.5;
const double _modW = 36.83;
const double _modH = 30.48;
const double _modR = 2.54;
const double _glassL = _modX - 18.216;
const double _glassR = _modX + 18.084;
const double _glassHalfH = 9.7;
const double _dispL = _modX - 17.191; // active area, corner radius 3
const double _dispR = _modX + 15.159;
const double _dispHalfH = 8.7;
const double _dispCorner = 3;
// Enlarged view for small screens (double tap the display to switch).
const double _zoomX = 33.5;
const double _zoomY = 43;
const double _zoomW = 52;

// SW1..SW4 MX2A: switch centre = 4 mm centre-post hole.
const List<double> _keyX = [10, 32, 54, 76];
const double _keyY = 100;
const double _keycap = 18; // provisional 1u keycap, no MPN yet

// Apparent lift of a part's top for its height (camera slightly in front).
const double _liftPerMm = 0.06;
const double _knobHeight = 12;
const double _keyHeight = 18;
const double _keyTravel = 4; // MX2A total travel
const double _moduleHeight = 8;

const double _dragMmFullTravel = 55;
// A second finger this far outside a held knob's skirt still grips it.
const double _twistReach = 8;
const double _defaultPot = 0.5;
const int _zoomUnset = -1;
// One wheel notch on a pot: 10 degrees of the 300-degree travel.
const double _wheelPotStep = 10 / _potTravel;

const List<LogicalKeyboardKey> _keyboardKeys = [
  LogicalKeyboardKey.digit1,
  LogicalKeyboardKey.digit2,
  LogicalKeyboardKey.digit3,
  LogicalKeyboardKey.digit4,
];

enum _Kind { pot, key, encTurn, encPush, display }

class _Grab {
  _Grab(this.kind, this.index, this.lastX, this.lastY)
      : x = lastX,
        y = lastY;
  final _Kind kind;
  final int index;
  double lastX, lastY;
  double lastAngle = 0;
  double accumulated = 0;
  // Latest position; pots are updated once all fingers have moved.
  double x, y;
}

double _wrap180(double diff) {
  if (diff > 180) diff -= 360;
  if (diff < -180) diff += 360;
  return diff;
}

double _deg(double radians) => radians * 180 / math.pi;
double _rad(double degrees) => degrees * math.pi / 180;

/// Panel state: control positions, held keys, pointer grabs, layout.
class PanelController extends ChangeNotifier {
  PanelController(this._prefs, this._art)
      : positions = List.generate(6, (i) => _prefs.getDouble('pot$i') ?? _defaultPot),
        _zoomSetting = _prefs.getInt('zoomDisplayMode') ?? _zoomUnset;

  final SharedPreferences _prefs;
  final PcbArt _art;
  final List<double> positions;
  final List<int> _keyPointers = List.filled(4, 0); // fingers or keyboard keys holding each MX key
  int _encoderPointers = 0;
  int _encoderDetents = 0;
  final Map<int, _Grab> _grabs = {};
  // Per pot: last angle of the line between the two gripping fingers, NaN if none.
  final List<double> _twistAngle = List.filled(6, double.nan);
  int _zoomSetting; // -1 auto, 0 real, 1 zoom
  bool _zoomDisplay = false;
  DateTime _lastDisplayTap = DateTime.fromMillisecondsSinceEpoch(0);
  final Set<LogicalKeyboardKey> _heldKeys = {};

  ui.Image? display;
  ui.Picture? _background;
  Size _size = Size.zero;
  double _scale = 1;
  double _originX = 0;
  double _originY = 0;

  // ---- Native link ----

  void sendAllPositions() {
    for (var channel = 0; channel < 6; ++channel) {
      siren.setPot(channel, positions[channel]);
    }
  }

  void savePositions() {
    for (var channel = 0; channel < 6; ++channel) {
      unawaited(_prefs.setDouble('pot$channel', positions[channel]));
    }
    unawaited(_prefs.setInt('zoomDisplayMode', _zoomSetting));
  }

  void releaseAll() {
    for (var key = 0; key < 4; ++key) {
      if (_keyPointers[key] > 0) {
        _keyPointers[key] = 0;
        siren.setButton(key + 1, false);
      }
    }
    if (_encoderPointers > 0) {
      _encoderPointers = 0;
      siren.setEncoderPressed(false);
    }
    _grabs.clear();
    _heldKeys.clear();
    notifyListeners();
  }

  void updateDisplay(ui.Image image) {
    display?.dispose();
    display = image;
    notifyListeners();
  }

  // ---- Layout ----

  void layout(Size size, double logicalPerMm) {
    if (size == _size || size.isEmpty) return;
    _size = size;
    const modelW = _boardW + 8;
    const modelH = _boardBottom - _boardTop + 10;
    _scale = math.min(size.width / modelW, size.height / modelH);
    _originX = (size.width - _boardW * _scale) / 2;
    _originY = (size.height - modelH * _scale) / 2 + (-_boardTop + 4) * _scale;
    // Real-size display unless the panel is drawn smaller than ~1.2x life size.
    _zoomDisplay = _zoomSetting == _zoomUnset ? _scale / logicalPerMm < 1.2 : _zoomSetting == 1;
    _buildBackground();
  }

  void _buildBackground() {
    final recorder = ui.PictureRecorder();
    final canvas = Canvas(recorder);
    final w = _size.width;
    final h = _size.height;
    // Studio desk: dark, lit from the top left.
    canvas.drawRect(
        Offset.zero & _size,
        Paint()
          ..shader = ui.Gradient.radial(Offset(w * 0.3, h * 0.2), math.max(w, h) * 0.95,
              const [Color(0xFF34373B), Color(0xFF1A1C1F), Color(0xFF08090A)], const [0, 0.5, 1]));
    canvas.save();
    canvas.translate(_originX, _originY);
    canvas.scale(_scale);
    _art.drawBoard(canvas, _scale);
    // Contact shadows of the parts standing on the board.
    for (var i = 0; i < 6; ++i) {
      PcbArt.shadowCircle(canvas, _potX[i], _potY[i], _potSkirtR, 1.6, 2.4, 2.6, 150, _scale);
      PcbArt.shadowCircle(canvas, _potX[i], _potY[i], _potSkirtR, 0.3, 0.5, 0.6, 190, _scale);
    }
    PcbArt.shadowCircle(canvas, _encX, _encY, _encR, 1.6, 2.4, 2.4, 150, _scale);
    PcbArt.shadowCircle(canvas, _encX, _encY, _encR, 0.3, 0.5, 0.6, 190, _scale);
    for (final x in _keyX) {
      final rect = Rect.fromCenter(center: Offset(x, _keyY), width: _keycap, height: _keycap);
      PcbArt.shadowRoundRect(canvas, rect, 1.5, 2.0, 3.0, 3.0, 150, _scale);
      PcbArt.shadowRoundRect(canvas, rect, 1.5, 0.3, 0.5, 0.7, 200, _scale);
    }
    if (!_zoomDisplay) {
      final rect = Rect.fromCenter(center: const Offset(_modX, _modY), width: _modW, height: _modH);
      PcbArt.shadowRoundRect(canvas, rect, _modR, 1.8, 3.0, 2.8, 170, _scale);
    }
    canvas.restore();
    _background?.dispose();
    _background = recorder.endRecording();
    notifyListeners();
  }

  double _mx(double px) => (px - _originX) / _scale;
  double _my(double px) => (px - _originY) / _scale;

  /// Active display area in board mm.
  Rect _displayRectMm() {
    if (_zoomDisplay) {
      const h = _zoomW * (2 * _dispHalfH) / (_dispR - _dispL);
      return Rect.fromCenter(center: const Offset(_zoomX, _zoomY), width: _zoomW, height: h);
    }
    const lift = _moduleHeight * _liftPerMm;
    return const Rect.fromLTRB(_dispL, _modY - _dispHalfH - lift, _dispR, _modY + _dispHalfH - lift);
  }

  // ---- Pointers (touch and mouse) ----

  void pointerDown(int pointer, Offset position) {
    final x = position.dx;
    final y = position.dy;
    final px = _mx(x);
    final py = _my(y);
    // A second finger on or near a held knob grips it: two fingers twist it.
    for (var i = 0; i < 6; ++i) {
      if (_grabs.values.any((g) => g.kind == _Kind.pot && g.index == i) &&
          _distance(px - _potX[i], py - _potY[i]) <= _potSkirtR + _twistReach) {
        _grabs[pointer] = _Grab(_Kind.pot, i, x, y);
        _twistAngle[i] = double.nan;
        return;
      }
    }
    for (var i = 0; i < 4; ++i) {
      const margin = 1.8;
      if ((px - _keyX[i]).abs() <= _keycap / 2 + margin && (py - _keyY).abs() <= _keycap / 2 + margin) {
        _grabs[pointer] = _Grab(_Kind.key, i, x, y);
        _pressKey(i);
        return;
      }
    }
    final encoderDistance = _distance(px - _encX, py - _encY);
    if (encoderDistance <= _encPushR + 0.4) {
      _grabs[pointer] = _Grab(_Kind.encPush, 0, x, y);
      _pressEncoder();
      return;
    }
    if (encoderDistance <= _encR + 6) {
      _grabs[pointer] = _Grab(_Kind.encTurn, 0, x, y)..lastAngle = _angleAround(x, y);
      return;
    }
    for (var i = 0; i < 6; ++i) {
      if (_distance(px - _potX[i], py - _potY[i]) <= _potSkirtR + 3) {
        _grabs[pointer] = _Grab(_Kind.pot, i, x, y);
        return;
      }
    }
    if (_displayRectMm().contains(Offset(px, py))) {
      final now = DateTime.now();
      if (now.difference(_lastDisplayTap).inMilliseconds < 350) {
        _zoomDisplay = !_zoomDisplay;
        _zoomSetting = _zoomDisplay ? 1 : 0;
        _lastDisplayTap = DateTime.fromMillisecondsSinceEpoch(0);
        savePositions();
        _buildBackground();
      } else {
        _lastDisplayTap = now;
      }
      _grabs[pointer] = _Grab(_Kind.display, 0, x, y);
    }
  }

  /// One event per moved pointer; pots turn after all pointers of the event moved.
  void pointerMove(int pointer, Offset position) {
    final grab = _grabs[pointer];
    if (grab == null) return;
    grab.x = position.dx;
    grab.y = position.dy;
    if (grab.kind == _Kind.encTurn) {
      final angle = _angleAround(grab.x, grab.y);
      final diff = _wrap180(angle - grab.lastAngle);
      grab.lastAngle = angle;
      grab.accumulated += diff;
      while (grab.accumulated >= _detentDeg) {
        _detent(1);
        grab.accumulated -= _detentDeg;
      }
      while (grab.accumulated <= -_detentDeg) {
        _detent(-1);
        grab.accumulated += _detentDeg;
      }
    } else if (grab.kind == _Kind.pot) {
      _turnPot(grab.index);
    }
  }

  void pointerUp(int pointer) {
    final grab = _grabs.remove(pointer);
    if (grab == null) return;
    switch (grab.kind) {
      case _Kind.key:
        _releaseKey(grab.index);
      case _Kind.encPush:
        _releaseEncoder();
      case _Kind.pot:
        // A finger left alone goes back to dragging from where it is.
        _twistAngle[grab.index] = double.nan;
        for (final other in _grabs.values) {
          if (other.kind == _Kind.pot && other.index == grab.index) other.lastY = other.y;
        }
        savePositions();
      case _Kind.encTurn || _Kind.display:
        break;
    }
    notifyListeners();
  }

  /// Wheel over a pot or the encoder: one notch = 10 degrees or one detent.
  void wheel(Offset position, double dy) {
    if (dy == 0) return;
    final direction = dy < 0 ? 1 : -1; // wheel up = clockwise
    final px = _mx(position.dx);
    final py = _my(position.dy);
    if (_distance(px - _encX, py - _encY) <= _encR + 6) {
      _detent(direction);
      return;
    }
    for (var i = 0; i < 6; ++i) {
      if (_distance(px - _potX[i], py - _potY[i]) <= _potSkirtR + 3) {
        _setPot(i, positions[i] + direction * _wheelPotStep);
        savePositions();
        return;
      }
    }
  }

  /// Keyboard: 1-4 held = MX keys, space = encoder push. Returns true if used.
  bool key(KeyEvent event) {
    final index = _keyboardKeys.indexOf(event.logicalKey);
    final isSpace = event.logicalKey == LogicalKeyboardKey.space;
    if (index < 0 && !isSpace) return false;
    if (event is KeyDownEvent && _heldKeys.add(event.logicalKey)) {
      isSpace ? _pressEncoder() : _pressKey(index);
    } else if (event is KeyUpEvent && _heldKeys.remove(event.logicalKey)) {
      isSpace ? _releaseEncoder() : _releaseKey(index);
      notifyListeners();
    }
    return true;
  }

  void _pressKey(int i) {
    if (_keyPointers[i]++ == 0) {
      siren.setButton(i + 1, true);
      HapticFeedback.lightImpact();
    }
    notifyListeners();
  }

  void _releaseKey(int i) {
    if (--_keyPointers[i] <= 0) {
      _keyPointers[i] = 0;
      siren.setButton(i + 1, false);
    }
  }

  void _pressEncoder() {
    if (_encoderPointers++ == 0) {
      siren.setEncoderPressed(true);
      HapticFeedback.lightImpact();
    }
    notifyListeners();
  }

  void _releaseEncoder() {
    if (--_encoderPointers <= 0) {
      _encoderPointers = 0;
      siren.setEncoderPressed(false);
    }
  }

  /// One finger: vertical drag, _dragMmFullTravel covers the 300 degrees.
  /// Two fingers (the two lowest pointer ids): the knob turns by the angle the
  /// line between them turns, clockwise = up, like gripping the real knob.
  void _turnPot(int pot) {
    MapEntry<int, _Grab>? first;
    MapEntry<int, _Grab>? second;
    for (final entry in _grabs.entries) {
      if (entry.value.kind != _Kind.pot || entry.value.index != pot) continue;
      if (first == null || entry.key < first.key) {
        second = first;
        first = entry;
      } else if (second == null || entry.key < second.key) {
        second = entry;
      }
    }
    if (first == null) return;
    double delta;
    if (second == null) {
      final grab = first.value;
      delta = (grab.lastY - grab.y) / (_dragMmFullTravel * _scale);
      grab.lastY = grab.y;
    } else {
      final a = first.value;
      final b = second.value;
      final angle = _deg(math.atan2(b.y - a.y, b.x - a.x));
      final previous = _twistAngle[pot];
      _twistAngle[pot] = angle;
      delta = previous.isNaN ? 0 : _wrap180(angle - previous) / _potTravel;
    }
    _setPot(pot, positions[pot] + delta);
  }

  void _setPot(int pot, double value) {
    final next = value.clamp(0.0, 1.0);
    if (next != positions[pot]) {
      positions[pot] = next;
      siren.setPot(pot, next);
      notifyListeners();
    }
  }

  void _detent(int direction) {
    // Clockwise = +1 (NEXT). The direction of the mounted encoder is still unverified.
    _encoderDetents += direction;
    siren.rotateEncoder(direction);
    HapticFeedback.selectionClick();
    notifyListeners();
  }

  double _angleAround(double x, double y) =>
      _deg(math.atan2(y - (_originY + _encY * _scale), x - (_originX + _encX * _scale)));

  static double _distance(double dx, double dy) => math.sqrt(dx * dx + dy * dy);
}

// ---- Drawing ----

class _PanelPainter extends CustomPainter {
  _PanelPainter(this.c) : super(repaint: c);
  final PanelController c;

  // The 5393 is a 320x172 IPS panel, about 10 pixels per mm: seen as glass,
  // not as enlarged squares. Filtered scaling, and black slightly lifted by
  // the backlight (a visual approximation, not a measured panel value).
  static final Paint _pixelPaint = Paint()
    ..filterQuality = FilterQuality.medium
    ..colorFilter = const ColorFilter.matrix([
      0.97, 0, 0, 0, 6, //
      0, 0.97, 0, 0, 6,
      0, 0, 0.97, 0, 8,
      0, 0, 0, 1, 0,
    ]);
  static final Paint _pixelGrid = Paint()..color = const Color(0x46000000);

  @override
  void paint(Canvas canvas, Size size) {
    final background = c._background;
    if (background != null) canvas.drawPicture(background);
    canvas.save();
    canvas.translate(c._originX, c._originY);
    canvas.scale(c._scale);
    _drawDisplay(canvas);
    for (var i = 0; i < 6; ++i) {
      _drawPot(canvas, i);
    }
    _drawEncoder(canvas);
    for (var i = 0; i < 4; ++i) {
      _drawKey(canvas, i);
    }
    canvas.restore();
  }

  void _drawDisplay(Canvas canvas) {
    const lift = _moduleHeight * _liftPerMm;
    final fill = Paint();
    if (!c._zoomDisplay) {
      // Breakout PCB on its headers: side towards the viewer, then the top.
      var rect = Rect.fromCenter(center: const Offset(_modX, _modY), width: _modW, height: _modH);
      canvas.drawRRect(RRect.fromRectAndRadius(rect, const Radius.circular(_modR)),
          fill..color = const Color(0xFF060707));
      rect = rect.shift(const Offset(0, -lift));
      fill.shader = ui.Gradient.linear(rect.topLeft, rect.bottomRight, const [Color(0xFF1C1E1F), Color(0xFF0A0B0B)]);
      canvas.drawRRect(RRect.fromRectAndRadius(rect, const Radius.circular(_modR)), fill);
      fill.shader = null;
      // Module mounting holes, 2.5 mm (footprint NPTH).
      fill.color = const Color(0xFF030303);
      for (final dx in const [-15.875, 15.875]) {
        for (final dy in const [-12.7, 12.7]) {
          canvas.drawCircle(Offset(_modX + dx, _modY + dy - lift), 1.25, fill);
        }
      }
      // TFT cover glass outline.
      canvas.drawRRect(
          RRect.fromLTRBR(_glassL, _modY - _glassHalfH - lift, _glassR, _modY + _glassHalfH - lift,
              const Radius.circular(3)),
          fill..color = const Color(0xFF020203));
    } else {
      final rect = c._displayRectMm().inflate(1.4);
      final rrect = RRect.fromRectAndRadius(rect, const Radius.circular(_dispCorner + 1.4));
      canvas.drawRRect(
          rrect.shift(const Offset(1.2, 2.0)),
          Paint()
            ..color = const Color(0xAA000000)
            ..maskFilter = const MaskFilter.blur(BlurStyle.normal, 2.5 * 0.57735 + 0.5));
      canvas.drawRRect(rrect, fill..color = const Color(0xFF020203));
    }
    final rect = c._displayRectMm();
    final corner = c._zoomDisplay ? _dispCorner * rect.width / (_dispR - _dispL) : _dispCorner;
    canvas.save();
    canvas.clipRRect(RRect.fromRectAndRadius(rect, Radius.circular(corner)));
    final image = c.display;
    if (image != null) {
      canvas.drawImageRect(
          image, Rect.fromLTWH(0, 0, image.width.toDouble(), image.height.toDouble()), rect, _pixelPaint);
    }
    // Gaps between pixels, only where a pixel spans several screen pixels.
    final pixelMm = rect.width / siren.displayWidth;
    if (pixelMm * c._scale >= 4) {
      _pixelGrid.strokeWidth = pixelMm * 0.15;
      for (var column = 1; column < siren.displayWidth; ++column) {
        final x = rect.left + column * pixelMm;
        canvas.drawLine(Offset(x, rect.top), Offset(x, rect.bottom), _pixelGrid);
      }
      for (var row = 1; row < siren.displayHeight; ++row) {
        final y = rect.top + row * pixelMm;
        canvas.drawLine(Offset(rect.left, y), Offset(rect.right, y), _pixelGrid);
      }
    }
    // Glass reflection.
    canvas.drawRect(
        rect,
        Paint()
          ..shader = ui.Gradient.linear(rect.topLeft, Offset(rect.left + rect.width * 0.55, rect.bottom),
              const [Color(0x22FFFFFF), Color(0x08FFFFFF), Color(0x00FFFFFF)], const [0, 0.45, 0.46]));
    canvas.restore();
  }

  void _drawPot(Canvas canvas, int i) {
    final x = _potX[i];
    final y = _potY[i];
    const lift = _knobHeight * _liftPerMm;
    final fill = Paint();
    final stroke = Paint()..style = PaintingStyle.stroke;
    // Skirt.
    fill.shader = ui.Gradient.linear(Offset(x - _potSkirtR, y - _potSkirtR), Offset(x + _potSkirtR, y + _potSkirtR),
        const [Color(0xFF484B4E), Color(0xFF0E0F10)]);
    canvas.drawCircle(Offset(x, y), _potSkirtR, fill);
    stroke
      ..strokeWidth = 0.18
      ..color = const Color(0x46FFFFFF);
    canvas.drawArc(Rect.fromCircle(center: Offset(x, y), radius: _potSkirtR - 0.1), _rad(180), _rad(90), false, stroke);
    // Body: cylinder side towards the viewer, then the top cap.
    fill.shader = ui.Gradient.linear(Offset(x - _potBodyR, 0), Offset(x + _potBodyR, 0),
        const [Color(0xFF222426), Color(0xFF3A3D40), Color(0xFF101112)], const [0, 0.3, 1]);
    canvas.drawCircle(Offset(x, y), _potBodyR, fill);
    canvas.drawRect(Rect.fromLTRB(x - _potBodyR, y - lift, x + _potBodyR, y), fill);
    fill.shader = ui.Gradient.radial(Offset(x - _potBodyR * 0.45, y - lift - _potBodyR * 0.5), _potBodyR * 1.7,
        const [Color(0xFF5A5E62), Color(0xFF2A2C2F), Color(0xFF161719)], const [0, 0.55, 1]);
    canvas.drawCircle(Offset(x, y - lift), _potBodyR, fill);
    stroke
      ..strokeWidth = 0.2
      ..color = const Color(0x5AFFFFFF);
    canvas.drawArc(
        Rect.fromCircle(center: Offset(x, y - lift), radius: _potBodyR - 0.15), _rad(190), _rad(70), false, stroke);
    // Indicator line.
    final angle = _rad((-_potTravel / 2 + _potTravel * c.positions[i]) - 90);
    final cs = math.cos(angle);
    final sn = math.sin(angle);
    stroke
      ..strokeCap = StrokeCap.round
      ..strokeWidth = 1.0
      ..color = const Color(0x78000000);
    canvas.drawLine(Offset(x + cs * 1.8 + 0.12, y - lift + sn * 1.8 + 0.18),
        Offset(x + cs * (_potBodyR - 0.9) + 0.12, y - lift + sn * (_potBodyR - 0.9) + 0.18), stroke);
    stroke
      ..color = const Color(0xFFF2F2EE)
      ..strokeWidth = 0.8;
    canvas.drawLine(Offset(x + cs * 1.8, y - lift + sn * 1.8),
        Offset(x + cs * (_potBodyR - 0.9), y - lift + sn * (_potBodyR - 0.9)), stroke);
  }

  void _drawEncoder(Canvas canvas) {
    final pressed = c._encoderPointers > 0;
    final lift = _knobHeight * _liftPerMm * (pressed ? 0.75 : 1);
    final top = _encY - lift;
    final fill = Paint();
    final stroke = Paint()..style = PaintingStyle.stroke;
    // Black anodized aluminium side and knurled top edge.
    fill.shader = ui.Gradient.linear(const Offset(_encX - _encR, 0), const Offset(_encX + _encR, 0),
        const [Color(0xFF1C1D20), Color(0xFF44464C), Color(0xFF0A0A0C)], const [0, 0.3, 1]);
    canvas.drawCircle(const Offset(_encX, _encY), _encR, fill);
    canvas.drawRect(Rect.fromLTRB(_encX - _encR, top, _encX + _encR, _encY), fill);
    // Top face, turned by whole detents like the real shaft.
    canvas.save();
    canvas.translate(_encX, top);
    canvas.rotate(_rad(c._encoderDetents * _detentDeg));
    fill.shader = ui.Gradient.sweep(Offset.zero, const [
      Color(0xFF1A1B1E),
      Color(0xFF3E4046),
      Color(0xFF1A1B1E),
      Color(0xFF36383E),
      Color(0xFF1A1B1E),
    ], const [0, 0.25, 0.5, 0.75, 1]);
    canvas.drawCircle(Offset.zero, _encR, fill);
    stroke.strokeWidth = 0.22;
    for (var line = 0; line < 60; ++line) {
      final a = _rad(line * 6.0);
      final ca = math.cos(a);
      final sa = math.sin(a);
      stroke.color = line.isEven ? const Color(0x6E000000) : const Color(0x28FFFFFF);
      canvas.drawLine(Offset(ca * (_encR - 1.3), sa * (_encR - 1.3)), Offset(ca * (_encR - 0.1), sa * (_encR - 0.1)), stroke);
    }
    canvas.restore();
    // Smooth centre face.
    fill.shader = ui.Gradient.radial(
        Offset(_encX - 1.6, top - 1.8), _encR, const [Color(0xFF4A4D54), Color(0xFF141517)]);
    canvas.drawCircle(Offset(_encX, top), _encR - 1.5, fill);
    stroke
      ..color = Color.fromARGB(pressed ? 40 : 80, 255, 255, 255)
      ..strokeWidth = 0.18;
    canvas.drawArc(Rect.fromCircle(center: Offset(_encX, top), radius: _encR - 1.6), _rad(190), _rad(70), false, stroke);
  }

  void _drawKey(Canvas canvas, int index) {
    final pressed = c._keyPointers[index] > 0;
    final x = _keyX[index];
    final height = _keyHeight - (pressed ? _keyTravel : 0);
    final lift = height * _liftPerMm;
    const half = _keycap / 2;
    // Base footprint of the cap and its top face (sculpted, narrower).
    final bl = x - half, br = x + half;
    const bt = _keyY - half, bb = _keyY + half;
    final tl = x - half + 2.4, tr = x + half - 2.4;
    final tt = _keyY - half + 1.8 - lift, tb = _keyY + half - 2.4 - lift;
    final shade = pressed ? 0.82 : 1.0;
    Color tone(int v) => Color.fromARGB(255, (v * shade).toInt(), ((v + 3) * shade).toInt(), (v * shade).toInt());
    final fill = Paint();
    void face(Color color, List<double> p) {
      final path = Path()..moveTo(p[0], p[1]);
      for (var k = 2; k < p.length; k += 2) {
        path.lineTo(p[k], p[k + 1]);
      }
      path.close();
      canvas.drawPath(path, fill..color = color);
    }

    canvas.drawRRect(RRect.fromLTRBR(bl, bt, br, bb, const Radius.circular(1.6)), fill..color = tone(0x1E));
    face(tone(0x52), [bl, bt, br, bt, tr, tt, tl, tt]); // back slope, faces the light
    face(tone(0x3E), [bl, bt, tl, tt, tl, tb, bl, bb]); // left
    face(tone(0x26), [br, bt, br, bb, tr, tb, tr, tt]); // right
    face(tone(0x2E), [bl, bb, tl, tb, tr, tb, br, bb]); // front, towards the viewer
    final topFace = RRect.fromLTRBR(tl, tt, tr, tb, const Radius.circular(1.8));
    canvas.drawRRect(topFace, Paint()..shader = ui.Gradient.linear(Offset(tl, tt), Offset(tr, tb), [tone(0x5C), tone(0x3C)]));
    // Spherical dish of the top face.
    canvas.drawRRect(
        topFace,
        Paint()
          ..shader = ui.Gradient.radial(
              Offset(x + 0.8, (tt + tb) / 2 + 0.8), (tr - tl) * 0.55, const [Color(0x32000000), Color(0x00000000)]));
    if (pressed) {
      canvas.drawRRect(
          topFace,
          Paint()
            ..style = PaintingStyle.stroke
            ..color = const Color(0x96B4E03C)
            ..strokeWidth = 0.3);
    }
  }

  @override
  bool shouldRepaint(_PanelPainter oldDelegate) => oldDelegate.c != c;
}

/// The panel: painter plus raw pointer, wheel and keyboard input.
class PanelView extends StatelessWidget {
  const PanelView({super.key, required this.controller});
  final PanelController controller;

  @override
  Widget build(BuildContext context) {
    // shortcut: nominal logical pixels per real mm (160 dpi Android, 96 dpi
    // desktop), not the screen's measured dpi; only picks the auto zoom.
    final logicalPerMm = (Platform.isAndroid ? 160 : 96) / 25.4;
    return Focus(
      autofocus: true,
      onKeyEvent: (_, event) => controller.key(event) ? KeyEventResult.handled : KeyEventResult.ignored,
      child: LayoutBuilder(builder: (context, constraints) {
        controller.layout(constraints.biggest, logicalPerMm);
        return Listener(
          behavior: HitTestBehavior.opaque,
          onPointerDown: (e) => controller.pointerDown(e.pointer, e.localPosition),
          onPointerMove: (e) => controller.pointerMove(e.pointer, e.localPosition),
          onPointerUp: (e) => controller.pointerUp(e.pointer),
          onPointerCancel: (e) => controller.pointerUp(e.pointer),
          onPointerSignal: (e) {
            if (e is PointerScrollEvent) controller.wheel(e.localPosition, e.scrollDelta.dy);
          },
          child: CustomPaint(painter: _PanelPainter(controller), size: constraints.biggest),
        );
      }),
    );
  }
}
