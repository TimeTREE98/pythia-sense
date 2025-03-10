import { Button } from '@/components/ui/button';
import { Input } from '@/components/ui/input';
import React, { useEffect, useState } from 'react';
import { createRoot } from 'react-dom/client';
import type { Params } from 'whisper';

const App: React.FC = () => {
  const [modelPath, setModelPath] = useState<string>('ggml-large-v3-turbo.bin');
  const [transcription, setTranscription] = useState<string>('');

  useEffect(() => {
    const handleTranscription = (_: unknown, text: string) => {
      console.log('result :', text);
      setTranscription((prev) => `${prev}\n${text}`);
    };

    window.electron.ipcRenderer.on('whisper:transcription', handleTranscription);

    return () => {
      window.electron.ipcRenderer.removeListener('whisper:transcription', handleTranscription);
    };
  }, []);

  const initWhisper = async () => {
    const params: Params = {
      model: modelPath,
      language: 'en',
      n_threads: 10,
      step_ms: 1000,
      length_ms: 5000,
      keep_ms: 600,
      max_tokens: 32,
    };

    await window.whisper.initialize(params);
  };

  const startListening = async () => {
    await window.whisper.startListening();
  };

  const stopListening = async () => {
    await window.whisper.stopListening();
  };

  const cleanup = async () => {
    await window.whisper.cleanup();
  };

  return (
    <div className="flex flex-col items-center min-h-screen justify-center p-4 gap-4">
      <div className="flex w-full max-w-sm items-center space-x-2">
        <Input placeholder="Model Path" value={modelPath} onChange={(e) => setModelPath(e.target.value)} />
        <Button onClick={initWhisper}>Initialize</Button>
      </div>

      <div className="flex w-full max-w-sm items-center space-x-2">
        <Button onClick={startListening}>start</Button>
      </div>

      <div className="flex w-full max-w-sm items-center space-x-2">
        <Button onClick={stopListening}>stop</Button>
      </div>

      <div className="flex w-full max-w-sm items-center space-x-2">
        <Button onClick={cleanup}>cleanup</Button>
      </div>

      {transcription && (
        <div className="w-full max-w-lg mt-4 p-4 border rounded-md bg-gray-50">
          <h2 className="text-lg font-semibold mb-2">result :</h2>
          <p>{transcription}</p>
        </div>
      )}
    </div>
  );
};

const root = createRoot(document.body);
root.render(<App />);
