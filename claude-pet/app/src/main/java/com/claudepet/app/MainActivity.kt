package com.claudepet.app

import android.app.Activity
import android.content.Intent
import android.content.pm.PackageManager
import android.net.Uri
import android.os.Build
import android.os.Bundle
import android.provider.Settings
import android.widget.Button
import android.widget.EditText
import android.widget.Toast

class MainActivity : Activity() {

    private lateinit var apiKeyEt: EditText
    private lateinit var modelEt: EditText

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        setContentView(R.layout.activity_main)

        apiKeyEt = findViewById(R.id.apiKey)
        modelEt = findViewById(R.id.model)
        apiKeyEt.setText(Prefs.apiKey(this))
        modelEt.setText(Prefs.model(this))

        findViewById<Button>(R.id.saveBtn).setOnClickListener {
            save()
            Toast.makeText(this, "Сохранено ✅", Toast.LENGTH_SHORT).show()
        }
        findViewById<Button>(R.id.permBtn).setOnClickListener { requestOverlay() }
        findViewById<Button>(R.id.startBtn).setOnClickListener { startPet() }
        findViewById<Button>(R.id.stopBtn).setOnClickListener {
            stopService(Intent(this, PetService::class.java))
            Toast.makeText(this, "Питомец ушёл 👋", Toast.LENGTH_SHORT).show()
        }

        maybeRequestNotifications()
    }

    private fun save() {
        Prefs.setApiKey(this, apiKeyEt.text.toString())
        Prefs.setModel(this, modelEt.text.toString())
    }

    private fun requestOverlay() {
        if (Settings.canDrawOverlays(this)) {
            Toast.makeText(this, "Разрешение уже есть 👍", Toast.LENGTH_SHORT).show()
        } else {
            startActivity(
                Intent(
                    Settings.ACTION_MANAGE_OVERLAY_PERMISSION,
                    Uri.parse("package:$packageName")
                )
            )
        }
    }

    private fun maybeRequestNotifications() {
        if (Build.VERSION.SDK_INT >= 33) {
            val granted = checkSelfPermission(android.Manifest.permission.POST_NOTIFICATIONS) ==
                PackageManager.PERMISSION_GRANTED
            if (!granted) {
                requestPermissions(arrayOf(android.Manifest.permission.POST_NOTIFICATIONS), 1)
            }
        }
    }

    private fun startPet() {
        save()
        if (!Settings.canDrawOverlays(this)) {
            Toast.makeText(this, "Сначала дай разрешение «поверх других приложений»", Toast.LENGTH_LONG).show()
            requestOverlay()
            return
        }
        val intent = Intent(this, PetService::class.java)
        if (Build.VERSION.SDK_INT >= 26) startForegroundService(intent) else startService(intent)
        Toast.makeText(this, "Питомец запущен 🐾 Сверни приложение!", Toast.LENGTH_SHORT).show()
    }
}
