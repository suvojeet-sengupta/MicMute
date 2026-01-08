package com.suvmusic.micmute

import androidx.compose.ui.window.application
import com.suvmusic.micmute.data.PreferencesManager

fun main() = application {
    val preferencesManager = PreferencesManager()
    
    App(
        preferencesManager = preferencesManager,
        onExitRequest = { exitApplication() }
    )
}
