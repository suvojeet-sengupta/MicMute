#pragma once

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif

#include <winsock2.h>
#include <windows.h>
#include <string>

// Simple HTTP Server for external integrations
// Listens on localhost:9876 for recording control signals

// Initialize the HTTP server
void InitHttpServer();

// Cleanup the HTTP server
void CleanupHttpServer();

// Check if server is running
bool IsHttpServerRunning();
