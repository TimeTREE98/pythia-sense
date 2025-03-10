declare module 'whisper' {
  export interface Params {
    n_threads?: number;
    step_ms?: number;
    length_ms?: number;
    keep_ms?: number;
    capture_id?: number;
    max_tokens?: number;
    audio_ctx?: number;

    vad_thold?: number;
    freq_thold?: number;

    translate?: boolean;
    no_fallback?: boolean;
    print_special?: boolean;
    no_context?: boolean;
    no_timestamps?: boolean;
    tinydiarize?: boolean;
    save_audio?: boolean;
    use_gpu?: boolean;
    flash_attn?: boolean;

    language?: string;
    model?: string;
    fname_out?: string;
  }

  // whisper.cpp
  export interface Whisper {
    initialize(params: Params): boolean;
    processAudio(): boolean;
    startListening(callback: (text: string) => void): boolean;
    stopListening(): boolean;
    cleanup(): boolean;
  }
}
