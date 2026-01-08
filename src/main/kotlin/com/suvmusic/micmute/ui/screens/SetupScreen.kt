package com.suvmusic.micmute.ui.screens

import androidx.compose.animation.core.*
import androidx.compose.foundation.background
import androidx.compose.foundation.border
import androidx.compose.foundation.clickable
import androidx.compose.foundation.layout.*
import androidx.compose.foundation.shape.CircleShape
import androidx.compose.foundation.shape.RoundedCornerShape
import androidx.compose.material.icons.Icons
import androidx.compose.material.icons.filled.Mic
import androidx.compose.material.icons.filled.MicOff
import androidx.compose.material.icons.outlined.CheckCircle
import androidx.compose.material3.*
import androidx.compose.runtime.*
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.draw.clip
import androidx.compose.ui.draw.scale
import androidx.compose.ui.draw.shadow
import androidx.compose.ui.graphics.Brush
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.text.font.FontWeight
import androidx.compose.ui.text.style.TextAlign
import androidx.compose.ui.unit.dp
import androidx.compose.ui.unit.sp
import com.suvmusic.micmute.ui.theme.*

@Composable
fun SetupScreen(
    isMuted: Boolean,
    onMuteToggle: () -> Unit,
    onFinishSetup: () -> Unit
) {
    // Pulse animation for the mic button
    val infiniteTransition = rememberInfiniteTransition()
    val pulseScale by infiniteTransition.animateFloat(
        initialValue = 1f,
        targetValue = 1.08f,
        animationSpec = infiniteRepeatable(
            animation = tween(1000, easing = EaseInOutCubic),
            repeatMode = RepeatMode.Reverse
        )
    )
    
    val glowAlpha by infiniteTransition.animateFloat(
        initialValue = 0.3f,
        targetValue = 0.7f,
        animationSpec = infiniteRepeatable(
            animation = tween(1500, easing = EaseInOutCubic),
            repeatMode = RepeatMode.Reverse
        )
    )

    Box(
        modifier = Modifier
            .fillMaxSize()
            .background(
                Brush.verticalGradient(
                    colors = listOf(PrimaryDark, PrimaryLight, PrimaryDark)
                )
            )
    ) {
        Column(
            modifier = Modifier
                .fillMaxSize()
                .padding(32.dp),
            horizontalAlignment = Alignment.CenterHorizontally,
            verticalArrangement = Arrangement.SpaceBetween
        ) {
            // Header
            Column(
                horizontalAlignment = Alignment.CenterHorizontally,
                modifier = Modifier.padding(top = 20.dp)
            ) {
                Text(
                    text = "🎙️",
                    fontSize = 48.sp
                )
                Spacer(modifier = Modifier.height(12.dp))
                Text(
                    text = "MicMute",
                    fontSize = 36.sp,
                    fontWeight = FontWeight.Bold,
                    color = TextPrimary
                )
                Spacer(modifier = Modifier.height(8.dp))
                Text(
                    text = "Control your microphone with one click",
                    fontSize = 14.sp,
                    color = TextSecondary,
                    textAlign = TextAlign.Center
                )
            }
            
            // Main Mic Button
            Column(
                horizontalAlignment = Alignment.CenterHorizontally
            ) {
                Box(
                    contentAlignment = Alignment.Center,
                    modifier = Modifier
                        .size(180.dp)
                        .scale(pulseScale)
                ) {
                    // Glow effect
                    Box(
                        modifier = Modifier
                            .size(180.dp)
                            .background(
                                color = (if (isMuted) MutedColor else UnmutedColor).copy(alpha = glowAlpha * 0.3f),
                                shape = CircleShape
                            )
                    )
                    
                    // Outer ring
                    Box(
                        modifier = Modifier
                            .size(160.dp)
                            .border(
                                width = 3.dp,
                                brush = Brush.linearGradient(
                                    colors = if (isMuted) 
                                        listOf(MutedColor, MutedColor.copy(alpha = 0.5f))
                                    else 
                                        listOf(UnmutedColor, UnmutedColor.copy(alpha = 0.5f))
                                ),
                                shape = CircleShape
                            )
                    )
                    
                    // Main button
                    Box(
                        modifier = Modifier
                            .size(140.dp)
                            .shadow(
                                elevation = 20.dp,
                                shape = CircleShape,
                                ambientColor = if (isMuted) MutedColor else UnmutedColor,
                                spotColor = if (isMuted) MutedColor else UnmutedColor
                            )
                            .clip(CircleShape)
                            .background(
                                Brush.radialGradient(
                                    colors = if (isMuted)
                                        listOf(MutedColor, MutedColor.copy(alpha = 0.8f))
                                    else
                                        listOf(UnmutedColor, UnmutedColor.copy(alpha = 0.8f))
                                )
                            )
                            .clickable { onMuteToggle() },
                        contentAlignment = Alignment.Center
                    ) {
                        Icon(
                            imageVector = if (isMuted) Icons.Filled.MicOff else Icons.Filled.Mic,
                            contentDescription = if (isMuted) "Unmute" else "Mute",
                            tint = Color.White,
                            modifier = Modifier.size(64.dp)
                        )
                    }
                }
                
                Spacer(modifier = Modifier.height(24.dp))
                
                // Status text
                Text(
                    text = if (isMuted) "MICROPHONE MUTED" else "MICROPHONE ACTIVE",
                    fontSize = 18.sp,
                    fontWeight = FontWeight.SemiBold,
                    color = if (isMuted) MutedColor else UnmutedColor,
                    letterSpacing = 2.sp
                )
                
                Spacer(modifier = Modifier.height(8.dp))
                
                Text(
                    text = "Click the button or tray icon to toggle",
                    fontSize = 12.sp,
                    color = TextMuted
                )
            }
            
            // Features & Finish
            Column(
                horizontalAlignment = Alignment.CenterHorizontally
            ) {
                // Features card
                Card(
                    modifier = Modifier
                        .fillMaxWidth()
                        .padding(horizontal = 8.dp),
                    colors = CardDefaults.cardColors(
                        containerColor = GlassBackground
                    ),
                    shape = RoundedCornerShape(16.dp)
                ) {
                    Column(
                        modifier = Modifier.padding(20.dp)
                    ) {
                        FeatureItem("Runs silently in system tray")
                        FeatureItem("Single-click mute/unmute")
                        FeatureItem("Always accessible from taskbar")
                    }
                }
                
                Spacer(modifier = Modifier.height(24.dp))
                
                // Finish button
                Button(
                    onClick = onFinishSetup,
                    modifier = Modifier
                        .fillMaxWidth()
                        .height(56.dp),
                    colors = ButtonDefaults.buttonColors(
                        containerColor = AccentPurple
                    ),
                    shape = RoundedCornerShape(16.dp)
                ) {
                    Text(
                        text = "Start MicMute",
                        fontSize = 16.sp,
                        fontWeight = FontWeight.SemiBold
                    )
                }
                
                Spacer(modifier = Modifier.height(16.dp))
                
                // Developer credit
                Text(
                    text = "Developed by Suvojeet Sengupta",
                    fontSize = 12.sp,
                    color = TextMuted
                )
            }
        }
    }
}

@Composable
private fun FeatureItem(text: String) {
    Row(
        modifier = Modifier.padding(vertical = 6.dp),
        verticalAlignment = Alignment.CenterVertically
    ) {
        Icon(
            imageVector = Icons.Outlined.CheckCircle,
            contentDescription = null,
            tint = AccentGreen,
            modifier = Modifier.size(20.dp)
        )
        Spacer(modifier = Modifier.width(12.dp))
        Text(
            text = text,
            fontSize = 14.sp,
            color = TextSecondary
        )
    }
}

private val EaseInOutCubic = CubicBezierEasing(0.65f, 0f, 0.35f, 1f)
