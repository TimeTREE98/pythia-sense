import { contextBridge, ipcRenderer } from 'electron';
import type { Params } from 'whisper';

contextBridge.exposeInMainWorld('electron', {
  ipcRenderer: {
    on: (channel: string, listener: (...args: unknown[]) => void) => {
      ipcRenderer.on(channel, (event, ...args) => listener(event, ...args));
    },
    removeListener: (channel: string, listener: (...args: unknown[]) => void) => {
      ipcRenderer.removeListener(channel, listener);
    },
  },
});

contextBridge.exposeInMainWorld('whisper', {
  initialize: (params: Params): Promise<boolean> => {
    return ipcRenderer.invoke('whisper:initialize', params);
  },
  startListening: (): Promise<boolean> => {
    return ipcRenderer.invoke('whisper:startListening');
  },
  stopListening: (): Promise<boolean> => {
    return ipcRenderer.invoke('whisper:stopListening');
  },
  cleanup: (): Promise<boolean> => {
    return ipcRenderer.invoke('whisper:cleanup');
  },
});
