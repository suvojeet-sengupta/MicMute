#include "network/updater.h"
#include "core/globals.h"
#include <winhttp.h>
#include <iostream>
#include <vector>
#include <thread>
#include <atomic>
#include <fstream>
#include <sstream>
#include <filesystem>

#pragma comment(lib, "winhttp.lib")
#pragma comment(lib, "version.lib")

namespace AutoUpdater {

    static HWND hParent = nullptr;
    static std::atomic<UpdateStatus> currentStatus = UpdateStatus::None;
    static bool isCheckSilent = false;
    
    // Simple JSON Value parser (minimal for extraction)
    // We only need tag_name, body, assets array
    struct ReleaseInfo {
        std::string tagName;
        std::string body;
        std::string installerUrl;
        std::string portableUrl;
        bool valid = false;
    };

    // Forward declarations
    std::string MakeHttpRequest(const std::wstring& domain, const std::wstring& path);
    ReleaseInfo ParseGitHubRelease(const std::string& json);
    bool IsVersionNewer(const std::string& current, const std::string& latest);
    bool IsPortableMode();

    void Init(HWND hParentWnd) {
        hParent = hParentWnd;
    }

    void CheckForUpdateAsync(bool silent) {
        if (currentStatus == UpdateStatus::Checking) return;
        
        isCheckSilent = silent;
        currentStatus = UpdateStatus::Checking;
        
        std::thread([]() {
            // GitHub API
            std::wstring domain = L"api.github.com";
            std::wstring path = L"/repos/suvojeet-sengupta/MicMute/releases/latest";
            
            std::string json = MakeHttpRequest(domain, path);
            
            if (json.empty()) {
                currentStatus = UpdateStatus::Error;
                if (!isCheckSilent) {
                    MessageBox(hParent, "Failed to check for updates. Please check your internet connection.", "Update Check", MB_ICONERROR);
                }
                return;
            }
            
            ReleaseInfo info = ParseGitHubRelease(json);
            
            if (!info.valid) {
                 currentStatus = UpdateStatus::Error;
                 if (!isCheckSilent) {
                     MessageBox(hParent, "Could not parse update information.", "Update Check", MB_ICONERROR);
                 }
                 return;
            }
            
            if (IsVersionNewer(APP_VERSION, info.tagName)) {
                currentStatus = UpdateStatus::UpdateAvailable;
                
                // Construct message
                std::string msg = "A new version of MicMute is available!\n\n";
                msg += "Current: " + std::string(APP_VERSION) + "\n";
                msg += "Latest: " + info.tagName + "\n\n";
                msg += "Release Notes:\n" + info.body + "\n\n";
                msg += "Do you want to update now?";
                
                int result = MessageBox(hParent, msg.c_str(), "Update Available", MB_YESNO | MB_ICONINFORMATION);
                
                if (result == IDYES) {
                    // Determine which file to download
                    bool portable = IsPortableMode();
                    std::string url = portable ? info.portableUrl : info.installerUrl;
                    
                    if (url.empty()) {
                         // Fallback to the other if one is missing
                         url = info.installerUrl.empty() ? info.portableUrl : info.installerUrl;
                    }
                    
                    if (url.empty()) {
                        MessageBox(hParent, "No suitable update file found in the release.", "Update Error", MB_ICONERROR);
                        return;
                    }
                    
                    // Trigger update (simple blocking for now, ideally shows progress dialog)
                    // For now, let's just create a thread detached to download/run
                    PerformUpdate(url, !portable, [](int p){}); 
                }
            } else {
                currentStatus = UpdateStatus::UpToDate;
                if (!isCheckSilent) {
                    MessageBox(hParent, "You are using the latest version.", "Update Check", MB_ICONINFORMATION);
                }
            }
            
        }).detach();
    }
    
    // --- Helper Implementation ---

    // Basic WinHTTP Request
    std::string MakeHttpRequest(const std::wstring& domain, const std::wstring& path) {
        HINTERNET hSession = WinHttpOpen(L"MicMute-Updater/1.0",
                                         WINHTTP_ACCESS_TYPE_DEFAULT_PROXY,
                                         WINHTTP_NO_PROXY_NAME,
                                         WINHTTP_NO_PROXY_BYPASS, 0);
        if (!hSession) return "";

        HINTERNET hConnect = WinHttpConnect(hSession, domain.c_str(), INTERNET_DEFAULT_HTTPS_PORT, 0);
        if (!hConnect) { WinHttpCloseHandle(hSession); return ""; }

        HINTERNET hRequest = WinHttpOpenRequest(hConnect, L"GET", path.c_str(),
                                                NULL, WINHTTP_NO_REFERER,
                                                WINHTTP_DEFAULT_ACCEPT_TYPES,
                                                WINHTTP_FLAG_SECURE);
        if (!hRequest) { WinHttpCloseHandle(hConnect); WinHttpCloseHandle(hSession); return ""; }

        bool result = WinHttpSendRequest(hRequest, WINHTTP_NO_ADDITIONAL_HEADERS, 0,
                                         WINHTTP_NO_REQUEST_DATA, 0, 0, 0);
        
        if (result) {
            result = WinHttpReceiveResponse(hRequest, NULL);
        }
        
        std::string response;
        if (result) {
            DWORD dwSize = 0;
            DWORD dwDownloaded = 0;
            do {
                dwSize = 0;
                if (!WinHttpQueryDataAvailable(hRequest, &dwSize)) break;
                if (!dwSize) break;
                
                std::vector<char> buffer(dwSize + 1);
                if (WinHttpReadData(hRequest, buffer.data(), dwSize, &dwDownloaded)) {
                     buffer[dwDownloaded] = 0;
                     response.append(buffer.data());
                }
            } while (dwSize > 0);
        }

        WinHttpCloseHandle(hRequest);
        WinHttpCloseHandle(hConnect);
        WinHttpCloseHandle(hSession);
        
        return response;
    }
    
    // Very naive JSON parser to avoid external deps
    ReleaseInfo ParseGitHubRelease(const std::string& json) {
        ReleaseInfo info;
        
        // Find tag_name
        size_t tagPos = json.find("\"tag_name\"");
        if (tagPos != std::string::npos) {
            size_t start = json.find("\"", tagPos + 10) + 1;
            size_t end = json.find("\"", start);
            info.tagName = json.substr(start, end - start);
        }
        
        // Find body
        size_t bodyPos = json.find("\"body\"");
        if (bodyPos != std::string::npos) {
            size_t start = json.find("\"", bodyPos + 6) + 1;
            // Handle escaped quotes? For now just find next "
            // Ideally we need a real parser, but typically body ends with "
            // A simple hack: search for next `",` or `"\n` or `}`
            size_t end = json.find("\"", start); 
            // This is brittle for "body", so let's check assets first which is safer
            // Actually, for "body", it might contain escaped quotes. 
            // GitHub API returns escaped newlines too. 
            // Let's assume standard formatting. 
            info.body = "See GitHub release for details."; // Placeholder if parsing fails
             // Try to extract a bit if possible
             std::string chunk = json.substr(start, 200);
             if (chunk.length() > 0) info.body = chunk + "...";
        }
        
        // Find assets
        // We look for .exe (installer) and .zip (portable)
        // Check for "browser_download_url"
        size_t currentPos = 0;
        while (true) {
            size_t urlPos = json.find("\"browser_download_url\"", currentPos);
            if (urlPos == std::string::npos) break;
            
            size_t start = json.find("\"", urlPos + 22) + 1;
            size_t end = json.find("\"", start);
            std::string url = json.substr(start, end - start);
            
            if (url.find(".exe") != std::string::npos && url.find("setup") != std::string::npos) {
                info.installerUrl = url;
            } else if (url.find("portable") != std::string::npos || url.find(".zip") != std::string::npos) {
                 info.portableUrl = url;
            } else if (url.find(".exe") != std::string::npos) {
                // Fallback: any exe might be it
                if (info.installerUrl.empty()) info.installerUrl = url;
            }
            
            currentPos = end;
        }
        
        info.valid = !info.tagName.empty();
        return info;
    }

    bool IsVersionNewer(const std::string& current, const std::string& latest) {
        // Simple string compare for now, e.g. v1.1.0 vs v1.2.0
        // Removing 'v' prefix if exists
        std::string v1 = current;
        std::string v2 = latest;
        if (v1.size() > 0 && v1[0] == 'v') v1 = v1.substr(1);
        if (v2.size() > 0 && v2[0] == 'v') v2 = v2.substr(1);
        
        // Naive lexicographical compare is often enough for standard SemVer
        // 1.10.0 > 1.9.0? String compare "1.10.0" < "1.9.0" is false (1 < 1, . < .)
        // Actually string compare fails for "1.10" vs "1.9" ("1" == "1", "." == ".", "1" < "9" -> true?? No '1' < '9')
        // So "1.10" comes BEFORE "1.9" in ASCII? '1' is 49, '9' is 57. So 1.10 < 1.9. Wait.
        // We need integer splitting.
        
        int a1=0, b1=0, c1=0;
        int a2=0, b2=0, c2=0;
        sscanf_s(v1.c_str(), "%d.%d.%d", &a1, &b1, &c1);
        sscanf_s(v2.c_str(), "%d.%d.%d", &a2, &b2, &c2);
        
        if (a2 > a1) return true;
        if (a2 == a1 && b2 > b1) return true;
        if (a2 == a1 && b2 == b1 && c2 > c1) return true;
        
        return false;
    }

    bool IsPortableMode() {
        // Logic: Check if we are in "Program Files" or have unins000.exe
        char path[MAX_PATH];
        GetModuleFileName(NULL, path, MAX_PATH);
        std::string exePath = path;
        std::string dir = exePath.substr(0, exePath.find_last_of("\\/"));
        
        std::string unins = dir + "\\unins000.exe";
        if (std::filesystem::exists(unins)) {
            return false;
        }
        return true;
    }
    
    // Download File Helper
    bool DownloadFile(const std::string& url, const std::string& destPath) {
        std::wstring wUrl(url.begin(), url.end());
        
        URL_COMPONENTS urlComp = {0};
        urlComp.dwStructSize = sizeof(urlComp);
        urlComp.dwHostNameLength = 1;
        urlComp.dwUrlPathLength = 1;
        urlComp.dwSchemeLength = 1;
        
        if (!WinHttpCrackUrl(wUrl.c_str(), (DWORD)wUrl.length(), 0, &urlComp)) return false;
        
        std::wstring hostName(urlComp.lpszHostName, urlComp.dwHostNameLength);
        std::wstring urlPath(urlComp.lpszUrlPath, urlComp.dwUrlPathLength);
        
        HINTERNET hSession = WinHttpOpen(L"MicMute-Updater/1.0", WINHTTP_ACCESS_TYPE_DEFAULT_PROXY, WINHTTP_NO_PROXY_NAME, WINHTTP_NO_PROXY_BYPASS, 0);
        if (!hSession) return false;
        
        HINTERNET hConnect = WinHttpConnect(hSession, hostName.c_str(), INTERNET_DEFAULT_HTTPS_PORT, 0);
        if (!hConnect) { WinHttpCloseHandle(hSession); return false; }
        
        HINTERNET hRequest = WinHttpOpenRequest(hConnect, L"GET", urlPath.c_str(), NULL, WINHTTP_NO_REFERER, WINHTTP_DEFAULT_ACCEPT_TYPES, WINHTTP_FLAG_SECURE);
        if (!hRequest) { WinHttpCloseHandle(hConnect); WinHttpCloseHandle(hSession); return false; }
        
        if (!WinHttpSendRequest(hRequest, WINHTTP_NO_ADDITIONAL_HEADERS, 0, WINHTTP_NO_REQUEST_DATA, 0, 0, 0)) {
            WinHttpCloseHandle(hRequest); WinHttpCloseHandle(hConnect); WinHttpCloseHandle(hSession); return false;
        }
        
        if (!WinHttpReceiveResponse(hRequest, NULL)) {
            WinHttpCloseHandle(hRequest); WinHttpCloseHandle(hConnect); WinHttpCloseHandle(hSession); return false;
        }
        
        std::ofstream outfile(destPath, std::ios::binary);
        if (!outfile.is_open()) {
             WinHttpCloseHandle(hRequest); WinHttpCloseHandle(hConnect); WinHttpCloseHandle(hSession); return false;
        }
        
        DWORD dwSize = 0;
        DWORD dwDownloaded = 0;
        do {
            dwSize = 0;
            if (!WinHttpQueryDataAvailable(hRequest, &dwSize)) break;
            if (!dwSize) break;
            
            std::vector<char> buffer(dwSize);
            if (WinHttpReadData(hRequest, buffer.data(), dwSize, &dwDownloaded)) {
                 outfile.write(buffer.data(), dwDownloaded);
            }
        } while (dwSize > 0);
        
        outfile.close();
        WinHttpCloseHandle(hRequest);
        WinHttpCloseHandle(hConnect);
        WinHttpCloseHandle(hSession);
        
        return true;
    }

    void PerformUpdate(const std::string& downloadUrl, bool isInstaller, ProgressCallback onProgress) {
        // Create temp file path
        char tempPath[MAX_PATH];
        GetTempPath(MAX_PATH, tempPath);
        std::string updateFile = std::string(tempPath) + (isInstaller ? "MicMuteUpdate.exe" : "MicMuteUpdate.zip");
        
        // Get our own exe path for restart logic
        char exePath[MAX_PATH];
        GetModuleFileName(NULL, exePath, MAX_PATH);
        std::string currentExe = exePath;
        std::string currentDir = std::filesystem::path(exePath).parent_path().string();
        
        if (DownloadFile(downloadUrl, updateFile)) {
            if (isInstaller) {
                // Create a batch script that waits for installer to finish, then restarts MicMute
                std::string batPath = std::string(tempPath) + "micmute_update_restart.bat";
                std::ofstream bat(batPath);
                bat << "@echo off\n";
                bat << "echo Updating MicMute-S...\n";
                // Run installer silently and wait for it to finish
                bat << "\"" << updateFile << "\" /SILENT /SP- /CLOSEAPPLICATIONS /RESTARTAPPLICATIONS\n";
                // Wait a moment for the installer to fully complete
                bat << "timeout /t 3 /nobreak > NUL\n";
                // Restart MicMute from its install location
                bat << "start \"\" \"" << currentExe << "\"\n";
                bat << "del \"%~f0\" & exit\n";
                bat.close();
                
                ShellExecute(NULL, "open", batPath.c_str(), NULL, NULL, SW_HIDE);
                PostQuitMessage(0);
            } else {
                // Portable update logic
                std::string batPath = currentDir + "\\update.bat";
                std::ofstream bat(batPath);
                
                bat << "@echo off\n";
                bat << "timeout /t 2 /nobreak > NUL\n"; // Wait for app close
                bat << "powershell -command \"Expand-Archive -Path '" << updateFile << "' -DestinationPath '" << currentDir << "' -Force\"\n";
                bat << "start \"\" \"" << exePath << "\"\n";
                bat << "del \"%~f0\" & exit\n";
                bat.close();
                
                // Run batch and exit
                ShellExecute(NULL, "open", batPath.c_str(), NULL, NULL, SW_HIDE);
                PostQuitMessage(0);
            }
        } else {
            MessageBox(hParent, "Download failed.", "Update Error", MB_ICONERROR);
        }
    }

    UpdateStatus GetCurrentStatus() { return currentStatus; }
}
