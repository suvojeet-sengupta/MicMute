// MicMute Ozonetel Connector - Background service worker
//
// All talking to the local MicMute app happens here, never in the content
// script. Two reasons:
//   1. The Ozonetel page ships "worker-src 'self'", so the blob-based timer
//      the content script used to create was blocked outright - no ticks, no
//      heartbeat, and the app never saw the extension.
//   2. Timers on a hidden tab get throttled to once a minute; a service
//      worker woken by ports/alarms is not.

const MICMUTE_HOSTS = ['http://127.0.0.1:9876', 'http://localhost:9876'];
const PING_INTERVAL_MS = 2000;
const REQUEST_TIMEOUT_MS = 3000;
const KEEPALIVE_ALARM = 'micmute-keepalive';

// Host that last answered. 'localhost' can resolve to ::1 on Windows while the
// app only listens on 127.0.0.1, so we try both and remember the winner.
let serverBase = MICMUTE_HOSTS[0];
let connected = false;
let pingTimer = null;
const ports = new Set();

async function postTo(base, action, metadata) {
    const response = await fetch(`${base}/${action}`, {
        method: 'POST',
        headers: { 'Content-Type': 'application/json' },
        body: JSON.stringify({
            source: 'ozonetel',
            timestamp: Date.now(),
            metadata: metadata || {}
        }),
        signal: AbortSignal.timeout(REQUEST_TIMEOUT_MS)
    });
    return response.ok;
}

async function signalMicMute(action, metadata) {
    const bases = [serverBase, ...MICMUTE_HOSTS.filter(h => h !== serverBase)];

    for (const base of bases) {
        try {
            if (await postTo(base, action, metadata)) {
                serverBase = base;
                setConnected(true);
                return true;
            }
            // Server answered but rejected the request - it is running, so no
            // point retrying the other host.
            setConnected(true);
            console.warn(`[MicMute] ${action} rejected by app`);
            return false;
        } catch (error) {
            // Unreachable on this host, fall through to the next one.
        }
    }

    setConnected(false);
    return false;
}

function setConnected(value) {
    if (value === connected) return;
    connected = value;
    console.log(`[MicMute] app ${connected ? `detected on ${serverBase}` : 'not running / unreachable'}`);
    updateBadge();
    broadcast({ type: 'micmute-connection', connected });
}

function updateBadge() {
    const text = ports.size === 0 ? '' : (connected ? 'ON' : 'OFF');
    chrome.action.setBadgeText({ text });
    chrome.action.setBadgeBackgroundColor({ color: connected ? '#2e7d32' : '#c62828' });
    chrome.action.setTitle({
        title: ports.size === 0
            ? 'MicMute: no Ozonetel tab open'
            : (connected ? 'MicMute app connected' : 'MicMute app not running')
    });
}

function broadcast(message) {
    for (const port of ports) {
        try {
            port.postMessage(message);
        } catch (error) {
            ports.delete(port);
        }
    }
}

function startPingLoop() {
    if (pingTimer !== null) return;
    signalMicMute('ping');
    pingTimer = setInterval(() => {
        if (ports.size === 0) {
            stopPingLoop();
            return;
        }
        signalMicMute('ping');
    }, PING_INTERVAL_MS);
}

function stopPingLoop() {
    if (pingTimer === null) return;
    clearInterval(pingTimer);
    pingTimer = null;
}

// A content script holds a port open for as long as its tab lives. Incoming
// port messages reset the service worker idle timer; if it is torn down
// anyway, the content script sees onDisconnect and reconnects, which wakes us
// straight back up.
chrome.runtime.onConnect.addListener(port => {
    if (port.name !== 'micmute') return;

    ports.add(port);
    updateBadge();
    startPingLoop();

    port.onMessage.addListener(message => {
        if (message && message.type === 'keepalive') {
            port.postMessage({ type: 'micmute-connection', connected });
        }
    });

    port.onDisconnect.addListener(() => {
        ports.delete(port);
        if (ports.size === 0) stopPingLoop();
        updateBadge();
    });
});

// Content script relays call start/stop here so the request is made from the
// extension origin - no page CSP, no CORS preflight surprises.
chrome.runtime.onMessage.addListener((message, sender, sendResponse) => {
    if (!message || message.type !== 'micmute-signal') return false;

    signalMicMute(message.action, message.metadata)
        .then(ok => sendResponse({ ok, connected }))
        .catch(() => sendResponse({ ok: false, connected }));

    return true; // async response
});

// Backstop: wakes the worker if it was torn down while a tab is still open.
chrome.alarms.create(KEEPALIVE_ALARM, { periodInMinutes: 0.5 });
chrome.alarms.onAlarm.addListener(alarm => {
    if (alarm.name === KEEPALIVE_ALARM && ports.size > 0) startPingLoop();
});
