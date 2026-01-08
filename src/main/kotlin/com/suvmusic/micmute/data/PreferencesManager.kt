package com.suvmusic.micmute.data

import java.util.prefs.Preferences

/**
 * Manages application preferences using Java Preferences API
 */
class PreferencesManager {
    
    private val prefs: Preferences = Preferences.userNodeForPackage(PreferencesManager::class.java)
    
    companion object {
        private const val KEY_SETUP_COMPLETE = "setup_complete"
        private const val KEY_START_MINIMIZED = "start_minimized"
        private const val KEY_SHOW_NOTIFICATIONS = "show_notifications"
    }
    
    /**
     * Check if initial setup has been completed
     */
    fun isSetupComplete(): Boolean {
        return prefs.getBoolean(KEY_SETUP_COMPLETE, false)
    }
    
    /**
     * Mark setup as complete
     */
    fun setSetupComplete(complete: Boolean) {
        prefs.putBoolean(KEY_SETUP_COMPLETE, complete)
    }
    
    /**
     * Check if app should start minimized to tray
     */
    fun shouldStartMinimized(): Boolean {
        return prefs.getBoolean(KEY_START_MINIMIZED, true)
    }
    
    /**
     * Set whether app should start minimized
     */
    fun setStartMinimized(minimized: Boolean) {
        prefs.putBoolean(KEY_START_MINIMIZED, minimized)
    }
    
    /**
     * Check if notifications should be shown
     */
    fun shouldShowNotifications(): Boolean {
        return prefs.getBoolean(KEY_SHOW_NOTIFICATIONS, true)
    }
    
    /**
     * Set whether to show notifications
     */
    fun setShowNotifications(show: Boolean) {
        prefs.putBoolean(KEY_SHOW_NOTIFICATIONS, show)
    }
    
    /**
     * Reset all preferences to defaults
     */
    fun resetAll() {
        prefs.clear()
    }
}
