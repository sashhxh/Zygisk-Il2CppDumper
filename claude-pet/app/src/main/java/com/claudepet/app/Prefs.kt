package com.claudepet.app

import android.content.Context

/** Tiny local store for the API key and model. Stored only on this device. */
object Prefs {
    private const val NAME = "claude_pet_prefs"
    private const val KEY_API = "api_key"
    private const val KEY_MODEL = "model"
    const val DEFAULT_MODEL = "claude-opus-5-5"

    private fun sp(c: Context) = c.getSharedPreferences(NAME, Context.MODE_PRIVATE)

    fun apiKey(c: Context): String = sp(c).getString(KEY_API, "").orEmpty()

    fun setApiKey(c: Context, value: String) {
        sp(c).edit().putString(KEY_API, value.trim()).apply()
    }

    fun model(c: Context): String {
        val m = sp(c).getString(KEY_MODEL, DEFAULT_MODEL).orEmpty().trim()
        return if (m.isEmpty()) DEFAULT_MODEL else m
    }

    fun setModel(c: Context, value: String) {
        sp(c).edit().putString(KEY_MODEL, value.trim()).apply()
    }
}
