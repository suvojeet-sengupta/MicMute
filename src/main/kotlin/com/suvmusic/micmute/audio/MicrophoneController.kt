package com.suvmusic.micmute.audio

/**
 * Controller for managing microphone mute state
 * Uses Windows Core Audio API via JNA
 */
class MicrophoneController {
    
    private var isInitialized = false
    private var fallbackToCommand = false
    
    init {
        try {
            isInitialized = WindowsAudioApi.initializeCOM()
        } catch (e: Exception) {
            println("Failed to initialize COM: ${e.message}")
            fallbackToCommand = true
        }
    }
    
    /**
     * Get current microphone mute state
     * @return true if muted, false if unmuted
     */
    fun getMuteState(): Boolean {
        if (fallbackToCommand) {
            return getMuteStateViaCommand()
        }
        
        return try {
            val device = WindowsAudioApi.getDefaultCaptureDevice()
            if (device != null) {
                val volume = WindowsAudioApi.getEndpointVolume(device)
                if (volume != null) {
                    WindowsAudioApi.getMute(volume)
                } else false
            } else false
        } catch (e: Exception) {
            println("Error getting mute state: ${e.message}")
            fallbackToCommand = true
            getMuteStateViaCommand()
        }
    }
    
    /**
     * Set microphone mute state
     * @param muted true to mute, false to unmute
     * @return true if successful
     */
    fun setMute(muted: Boolean): Boolean {
        if (fallbackToCommand) {
            return setMuteViaCommand(muted)
        }
        
        return try {
            val device = WindowsAudioApi.getDefaultCaptureDevice()
            if (device != null) {
                val volume = WindowsAudioApi.getEndpointVolume(device)
                if (volume != null) {
                    WindowsAudioApi.setMute(volume, muted)
                } else false
            } else false
        } catch (e: Exception) {
            println("Error setting mute state: ${e.message}")
            fallbackToCommand = true
            setMuteViaCommand(muted)
        }
    }
    
    /**
     * Toggle microphone mute state
     * @return new mute state (true = muted)
     */
    fun toggleMute(): Boolean {
        val currentState = getMuteState()
        val newState = !currentState
        setMute(newState)
        return newState
    }
    
    /**
     * Fallback: Get mute state using PowerShell command
     */
    private fun getMuteStateViaCommand(): Boolean {
        return try {
            val process = ProcessBuilder(
                "powershell", "-Command",
                "(Get-AudioDevice -RecordingDefault).Mute"
            ).start()
            val result = process.inputStream.bufferedReader().readText().trim()
            process.waitFor()
            result.equals("True", ignoreCase = true)
        } catch (e: Exception) {
            println("Fallback command failed: ${e.message}")
            false
        }
    }
    
    /**
     * Fallback: Set mute state using PowerShell command
     */
    private fun setMuteViaCommand(muted: Boolean): Boolean {
        return try {
            // Using nircmd as fallback for muting
            val command = if (muted) {
                arrayOf("powershell", "-Command", 
                    "Set-AudioDevice -RecordingDefault -Mute \$true")
            } else {
                arrayOf("powershell", "-Command", 
                    "Set-AudioDevice -RecordingDefault -Mute \$false")
            }
            val process = ProcessBuilder(*command).start()
            process.waitFor() == 0
        } catch (e: Exception) {
            println("Fallback mute command failed: ${e.message}")
            false
        }
    }
    
    fun cleanup() {
        if (isInitialized && !fallbackToCommand) {
            try {
                WindowsAudioApi.uninitializeCOM()
            } catch (e: Exception) {
                // Ignore cleanup errors
            }
        }
    }
}
