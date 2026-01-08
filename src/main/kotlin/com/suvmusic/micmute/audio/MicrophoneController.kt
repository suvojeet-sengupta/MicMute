package com.suvmusic.micmute.audio

/**
 * Controller for managing microphone mute state
 * Uses PowerShell with Windows API for audio control
 */
class MicrophoneController {
    
    private var cachedMuteState: Boolean = false
    
    /**
     * Get current microphone mute state
     */
    fun getMuteState(): Boolean = cachedMuteState
    
    /**
     * Set microphone mute state
     */
    fun setMute(muted: Boolean): Boolean {
        return try {
            // PowerShell script to toggle microphone mute using Windows API
            val script = """
                Add-Type -TypeDefinition '
                using System;
                using System.Runtime.InteropServices;
                public class AudioControl {
                    [DllImport("user32.dll")]
                    public static extern IntPtr SendMessage(IntPtr hWnd, int Msg, IntPtr wParam, IntPtr lParam);
                    [DllImport("user32.dll")]
                    public static extern IntPtr FindWindow(string lpClassName, string lpWindowName);
                    
                    public static void ToggleMicMute() {
                        IntPtr hwnd = FindWindow("Shell_TrayWnd", null);
                        SendMessage(hwnd, 0x0319, hwnd, (IntPtr)0x180000);
                    }
                }
                '
                [AudioControl]::ToggleMicMute()
            """.trimIndent()
            
            val process = ProcessBuilder("powershell", "-Command", script)
                .redirectErrorStream(true)
                .start()
            process.waitFor()
            cachedMuteState = muted
            true
        } catch (e: Exception) {
            println("Error setting mute: " + e.message)
            false
        }
    }
    
    /**
     * Toggle microphone mute state
     */
    fun toggleMute(): Boolean {
        val newState = !cachedMuteState
        setMute(newState)
        cachedMuteState = newState
        return newState
    }
}
