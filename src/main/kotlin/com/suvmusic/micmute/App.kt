package com.suvmusic.micmute

import androidx.compose.runtime.*
import androidx.compose.ui.geometry.Size
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.graphics.drawscope.DrawScope
import androidx.compose.ui.graphics.painter.Painter
import androidx.compose.ui.unit.DpSize
import androidx.compose.ui.unit.dp
import androidx.compose.ui.window.Tray
import androidx.compose.ui.window.Window
import androidx.compose.ui.window.application
import androidx.compose.ui.window.rememberTrayState
import androidx.compose.ui.window.rememberWindowState
import com.suvmusic.micmute.audio.MicrophoneController
import com.suvmusic.micmute.data.PreferencesManager
import com.suvmusic.micmute.ui.screens.SetupScreen
import com.suvmusic.micmute.ui.theme.MicMuteTheme

@Composable
fun App(
    preferencesManager: PreferencesManager,
    onExitRequest: () -> Unit
) {
    val micController = remember { MicrophoneController() }
    var isMuted by remember { mutableStateOf(micController.getMuteState()) }
    var showSetup by remember { mutableStateOf(!preferencesManager.isSetupComplete()) }
    var isWindowVisible by remember { mutableStateOf(showSetup) }
    
    val trayState = rememberTrayState()
    
    // System Tray
    Tray(
        state = trayState,
        icon = if (isMuted) MutedTrayIcon else ActiveTrayIcon,
        tooltip = if (isMuted) "Microphone Muted - Click to Unmute" else "Microphone Active - Click to Mute",
        onAction = {
            isMuted = micController.toggleMute()
        },
        menu = {
            Item(
                if (isMuted) "Unmute Microphone" else "Mute Microphone",
                onClick = {
                    isMuted = micController.toggleMute()
                }
            )
            Separator()
            Item(
                "Settings",
                onClick = {
                    isWindowVisible = true
                }
            )
            Separator()
            Item(
                "Exit",
                onClick = onExitRequest
            )
        }
    )
    
    // Main Window
    if (isWindowVisible) {
        Window(
            onCloseRequest = { 
                if (preferencesManager.isSetupComplete()) {
                    isWindowVisible = false
                } else {
                    onExitRequest()
                }
            },
            title = "MicMute",
            state = rememberWindowState(size = DpSize(450.dp, 600.dp)),
            resizable = false,
            icon = AppIcon
        ) {
            MicMuteTheme {
                SetupScreen(
                    isMuted = isMuted,
                    onMuteToggle = {
                        isMuted = micController.toggleMute()
                    },
                    onFinishSetup = {
                        preferencesManager.setSetupComplete(true)
                        showSetup = false
                        isWindowVisible = false
                    }
                )
            }
        }
    }
}

// Active microphone icon (green)
object ActiveTrayIcon : Painter() {
    override val intrinsicSize = Size(256f, 256f)
    override fun DrawScope.onDraw() {
        drawCircle(Color(0xFF22C55E))
    }
}

// Muted microphone icon (red)
object MutedTrayIcon : Painter() {
    override val intrinsicSize = Size(256f, 256f)
    override fun DrawScope.onDraw() {
        drawCircle(Color(0xFFEF4444))
    }
}

// App icon (purple)
object AppIcon : Painter() {
    override val intrinsicSize = Size(256f, 256f)
    override fun DrawScope.onDraw() {
        drawCircle(Color(0xFF8B5CF6))
    }
}
