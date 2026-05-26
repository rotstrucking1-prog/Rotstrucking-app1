/**
 * Oracle Log Bridge — Watches oracle_chat_log.txt and sends bug reports to Tasklet
 * 
 * This module runs alongside server.js in the AoC game server.
 * It watches the Oracle NPC's chat log file for errors and diagnostics,
 * batches them, and POSTs to the Tasklet webhook so the AI agent
 * can automatically analyze and fix issues.
 * 
 * Flow: Oracle writes log → Bridge detects new entries → POST to webhook → Agent fixes code
 */

const fs = require('fs');
const path = require('path');
const https = require('https');
const http = require('http');

class OracleLogBridge {
    constructor(options = {}) {
        // Path to the oracle chat log (UE5 writes here)
        this.logPath = options.logPath || path.join(
            process.env.UE5_PROJECT_DIR || 
            'C:\\Users\\Bradh\\Documents\\Unreal Projects\\AOC',
            'AoC_GameData', 'oracle_chat_log.txt'
        );
        
        // Webhook URL — user must set this from the Tasklet UI
        // Stored in a config file next to this script
        this.webhookUrl = options.webhookUrl || this.loadWebhookUrl();
        
        // How often to check for new log entries (ms)
        this.pollInterval = options.pollInterval || 15000; // 15 seconds
        
        // How often to send hourly summary reports (ms)
        this.summaryInterval = options.summaryInterval || 3600000; // 1 hour
        
        // Minimum errors before sending a report (prevents spam)
        this.minErrorsToReport = options.minErrorsToReport || 1;
        
        // Cooldown between reports (prevent spam if many errors at once)
        this.reportCooldown = options.reportCooldown || 300000; // 5 minutes
        
        // Internal state
        this.lastReadPosition = 0;
        this.lastReportTime = 0;
        this.pendingErrors = [];
        this.pendingDiagnostics = [];
        this.allEntriesSinceLastSummary = [];
        this.watcher = null;
        this.pollTimer = null;
        this.summaryTimer = null;
        this.isRunning = false;
        
        // Stats
        this.stats = {
            totalEntriesProcessed: 0,
            totalErrorsFound: 0,
            totalReportsSent: 0,
            bridgeStartTime: Date.now(),
            lastErrorTime: null,
            lastReportTime: null
        };
    }
    
    /**
     * Load webhook URL from config file
     */
    loadWebhookUrl() {
        const configPath = path.join(__dirname, 'oracle-webhook-config.json');
        try {
            if (fs.existsSync(configPath)) {
                const config = JSON.parse(fs.readFileSync(configPath, 'utf-8'));
                if (config.webhookUrl) {
                    console.log('[Oracle Bridge] Webhook URL loaded from config');
                    return config.webhookUrl;
                }
            }
        } catch (e) {
            console.warn('[Oracle Bridge] Could not load webhook config:', e.message);
        }
        console.warn('[Oracle Bridge] No webhook URL configured! Reports will be queued but not sent.');
        console.warn('[Oracle Bridge] Create oracle-webhook-config.json with: { "webhookUrl": "https://webhooks.tasklet.ai/..." }');
        return null;
    }
    
    /**
     * Save webhook URL to config
     */
    setWebhookUrl(url) {
        this.webhookUrl = url;
        const configPath = path.join(__dirname, 'oracle-webhook-config.json');
        fs.writeFileSync(configPath, JSON.stringify({ webhookUrl: url }, null, 2));
        console.log('[Oracle Bridge] Webhook URL saved to config');
    }
    
    /**
     * Start the bridge
     */
    start() {
        if (this.isRunning) return;
        this.isRunning = true;
        
        console.log('[Oracle Bridge] Starting...');
        console.log(`[Oracle Bridge] Watching: ${this.logPath}`);
        console.log(`[Oracle Bridge] Webhook: ${this.webhookUrl ? 'configured' : 'NOT SET'}`);
        console.log(`[Oracle Bridge] Poll interval: ${this.pollInterval / 1000}s`);
        console.log(`[Oracle Bridge] Summary interval: ${this.summaryInterval / 1000}s`);
        
        // Read existing file to get current position (don't report old errors)
        this.initializePosition();
        
        // Start polling for changes
        this.pollTimer = setInterval(() => this.checkForNewEntries(), this.pollInterval);
        
        // Start hourly summary timer
        this.summaryTimer = setInterval(() => this.sendHourlySummary(), this.summaryInterval);
        
        // Also try fs.watch for faster detection (not reliable on all systems)
        try {
            if (fs.existsSync(this.logPath)) {
                this.watcher = fs.watch(this.logPath, () => {
                    // Debounce — check after a brief delay
                    setTimeout(() => this.checkForNewEntries(), 1000);
                });
            }
        } catch (e) {
            console.log('[Oracle Bridge] fs.watch not available, using polling only');
        }
        
        console.log('[Oracle Bridge] Running! Waiting for Oracle NPC activity...');
    }
    
    /**
     * Stop the bridge
     */
    stop() {
        this.isRunning = false;
        if (this.pollTimer) clearInterval(this.pollTimer);
        if (this.summaryTimer) clearInterval(this.summaryTimer);
        if (this.watcher) this.watcher.close();
        console.log('[Oracle Bridge] Stopped.');
    }
    
    /**
     * Initialize read position to end of existing file
     */
    initializePosition() {
        try {
            if (fs.existsSync(this.logPath)) {
                const stat = fs.statSync(this.logPath);
                this.lastReadPosition = stat.size;
                console.log(`[Oracle Bridge] Existing log found (${stat.size} bytes). Starting from end.`);
            } else {
                this.lastReadPosition = 0;
                console.log('[Oracle Bridge] No existing log. Will start watching when Oracle creates it.');
            }
        } catch (e) {
            this.lastReadPosition = 0;
        }
    }
    
    /**
     * Check for new log entries since last read
     */
    checkForNewEntries() {
        if (!fs.existsSync(this.logPath)) return;
        
        try {
            const stat = fs.statSync(this.logPath);
            
            // File was truncated/reset
            if (stat.size < this.lastReadPosition) {
                this.lastReadPosition = 0;
            }
            
            // No new data
            if (stat.size <= this.lastReadPosition) return;
            
            // Read new data
            const fd = fs.openSync(this.logPath, 'r');
            const buffer = Buffer.alloc(stat.size - this.lastReadPosition);
            fs.readSync(fd, buffer, 0, buffer.length, this.lastReadPosition);
            fs.closeSync(fd);
            
            this.lastReadPosition = stat.size;
            
            // Parse new lines
            const newText = buffer.toString('utf-8');
            const lines = newText.split('\n').filter(l => l.trim());
            
            this.processNewLines(lines);
            
        } catch (e) {
            // File might be locked by UE5, that's OK
        }
    }
    
    /**
     * Process new log lines — categorize and queue for reporting
     */
    processNewLines(lines) {
        for (const line of lines) {
            this.stats.totalEntriesProcessed++;
            this.allEntriesSinceLastSummary.push(line);
            
            // Parse the line format: [YYYY-MM-DD HH:MM:SS] [TAG] message
            const match = line.match(/^\[([^\]]+)\]\s*\[([^\]]+)\]\s*(.+)$/);
            if (!match) continue;
            
            const [, timestamp, tag, message] = match;
            const entry = { timestamp, tag, message, raw: line };
            
            // Categorize
            if (tag === 'ERROR') {
                this.pendingErrors.push(entry);
                this.stats.totalErrorsFound++;
                this.stats.lastErrorTime = Date.now();
                console.log(`[Oracle Bridge] ERROR: ${message}`);
            } 
            else if (tag === 'DIAGNOSTIC') {
                this.pendingDiagnostics.push(entry);
                console.log(`[Oracle Bridge] DIAGNOSTIC: ${message}`);
            }
            else if (tag === 'FULLTEST') {
                // Full test results are always interesting
                if (message.includes('✗') || message.includes('FAIL')) {
                    this.pendingErrors.push(entry);
                    this.stats.totalErrorsFound++;
                    console.log(`[Oracle Bridge] FULLTEST FAIL: ${message}`);
                }
            }
            else if (tag === 'COMMENTARY') {
                // Smart observations about the world
                if (message.includes('missing') || message.includes('broken') || 
                    message.includes('empty') || message.includes('can\'t find')) {
                    this.pendingDiagnostics.push(entry);
                }
            }
        }
        
        // Check if we should send a report
        this.maybeReport();
    }
    
    /**
     * Decide if it's time to send a bug report
     */
    maybeReport() {
        const now = Date.now();
        const timeSinceLastReport = now - this.lastReportTime;
        
        // Respect cooldown
        if (timeSinceLastReport < this.reportCooldown) return;
        
        // Need at least minErrorsToReport errors
        if (this.pendingErrors.length < this.minErrorsToReport && 
            this.pendingDiagnostics.length < 3) return;
        
        this.sendBugReport();
    }
    
    /**
     * Send a bug report to the Tasklet webhook
     */
    async sendBugReport() {
        if (!this.webhookUrl) {
            console.warn('[Oracle Bridge] Cannot send report — no webhook URL configured');
            return;
        }
        
        if (this.pendingErrors.length === 0 && this.pendingDiagnostics.length === 0) return;
        
        const report = {
            type: 'oracle_bug_report',
            timestamp: new Date().toISOString(),
            game: 'Architect of Creation',
            source: 'Oracle NPC',
            
            errors: this.pendingErrors.map(e => ({
                time: e.timestamp,
                tag: e.tag,
                message: e.message
            })),
            
            diagnostics: this.pendingDiagnostics.map(d => ({
                time: d.timestamp,
                tag: d.tag,
                message: d.message
            })),
            
            summary: {
                errorCount: this.pendingErrors.length,
                diagnosticCount: this.pendingDiagnostics.length,
                totalProcessed: this.stats.totalEntriesProcessed,
                uptimeMinutes: Math.floor((Date.now() - this.stats.bridgeStartTime) / 60000)
            }
        };
        
        try {
            await this.postToWebhook(report);
            this.stats.totalReportsSent++;
            this.stats.lastReportTime = Date.now();
            this.lastReportTime = Date.now();
            
            // Clear pending
            this.pendingErrors = [];
            this.pendingDiagnostics = [];
            
            console.log(`[Oracle Bridge] Bug report sent! (${report.summary.errorCount} errors, ${report.summary.diagnosticCount} diagnostics)`);
        } catch (e) {
            console.error(`[Oracle Bridge] Failed to send report: ${e.message}`);
        }
    }
    
    /**
     * Send hourly summary regardless of errors
     */
    async sendHourlySummary() {
        if (!this.webhookUrl) return;
        if (this.allEntriesSinceLastSummary.length === 0) return;
        
        const summary = {
            type: 'oracle_hourly_summary',
            timestamp: new Date().toISOString(),
            game: 'Architect of Creation',
            source: 'Oracle NPC',
            
            // Last 200 log lines (keep payload reasonable)
            recentLog: this.allEntriesSinceLastSummary.slice(-200),
            
            stats: {
                entriesThisHour: this.allEntriesSinceLastSummary.length,
                errorsTotal: this.stats.totalErrorsFound,
                reportsSent: this.stats.totalReportsSent,
                uptimeMinutes: Math.floor((Date.now() - this.stats.bridgeStartTime) / 60000),
                bridgeStartTime: new Date(this.stats.bridgeStartTime).toISOString()
            }
        };
        
        try {
            await this.postToWebhook(summary);
            this.allEntriesSinceLastSummary = [];
            console.log('[Oracle Bridge] Hourly summary sent.');
        } catch (e) {
            console.error(`[Oracle Bridge] Failed to send summary: ${e.message}`);
        }
    }
    
    /**
     * HTTP POST to webhook URL
     */
    postToWebhook(data) {
        return new Promise((resolve, reject) => {
            const body = JSON.stringify(data);
            const url = new URL(this.webhookUrl);
            
            const options = {
                hostname: url.hostname,
                port: url.port || (url.protocol === 'https:' ? 443 : 80),
                path: url.pathname + url.search,
                method: 'POST',
                headers: {
                    'Content-Type': 'application/json',
                    'Content-Length': Buffer.byteLength(body)
                }
            };
            
            const client = url.protocol === 'https:' ? https : http;
            const req = client.request(options, (res) => {
                let responseBody = '';
                res.on('data', chunk => responseBody += chunk);
                res.on('end', () => {
                    if (res.statusCode >= 200 && res.statusCode < 300) {
                        resolve(responseBody);
                    } else {
                        reject(new Error(`Webhook returned ${res.statusCode}: ${responseBody}`));
                    }
                });
            });
            
            req.on('error', reject);
            req.write(body);
            req.end();
        });
    }
    
    /**
     * Get bridge status (for the launcher dashboard)
     */
    getStatus() {
        return {
            running: this.isRunning,
            webhookConfigured: !!this.webhookUrl,
            logFileExists: fs.existsSync(this.logPath),
            logFileSize: fs.existsSync(this.logPath) ? fs.statSync(this.logPath).size : 0,
            ...this.stats,
            pendingErrors: this.pendingErrors.length,
            pendingDiagnostics: this.pendingDiagnostics.length,
            uptimeMinutes: Math.floor((Date.now() - this.stats.bridgeStartTime) / 60000)
        };
    }
    
    /**
     * Read the full log for on-demand requests (e.g., schedule trigger)
     */
    readFullLog(maxLines = 200) {
        if (!fs.existsSync(this.logPath)) return { lines: [], exists: false };
        
        try {
            const content = fs.readFileSync(this.logPath, 'utf-8');
            const allLines = content.split('\n').filter(l => l.trim());
            const lines = allLines.slice(-maxLines);
            
            // Categorize
            const errors = lines.filter(l => l.includes('[ERROR]') || l.includes('[DIAGNOSTIC]'));
            const fulltest = lines.filter(l => l.includes('[FULLTEST]'));
            const speech = lines.filter(l => l.includes('[SPEECH]'));
            const combat = lines.filter(l => l.includes('[COMBAT]'));
            
            return {
                exists: true,
                totalLines: allLines.length,
                recentLines: lines,
                errors,
                fulltest,
                speech,
                combat,
                lastEntry: allLines[allLines.length - 1] || null
            };
        } catch (e) {
            return { exists: true, error: e.message };
        }
    }
}

// Express route handlers (add to server.js)
function addOracleRoutes(app, bridge) {
    // GET /oracle/status — bridge health
    app.get('/oracle/status', (req, res) => {
        res.json(bridge.getStatus());
    });
    
    // GET /oracle/log — read recent log
    app.get('/oracle/log', (req, res) => {
        const maxLines = parseInt(req.query.lines) || 200;
        res.json(bridge.readFullLog(maxLines));
    });
    
    // GET /oracle/log/errors — just errors  
    app.get('/oracle/log/errors', (req, res) => {
        const log = bridge.readFullLog(500);
        res.json({
            errors: log.errors || [],
            fulltest: log.fulltest || [],
            count: (log.errors || []).length
        });
    });
    
    // POST /oracle/webhook — set the webhook URL
    app.post('/oracle/webhook', (req, res) => {
        const { url } = req.body;
        if (!url) return res.status(400).json({ error: 'url required' });
        bridge.setWebhookUrl(url);
        res.json({ success: true, message: 'Webhook URL saved' });
    });
    
    // POST /oracle/report — force send a report now
    app.post('/oracle/report', (req, res) => {
        bridge.sendBugReport().then(() => {
            res.json({ success: true });
        }).catch(e => {
            res.status(500).json({ error: e.message });
        });
    });
    
    console.log('[Oracle Bridge] Routes added: /oracle/status, /oracle/log, /oracle/log/errors, /oracle/webhook, /oracle/report');
}

module.exports = { OracleLogBridge, addOracleRoutes };
