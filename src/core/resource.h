#ifndef RESOURCE_H
#define RESOURCE_H

#define IDI_APP_ICON 101
#define IDI_MIC_ON   102
#define IDI_MIC_OFF  103

#define ID_TRAY_APP_ICON 1001
#define ID_TRAY_EXIT     1002
#define ID_TRAY_OPEN     1003
#define ID_FORCE_MUTE    1004
#define ID_RUN_STARTUP   1005
#define ID_STATUS_LABEL  1006
#define ID_SHOW_OVERLAY  1007
#define ID_SHOW_METER    1008
#define ID_SHOW_RECORDER 1009
#define ID_SHOW_NOTIFICATIONS 1011
#define ID_TAB_CONTROL   1010
#define ID_AUTO_RECORD_CALLS 1012
#define ID_BEEP_ON_CALL      1013

// Hide/Unhide tab controls
#define ID_HIDE_MUTE_BTN     1020
#define ID_HIDE_VOICE_METER  1021
#define ID_HIDE_REC_STATUS   1022
#define ID_HIDE_CALL_STATS   1023
#define ID_HIDE_MANUAL_REC   1024

// Shape & Size tab controls
#define ID_SIZE_COMPACT      1030
#define ID_SIZE_NORMAL       1031
#define ID_SIZE_WIDE         1032

// Auto-delete combo
#define ID_AUTO_DELETE_COMBO  1040
#define ID_AUTO_DELETE_LABEL  1041

#define WM_TRAYICON (WM_USER + 1)
#define WM_APP_MUTE_CHANGED (WM_APP + 100)

#endif
