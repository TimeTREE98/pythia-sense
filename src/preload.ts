import { contextBridge, ipcRenderer } from 'electron';
import type { Params } from 'whisper';

contextBridge.exposeInMainWorld('whisper', {
  initialize: (params: Params): Promise<boolean> => {
    return ipcRenderer.invoke('whisper:initialize', params);
  },
});
