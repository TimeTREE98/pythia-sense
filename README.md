# Pythia Sense: Real-time Speech Recognition with Whisper.cpp

## Overview

Pythia Sense is an Electron-based desktop application that provides real-time speech recognition capabilities using OpenAI's Whisper model through the whisper.cpp implementation. The name "Pythia" is derived from the Oracle of Delphi in ancient Greece, where the Pythia was carefully selected by priests who would interpret and relay the oracle's messages - similar to how this application interprets and transcribes human speech.

## Architecture

### Technology Stack

- **Desktop Framework**: Electron
- **UI Components**: Custom UI components with Tailwind CSS
- **Speech Recognition**: whisper.cpp (C++ implementation of OpenAI's Whisper)
- **Build System**: Webpack with custom rules for native modules

### Application Structure

The application follows a typical Electron architecture with three main processes:

1. **Main Process** (`index.ts`): Manages the application lifecycle, creates browser windows, and handles IPC communication between the renderer and native modules.

2. **Renderer Process** (`app.tsx`, `renderer.ts`): Handles the user interface and interactions.

3. **Native Module** (`whisper.cpp`): C++ implementation of the Whisper model that provides speech recognition capabilities.

### Communication Flow

1. User interactions in the UI trigger IPC calls to the main process
2. The main process communicates with the native whisper.cpp module
3. The native module processes audio input and returns transcription results
4. Results are sent back to the renderer process via IPC events

## Directory Structure

```
src/
├── components/       # React UI components
│   └── ui/           # Base UI components (shadcn)
├── lib/              # Utility functions
├── styles/           # Global CSS styles
├── types/            # TypeScript type definitions
├── app.tsx           # Main React application component
├── index.html        # HTML entry point
├── index.ts          # Electron main process
├── preload.ts        # Preload script for secure IPC
└── renderer.ts       # Renderer process entry point
```

## Key Components

### Native Whisper Module

The core functionality is provided by a native Node.js addon built from whisper.cpp. This module:

- Initializes the Whisper model with specified parameters
- Captures audio from the system microphone
- Processes audio in real-time using the Whisper model
- Returns transcription results via callbacks

### Electron Main Process

The main process (`index.ts`):

- Creates and manages the application window
- Exposes the native Whisper module's functionality via IPC handlers
- Forwards transcription results to the renderer process

### Preload Script

The preload script (`preload.ts`) securely exposes:

- IPC communication channels to the renderer
- A simplified API for interacting with the Whisper module

### React UI

The React application (`app.tsx`):

- Provides controls for initializing the model
- Allows starting and stopping speech recognition
- Displays transcription results in real-time

## Features

- Real-time speech recognition using OpenAI's Whisper model
- Support for different Whisper model sizes
- Configurable recognition parameters

## Technical Implementation Details

### Native Module Integration

The application uses Node-API (N-API) to create a bridge between JavaScript and the C++ implementation of Whisper. This allows for efficient audio processing while maintaining a responsive UI.

### Audio Processing

Audio is captured in chunks and processed in a separate thread to avoid blocking the main application. The processing includes:

1. Capturing audio from the system microphone
2. Preprocessing the audio data
3. Running the Whisper model inference
4. Returning transcription results

### IPC Communication

The application uses Electron's IPC (Inter-Process Communication) system to safely communicate between processes:

- `whisper:initialize`: Initializes the Whisper model with specified parameters
- `whisper:startListening`: Begins audio capture and transcription
- `whisper:stopListening`: Stops audio capture and transcription
- `whisper:transcription`: Event emitted when new transcription results are available
- `whisper:cleanup`: Releases resources used by the Whisper model
