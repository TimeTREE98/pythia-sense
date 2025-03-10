import type { Params } from 'whisper';

declare global {
  interface Window {
    whisper: {
      initialize: (params: Params) => Promise<boolean>;
      startListening: () => Promise<boolean>;
      stopListening: () => Promise<boolean>;
      cleanup: () => Promise<boolean>;
    };
    electron: {
      ipcRenderer: {
        on: (channel: string, listener: (...args: unknown[]) => void) => void;
        removeListener: (channel: string, listener: (...args: unknown[]) => void) => void;
      };
    };
  }
}

export {};
