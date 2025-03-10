import { Button } from '@/components/ui/button';
import { Input } from '@/components/ui/input';
import React, { useState } from 'react';
import { createRoot } from 'react-dom/client';
import type { Params } from 'whisper';

const App: React.FC = () => {
  const [modelPath, setModelPath] = useState<string>('ggml-large-v3-turbo.bin');

  const initWhisper = async () => {
    const params: Params = {
      model: modelPath,
    };

    const result = await window.whisper.initialize(params);
    console.log(result);
  };

  return (
    <div className="flex flex-col items-center min-h-screen justify-center">
      <div className="flex w-full max-w-sm items-center space-x-2">
        <Input placeholder="Model Path" value={modelPath} onChange={(e) => setModelPath(e.target.value)} />
        <Button onClick={initWhisper}>Initialize</Button>
      </div>
    </div>
  );
};

const root = createRoot(document.body);
root.render(<App />);
