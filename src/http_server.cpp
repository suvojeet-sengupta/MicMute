#include "http_server.h"
#include "call_recorder.h"
#include "recorder.h"
#include <ws2tcpip.h>
#include <thread>
#include <atomic>

#pragma comment(lib, "ws2_32.lib")

#define HTTP_PORT 9876
#define BUFFER_SIZE 4096

static SOCKET serverSocket = INVALID_SOCKET;
static std::atomic<bool> serverRunning(false);
static std::thread serverThread;

// Extension connection tracking
static DWORD lastHeartbeatTime = 0;
static std::atomic<bool> extensionConnected(false);
#define HEARTBEAT_TIMEOUT_MS 5000  // 5 seconds without heartbeat = disconnected

// Send HTTP response
void SendResponse(SOCKET client, int statusCode, const char* statusText, const char* body) {
    char response[1024];
    snprintf(response, sizeof(response),
        "HTTP/1.1 %d %s\r\n"
        "Content-Type: application/json\r\n"
        "Access-Control-Allow-Origin: *\r\n"
        "Access-Control-Allow-Methods: POST, OPTIONS\r\n"
        "Access-Control-Allow-Headers: Content-Type\r\n"
        "Content-Length: %zu\r\n"
        "\r\n"
        "%s",
        statusCode, statusText, strlen(body), body);
    
    send(client, response, (int)strlen(response), 0);
}

// Handle incoming request
void HandleRequest(SOCKET client) {
    char buffer[BUFFER_SIZE];
    int bytesReceived = recv(client, buffer, BUFFER_SIZE - 1, 0);
    
    if (bytesReceived <= 0) {
        closesocket(client);
        return;
    }
    
    buffer[bytesReceived] = '\0';
    
    // Parse HTTP method and path
    char method[16], path[256];
    sscanf(buffer, "%15s %255s", method, path);
    
    // Handle CORS preflight
    if (strcmp(method, "OPTIONS") == 0) {
        SendResponse(client, 200, "OK", "{}");
        closesocket(client);
        return;
    }
    
    // Handle POST requests
    if (strcmp(method, "POST") == 0) {
        if (strcmp(path, "/ping") == 0) {
            // Heartbeat from extension - update connection status
            lastHeartbeatTime = GetTickCount();
            extensionConnected = true;
            SendResponse(client, 200, "OK", "{\"status\":\"pong\",\"connected\":true}");
        }
        else if (strcmp(path, "/start") == 0) {
            // Start recording via extension signal
            lastHeartbeatTime = GetTickCount();
            extensionConnected = true;
            HttpForceStartRecording();
            SendResponse(client, 200, "OK", "{\"status\":\"recording_started\"}");
        }
        else if (strcmp(path, "/stop") == 0) {
            // Stop recording and save via extension signal
            lastHeartbeatTime = GetTickCount();
            extensionConnected = true;
            HttpForceStopRecording();
            SendResponse(client, 200, "OK", "{\"status\":\"recording_stopped\"}");
        }
        else if (strcmp(path, "/status") == 0) {
            // Get current status
            const char* status = "idle";
            if (g_CallRecorder) {
                if (g_CallRecorder->GetState() == CallAutoRecorder::State::RECORDING) {
                    status = "recording";
                } else if (g_CallRecorder->IsEnabled()) {
                    status = "waiting";
                }
            }
            char body[128];
            snprintf(body, sizeof(body), "{\"status\":\"%s\",\"connected\":%s}", 
                     status, extensionConnected ? "true" : "false");
            SendResponse(client, 200, "OK", body);
        }
        else {
            SendResponse(client, 404, "Not Found", "{\"error\":\"unknown endpoint\"}");
        }
    } else {
        SendResponse(client, 405, "Method Not Allowed", "{\"error\":\"use POST\"}");
    }
    
    closesocket(client);
}

// Server thread function
void ServerThreadFunc() {
    while (serverRunning) {
        fd_set readSet;
        FD_ZERO(&readSet);
        FD_SET(serverSocket, &readSet);
        
        timeval timeout;
        timeout.tv_sec = 1;
        timeout.tv_usec = 0;
        
        int result = select(0, &readSet, NULL, NULL, &timeout);
        
        if (result > 0 && FD_ISSET(serverSocket, &readSet)) {
            sockaddr_in clientAddr;
            int clientAddrLen = sizeof(clientAddr);
            SOCKET clientSocket = accept(serverSocket, (sockaddr*)&clientAddr, &clientAddrLen);
            
            if (clientSocket != INVALID_SOCKET) {
                HandleRequest(clientSocket);
            }
        }
    }
}

void InitHttpServer() {
    // Initialize Winsock
    WSADATA wsaData;
    if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) {
        return;
    }
    
    // Create socket
    serverSocket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (serverSocket == INVALID_SOCKET) {
        WSACleanup();
        return;
    }
    
    // Allow address reuse
    int opt = 1;
    setsockopt(serverSocket, SOL_SOCKET, SO_REUSEADDR, (char*)&opt, sizeof(opt));
    
    // Bind to localhost:9876
    sockaddr_in serverAddr;
    serverAddr.sin_family = AF_INET;
    serverAddr.sin_addr.s_addr = inet_addr("127.0.0.1");
    serverAddr.sin_port = htons(HTTP_PORT);
    
    if (bind(serverSocket, (sockaddr*)&serverAddr, sizeof(serverAddr)) == SOCKET_ERROR) {
        closesocket(serverSocket);
        WSACleanup();
        return;
    }
    
    // Listen
    if (listen(serverSocket, SOMAXCONN) == SOCKET_ERROR) {
        closesocket(serverSocket);
        WSACleanup();
        return;
    }
    
    // Start server thread
    serverRunning = true;
    serverThread = std::thread(ServerThreadFunc);
}

void CleanupHttpServer() {
    serverRunning = false;
    
    if (serverThread.joinable()) {
        serverThread.join();
    }
    
    if (serverSocket != INVALID_SOCKET) {
        closesocket(serverSocket);
        serverSocket = INVALID_SOCKET;
    }
    
    WSACleanup();
}

bool IsHttpServerRunning() {
    return serverRunning && serverSocket != INVALID_SOCKET;
}

bool IsExtensionConnected() {
    if (!extensionConnected) return false;
    
    // Check if heartbeat timed out
    DWORD now = GetTickCount();
    if (lastHeartbeatTime > 0 && (now - lastHeartbeatTime) > HEARTBEAT_TIMEOUT_MS) {
        extensionConnected = false;
        return false;
    }
    return true;
}

DWORD GetTimeSinceLastHeartbeat() {
    if (lastHeartbeatTime == 0) return UINT_MAX;
    return GetTickCount() - lastHeartbeatTime;
}

void HttpForceStartRecording() {
    if (!g_CallRecorder) return;
    
    // Force the recorder to start recording immediately
    g_CallRecorder->ForceStartRecording();
}

void HttpForceStopRecording() {
    if (!g_CallRecorder) return;
    
    // Force stop and save
    g_CallRecorder->ForceStopRecording();
}

