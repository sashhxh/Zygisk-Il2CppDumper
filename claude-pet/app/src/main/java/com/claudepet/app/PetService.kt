package com.claudepet.app

import android.annotation.SuppressLint
import android.app.Notification
import android.app.NotificationChannel
import android.app.NotificationManager
import android.app.PendingIntent
import android.app.Service
import android.content.Context
import android.content.Intent
import android.content.pm.ServiceInfo
import android.graphics.PixelFormat
import android.os.Build
import android.os.Handler
import android.os.IBinder
import android.os.Looper
import android.view.Gravity
import android.view.MotionEvent
import android.view.View
import android.view.ViewConfiguration
import android.view.WindowManager
import android.widget.FrameLayout
import android.widget.ImageView
import android.widget.TextView
import kotlin.math.abs
import kotlin.math.sin

/**
 * Foreground service that shows the pet as a system overlay:
 * it walks across the screen, can be dragged, shows speech bubbles,
 * and opens the chat when tapped.
 */
class PetService : Service() {

    companion object {
        /** Live reference so the chat screen can push replies into the bubble. */
        @Volatile
        var instance: PetService? = null

        private const val CHANNEL_ID = "claude_pet"
        private const val NOTIF_ID = 1
    }

    private lateinit var wm: WindowManager
    private lateinit var root: FrameLayout
    private lateinit var pet: ImageView
    private lateinit var bubble: TextView
    private lateinit var params: WindowManager.LayoutParams
    private val handler = Handler(Looper.getMainLooper())

    private var density = 1f
    private var screenW = 0
    private var petSize = 0
    private var dir = 1          // 1 = right, -1 = left
    private var speed = 5        // px per frame
    private var walking = true
    private var frame = 0L
    private var bubbleHideAt = 0L

    private val idlePhrases = listOf(
        "Привет! 👋", "Чем занят? 😊", "Я тут гуляю 🐾",
        "Нажми на меня! ✨", "Хорошего дня! ☀️", "Мур-р 😺"
    )

    private val loop = object : Runnable {
        override fun run() {
            if (walking) step()

            if (bubble.visibility == View.VISIBLE && System.currentTimeMillis() > bubbleHideAt) {
                bubble.visibility = View.GONE
                safeUpdate()
            }

            frame++
            if (frame % 500L == 0L && walking && bubble.visibility != View.VISIBLE) {
                showBubble(idlePhrases.random(), 2500)
            }
            handler.postDelayed(this, 16)
        }
    }

    override fun onBind(intent: Intent?): IBinder? = null

    override fun onCreate() {
        super.onCreate()
        instance = this
        startAsForeground()

        wm = getSystemService(Context.WINDOW_SERVICE) as WindowManager
        val metrics = resources.displayMetrics
        density = metrics.density
        screenW = metrics.widthPixels
        petSize = (72 * density).toInt()

        buildViews()

        val type = if (Build.VERSION.SDK_INT >= 26) {
            WindowManager.LayoutParams.TYPE_APPLICATION_OVERLAY
        } else {
            @Suppress("DEPRECATION")
            WindowManager.LayoutParams.TYPE_PHONE
        }

        params = WindowManager.LayoutParams(
            WindowManager.LayoutParams.WRAP_CONTENT,
            WindowManager.LayoutParams.WRAP_CONTENT,
            type,
            WindowManager.LayoutParams.FLAG_NOT_FOCUSABLE or
                WindowManager.LayoutParams.FLAG_LAYOUT_NO_LIMITS,
            PixelFormat.TRANSLUCENT
        ).apply {
            gravity = Gravity.TOP or Gravity.START
            x = 0
            y = (metrics.heightPixels * 0.5f).toInt()
        }

        try {
            wm.addView(root, params)
        } catch (e: Exception) {
            stopSelf()
            return
        }
        handler.post(loop)
    }

    @SuppressLint("ClickableViewAccessibility")
    private fun buildViews() {
        root = FrameLayout(this)

        bubble = TextView(this).apply {
            setBackgroundResource(R.drawable.bubble)
            setTextColor(0xFF3A2A20.toInt())
            textSize = 13f
            val padH = (12 * density).toInt()
            val padV = (8 * density).toInt()
            setPadding(padH, padV, padH, padV)
            maxWidth = (230 * density).toInt()
            visibility = View.GONE
        }

        pet = ImageView(this).apply {
            setImageResource(R.drawable.pet)
        }

        root.addView(
            bubble,
            FrameLayout.LayoutParams(
                FrameLayout.LayoutParams.WRAP_CONTENT,
                FrameLayout.LayoutParams.WRAP_CONTENT
            )
        )
        root.addView(
            pet,
            FrameLayout.LayoutParams(petSize, petSize).apply {
                topMargin = (46 * density).toInt() // leave room for the bubble above
            }
        )

        val slop = ViewConfiguration.get(this).scaledTouchSlop
        var downX = 0f
        var downY = 0f
        var startX = 0
        var startY = 0
        var dragging = false

        pet.setOnTouchListener { _, e ->
            when (e.action) {
                MotionEvent.ACTION_DOWN -> {
                    downX = e.rawX; downY = e.rawY
                    startX = params.x; startY = params.y
                    dragging = false
                    walking = false
                    true
                }
                MotionEvent.ACTION_MOVE -> {
                    val dx = e.rawX - downX
                    val dy = e.rawY - downY
                    if (!dragging && (abs(dx) > slop || abs(dy) > slop)) dragging = true
                    if (dragging) {
                        params.x = (startX + dx).toInt()
                        params.y = (startY + dy).toInt()
                        safeUpdate()
                    }
                    true
                }
                MotionEvent.ACTION_UP, MotionEvent.ACTION_CANCEL -> {
                    if (!dragging) onTap()
                    walking = true
                    params.x = params.x.coerceIn(0, (screenW - petSize).coerceAtLeast(0))
                    safeUpdate()
                    true
                }
                else -> false
            }
        }
    }

    private fun step() {
        params.x += dir * speed
        if (params.x <= 0) {
            params.x = 0
            dir = 1
        }
        if (params.x >= screenW - petSize) {
            params.x = screenW - petSize
            dir = -1
        }
        pet.scaleX = if (dir >= 0) 1f else -1f
        pet.translationY = (sin(frame / 6.0) * 4.0 * density).toFloat()
        safeUpdate()
    }

    private fun onTap() {
        if (Prefs.apiKey(this).isBlank()) {
            showBubble("Добавь API-ключ в приложении, и я заговорю! 🔑", 3500)
            return
        }
        try {
            startActivity(
                Intent(this, ChatActivity::class.java).addFlags(Intent.FLAG_ACTIVITY_NEW_TASK)
            )
        } catch (_: Exception) {
        }
    }

    fun showBubble(text: String, durationMs: Long = 4000) {
        handler.post {
            bubble.text = text
            bubble.visibility = View.VISIBLE
            bubbleHideAt = System.currentTimeMillis() + durationMs
            safeUpdate()
        }
    }

    private fun safeUpdate() {
        try {
            wm.updateViewLayout(root, params)
        } catch (_: Exception) {
        }
    }

    private fun startAsForeground() {
        if (Build.VERSION.SDK_INT >= 26) {
            val channel = NotificationChannel(
                CHANNEL_ID, "Claude Pet", NotificationManager.IMPORTANCE_LOW
            )
            getSystemService(NotificationManager::class.java).createNotificationChannel(channel)
        }

        val pi = PendingIntent.getActivity(
            this, 0, Intent(this, MainActivity::class.java),
            PendingIntent.FLAG_IMMUTABLE
        )

        val builder = if (Build.VERSION.SDK_INT >= 26) {
            Notification.Builder(this, CHANNEL_ID)
        } else {
            @Suppress("DEPRECATION")
            Notification.Builder(this)
        }

        val notif = builder
            .setContentTitle("Claude гуляет по экрану 🐾")
            .setContentText("Нажми, чтобы открыть настройки")
            .setSmallIcon(R.drawable.pet)
            .setContentIntent(pi)
            .build()

        if (Build.VERSION.SDK_INT >= 34) {
            startForeground(NOTIF_ID, notif, ServiceInfo.FOREGROUND_SERVICE_TYPE_SPECIAL_USE)
        } else {
            startForeground(NOTIF_ID, notif)
        }
    }

    override fun onStartCommand(intent: Intent?, flags: Int, startId: Int): Int = START_STICKY

    override fun onDestroy() {
        super.onDestroy()
        handler.removeCallbacksAndMessages(null)
        try {
            wm.removeView(root)
        } catch (_: Exception) {
        }
        instance = null
    }
}
