package com.claudepet.app

import org.json.JSONArray
import org.json.JSONObject
import java.io.BufferedReader
import java.io.InputStreamReader
import java.net.HttpURLConnection
import java.net.URL

/**
 * Talks to the Anthropic Messages API over plain HTTPS.
 * Call [chat] from a background thread — it blocks on network I/O.
 */
object ChatClient {
    private const val ENDPOINT = "https://api.anthropic.com/v1/messages"
    private const val ANTHROPIC_VERSION = "2023-06-01"

    private const val SYSTEM =
        "Ты — Клод (Claude), милый маленький питомец, который живёт на экране телефона пользователя. " +
        "Отвечай коротко (1–3 предложения), по-доброму, живо и с эмодзи. " +
        "Отвечай на том же языке, на котором пишет пользователь."

    data class Msg(val role: String, val text: String)

    fun chat(apiKey: String, model: String, history: List<Msg>): String {
        val conn = (URL(ENDPOINT).openConnection() as HttpURLConnection).apply {
            requestMethod = "POST"
            connectTimeout = 20_000
            readTimeout = 60_000
            doOutput = true
            setRequestProperty("content-type", "application/json")
            setRequestProperty("x-api-key", apiKey)
            setRequestProperty("anthropic-version", ANTHROPIC_VERSION)
        }

        try {
            val messages = JSONArray()
            for (m in history) {
                messages.put(JSONObject().put("role", m.role).put("content", m.text))
            }
            val body = JSONObject()
                .put("model", model)
                .put("max_tokens", 400)
                .put("system", SYSTEM)
                .put("messages", messages)

            conn.outputStream.use { it.write(body.toString().toByteArray(Charsets.UTF_8)) }

            val code = conn.responseCode
            val stream = if (code in 200..299) conn.inputStream else conn.errorStream
            val raw = stream?.let {
                BufferedReader(InputStreamReader(it, Charsets.UTF_8)).use { r -> r.readText() }
            }.orEmpty()

            if (code !in 200..299) {
                val msg = runCatching {
                    JSONObject(raw).getJSONObject("error").getString("message")
                }.getOrDefault(raw.ifBlank { "неизвестная ошибка" })
                throw RuntimeException("API $code: $msg")
            }

            val json = JSONObject(raw)
            val stopReason = json.optString("stop_reason", "")
            val content = json.optJSONArray("content") ?: JSONArray()
            val sb = StringBuilder()
            for (i in 0 until content.length()) {
                val block = content.optJSONObject(i) ?: continue
                if (block.optString("type") == "text") sb.append(block.optString("text"))
            }
            val text = sb.toString().trim()
            return when {
                text.isNotEmpty() -> text
                stopReason == "refusal" -> "Прости, на это я ответить не могу 🙈"
                else -> "Хм, я не придумал, что сказать 🤔"
            }
        } finally {
            conn.disconnect()
        }
    }
}
