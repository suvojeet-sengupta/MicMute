#pragma once

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif

#include <winsock2.h>
#include <windows.h>
#include <string>
#include <map>

// Simple HTTP Server for external integrations
// Listens on localhost:9876 for recording control signals

// Initialize the HTTP server
void InitHttpServer();

// Cleanup the HTTP server
void CleanupHttpServer();

// Check if server is running
bool IsHttpServerRunning();

// Check if extension is connected (heartbeat received recently)
bool IsExtensionConnected();

// Get time since last heartbeat in milliseconds
ULONGLONG GetTimeSinceLastHeartbeat();

// Force start/stop recording (called by HTTP endpoints)
void HttpForceStartRecording(const std::map<std::string, std::string>& metadata = {});
void HttpForceStopRecording(const std::map<std::string, std::string>& metadata = {});

