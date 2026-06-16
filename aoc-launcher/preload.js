const { contextBridge, ipcRenderer } = require('electron');

contextBridge.exposeInMainWorld('aocAPI', {
  // ── Window controls ──
  minimize: () => ipcRenderer.send('win-minimize'),
  close: () => ipcRenderer.send('win-close'),

  // ── Navigation ──
  navigate: (screen) => ipcRenderer.send('navigate', screen),

  // ── Auth ──
  login: (username, password) => ipcRenderer.invoke('auth-login', username, password),
  register: (username, password, email) => ipcRenderer.invoke('auth-register', username, password, email),
  getCurrentUser: () => ipcRenderer.invoke('get-current-user'),

  // ── Characters ──
  getCharacters: () => ipcRenderer.invoke('get-characters'),
  createCharacter: (data) => ipcRenderer.invoke('create-character', data),
  deleteCharacter: (charId) => ipcRenderer.invoke('delete-character', charId),
  selectCharacter: (char) => ipcRenderer.send('select-character', char),

  // ── Server ──
  startServer: () => ipcRenderer.invoke('start-server'),
  stopServer: () => ipcRenderer.invoke('stop-server'),
  getServerStatus: () => ipcRenderer.invoke('get-server-status'),
  getServerList: () => ipcRenderer.invoke('get-server-list'),
  selectServer: (server) => ipcRenderer.send('select-server', server),

  // ── Settings ──
  getSystemInfo: () => ipcRenderer.invoke('get-system-info'),
  saveSettings: (settings) => ipcRenderer.invoke('save-settings', settings),
  loadSettings: () => ipcRenderer.invoke('load-settings'),

  // ── Game ──
  launchGame: () => ipcRenderer.invoke('launch-game'),

  // ── Events ──
  onServerStatus: (cb) => ipcRenderer.on('server-status', (_, data) => cb(data)),
  onNavigate: (cb) => ipcRenderer.on('do-navigate', (_, screen) => cb(screen)),
});
