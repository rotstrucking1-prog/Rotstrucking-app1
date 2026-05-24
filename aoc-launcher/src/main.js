const { app, BrowserWindow, ipcMain, screen } = require('electron');
const path = require('path');
const http = require('http');
const { spawn, execSync } = require('child_process');
const fs = require('fs');
const os = require('os');

// ═══════════════════════════════════════════
//  ARCHITECT OF CREATION — LAUNCHER v6.2
//  Flow: Login → Cinematic (1st time) → CharCreate/Select → ServerSelect → Settings → Loading → Game
//  Freeform classless character system
// ═══════════════════════════════════════════

let mainWindow = null;
let serverProcess = null;
let currentUser = null;
let authToken = null;
let selectedCharacter = null;
let selectedServer = null;
let previousScreen = 'server-select';

// ── In-memory database ──
const users = {};
const sessions = {};

// ── Paths ──
function getSettingsPath() { return path.join(app.getPath('userData'), 'aoc-settings.json'); }
function getFirstLoginPath() { return path.join(app.getPath('userData'), 'aoc-seen-cinematic.json'); }
function getGameUserSettingsPath() {
  return path.join(os.homedir(), 'AppData', 'Local', 'AOC', 'Saved', 'Config', 'Windows', 'GameUserSettings.ini');
}

// ── First login tracking ──
function hasSeenCinematic(username) {
  try {
    const data = JSON.parse(fs.readFileSync(getFirstLoginPath(), 'utf8'));
    return data[username.toLowerCase()] === true;
  } catch(e) { return false; }
}

function markCinematicSeen(username) {
  let data = {};
  try { data = JSON.parse(fs.readFileSync(getFirstLoginPath(), 'utf8')); } catch(e) {}
  data[username.toLowerCase()] = true;
  fs.writeFileSync(getFirstLoginPath(), JSON.stringify(data, null, 2));
}

// ── Kill old instances on port 4000 ──
function killPort4000() {
  return new Promise(resolve => {
    if (process.platform !== 'win32') return resolve();
    try {
      execSync('for /f "tokens=5" %a in (\'netstat -aon ^| findstr :4000 ^| findstr LISTENING\') do taskkill /F /PID %a', { shell: 'cmd.exe', timeout: 5000 });
    } catch(e) {}
    setTimeout(resolve, 500);
  });
}

// ── Direct auth (NO HTTP round-trip) ──
function directLogin(username, password) {
  if (!username || !password) return { success: false, message: 'Username and password required' };
  const key = username.toLowerCase();
  // Auto-register on first login (dev convenience)
  if (!users[key]) { users[key] = { password, characters: [] }; }
  if (users[key].password !== password) return { success: false, message: 'Invalid credentials' };
  const token = 'tok_' + Math.random().toString(36).substr(2) + Date.now().toString(36);
  sessions[token] = key;
  return { success: true, token, username: key, message: 'Welcome back, ' + username + '!' };
}

function directRegister(username, password, email) {
  if (!username || !password) return { success: false, message: 'Username and password required' };
  if (username.length < 3) return { success: false, message: 'Username must be at least 3 characters' };
  if (password.length < 4) return { success: false, message: 'Password must be at least 4 characters' };
  const key = username.toLowerCase();
  if (users[key]) return { success: false, message: 'Username already taken' };
  users[key] = { password, email: email || '', characters: [] };
  return { success: true, message: 'Account created! You may now log in.' };
}

function directGetCharacters(token) {
  const user = sessions[token];
  if (!user) return { success: false, characters: [] };
  return { success: true, characters: users[user]?.characters || [] };
}

function directCreateCharacter(token, data) {
  const user = sessions[token];
  if (!user) return { success: false, message: 'Not authenticated' };
  if (!users[user]) users[user] = { password: '', characters: [] };
  if (users[user].characters.length >= 5) return { success: false, message: 'Maximum 5 characters per account' };
  if (users[user].characters.some(c => c.name.toLowerCase() === (data.name || '').toLowerCase())) {
    return { success: false, message: 'Character name already exists' };
  }
  const char = {
    id: 'chr_' + Math.random().toString(36).substr(2,8),
    name: data.name || 'Unnamed',
    level: 1,
    race: data.race || 'Human',
    alignment: data.alignment || 'Neutral',
    guild: '—',
    stats: data.stats || { str:50, dex:50, int:50, con:50, wis:50, vit:50 },
    racialBonus: data.racialBonus || {},
    appearance: data.appearance || {},
    zone: 'Starting Area',
    playtime: '0h',
    lastPlayed: new Date().toLocaleDateString('en-US', { month:'short', day:'numeric', year:'numeric' }),
    createdAt: new Date().toISOString(),
  };
  users[user].characters.push(char);
  return { success: true, character: char };
}

function directDeleteCharacter(token, charId) {
  const user = sessions[token];
  if (!user) return { success: false, message: 'Not authenticated' };
  const chars = users[user]?.characters || [];
  const idx = chars.findIndex(c => c.id === charId || c.name === charId);
  if (idx === -1) return { success: false, message: 'Character not found' };
  const deleted = chars.splice(idx, 1)[0];
  return { success: true, message: deleted.name + ' has been deleted', deleted };
}

function getServerList() {
  return { success: true, servers: [
    { id:'realm1', name:'AoC Realm 1', region:'US-East', population:47, maxPop:500, ping:32, status:'online', mode:'PvPvE', uptime:'3d 14h', rules:'Full Loot PvP', description:'The original realm. Battle-hardened warriors clash over territory and resources. Full loot on death.' },
    { id:'realm2', name:'AoC Realm 2', region:'US-West', population:23, maxPop:500, ping:58, status:'online', mode:'PvE', uptime:'2d 8h', rules:'Safe Trading', description:'A peaceful realm for crafters, explorers, and builders. PvP only in designated zones.' },
    { id:'shadowmere', name:'Shadowmere', region:'EU-Central', population:89, maxPop:500, ping:110, status:'online', mode:'Hardcore PvP', uptime:'5d 2h', rules:'Permadeath Enabled', description:'Only the strongest survive. Death is permanent. The ultimate challenge for hardcore players.' },
    { id:'test', name:'Test Realm', region:'Local', population:1, maxPop:50, ping:5, status:'online', mode:'Development', uptime:'0d 1h', rules:'Admin Access', description:'Development testing realm. Expect frequent resets and experimental features.' }
  ]};
}

// ═══════════════════════════════════════════
//  EMBEDDED HTTP SERVER (for external access)
// ═══════════════════════════════════════════
let embeddedServer = null;

function startEmbeddedServer() {
  return new Promise((resolve, reject) => {
    embeddedServer = http.createServer((req, res) => {
      res.setHeader('Access-Control-Allow-Origin', '*');
      res.setHeader('Access-Control-Allow-Methods', 'GET,POST,PUT,DELETE,OPTIONS');
      res.setHeader('Access-Control-Allow-Headers', 'Content-Type,Authorization');
      if (req.method === 'OPTIONS') { res.writeHead(200); res.end(); return; }

      let body = '';
      req.on('data', c => body += c);
      req.on('end', () => {
        let data = {}; try { data = body ? JSON.parse(body) : {}; } catch(e) {}
        const url = req.url, method = req.method;
        const json = (code, obj) => { res.writeHead(code, {'Content-Type':'application/json'}); res.end(JSON.stringify(obj)); };
        const getToken = () => (req.headers.authorization || '').replace('Bearer ', '');

        if (url === '/api/health' && method === 'GET') {
          json(200, { status:'online', server:'AoC Realm 1', players: Object.keys(sessions).length, uptime: process.uptime() });
        }
        else if (url === '/api/auth/login' && method === 'POST') { json(200, directLogin(data.username, data.password)); }
        else if (url === '/api/auth/register' && method === 'POST') { json(200, directRegister(data.username, data.password, data.email)); }
        else if (url === '/api/characters' && method === 'GET') { json(200, directGetCharacters(getToken())); }
        else if (url === '/api/characters' && method === 'POST') { json(200, directCreateCharacter(getToken(), data)); }
        else if (url.startsWith('/api/characters/') && method === 'DELETE') { json(200, directDeleteCharacter(getToken(), decodeURIComponent(url.split('/').pop()))); }
        else if (url === '/api/servers' && method === 'GET') { json(200, getServerList()); }
        else if (url === '/api/status' && method === 'GET') { json(200, { status:'online', name:'AoC Realm 1', players: Object.keys(sessions).length, maxPlayers:500, uptime: Math.floor(process.uptime()) }); }
        else { json(404, { error:'Not found' }); }
      });
    });
    embeddedServer.listen(4000, () => { console.log('[AoC] Embedded server on port 4000'); resolve(); });
    embeddedServer.on('error', e => { console.error('[AoC] Server error:', e.message); reject(e); });
  });
}

// ═══════════════════════════════════════════
//  SYSTEM INFO (for auto-detect)
// ═══════════════════════════════════════════
function getSystemInfo() {
  const info = {
    cpu: 'Unknown', gpu: 'Unknown', vram: 'Unknown', vramMB: 0,
    ram: Math.round(os.totalmem() / (1024*1024*1024)) + ' GB',
    ramMB: Math.round(os.totalmem() / (1024*1024)),
    cores: os.cpus().length, platform: os.platform(), arch: os.arch(),
  };
  const cpus = os.cpus();
  if (cpus.length > 0) info.cpu = cpus[0].model.trim();

  if (process.platform === 'win32') {
    try {
      const gpuOut = execSync('wmic path win32_VideoController get Name,AdapterRAM /format:csv', { timeout:5000, encoding:'utf8' });
      const lines = gpuOut.split('\n').filter(l => l.trim() && !l.includes('Node'));
      let bestGPU='', bestVRAM=0;
      lines.forEach(line => {
        const parts = line.split(',');
        if (parts.length >= 3) { const ram = parseInt(parts[1])||0; const name=parts[2]?.trim()||''; if (ram>bestVRAM && name) { bestVRAM=ram; bestGPU=name; } }
      });
      if (bestGPU) info.gpu = bestGPU;
      if (bestVRAM > 0) { info.vramMB = Math.round(bestVRAM/(1024*1024)); info.vram = Math.round(bestVRAM/(1024*1024*1024)*10)/10 + ' GB'; }
    } catch(e) {
      try {
        const psOut = execSync('powershell -command "Get-CimInstance -ClassName Win32_VideoController | Select-Object Name,AdapterRAM | ConvertTo-Json"', { timeout:8000, encoding:'utf8' });
        const gpuData = JSON.parse(psOut);
        const gpus = Array.isArray(gpuData) ? gpuData : [gpuData];
        let bestGPU='', bestVRAM=0;
        gpus.forEach(g => { if (g.AdapterRAM > bestVRAM) { bestVRAM=g.AdapterRAM; bestGPU=g.Name; } });
        if (bestGPU) info.gpu = bestGPU;
        if (bestVRAM > 0) { info.vramMB = Math.round(bestVRAM/(1024*1024)); info.vram = Math.round(bestVRAM/(1024*1024*1024)*10)/10 + ' GB'; }
      } catch(e2) {}
    }
  }
  try { const displays = screen.getAllDisplays(); info.displays = displays.map(d => ({ width:d.size.width, height:d.size.height, scaleFactor:d.scaleFactor, primary:d.id===screen.getPrimaryDisplay().id })); } catch(e) {}
  return info;
}

// ═══════════════════════════════════════════
//  SETTINGS
// ═══════════════════════════════════════════
function loadSettings() {
  try { return JSON.parse(fs.readFileSync(getSettingsPath(), 'utf8')); } catch(e) { return null; }
}

function saveSettings(settings) {
  try {
    fs.writeFileSync(getSettingsPath(), JSON.stringify(settings, null, 2));
    writeGameUserSettings(settings);
    return true;
  } catch(e) { return false; }
}

function writeGameUserSettings(settings) {
  if (!settings || !settings.video) return;
  const v = settings.video;
  const [resX, resY] = (v.resolution || '1920x1080').split('x');
  const modeMap = { fullscreen: 0, borderless: 1, windowed: 2 };
  const fullscreenMode = modeMap[v.windowMode] || 2;
  const q = v.quality || {};
  const ini = `[ScalabilityGroups]
sg.ResolutionQuality=${v.resolutionScale || 100}
sg.ViewDistanceQuality=${q.viewDistance ?? 2}
sg.AntiAliasingQuality=${q.antiAliasing ?? 2}
sg.ShadowQuality=${q.shadows ?? 2}
sg.GlobalIlluminationQuality=${q.globalIllum ?? 2}
sg.ReflectionQuality=${q.reflections ?? 2}
sg.PostProcessQuality=${q.postProcess ?? 2}
sg.TextureQuality=${q.textures ?? 2}
sg.EffectsQuality=${q.effects ?? 2}
sg.FoliageQuality=${q.foliage ?? 2}
sg.ShadingQuality=${q.shading ?? 2}

[/Script/Engine.GameUserSettings]
bUseVSync=${v.vsync ? 'True' : 'False'}
bUseDynamicResolution=${v.dynamicRes ? 'True' : 'False'}
ResolutionSizeX=${resX}
ResolutionSizeY=${resY}
LastUserConfirmedResolutionSizeX=${resX}
LastUserConfirmedResolutionSizeY=${resY}
FullscreenMode=${fullscreenMode}
LastConfirmedFullscreenMode=${fullscreenMode}
PreferredFullscreenMode=${fullscreenMode === 0 ? 0 : 1}
FrameRateLimit=${v.fpsLimit === 0 ? '0.000000' : v.fpsLimit + '.000000'}
AudioQualityLevel=2

[Audio]
MasterVolume=${(settings.audio?.master ?? 80) / 100}
MusicVolume=${(settings.audio?.music ?? 70) / 100}
SoundEffectsVolume=${(settings.audio?.sfx ?? 85) / 100}
VoiceVolume=${(settings.audio?.voice ?? 90) / 100}
AmbientVolume=${(settings.audio?.ambient ?? 60) / 100}
`;
  try {
    const iniPath = getGameUserSettingsPath();
    fs.mkdirSync(path.dirname(iniPath), { recursive: true });
    fs.writeFileSync(iniPath, ini);
  } catch(e) {}
}

// ═══════════════════════════════════════════
//  WINDOW + IPC
// ═══════════════════════════════════════════
function createWindow() {
  mainWindow = new BrowserWindow({
    width: 1280, height: 800, minWidth: 1024, minHeight: 700,
    frame: false, resizable: true,
    icon: path.join(__dirname, 'img', 'icon.png'),
    webPreferences: {
      preload: path.join(__dirname, 'preload.js'),
      contextIsolation: true, nodeIntegration: false,
    },
    backgroundColor: '#0a0a0a', show: false,
  });
  mainWindow.loadFile(path.join(__dirname, 'login.html'));
  mainWindow.once('ready-to-show', () => mainWindow.show());
}

// ── Window controls ──
ipcMain.on('win-minimize', () => mainWindow?.minimize());
ipcMain.on('win-close', () => mainWindow?.close());

// ── Navigation ──
const SCREEN_FILES = {
  'login': 'login.html',
  'cinematic': 'cinematic.html',
  'character-select': 'character-select.html',
  'character-creation': 'character-creation.html',
  'server-select': 'server-select.html',
  'settings': 'settings.html',
  'loading': 'loading.html',
};

ipcMain.on('navigate', (_, dest) => {
  if (dest !== 'settings') previousScreen = dest;

  if (dest === 'character-flow') {
    // Smart flow: first login → cinematic → create. Returning → select if has chars.
    if (currentUser && !hasSeenCinematic(currentUser)) {
      markCinematicSeen(currentUser);
      mainWindow.loadFile(path.join(__dirname, 'cinematic.html'));
      return;
    }
    const chars = directGetCharacters(authToken);
    if (chars.characters && chars.characters.length > 0) {
      mainWindow.loadFile(path.join(__dirname, 'character-select.html'));
    } else {
      mainWindow.loadFile(path.join(__dirname, 'character-creation.html'));
    }
    return;
  }

  // After cinematic ends → go to character creation
  if (dest === 'post-cinematic') {
    mainWindow.loadFile(path.join(__dirname, 'character-creation.html'));
    return;
  }

  const file = SCREEN_FILES[dest];
  if (file) mainWindow.loadFile(path.join(__dirname, file));
});

// ── Auth (DIRECT — no HTTP round-trip) ──
ipcMain.handle('auth-login', async (_, username, password) => {
  const result = directLogin(username, password);
  if (result.success && result.token) {
    authToken = result.token;
    currentUser = username;
  }
  return result;
});

ipcMain.handle('auth-register', async (_, username, password, email) => {
  return directRegister(username, password, email);
});

ipcMain.handle('get-current-user', async () => {
  return { username: currentUser, token: authToken };
});

// ── Characters (DIRECT) ──
ipcMain.handle('get-characters', async () => {
  return directGetCharacters(authToken);
});

ipcMain.handle('create-character', async (_, data) => {
  return directCreateCharacter(authToken, data);
});

ipcMain.handle('delete-character', async (_, charId) => {
  return directDeleteCharacter(authToken, charId);
});

ipcMain.on('select-character', (_, char) => { selectedCharacter = char; });

// ── Server ──
ipcMain.handle('start-server', async () => {
  return { success: true, message: 'Server is running (embedded)' };
});

ipcMain.handle('stop-server', async () => {
  return { success: true };
});

ipcMain.handle('get-server-status', async () => {
  return { status: 'online', name: 'AoC Realm 1', players: Object.keys(sessions).length, maxPlayers: 500, uptime: Math.floor(process.uptime()) };
});

ipcMain.handle('get-server-list', async () => {
  return getServerList();
});

ipcMain.on('select-server', (_, server) => { selectedServer = server; });

// ── Settings ──
ipcMain.handle('get-system-info', async () => getSystemInfo());
ipcMain.handle('save-settings', async (_, settings) => ({ success: saveSettings(settings) }));
ipcMain.handle('load-settings', async () => loadSettings());

// ── Game Launch ──
ipcMain.handle('launch-game', async () => {
  const ue5Paths = [
    'C:\\Program Files\\Epic Games\\UE_5.7\\Engine\\Binaries\\Win64\\UnrealEditor.exe',
    'C:\\Program Files\\Epic Games\\UE_5.5\\Engine\\Binaries\\Win64\\UnrealEditor.exe',
    'C:\\Program Files\\Epic Games\\UE_5.4\\Engine\\Binaries\\Win64\\UnrealEditor.exe',
  ];
  const projectPaths = [
    'C:\\Users\\Bradh\\Documents\\Unreal Projects\\AOC\\AOC.uproject',
    'C:\\Users\\Bradh\\Documents\\Unreal Projects\\ArchitectOfCreation\\ArchitectOfCreation.uproject',
  ];
  let ue5Path = null, projectPath = null;
  for (const p of ue5Paths) { try { if (fs.existsSync(p)) { ue5Path = p; break; } } catch(e) {} }
  for (const p of projectPaths) { try { if (fs.existsSync(p)) { projectPath = p; break; } } catch(e) {} }
  if (!ue5Path) return { success: false, message: 'UE5 not found at C:\\Program Files\\Epic Games\\' };
  if (!projectPath) return { success: false, message: 'AoC project not found' };

  const settings = loadSettings();
  const args = [projectPath, 'AoCWorld', '-game', '-log'];
  if (settings && settings.video) {
    const v = settings.video;
    const [resX, resY] = (v.resolution || '1920x1080').split('x');
    if (v.windowMode === 'fullscreen') args.push('-fullscreen');
    else if (v.windowMode === 'borderless') args.push('-windowed', '-fullscreen');
    else args.push('-windowed');
    args.push('-resx=' + resX, '-resy=' + resY);
    if (v.fpsLimit > 0) args.push('-fps=' + v.fpsLimit);
    if (v.vsync) args.push('-vsync');
  } else { args.push('-windowed', '-resx=1920', '-resy=1080'); }

  try {
    const game = spawn(ue5Path, args, { detached: true, stdio: 'ignore' });
    game.unref();
    return { success: true, message: 'Game launching...' };
  } catch(e) { return { success: false, message: 'Failed: ' + e.message }; }
});

// ═══════════════════════════════════════════
//  APP LIFECYCLE
// ═══════════════════════════════════════════
app.whenReady().then(async () => {
  await killPort4000();
  try { await startEmbeddedServer(); console.log('[AoC] Server ready'); } catch(e) { console.error('[AoC] Server failed:', e.message); }
  createWindow();
});
app.on('window-all-closed', () => app.quit());
app.on('before-quit', () => { if (embeddedServer) embeddedServer.close(); });
