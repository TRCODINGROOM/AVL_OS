const { contextBridge, ipcRenderer } = require('electron');
contextBridge.exposeInMainWorld('avl', {
  native: true,
  call: (name, ...args) => ipcRenderer.invoke('avl:' + name, ...args),
  on: (ch, fn) => ipcRenderer.on('avl:' + ch, (_e, ...a) => fn(...a)),
});
