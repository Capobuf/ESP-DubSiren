package com.capobuf.dubsiren

import android.annotation.SuppressLint
import android.content.Context
import android.content.SharedPreferences
import android.graphics.Bitmap
import android.graphics.Canvas
import android.graphics.Color
import android.graphics.LinearGradient
import android.graphics.Paint
import android.graphics.Path
import android.graphics.RadialGradient
import android.graphics.RectF
import android.graphics.Shader
import android.graphics.SweepGradient
import android.graphics.Typeface
import android.os.SystemClock
import android.view.HapticFeedbackConstants
import android.view.MotionEvent
import android.view.View
import kotlin.math.abs
import kotlin.math.atan2
import kotlin.math.cos
import kotlin.math.hypot
import kotlin.math.min
import kotlin.math.sin

/**
 * Front panel render. Positions come from hardware/kicad/ESP-DubSiren.kicad_pcb
 * (8 Nov 2026); the board itself is drawn by [PcbArt] from the exported copper,
 * silkscreen and outline. Only physical gestures are offered: six pots, four
 * momentary keys, encoder turn (detented) and push. Volume = tablet keys
 * (the rear Master is held at full scale).
 *
 * Depth: light from the top left, camera slightly in front of the panel, so
 * tall parts show their top shifted up and their side towards the viewer.
 */
@SuppressLint("ViewConstructor")
class PanelView(context: Context, private val prefs: SharedPreferences) : View(context) {

    private companion object {
        const val BOARD_W = 86f
        const val BOARD_TOP = -6f
        const val BOARD_BOTTOM = 120f

        val POT_X = floatArrayOf(15f, 43f, 71f, 15f, 43f, 71f) // RV1..RV6
        val POT_Y = floatArrayOf(18f, 18f, 18f, 76f, 76f, 76f)
        const val POT_SKIRT_R = 19.84f / 2 // Davies 1100 skirt 25/32"
        const val POT_BODY_R = 16.67f / 2 // body 21/32"
        const val POT_TRAVEL = 300f // Bourns PTV111 mechanical angle

        const val ENC_X = 75f // SW7 PEC11R-4120F-S0018
        const val ENC_Y = 46.5f
        const val ENC_R = 7f // SAK-023: visual radius, not dimensioned in the repo
        const val ENC_PUSH_R = 3.6f
        const val DETENT_DEG = 20f // 18 detents per turn (PEC11R datasheet)

        // U5 Adafruit 5393 (official Eagle geometry in the footprint F.Fab).
        const val MOD_X = 44.016f
        const val MOD_Y = 46.5f
        const val MOD_W = 36.83f
        const val MOD_H = 30.48f
        const val MOD_R = 2.54f
        const val GLASS_L = MOD_X - 18.216f
        const val GLASS_R = MOD_X + 18.084f
        const val GLASS_HALF_H = 9.7f
        const val DISP_L = MOD_X - 17.191f // active area, corner radius 3
        const val DISP_R = MOD_X + 15.159f
        const val DISP_HALF_H = 8.7f
        const val DISP_CORNER = 3f
        // Enlarged view for small phones (double tap the display to switch).
        const val ZOOM_X = 33.5f
        const val ZOOM_Y = 43f
        const val ZOOM_W = 52f

        // SW1..SW4 MX2A: switch centre = 4 mm centre-post hole, (-2.54, +5.08) from
        // the KiCad footprint origin (pin 1).
        val KEY_X = floatArrayOf(10f, 32f, 54f, 76f)
        const val KEY_Y = 100f
        const val KEYCAP = 18f // provisional 1u keycap, no MPN yet

        // Apparent lift of a part's top for its height (camera slightly in front).
        const val LIFT_PER_MM = 0.06f
        const val KNOB_HEIGHT = 12f
        const val KEY_HEIGHT = 18f
        const val KEY_TRAVEL = 4f // MX2A total travel
        const val MODULE_HEIGHT = 8f

        const val DRAG_MM_FULL_TRAVEL = 55f
        const val DEFAULT_POT = 0.5f
        const val ZOOM_UNSET = -1
    }

    private enum class Kind { POT, KEY, ENC_TURN, ENC_PUSH, DISPLAY }

    private class Grab(val kind: Kind, val index: Int, var lastX: Float, var lastY: Float) {
        var lastAngle = 0f
        var accumulated = 0f
    }

    private val art = PcbArt(context)
    private val positions = FloatArray(6) { prefs.getFloat("pot$it", DEFAULT_POT) }
    private val keyPointers = IntArray(4)
    private var encoderPointers = 0
    private var encoderDetents = 0
    private val grabs = HashMap<Int, Grab>()
    private var zoomSetting = prefs.getInt("zoomDisplayMode", ZOOM_UNSET) // -1 auto, 0 real, 1 zoom
    private var zoomDisplay = false
    private var lastDisplayTap = 0L

    private val potLabels = Array(6) { "" }
    private val keyLabels = Array(4) { "" }
    private var status = ""

    private val display = Bitmap.createBitmap(
        NativeSiren.nativeDisplayWidth(), NativeSiren.nativeDisplayHeight(), Bitmap.Config.ARGB_8888)
    private var background: Bitmap? = null

    private var scale = 1f
    private var originX = 0f
    private var originY = 0f
    private var statusTop = 0f

    private val fill = Paint(Paint.ANTI_ALIAS_FLAG)
    private val stroke = Paint(Paint.ANTI_ALIAS_FLAG).apply { style = Paint.Style.STROKE }
    private val hint = Paint(Paint.ANTI_ALIAS_FLAG).apply {
        textAlign = Paint.Align.CENTER
        typeface = Typeface.create("sans-serif-condensed", Typeface.BOLD)
        color = Color.rgb(0xB4, 0xE0, 0x3C)
    }
    private val mono = Paint(Paint.ANTI_ALIAS_FLAG).apply {
        typeface = Typeface.MONOSPACE
        color = Color.rgb(0x70, 0x76, 0x7A)
    }
    private val pixelPaint = Paint().apply { isFilterBitmap = false }
    private val rect = RectF()
    private val path = Path()

    // Shaders in board millimetres (the canvas is scaled), so built once.
    private val potSkirt = Array(6) { i ->
        LinearGradient(POT_X[i] - POT_SKIRT_R, POT_Y[i] - POT_SKIRT_R, POT_X[i] + POT_SKIRT_R, POT_Y[i] + POT_SKIRT_R,
            Color.rgb(0x48, 0x4B, 0x4E), Color.rgb(0x0E, 0x0F, 0x10), Shader.TileMode.CLAMP)
    }
    private val potCap = Array(6) { i ->
        val lift = KNOB_HEIGHT * LIFT_PER_MM
        RadialGradient(POT_X[i] - POT_BODY_R * 0.45f, POT_Y[i] - lift - POT_BODY_R * 0.5f, POT_BODY_R * 1.7f,
            intArrayOf(Color.rgb(0x5A, 0x5E, 0x62), Color.rgb(0x2A, 0x2C, 0x2F), Color.rgb(0x16, 0x17, 0x19)),
            floatArrayOf(0f, 0.55f, 1f), Shader.TileMode.CLAMP)
    }
    private val potSide = Array(6) { i ->
        LinearGradient(POT_X[i] - POT_BODY_R, 0f, POT_X[i] + POT_BODY_R, 0f,
            intArrayOf(Color.rgb(0x22, 0x24, 0x26), Color.rgb(0x3A, 0x3D, 0x40), Color.rgb(0x10, 0x11, 0x12)),
            floatArrayOf(0f, 0.3f, 1f), Shader.TileMode.CLAMP)
    }
    private val encSide = LinearGradient(ENC_X - ENC_R, 0f, ENC_X + ENC_R, 0f,
        intArrayOf(Color.rgb(0x1C, 0x1D, 0x20), Color.rgb(0x44, 0x46, 0x4C), Color.rgb(0x0A, 0x0A, 0x0C)),
        floatArrayOf(0f, 0.3f, 1f), Shader.TileMode.CLAMP)
    private val encTop = SweepGradient(0f, 0f,
        intArrayOf(Color.rgb(0x1A, 0x1B, 0x1E), Color.rgb(0x3E, 0x40, 0x46), Color.rgb(0x1A, 0x1B, 0x1E),
            Color.rgb(0x36, 0x38, 0x3E), Color.rgb(0x1A, 0x1B, 0x1E)), null)

    init {
        isHapticFeedbackEnabled = true
    }

    // ---- Native link -------------------------------------------------------

    fun sendAllPositions() {
        for (channel in 0..5) NativeSiren.nativeSetPot(channel, positions[channel])
    }

    fun savePositions() {
        val editor = prefs.edit()
        for (channel in 0..5) editor.putFloat("pot$channel", positions[channel])
        editor.putInt("zoomDisplayMode", zoomSetting)
        editor.apply()
    }

    fun releaseAll() {
        for (key in 0..3) if (keyPointers[key] > 0) {
            keyPointers[key] = 0
            NativeSiren.nativeSetButton(key + 1, false)
        }
        if (encoderPointers > 0) {
            encoderPointers = 0
            NativeSiren.nativeSetEncoderPressed(false)
        }
        grabs.clear()
        invalidate()
    }

    fun updateDisplay(pixels: IntArray) {
        display.setPixels(pixels, 0, display.width, 0, 0, display.width, display.height)
        invalidate()
    }

    fun setLabels(labels: String) {
        val lines = labels.split('\n')
        for (i in 0..5) potLabels[i] = lines.getOrElse(i) { "" }.replace('_', ' ')
        for (i in 0..3) keyLabels[i] = lines.getOrElse(6 + i) { "" }.replace('_', ' ')
        invalidate()
    }

    fun setStatus(value: String) {
        if (value != status) {
            status = value
            invalidate()
        }
    }

    // ---- Layout ------------------------------------------------------------

    override fun onSizeChanged(w: Int, h: Int, oldw: Int, oldh: Int) {
        if (w == 0 || h == 0) return
        val density = resources.displayMetrics.density
        val statusHeight = 16f * density
        val modelW = BOARD_W + 8f
        val modelH = BOARD_BOTTOM - BOARD_TOP + 10f
        scale = min(w / modelW, (h - statusHeight) / modelH)
        originX = (w - BOARD_W * scale) / 2f
        val used = modelH * scale + statusHeight
        originY = (h - used) / 2f + (-BOARD_TOP + 4f) * scale
        statusTop = originY + (BOARD_BOTTOM + 5f) * scale
        mono.textSize = 9f * resources.displayMetrics.scaledDensity
        // Real-size display unless the panel is drawn smaller than ~1.2x life size.
        val pixelsPerRealMm = resources.displayMetrics.xdpi / 25.4f
        zoomDisplay = if (zoomSetting == ZOOM_UNSET) scale / pixelsPerRealMm < 1.2f else zoomSetting == 1
        buildBackground(w, h)
    }

    private fun buildBackground(w: Int, h: Int) {
        val bitmap = background?.takeIf { it.width == w && it.height == h }
            ?: Bitmap.createBitmap(w, h, Bitmap.Config.ARGB_8888).also { background = it }
        val canvas = Canvas(bitmap)
        // Studio desk: dark, lit from the top left.
        fill.shader = RadialGradient(w * 0.3f, h * 0.2f, maxOf(w, h) * 0.95f,
            intArrayOf(Color.rgb(0x34, 0x37, 0x3B), Color.rgb(0x1A, 0x1C, 0x1F), Color.rgb(0x08, 0x09, 0x0A)),
            floatArrayOf(0f, 0.5f, 1f), Shader.TileMode.CLAMP)
        canvas.drawRect(0f, 0f, w.toFloat(), h.toFloat(), fill)
        fill.shader = null

        canvas.save()
        canvas.translate(originX, originY)
        canvas.scale(scale, scale)
        art.drawBoard(canvas, scale)
        // Contact shadows of the parts standing on the board.
        for (i in 0..5) {
            art.shadowCircle(canvas, POT_X[i], POT_Y[i], POT_SKIRT_R, 1.6f, 2.4f, 2.6f, 150, scale)
            art.shadowCircle(canvas, POT_X[i], POT_Y[i], POT_SKIRT_R, 0.3f, 0.5f, 0.6f, 190, scale)
        }
        art.shadowCircle(canvas, ENC_X, ENC_Y, ENC_R, 1.6f, 2.4f, 2.4f, 150, scale)
        art.shadowCircle(canvas, ENC_X, ENC_Y, ENC_R, 0.3f, 0.5f, 0.6f, 190, scale)
        for (x in KEY_X) {
            rect.set(x - KEYCAP / 2, KEY_Y - KEYCAP / 2, x + KEYCAP / 2, KEY_Y + KEYCAP / 2)
            art.shadowRoundRect(canvas, rect, 1.5f, 2.0f, 3.0f, 3.0f, 150, scale)
            art.shadowRoundRect(canvas, rect, 1.5f, 0.3f, 0.5f, 0.7f, 200, scale)
        }
        if (!zoomDisplay) {
            rect.set(MOD_X - MOD_W / 2, MOD_Y - MOD_H / 2, MOD_X + MOD_W / 2, MOD_Y + MOD_H / 2)
            art.shadowRoundRect(canvas, rect, MOD_R, 1.8f, 3.0f, 2.8f, 170, scale)
        }
        canvas.restore()
        invalidate()
    }

    private fun sx(mm: Float) = originX + mm * scale
    private fun sy(mm: Float) = originY + mm * scale
    private fun mx(px: Float) = (px - originX) / scale
    private fun my(px: Float) = (px - originY) / scale

    /** Active display area in board mm. */
    private fun displayRectMm(out: RectF) {
        if (zoomDisplay) {
            val h = ZOOM_W * (2 * DISP_HALF_H) / (DISP_R - DISP_L)
            out.set(ZOOM_X - ZOOM_W / 2, ZOOM_Y - h / 2, ZOOM_X + ZOOM_W / 2, ZOOM_Y + h / 2)
        } else {
            val lift = MODULE_HEIGHT * LIFT_PER_MM
            out.set(DISP_L, MOD_Y - DISP_HALF_H - lift, DISP_R, MOD_Y + DISP_HALF_H - lift)
        }
    }

    // ---- Drawing -----------------------------------------------------------

    override fun onDraw(canvas: Canvas) {
        background?.let { canvas.drawBitmap(it, 0f, 0f, null) }
        canvas.save()
        canvas.translate(originX, originY)
        canvas.scale(scale, scale)
        drawDisplay(canvas)
        for (i in 0..5) drawPot(canvas, i)
        drawEncoder(canvas)
        for (i in 0..3) drawKey(canvas, i)
        canvas.restore()
        drawHints(canvas)
        canvas.drawText(status, sx(0f), statusTop + mono.textSize, mono)
    }

    private fun drawDisplay(canvas: Canvas) {
        val lift = MODULE_HEIGHT * LIFT_PER_MM
        if (!zoomDisplay) {
            // Breakout PCB on its headers: side towards the viewer, then the top.
            fill.color = Color.rgb(0x06, 0x07, 0x07)
            rect.set(MOD_X - MOD_W / 2, MOD_Y - MOD_H / 2, MOD_X + MOD_W / 2, MOD_Y + MOD_H / 2)
            canvas.drawRoundRect(rect, MOD_R, MOD_R, fill)
            rect.offset(0f, -lift)
            fill.shader = LinearGradient(rect.left, rect.top, rect.right, rect.bottom,
                Color.rgb(0x1C, 0x1E, 0x1F), Color.rgb(0x0A, 0x0B, 0x0B), Shader.TileMode.CLAMP)
            canvas.drawRoundRect(rect, MOD_R, MOD_R, fill)
            fill.shader = null
            // Module mounting holes, 2.5 mm (footprint NPTH).
            fill.color = Color.rgb(0x03, 0x03, 0x03)
            for (dx in floatArrayOf(-15.875f, 15.875f)) for (dy in floatArrayOf(-12.7f, 12.7f)) {
                canvas.drawCircle(MOD_X + dx, MOD_Y + dy - lift, 1.25f, fill)
            }
            // TFT cover glass outline.
            fill.color = Color.rgb(0x02, 0x02, 0x03)
            rect.set(GLASS_L, MOD_Y - GLASS_HALF_H - lift, GLASS_R, MOD_Y + GLASS_HALF_H - lift)
            canvas.drawRoundRect(rect, 3f, 3f, fill)
        } else {
            displayRectMm(rect)
            rect.inset(-1.4f, -1.4f)
            fill.color = Color.rgb(0x02, 0x02, 0x03)
            fill.setShadowLayer(2.5f, 1.2f, 2.0f, Color.argb(170, 0, 0, 0))
            canvas.drawRoundRect(rect, DISP_CORNER + 1.4f, DISP_CORNER + 1.4f, fill)
            fill.clearShadowLayer()
        }
        displayRectMm(rect)
        val corner = if (zoomDisplay) DISP_CORNER * rect.width() / (DISP_R - DISP_L) else DISP_CORNER
        canvas.save()
        path.reset()
        path.addRoundRect(rect, corner, corner, Path.Direction.CW)
        canvas.clipPath(path)
        canvas.drawBitmap(display, null, rect, pixelPaint)
        // Glass reflection.
        fill.shader = LinearGradient(rect.left, rect.top, rect.left + rect.width() * 0.55f, rect.bottom,
            intArrayOf(Color.argb(34, 255, 255, 255), Color.argb(8, 255, 255, 255), Color.argb(0, 255, 255, 255)),
            floatArrayOf(0f, 0.45f, 0.46f), Shader.TileMode.CLAMP)
        canvas.drawRect(rect, fill)
        fill.shader = null
        canvas.restore()
    }

    private fun drawPot(canvas: Canvas, i: Int) {
        val x = POT_X[i]
        val y = POT_Y[i]
        val lift = KNOB_HEIGHT * LIFT_PER_MM
        // Skirt.
        fill.shader = potSkirt[i]
        canvas.drawCircle(x, y, POT_SKIRT_R, fill)
        fill.shader = null
        stroke.strokeWidth = 0.18f
        stroke.color = Color.argb(70, 255, 255, 255)
        canvas.drawArc(x - POT_SKIRT_R + 0.1f, y - POT_SKIRT_R + 0.1f, x + POT_SKIRT_R - 0.1f, y + POT_SKIRT_R - 0.1f,
            180f, 90f, false, stroke)
        // Body: cylinder side towards the viewer, then the top cap.
        fill.shader = potSide[i]
        canvas.drawCircle(x, y, POT_BODY_R, fill)
        canvas.drawRect(x - POT_BODY_R, y - lift, x + POT_BODY_R, y, fill)
        fill.shader = potCap[i]
        canvas.drawCircle(x, y - lift, POT_BODY_R, fill)
        fill.shader = null
        stroke.strokeWidth = 0.2f
        stroke.color = Color.argb(90, 255, 255, 255)
        canvas.drawArc(x - POT_BODY_R + 0.15f, y - lift - POT_BODY_R + 0.15f, x + POT_BODY_R - 0.15f,
            y - lift + POT_BODY_R - 0.15f, 190f, 70f, false, stroke)
        // Indicator line.
        val angle = Math.toRadians((-POT_TRAVEL / 2 + POT_TRAVEL * positions[i]) - 90.0)
        val c = cos(angle).toFloat()
        val s = sin(angle).toFloat()
        stroke.strokeCap = Paint.Cap.ROUND
        stroke.strokeWidth = 1.0f
        stroke.color = Color.argb(120, 0, 0, 0)
        canvas.drawLine(x + c * 1.8f + 0.12f, y - lift + s * 1.8f + 0.18f, x + c * (POT_BODY_R - 0.9f) + 0.12f,
            y - lift + s * (POT_BODY_R - 0.9f) + 0.18f, stroke)
        stroke.color = Color.rgb(0xF2, 0xF2, 0xEE)
        stroke.strokeWidth = 0.8f
        canvas.drawLine(x + c * 1.8f, y - lift + s * 1.8f, x + c * (POT_BODY_R - 0.9f),
            y - lift + s * (POT_BODY_R - 0.9f), stroke)
        stroke.strokeCap = Paint.Cap.BUTT
    }

    private fun drawEncoder(canvas: Canvas) {
        val pressed = encoderPointers > 0
        val lift = KNOB_HEIGHT * LIFT_PER_MM * if (pressed) 0.75f else 1f
        val top = ENC_Y - lift
        // Black anodized aluminium side and knurled top edge.
        fill.shader = encSide
        canvas.drawCircle(ENC_X, ENC_Y, ENC_R, fill)
        canvas.drawRect(ENC_X - ENC_R, top, ENC_X + ENC_R, ENC_Y, fill)
        // Top face, turned by whole detents like the real shaft.
        canvas.save()
        canvas.translate(ENC_X, top)
        canvas.rotate(encoderDetents * DETENT_DEG)
        fill.shader = encTop
        canvas.drawCircle(0f, 0f, ENC_R, fill)
        fill.shader = null
        stroke.strokeWidth = 0.22f
        for (line in 0 until 60) {
            val a = Math.toRadians(line * 6.0)
            val ca = cos(a).toFloat()
            val sa = sin(a).toFloat()
            stroke.color = if (line % 2 == 0) Color.argb(110, 0, 0, 0) else Color.argb(40, 255, 255, 255)
            canvas.drawLine(ca * (ENC_R - 1.3f), sa * (ENC_R - 1.3f), ca * (ENC_R - 0.1f), sa * (ENC_R - 0.1f), stroke)
        }
        canvas.restore()
        // Smooth centre face.
        fill.shader = RadialGradient(ENC_X - 1.6f, top - 1.8f, ENC_R,
            Color.rgb(0x4A, 0x4D, 0x54), Color.rgb(0x14, 0x15, 0x17), Shader.TileMode.CLAMP)
        canvas.drawCircle(ENC_X, top, ENC_R - 1.5f, fill)
        fill.shader = null
        stroke.color = Color.argb(if (pressed) 40 else 80, 255, 255, 255)
        stroke.strokeWidth = 0.18f
        canvas.drawArc(ENC_X - ENC_R + 1.6f, top - ENC_R + 1.6f, ENC_X + ENC_R - 1.6f, top + ENC_R - 1.6f,
            190f, 70f, false, stroke)
    }

    private fun drawKey(canvas: Canvas, index: Int) {
        val pressed = keyPointers[index] > 0
        val x = KEY_X[index]
        val height = KEY_HEIGHT - if (pressed) KEY_TRAVEL else 0f
        val lift = height * LIFT_PER_MM
        val half = KEYCAP / 2
        // Base footprint of the cap and its top face (sculpted, narrower).
        val bl = x - half; val br = x + half; val bt = KEY_Y - half; val bb = KEY_Y + half
        val tl = x - half + 2.4f; val tr = x + half - 2.4f
        val tt = KEY_Y - half + 1.8f - lift; val tb = KEY_Y + half - 2.4f - lift
        val shade = if (pressed) 0.82f else 1f
        fun face(color: Int, vararg p: Float) {
            path.reset()
            path.moveTo(p[0], p[1])
            var k = 2
            while (k < p.size) { path.lineTo(p[k], p[k + 1]); k += 2 }
            path.close()
            fill.color = color
            canvas.drawPath(path, fill)
        }
        fun tone(v: Int) = Color.rgb((v * shade).toInt(), ((v + 3) * shade).toInt(), (v * shade).toInt())
        rect.set(bl, bt, br, bb)
        fill.color = tone(0x1E)
        canvas.drawRoundRect(rect, 1.6f, 1.6f, fill)
        face(tone(0x52), bl, bt, br, bt, tr, tt, tl, tt) // back slope, faces the light
        face(tone(0x3E), bl, bt, tl, tt, tl, tb, bl, bb) // left
        face(tone(0x26), br, bt, br, bb, tr, tb, tr, tt) // right
        face(tone(0x2E), bl, bb, tl, tb, tr, tb, br, bb) // front, towards the viewer
        rect.set(tl, tt, tr, tb)
        fill.shader = LinearGradient(tl, tt, tr, tb, tone(0x5C), tone(0x3C), Shader.TileMode.CLAMP)
        canvas.drawRoundRect(rect, 1.8f, 1.8f, fill)
        fill.shader = null
        // Spherical dish of the top face.
        fill.shader = RadialGradient(x + 0.8f, (tt + tb) / 2 + 0.8f, (tr - tl) * 0.55f,
            Color.argb(50, 0, 0, 0), Color.argb(0, 0, 0, 0), Shader.TileMode.CLAMP)
        canvas.drawRoundRect(rect, 1.8f, 1.8f, fill)
        fill.shader = null
        if (pressed) {
            stroke.color = Color.argb(150, 0xB4, 0xE0, 0x3C)
            stroke.strokeWidth = 0.3f
            canvas.drawRoundRect(rect, 1.8f, 1.8f, stroke)
        }
    }

    private fun drawHints(canvas: Canvas) {
        // Simulator aid (not on the hardware): current function of each control.
        hint.textSize = 2.0f * scale
        hint.setShadowLayer(0.4f * scale, 0f, 0.15f * scale, Color.BLACK)
        for (i in 0..5) {
            canvas.drawText(potLabels[i], sx(POT_X[i]), sy(POT_Y[i] - POT_SKIRT_R - 1.4f), hint)
        }
        for (i in 0..3) {
            val pressed = keyPointers[i] > 0
            val lift = (KEY_HEIGHT - if (pressed) KEY_TRAVEL else 0f) * LIFT_PER_MM
            hint.textSize = 1.9f * scale
            canvas.drawText(keyLabels[i], sx(KEY_X[i]), sy(KEY_Y - 0.4f - lift), hint)
        }
        hint.clearShadowLayer()
    }

    // ---- Touch -------------------------------------------------------------

    @SuppressLint("ClickableViewAccessibility")
    override fun onTouchEvent(event: MotionEvent): Boolean {
        when (event.actionMasked) {
            MotionEvent.ACTION_DOWN, MotionEvent.ACTION_POINTER_DOWN -> {
                val i = event.actionIndex
                begin(event.getPointerId(i), event.getX(i), event.getY(i))
            }
            MotionEvent.ACTION_MOVE -> {
                for (i in 0 until event.pointerCount) move(event.getPointerId(i), event.getX(i), event.getY(i))
            }
            MotionEvent.ACTION_UP, MotionEvent.ACTION_POINTER_UP -> end(event.getPointerId(event.actionIndex))
            MotionEvent.ACTION_CANCEL -> releaseAll()
        }
        return true
    }

    private fun begin(pointer: Int, x: Float, y: Float) {
        val px = mx(x)
        val py = my(y)
        for (i in 0..3) {
            val margin = 1.8f
            if (abs(px - KEY_X[i]) <= KEYCAP / 2 + margin && abs(py - KEY_Y) <= KEYCAP / 2 + margin) {
                grabs[pointer] = Grab(Kind.KEY, i, x, y)
                if (keyPointers[i]++ == 0) {
                    NativeSiren.nativeSetButton(i + 1, true)
                    performHapticFeedback(HapticFeedbackConstants.VIRTUAL_KEY)
                }
                invalidate()
                return
            }
        }
        val encoderDistance = hypot(px - ENC_X, py - ENC_Y)
        if (encoderDistance <= ENC_PUSH_R + 0.4f) {
            grabs[pointer] = Grab(Kind.ENC_PUSH, 0, x, y)
            if (encoderPointers++ == 0) {
                NativeSiren.nativeSetEncoderPressed(true)
                performHapticFeedback(HapticFeedbackConstants.VIRTUAL_KEY)
            }
            invalidate()
            return
        }
        if (encoderDistance <= ENC_R + 6f) {
            grabs[pointer] = Grab(Kind.ENC_TURN, 0, x, y).also { it.lastAngle = angleAround(x, y) }
            return
        }
        for (i in 0..5) {
            if (hypot(px - POT_X[i], py - POT_Y[i]) <= POT_SKIRT_R + 3f) {
                grabs[pointer] = Grab(Kind.POT, i, x, y)
                return
            }
        }
        displayRectMm(rect)
        if (rect.contains(px, py)) {
            val now = SystemClock.uptimeMillis()
            if (now - lastDisplayTap < 350) {
                zoomDisplay = !zoomDisplay
                zoomSetting = if (zoomDisplay) 1 else 0
                lastDisplayTap = 0
                savePositions()
                buildBackground(width, height)
            } else {
                lastDisplayTap = now
            }
            grabs[pointer] = Grab(Kind.DISPLAY, 0, x, y)
        }
    }

    private fun move(pointer: Int, x: Float, y: Float) {
        val grab = grabs[pointer] ?: return
        when (grab.kind) {
            Kind.POT -> {
                // Vertical drag turns the knob; DRAG_MM_FULL_TRAVEL covers 300 degrees.
                val delta = (grab.lastY - y) / (DRAG_MM_FULL_TRAVEL * scale)
                grab.lastY = y
                val next = (positions[grab.index] + delta).coerceIn(0f, 1f)
                if (next != positions[grab.index]) {
                    positions[grab.index] = next
                    NativeSiren.nativeSetPot(grab.index, next)
                    invalidate()
                }
            }
            Kind.ENC_TURN -> {
                val angle = angleAround(x, y)
                var diff = angle - grab.lastAngle
                if (diff > 180f) diff -= 360f
                if (diff < -180f) diff += 360f
                grab.lastAngle = angle
                grab.accumulated += diff
                while (grab.accumulated >= DETENT_DEG) { detent(1); grab.accumulated -= DETENT_DEG }
                while (grab.accumulated <= -DETENT_DEG) { detent(-1); grab.accumulated += DETENT_DEG }
            }
            else -> Unit
        }
    }

    private fun detent(direction: Int) {
        // Clockwise = +1 (NEXT). The direction of the mounted encoder is still unverified.
        encoderDetents += direction
        NativeSiren.nativeRotateEncoder(direction)
        performHapticFeedback(HapticFeedbackConstants.CLOCK_TICK)
        invalidate()
    }

    private fun end(pointer: Int) {
        val grab = grabs.remove(pointer) ?: return
        when (grab.kind) {
            Kind.KEY -> if (--keyPointers[grab.index] <= 0) {
                keyPointers[grab.index] = 0
                NativeSiren.nativeSetButton(grab.index + 1, false)
            }
            Kind.ENC_PUSH -> if (--encoderPointers <= 0) {
                encoderPointers = 0
                NativeSiren.nativeSetEncoderPressed(false)
            }
            Kind.POT -> savePositions()
            else -> Unit
        }
        invalidate()
    }

    private fun angleAround(x: Float, y: Float): Float =
        Math.toDegrees(atan2((y - sy(ENC_Y)).toDouble(), (x - sx(ENC_X)).toDouble())).toFloat()
}
