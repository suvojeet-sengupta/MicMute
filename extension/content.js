// MicMute Ozonetel Connector - Content Script
// Monitors Ozonetel agent status and signals MicMute to start/stop recording

(function () {
    'use strict';

    let currentStatus = 'unknown';
    let isRecording = false;

    // Status indicator element selector (adjust if needed)
    // Looking for the status text like "Ready" or "Busy"
    function getStatusText() {
        // Try multiple selectors to find the status element
        const selectors = [
            '[class*="status"]',
            '[class*="agent-state"]',
            '[class*="call-state"]',
            '.agent-status',
            '#agentStatus'
        ];

        // Also look for text content directly
        const pageText = document.body.innerText;

        // Check for "Busy" indicator (on call)
        if (pageText.includes('Busy') && pageText.match(/Busy\s+\d+:\d+:\d+/)) {
            return 'busy';
        }

        // Check for "Ready" indicator (idle)
        if (pageText.includes('Ready') && pageText.match(/Ready\s+\d+:\d+:\d+/)) {
            return 'ready';
        }

        // Check for "Break" indicator (agent on break -> not in call)
        if (pageText.match(/Break/i)) {
            return 'ready';
        }

        // Fallback: look for IncomingCall or call details
        if (pageText.includes('IncomingCall') || pageText.includes('Call Details:')) {
            return 'busy';
        }

        return 'unknown';
    }

    // Scrape all call details from the UI
    function scrapeCallDetails() {
        const details = {};

        // Target the Left Side Panel (usually "Call details" inside "Inbound" or just the active call container)
        // Based on user description: "left side show hota hai call details"

        // Strategy 1: Look for the specific "Call details" expansion panel or container
        // In Ozonetel Agent Toolbar, this is often in a specific left column or tab
        const callDetailsHeaders = Array.from(document.querySelectorAll('.card-header, .accordion-toggle, h4, h5'));
        let detailsContainer = null;

        for (const header of callDetailsHeaders) {
            if (header.innerText.includes('Call details') || header.innerText.includes('IncomingCall')) {
                // Find the parent or associated content container
                // Often it's the next sibling or a parent's find
                const content = header.closest('.card, .panel')?.querySelector('.card-body, .panel-body, .collapse.in, .show');
                if (content) {
                    detailsContainer = content;
                    break;
                }
                // Fallback: just use the parent if it looks like a container
                detailsContainer = header.closest('.card, .panel');
                if (detailsContainer) break;
            }
        }

        // Strategy 2: If no specific "Call details" container found, look for known key-value structures in the whole left sidebar
        if (!detailsContainer) {
            detailsContainer = document.querySelector('#left-panel, .left-pane, .sidebar-left');
        }

        // If still nothing, fallback to body (but be careful of noise)
        const range = detailsContainer || document.body;

        // Extract key-value pairs
        // Common Ozonetel formats:
        // 1. <label>Key:</label> <span>Value</span>
        // 2. <div><span class="label">Key</span> <span class="value">Value</span></div>
        // 3. Grid: <div>Key</div> <div>Value</div>

        // Let's try text parsing of lines first as it's often most robust for simple key:value lists
        const textLines = range.innerText.split('\n').map(l => l.trim()).filter(l => l.length > 0);

        for (let i = 0; i < textLines.length; i++) {
            let line = textLines[i];

            // Check for Key: Value format
            const separatorIndex = line.indexOf(':');
            if (separatorIndex > 0 && separatorIndex < line.length - 1) {
                let key = line.substring(0, separatorIndex).trim();
                let value = line.substring(separatorIndex + 1).trim();

                // Save if looks valid
                if (isValidKey(key)) details[key] = value;
            }
            // Check for Key (newline) Value format
            else if (separatorIndex === line.length - 1 || isValidKey(line.replace(':', ''))) {
                // This line is a key, next line is likely value
                let key = line.replace(':', '').trim();

                // Look ahead for value
                if (i + 1 < textLines.length) {
                    let value = textLines[i + 1];

                    // Verify next line isn't another key (heuristic)
                    if (!isValidKey(value.replace(':', ''))) {
                        details[key] = value;
                        // Skip next line since we consumed it
                        if (!value.includes(':')) i++;
                    }
                }
            }
        }

        // Helper to define what we consider a "Field Key"
        function isValidKey(k) {
            const knownKeys = ['UCID', 'Monitor UCID', 'Campaign', 'Agent ID', 'Agent Number', 'Skill Name', 'Skill', 'Call Details', 'Caller ID', 'Phone', 'Process'];
            // Check exact match or starts with known key
            return knownKeys.some(known => k.toLowerCase() === known.toLowerCase() || k.toLowerCase().startsWith(known.toLowerCase()));
        }

        // Specific Selector back-up for UCID if missed by text parsing
        if (!details['UCID']) {
            // Try finding element with ID or Class containing UCID
            const ucidEl = document.querySelector('[id*="ucid" i], [class*="ucid" i]');
            if (ucidEl) {
                // Check if it has a value attribute or text
                const val = ucidEl.value || ucidEl.innerText;
                if (val && val.length > 5) details['UCID'] = val;
            }
        }

        // Specific Selector for Campaign
        if (!details['Campaign']) {
            const campEl = document.querySelector('[id*="campaign" i], [class*="campaign" i]');
            if (campEl) details['Campaign'] = campEl.innerText.trim();
        }

        console.log('[MicMute Connector] Scraped Details:', details);
        return details;
    }

    // Ask the service worker to talk to MicMute. The content script must not
    // fetch localhost itself: it runs in the page's security context, so the
    // request is at the mercy of the page CSP and CORS.
    function signalMicMute(action) {
        const metadata = (action === 'start' || action === 'stop') ? scrapeCallDetails() : {};

        return new Promise(resolve => {
            if (!isExtensionAlive()) {
                resolve(false);
                return;
            }
            try {
                chrome.runtime.sendMessage(
                    { type: 'micmute-signal', action, metadata },
                    response => {
                        if (chrome.runtime.lastError) {
                            resolve(false);
                            return;
                        }
                        const ok = !!(response && response.ok);
                        if (ok) console.log(`[MicMute Connector] Signal sent: ${action}`, metadata);
                        resolve(ok);
                    }
                );
            } catch (error) {
                resolve(false);
            }
        });
    }

    // False once the extension is reloaded/updated - every chrome.* call from
    // this orphaned script would throw "Extension context invalidated".
    function isExtensionAlive() {
        try {
            return !!(chrome.runtime && chrome.runtime.id);
        } catch (error) {
            return false;
        }
    }

    // Check status and trigger recording
    function checkStatusAndTrigger() {
        const status = getStatusText();

        if (status === currentStatus) {
            return; // No change
        }

        console.log(`[MicMute Connector] Status changed: ${currentStatus} -> ${status}`);
        currentStatus = status;

        if (status === 'busy' && !isRecording) {
            // Call started - start recording
            signalMicMute('start').then(success => {
                if (success) {
                    isRecording = true;
                    showNotification('Recording Started', 'green');
                }
            });
        } else if (status === 'ready' && isRecording) {
            // Call ended - stop recording
            // We signal STOP *with* metadata to ensure we capture any details that might have loaded late
            signalMicMute('stop').then(success => {
                if (success) {
                    isRecording = false;
                    showNotification('Recording Saved', 'blue');
                }
            });
        }
    }

    // Visual notification on page
    function showNotification(message, color) {
        const notification = document.createElement('div');
        notification.style.cssText = `
            position: fixed;
            top: 10px;
            right: 10px;
            background: ${color === 'green' ? '#4CAF50' : '#2196F3'};
            color: white;
            padding: 10px 20px;
            border-radius: 5px;
            z-index: 99999;
            font-family: Arial, sans-serif;
            font-size: 14px;
            box-shadow: 0 2px 10px rgba(0,0,0,0.3);
            animation: fadeInOut 3s ease-in-out forwards;
        `;
        notification.textContent = `🎤 MicMute: ${message}`;
        document.body.appendChild(notification);

        setTimeout(() => notification.remove(), 3000);
    }

    // Add animation style
    const style = document.createElement('style');
    style.textContent = `
        @keyframes fadeInOut {
            0% { opacity: 0; transform: translateY(-20px); }
            15% { opacity: 1; transform: translateY(0); }
            85% { opacity: 1; transform: translateY(0); }
            100% { opacity: 0; transform: translateY(-20px); }
        }
    `;
    document.head.appendChild(style);

    // --- Link to the service worker -------------------------------------------
    // The port is what tells the worker a monitored tab is open; it is also what
    // keeps the worker alive so the 2s heartbeat to MicMute keeps flowing. If the
    // worker is torn down the port drops and we reconnect, waking it again.

    let port = null;
    let reconnectTimer = null;

    function connectToWorker() {
        if (!isExtensionAlive()) {
            teardown();
            return;
        }

        try {
            port = chrome.runtime.connect({ name: 'micmute' });
        } catch (error) {
            scheduleReconnect();
            return;
        }

        port.onMessage.addListener(message => {
            if (message && message.type === 'micmute-connection') {
                if (message.connected !== appConnected) {
                    appConnected = message.connected;
                    console.log(`[MicMute Connector] MicMute app ${appConnected ? 'detected' : 'NOT detected - is MicMute-S running?'}`);
                }
            }
        });

        port.onDisconnect.addListener(() => {
            port = null;
            scheduleReconnect();
        });
    }

    function scheduleReconnect() {
        if (reconnectTimer !== null || !isExtensionAlive()) return;
        reconnectTimer = setTimeout(() => {
            reconnectTimer = null;
            connectToWorker();
        }, 1000);
    }

    function teardown() {
        clearInterval(pollTimer);
        clearInterval(keepaliveTimer);
        observer.disconnect();
    }

    let appConnected = false;

    // Poll the DOM on a fixed cadence...
    const pollTimer = setInterval(() => {
        if (!isExtensionAlive()) {
            teardown();
            return;
        }
        checkStatusAndTrigger();
    }, 500);

    // ...and nudge the worker so it does not idle out while this tab is open.
    const keepaliveTimer = setInterval(() => {
        if (!isExtensionAlive()) {
            teardown();
            return;
        }
        if (port) {
            try {
                port.postMessage({ type: 'keepalive' });
            } catch (error) {
                port = null;
                scheduleReconnect();
            }
        } else {
            scheduleReconnect();
        }
    }, 1000);

    // Mutations on this page fire in bursts and getStatusText() reads
    // document.body.innerText, so coalesce them instead of scanning per mutation.
    let mutationTimer = null;
    const observer = new MutationObserver(() => {
        if (mutationTimer !== null) return;
        mutationTimer = setTimeout(() => {
            mutationTimer = null;
            if (isExtensionAlive()) checkStatusAndTrigger();
        }, 250);
    });

    observer.observe(document.body, {
        childList: true,
        subtree: true,
        characterData: true
    });

    // Start monitoring
    console.log('[MicMute Connector] Ozonetel integration active');
    connectToWorker();
    setTimeout(checkStatusAndTrigger, 2000);
})();
