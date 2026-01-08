package com.suvmusic.micmute

import androidx.compose.foundation.background
import androidx.compose.foundation.layout.*
import androidx.compose.foundation.shape.CircleShape
import androidx.compose.foundation.shape.RoundedCornerShape
import androidx.compose.material3.*
import androidx.compose.runtime.*
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.draw.clip
import androidx.compose.ui.geometry.Size
import androidx.compose.ui.graphics.Brush
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.graphics.drawscope.DrawScope
import androidx.compose.ui.graphics.painter.Painter
import androidx.compose.ui.text.font.FontWeight
import androidx.compose.ui.text.style.TextAlign
import androidx.compose.ui.unit.DpSize
import androidx.compose.ui.unit.dp
import androidx.compose.ui.unit.sp
import androidx.compose.ui.window.*
import com.suvmusic.micmute.audio.MicrophoneController
import com.suvmusic.micmute.data.PreferencesManager

fun main() = application {
    val preferencesManager = remember { PreferencesManager() }
    val micController = remember { MicrophoneController() }
    var isMuted by remember { mutableStateOf(micController.getMuteState()) }
    var isWindowVisible by remember { mutableStateOf(!preferencesManager.isSetupComplete()) }
    
    val trayState = rememberTrayState()
    
    // System Tray
    Tray(
        state = trayState,
        icon = if (isMuted) MutedTrayIcon else ActiveTrayIcon,
        tooltip = if (isMuted) "Microphone Muted" else "Microphone Active",
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
                onClick = ::exitApplication
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
                    exitApplication()
                }
            },
            title = "MicMute",
            state = rememberWindowState(size = DpSize(450.dp, 600.dp)),
            resizable = false,
            icon = AppIcon
        ) {
            MaterialTheme(
                colorScheme = darkColorScheme(
                    primary = Color(0xFF8B5CF6),
                    background = Color(0xFF1A1B26),
                    surface = Color(0xFF1F2937)
                )
            ) {
                SetupScreen(
                    isMuted = isMuted,
                    onMuteToggle = {
                        isMuted = micController.toggleMute()
                    },
                    onFinishSetup = {
                        preferencesManager.setSetupComplete(true)
                        isWindowVisible = false
                    }
                )
            }
        }
    }
}

@Composable
fun SetupScreen(
    isMuted: Boolean,
    onMuteToggle: () -> Unit,
    onFinishSetup: () -> Unit
) {
    Box(
        modifier = Modifier
            .fillMaxSize()
            .background(
                Brush.verticalGradient(
                    colors = listOf(Color(0xFF1A1B26), Color(0xFF24283B))
                )
            )
    ) {
        Column(
            modifier = Modifier.fillMaxSize().padding(32.dp),
            horizontalAlignment = Alignment.CenterHorizontally,
            verticalArrangement = Arrangement.SpaceBetween
        ) {
            // Header
            Column(horizontalAlignment = Alignment.CenterHorizontally) {
                Spacer(modifier = Modifier.height(20.dp))
                Text("MicMute", fontSize = 36.sp, fontWeight = FontWeight.Bold, color = Color.White)
                Text("Control your microphone with one click", fontSize = 14.sp, color = Color.Gray)
            }
            
            // Main Button
            Column(horizontalAlignment = Alignment.CenterHorizontally) {
                Box(
                    modifier = Modifier
                        .size(140.dp)
                        .clip(CircleShape)
                        .background(if (isMuted) Color(0xFFEF4444) else Color(0xFF22C55E)),
                    contentAlignment = Alignment.Center
                ) {
                    Button(
                        onClick = onMuteToggle,
                        modifier = Modifier.size(130.dp),
                        shape = CircleShape,
                        colors = ButtonDefaults.buttonColors(
                            containerColor = if (isMuted) Color(0xFFEF4444) else Color(0xFF22C55E)
                        )
                    ) {
                        Text(
                            if (isMuted) "MUTED" else "ACTIVE",
                            fontSize = 16.sp,
                            fontWeight = FontWeight.Bold
                        )
                    }
                }
                Spacer(modifier = Modifier.height(16.dp))
                Text(
                    if (isMuted) "MICROPHONE MUTED" else "MICROPHONE ACTIVE",
                    fontSize = 18.sp,
                    fontWeight = FontWeight.SemiBold,
                    color = if (isMuted) Color(0xFFEF4444) else Color(0xFF22C55E)
                )
            }
            
            // Footer
            Column(horizontalAlignment = Alignment.CenterHorizontally) {
                Card(
                    modifier = Modifier.fillMaxWidth(),
                    colors = CardDefaults.cardColors(containerColor = Color(0x1AFFFFFF)),
                    shape = RoundedCornerShape(16.dp)
                ) {
                    Column(modifier = Modifier.padding(20.dp)) {
                        Text("• Runs silently in system tray", color = Color.LightGray)
                        Text("• Single-click mute/unmute", color = Color.LightGray)
                        Text("• Always accessible from taskbar", color = Color.LightGray)
                    }
                }
                Spacer(modifier = Modifier.height(24.dp))
                Button(
                    onClick = onFinishSetup,
                    modifier = Modifier.fillMaxWidth().height(56.dp),
                    colors = ButtonDefaults.buttonColors(containerColor = Color(0xFF8B5CF6)),
                    shape = RoundedCornerShape(16.dp)
                ) {
                    Text("Start MicMute", fontSize = 16.sp, fontWeight = FontWeight.SemiBold)
                }
                Spacer(modifier = Modifier.height(16.dp))
                Text("Developed by Suvojeet Sengupta", fontSize = 12.sp, color = Color.Gray)
            }
        }
    }
}

// Tray Icons
object ActiveTrayIcon : Painter() {
    override val intrinsicSize = Size(256f, 256f)
    override fun DrawScope.onDraw() { drawCircle(Color(0xFF22C55E)) }
}

object MutedTrayIcon : Painter() {
    override val intrinsicSize = Size(256f, 256f)
    override fun DrawScope.onDraw() { drawCircle(Color(0xFFEF4444)) }
}

object AppIcon : Painter() {
    override val intrinsicSize = Size(256f, 256f)
    override fun DrawScope.onDraw() { drawCircle(Color(0xFF8B5CF6)) }
}
