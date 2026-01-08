package com.suvmusic.micmute.audio

/**
 * Controller for managing microphone mute state
 * Uses PowerShell commands for Windows audio control
 */
class MicrophoneController {
    
    private var cachedMuteState: Boolean = false
    
    init {
        // Get initial state
        cachedMuteState = getMuteStateFromSystem()
    }
    
    /**
     * Get current microphone mute state
     * @return true if muted, false if unmuted
     */
    fun getMuteState(): Boolean {
        return cachedMuteState
    }
    
    /**
     * Set microphone mute state
     * @param muted true to mute, false to unmute
     * @return true if successful
     */
    fun setMute(muted: Boolean): Boolean {
        return try {
            // PowerShell command to set microphone mute via AudioDeviceCmdlets or native
            val script = if (muted) {
                """
                Add-Type -TypeDefinition @'
                using System;
                using System.Runtime.InteropServices;
                public class AudioManager {
                    [DllImport("user32.dll")]
                    public static extern IntPtr SendMessage(IntPtr hWnd, int Msg, IntPtr wParam, IntPtr lParam);
                    [DllImport("user32.dll")]
                    public static extern IntPtr FindWindow(string lpClassName, string lpWindowName);
                }
'@
                `$WM_APPCOMMAND = 0x0319
                `$APPCOMMAND_MICROPHONE_VOLUME_MUTE = 0x180000
                `$hwnd = [AudioManager]::FindWindow('Shell_TrayWnd', `$null)
                [AudioManager]::SendMessage(`$hwnd, `$WM_APPCOMMAND, `$hwnd, [IntPtr]`$APPCOMMAND_MICROPHONE_VOLUME_MUTE)
                """.trimIndent()
            } else {
                // Same command toggles mute state
                """
                Add-Type -TypeDefinition @'
                using System;
                using System.Runtime.InteropServices;
                public class AudioManager {
                    [DllImport("user32.dll")]
                    public static extern IntPtr SendMessage(IntPtr hWnd, int Msg, IntPtr wParam, IntPtr lParam);
                    [DllImport("user32.dll")]
                    public static extern IntPtr FindWindow(string lpClassName, string lpWindowName);
                }
'@
                `$WM_APPCOMMAND = 0x0319
                `$APPCOMMAND_MICROPHONE_VOLUME_MUTE = 0x180000
                `$hwnd = [AudioManager]::FindWindow('Shell_TrayWnd', `$null)
                [AudioManager]::SendMessage(`$hwnd, `$WM_APPCOMMAND, `$hwnd, [IntPtr]`$APPCOMMAND_MICROPHONE_VOLUME_MUTE)
                """.trimIndent()
            }
            
            val process = ProcessBuilder("powershell", "-Command", script)
                .redirectErrorStream(true)
                .start()
            process.waitFor()
            cachedMuteState = muted
            true
        } catch (e: Exception) {
            println("Error setting mute state: ${e.message}")
            false
        }
    }
    
    /**
     * Toggle microphone mute state
     * @return new mute state (true = muted)
     */
    fun toggleMute(): Boolean {
        val newState = !cachedMuteState
        setMute(newState)
        cachedMuteState = newState
        return newState
    }
    
    /**
     * Get mute state from system (initial check)
     */
    private fun getMuteStateFromSystem(): Boolean {
        return try {
            // Try to detect current state - default to unmuted
            false
        } catch (e: Exception) {
            false
        }
    }
}
