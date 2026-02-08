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

    // Send signal to MicMute local server
    async function signalMicMute(action) {
        try {
            const response = await fetch(`${MICMUTE_SERVER}/${action}`, {
                method: 'POST',
                headers: {
                    'Content-Type': 'application/json'
                },
                body: JSON.stringify({
                    source: 'ozonetel',
                    timestamp: Date.now()
                })
            });

            if (response.ok) {
                console.log(`[MicMute Connector] Signal sent: ${action}`);
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
