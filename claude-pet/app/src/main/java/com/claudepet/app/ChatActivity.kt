package com.claudepet.app

import android.app.Activity
import android.os.Bundle
import android.os.Handler
import android.os.Looper
import android.view.View
import android.widget.Button
import android.widget.EditText
import android.widget.ScrollView
import android.widget.TextView
import android.widget.Toast
import kotlin.concurrent.thread

/** A small floating chat window, opened by tapping the pet. */
class ChatActivity : Activity() {

    private val history = ArrayList<ChatClient.Msg>()
    private val main = Handler(Looper.getMainLooper())

    private lateinit var log: TextView
    private lateinit var scroll: ScrollView
    private lateinit var input: EditText
    private lateinit var sendBtn: Button

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        setTitle("Claude 🐾")
        setContentView(R.layout.activity_chat)

        log = findViewById(R.id.log)
        scroll = findViewById(R.id.scroll)
        input = findViewById(R.id.input)
        sendBtn = findViewById(R.id.send)

        sendBtn.setOnClickListener { send() }
        findViewById<Button>(R.id.close).setOnClickListener { finish() }

        append("Claude", "Привет! Я тут 🐾 Спроси меня о чём угодно.")
    }

    private fun append(who: String, text: String) {
        log.append("$who: $text\n\n")
        scroll.post { scroll.fullScroll(View.FOCUS_DOWN) }
    }

    private fun send() {
        val text = input.text.toString().trim()
        if (text.isEmpty()) return

        val apiKey = Prefs.apiKey(this)
        if (apiKey.isBlank()) {
            Toast.makeText(this, "Сначала добавь API-ключ в приложении", Toast.LENGTH_LONG).show()
            return
        }
        val model = Prefs.model(this)

        input.setText("")
        append("Ты", text)
        history.add(ChatClient.Msg("user", text))

        sendBtn.isEnabled = false
        thread {
            val reply = try {
                ChatClient.chat(apiKey, model, history)
            } catch (e: Exception) {
                "Ошибка: ${e.message}"
            }
            main.post {
                history.add(ChatClient.Msg("assistant", reply))
                append("Claude", reply)
                sendBtn.isEnabled = true
                PetService.instance?.showBubble(reply, 6000)
            }
        }
    }
}
