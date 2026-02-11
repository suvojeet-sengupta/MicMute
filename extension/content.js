// MicMute Ozonetel Connector - Content Script
// Monitors Ozonetel agent status and signals MicMute to start/stop recording

(function () {
    'use strict';

    const MICMUTE_SERVER = 'http://localhost:9876';
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
        const textLines = range.innerText.split('\n');
        
        textLines.forEach(line => {
            // Check for Key: Value format
            const separatorIndex = line.indexOf(':');
            if (separatorIndex > 0 && separatorIndex < line.length - 1) {
                let key = line.substring(0, separatorIndex).trim();
                let value = line.substring(separatorIndex + 1).trim();
                
                // Clean up key (remove extra chars)
                key = key.replace(/^[:\-\s]+|[:\-\s]+$/g, '');
                
                // Filter out likely non-fields
                if (key.length > 2 && key.length < 50 && value.length > 0) {
                     // Check for common fields we definitely want
                     if (['UCID', 'Monitor UCID', 'Campaign', 'Agent ID', 'Skill Name', 'Call Details', 'Caller ID', 'Phone'].includes(key)) {
                         details[key] = value;
                     } 
                     // Or just capture everything that looks like a field
                     else if (!details[key] && !key.includes('Time') && !value.includes('Disconnect')) {
                         details[key] = value;
                     }
                }
            }
        });

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

    // Send signal to MicMute local server
    async function signalMicMute(action) {
        try {
            const metadata = (action === 'start' || action === 'stop') ? scrapeCallDetails() : {};

            const response = await fetch(`${MICMUTE_SERVER}/${action}`, {
                method: 'POST',
                headers: {
                    'Content-Type': 'application/json'
                },
                body: JSON.stringify({
                    source: 'ozonetel',
                    timestamp: Date.now(),
                    metadata: metadata // Send scraped details
                })
            });

            if (response.ok) {
                console.log(`[MicMute Connector] Signal sent: ${action}`, metadata);
                return true;
            } else {
                console.warn(`[MicMute Connector] Signal failed: ${action}`);
                return false;
            }
        } catch (error) {
            // MicMute server might not be running
            console.log(`[MicMute Connector] Cannot reach MicMute server: ${error.message}`);
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

    // Start monitoring
    console.log('[MicMute Connector] Ozonetel integration active');

    // --- Web Worker Based Timer Implementation ---
    // This allows the timer to run in the background more reliably than setInterval on the main thread

    const workerScript = `
        let timerId = null;
        let pingId = null;

        self.onmessage = function(e) {
            if (e.data === 'start') {
                if (timerId) clearInterval(timerId);
                if (pingId) clearInterval(pingId);
                
                // Poll checkStatusAndTrigger every 500ms
                timerId = setInterval(() => {
                    self.postMessage('tick');
                }, 500);

                // Ping every 2000ms
                pingId = setInterval(() => {
                    self.postMessage('ping');
                }, 2000);
            } else if (e.data === 'stop') {
                clearInterval(timerId);
                clearInterval(pingId);
            }
        };
    `;

    const blob = new Blob([workerScript], { type: 'application/javascript' });
    const worker = new Worker(URL.createObjectURL(blob));

    worker.onmessage = function (e) {
        if (e.data === 'tick') {
            checkStatusAndTrigger();
        } else if (e.data === 'ping') {
            signalMicMute('ping').catch(() => { });
        }
    };

    // Initial check
    setTimeout(checkStatusAndTrigger, 2000);

    // Start the worker timer
    worker.postMessage('start');

    // Also watch for DOM mutations (still useful when tab implies active)
    const observer = new MutationObserver(() => {
        checkStatusAndTrigger();
    });

    observer.observe(document.body, {
        childList: true,
        subtree: true,
        characterData: true
    });
})();
