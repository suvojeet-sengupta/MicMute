#include "network/http_server.h"
#include "audio/call_recorder.h"
#include "audio/recorder.h"
#include <ws2tcpip.h>
#include <thread>
#include "core/globals.h"
#include <atomic>
#include <cctype>
#include <cstdlib>
#include <string>

#pragma comment(lib, "ws2_32.lib")

constexpr int HTTP_PORT = 9876;
constexpr size_t BUFFER_SIZE = 16384; // Increased buffer for metadata

static SOCKET serverSocket = INVALID_SOCKET;
static std::atomic<bool> serverRunning(false);
static std::thread serverThread;

#include <map>
#include <vector>

// Extension connection tracking
static std::atomic<ULONGLONG> lastHeartbeatTime(0);
static std::atomic<bool> extensionConnected(false);
// The extension heartbeats every 2s from its service worker, which Chrome is
// free to tear down and respawn between beats. Anything near 5s produced false
// disconnects that cut live recordings short (see CallAutoRecorder::Poll).
#define HEARTBEAT_TIMEOUT_MS 15000  // 15 seconds without heartbeat = disconnected

// Very basic JSON parser for flat string key-value pairs
// e.g. {"source":"ozonetel", "metadata":{"key":"value"}}
std::map<std::string, std::string> ParseMetadataFromJSON(const std::string& json) {
    std::map<std::string, std::string> metadata;
    
    // Find "metadata" key
    size_t metadataPos = json.find("\"metadata\"");
    if (metadataPos == std::string::npos) return metadata;
    
    // Find opening brace of metadata object
    size_t startObj = json.find('{', metadataPos);
    if (startObj == std::string::npos) return metadata;
    
    // Naive parsing: find "key":"value" pairs inside the object
    // This assumes no nested objects in metadata and standard formatting
    size_t curr = startObj + 1;
    while (curr < json.length()) {
        // Find key start
        size_t keyStart = json.find('"', curr);
        if (keyStart == std::string::npos) break;
        
        // Check if we hit end of object before key
        size_t endObj = json.find('}', curr);
        if (endObj != std::string::npos && endObj < keyStart) break;
        
        size_t keyEnd = json.find('"', keyStart + 1);
        if (keyEnd == std::string::npos) break;
        
        std::string key = json.substr(keyStart + 1, keyEnd - keyStart - 1);
        
        // Find colon
        size_t colon = json.find(':', keyEnd);
        if (colon == std::string::npos) break;
        
        // Find value
        size_t valStart = json.find('"', colon);
        if (valStart == std::string::npos) break;
        
        size_t valEnd = json.find('"', valStart + 1);
        if (valEnd == std::string::npos) break;
        
        std::string val = json.substr(valStart + 1, valEnd - valStart - 1);
        
        // Unescape minimal chars if needed (basic)
        // For now, accept as is
        
        metadata[key] = val;
        
        curr = valEnd + 1;
    }
    
    return metadata;
}

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

// Case-insensitive Content-Length lookup within the header block
static size_t ParseContentLength(const std::string& request, size_t headerEnd) {
    std::string headers = request.substr(0, headerEnd);
    std::string lower = headers;
    for (char& c : lower) c = (char)tolower((unsigned char)c);

    size_t pos = lower.find("content-length:");
    if (pos == std::string::npos) return 0;

    long declared = strtol(headers.c_str() + pos + 15, nullptr, 10);
    if (declared < 0) return 0;
    return (size_t)declared;
}

// Handle incoming request
void HandleRequest(SOCKET client) {
    // The accept loop is single threaded, so never block on a stalled client.
    DWORD recvTimeout = 2000;
    setsockopt(client, SOL_SOCKET, SO_RCVTIMEO, (char*)&recvTimeout, (int)sizeof(recvTimeout));

    // Read until the headers are complete and the declared body has arrived.
    // A single recv() would drop the call metadata whenever Chrome splits the
    // POST across TCP segments.
    std::string request;
    char chunk[4096];
    size_t headerEnd = std::string::npos;
    size_t contentLength = 0;

    while (request.size() < BUFFER_SIZE) {
        headerEnd = request.find("\r\n\r\n");
        if (headerEnd != std::string::npos) {
            contentLength = ParseContentLength(request, headerEnd);
            if (request.size() - (headerEnd + 4) >= contentLength) break;
        }

        int bytesReceived = recv(client, chunk, sizeof(chunk), 0);
        if (bytesReceived <= 0) break;
        request.append(chunk, bytesReceived);
    }

    if (request.empty()) {
        closesocket(client);
        return;
    }

    // Parse HTTP method and path
    char method[16], path[256];
    if (sscanf_s(request.c_str(), "%15s %255s", method, (unsigned)_countof(method), path, (unsigned)_countof(path)) != 2) {
        SendResponse(client, 400, "Bad Request", "{\"error\":\"malformed request\"}");
        closesocket(client);
        return;
    }

    // Handle CORS preflight
    if (strcmp(method, "OPTIONS") == 0) {
        SendResponse(client, 200, "OK", "{}");
        closesocket(client);
        return;
    }

    std::string bodyStr;
    if (headerEnd != std::string::npos) {
        size_t bodyStart = headerEnd + 4;
        size_t available = request.size() - bodyStart;
        size_t bodyLen = (contentLength > 0 && contentLength < available) ? contentLength : available;
        bodyStr = request.substr(bodyStart, bodyLen);
    }

    // Handle POST requests
    if (strcmp(method, "POST") == 0) {
        if (strcmp(path, "/ping") == 0) {
            // Heartbeat from extension - update connection status
            lastHeartbeatTime = GetTickCount64();
            extensionConnected = true;
            SendResponse(client, 200, "OK", "{\"status\":\"pong\",\"connected\":true}");
        }
        else if (strcmp(path, "/start") == 0) {
            // Start recording via extension signal
            lastHeartbeatTime = GetTickCount64();
            extensionConnected = true;
            
            std::map<std::string, std::string> metadata = ParseMetadataFromJSON(bodyStr);
            HttpForceStartRecording(metadata);
            
            SendResponse(client, 200, "OK", "{\"status\":\"recording_started\"}");
        }
        else if (strcmp(path, "/stop") == 0) {
            // Stop recording and save via extension signal
            lastHeartbeatTime = GetTickCount64();
            extensionConnected = true;
            
            std::map<std::string, std::string> metadata = ParseMetadataFromJSON(bodyStr);
            HttpForceStopRecording(metadata);
            
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
        
        int result = select(0, &readSet, nullptr, nullptr, &timeout);
        
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
    InetPton(AF_INET, "127.0.0.1", &serverAddr.sin_addr);
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
    ULONGLONG now = GetTickCount64();
    ULONGLONG last = lastHeartbeatTime.load();
    if (last > 0 && (now - last) > (ULONGLONG)HEARTBEAT_TIMEOUT_MS) {
        extensionConnected = false;
        return false;
    }
    return true;
}

ULONGLONG GetTimeSinceLastHeartbeat() {
    ULONGLONG last = lastHeartbeatTime.load();
    if (last == 0) return _UI64_MAX;
    return GetTickCount64() - last;
}

void HttpForceStartRecording(const std::map<std::string, std::string>& metadata) {
    // Current "Beep on Call" feature
    // Play beep if enabled, regardless of whether recording is enabled or success
    if (beepOnCall) {
        // High pitch beep (750Hz) for 300ms 
        Beep(750, 300);
    }

    if (!g_CallRecorder) return;
    
    // Force the recorder to start recording immediately
    g_CallRecorder->ForceStartRecording(metadata);
}

void HttpForceStopRecording(const std::map<std::string, std::string>& metadata) {
    if (!g_CallRecorder) return;
    
    // Force stop and save
    g_CallRecorder->ForceStopRecording(metadata);
}

