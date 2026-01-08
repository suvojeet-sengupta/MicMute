package com.suvmusic.micmute

import androidx.compose.runtime.*
import androidx.compose.ui.res.painterResource
import androidx.compose.ui.unit.DpSize
import androidx.compose.ui.unit.dp
import androidx.compose.ui.window.Tray
import androidx.compose.ui.window.Window
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
        icon = painterResource(if (isMuted) "icons/mic_off.png" else "icons/mic_on.png"),
        tooltip = if (isMuted) "Microphone Muted - Click to Unmute" else "Microphone Active - Click to Mute",
        onAction = {
            // Single click toggles mute
            isMuted = micController.toggleMute()
        },
        menu = {
            Item(
                text = if (isMuted) "Unmute Microphone" else "Mute Microphone",
                onClick = {
                    isMuted = micController.toggleMute()
                }
            )
            Separator()
            Item(
                text = "Settings",
                onClick = {
                    isWindowVisible = true
                }
            )
            Separator()
            Item(
                text = "Exit",
                onClick = onExitRequest
            )
        }
    )
    
    // Main Window (Setup or Settings)
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
            state = rememberWindowState(
                size = DpSize(450.dp, 600.dp)
            ),
            resizable = false,
            icon = painterResource("icons/app_icon.png")
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
