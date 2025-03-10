#include "napi.h"
#include "common-sdl.h"
#include "common.h"
#include "whisper.h"

#include <string>
#include <vector>
#include <thread>
#include <iostream>
#include <mutex>
#include <atomic>

struct whisper_params
{
    int32_t n_threads = std::min(4, (int32_t)std::thread::hardware_concurrency());
    int32_t step_ms = 3000;
    int32_t length_ms = 10000;
    int32_t keep_ms = 200;
    int32_t capture_id = -1;
    int32_t max_tokens = 32;
    int32_t audio_ctx = 0;

    float vad_thold = 0.6f;
    float freq_thold = 100.0f;

    bool translate = false;
    bool no_fallback = false;
    bool print_special = false;
    bool no_context = true;
    bool no_timestamps = false;
    bool tinydiarize = false;
    bool save_audio = false; // save audio to wav file
    bool use_gpu = true;
    bool flash_attn = false;

    std::string language = "en";
    std::string model = "models/ggml-base.en.bin";
    std::string fname_out;
};

struct whisper_state
{
    struct whisper_context *ctx;
    audio_async audio;
    whisper_params params;
    std::atomic<bool> is_running{false};
    std::thread processing_thread;
    std::mutex transcription_mutex;
    std::string last_transcription;

    whisper_state(int32_t length_ms, int32_t capture_id) : audio(length_ms)
    {
        this->ctx = nullptr;
    }
};

static whisper_state *global_whisper_state = nullptr;
static Napi::ThreadSafeFunction tsfn;

void ProcessAudioThread(whisper_state *whisperInstance)
{
    const int n_samples_step = whisperInstance->params.step_ms * WHISPER_SAMPLE_RATE / 1000;
    const int n_samples_len = whisperInstance->params.length_ms * WHISPER_SAMPLE_RATE / 1000;
    const int n_samples_keep = whisperInstance->params.keep_ms * WHISPER_SAMPLE_RATE / 1000;
    const int n_samples_30s = 30 * WHISPER_SAMPLE_RATE;

    std::vector<float> pcmf32(n_samples_30s, 0.0f);
    std::vector<float> pcmf32_old;
    std::vector<float> pcmf32_new(n_samples_30s, 0.0f);
    std::vector<whisper_token> prompt_tokens;

    int n_iter = 0;
    const int n_new_line = 1;

    while (whisperInstance->is_running.load())
    {
        while (true)
        {
            whisperInstance->audio.get(whisperInstance->params.step_ms, pcmf32_new);

            if ((int)pcmf32_new.size() > 2 * n_samples_step)
            {
                whisperInstance->audio.clear();
                continue;
            }

            if ((int)pcmf32_new.size() >= n_samples_step)
            {
                whisperInstance->audio.clear();
                break;
            }

            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }

        const int n_samples_new = pcmf32_new.size();
        const int n_samples_take = std::min((int)pcmf32_old.size(), std::max(0, n_samples_keep + n_samples_len - n_samples_new));

        pcmf32.resize(n_samples_new + n_samples_take);

        for (int i = 0; i < n_samples_take; i++)
        {
            pcmf32[i] = pcmf32_old[pcmf32_old.size() - n_samples_take + i];
        }

        memcpy(pcmf32.data() + n_samples_take, pcmf32_new.data(), n_samples_new * sizeof(float));
        pcmf32_old = pcmf32;

        whisper_full_params wparams = whisper_full_default_params(WHISPER_SAMPLING_GREEDY);

        wparams.print_progress = false;
        wparams.print_special = whisperInstance->params.print_special;
        wparams.print_realtime = false;
        wparams.print_timestamps = !whisperInstance->params.no_timestamps;
        wparams.translate = whisperInstance->params.translate;
        wparams.single_segment = true;
        wparams.max_tokens = whisperInstance->params.max_tokens;
        wparams.language = whisperInstance->params.language.c_str();
        wparams.n_threads = whisperInstance->params.n_threads;
        wparams.audio_ctx = whisperInstance->params.audio_ctx;
        wparams.tdrz_enable = whisperInstance->params.tinydiarize;
        wparams.temperature_inc = whisperInstance->params.no_fallback ? 0.0f : wparams.temperature_inc;
        wparams.prompt_tokens = whisperInstance->params.no_context ? nullptr : prompt_tokens.data();
        wparams.prompt_n_tokens = whisperInstance->params.no_context ? 0 : prompt_tokens.size();

        if (whisper_full(whisperInstance->ctx, wparams, pcmf32.data(), pcmf32.size()) != 0)
        {
            fprintf(stderr, "Failed to process audio\n");
            continue;
        }

        const int n_segments = whisper_full_n_segments(whisperInstance->ctx);
        for (int i = 0; i < n_segments; ++i)
        {
            const char *text = whisper_full_get_segment_text(whisperInstance->ctx, i);

            if (text != nullptr && strlen(text) > 0)
            {
                std::string transcription = text;

                {
                    std::lock_guard<std::mutex> lock(whisperInstance->transcription_mutex);
                    whisperInstance->last_transcription = transcription;
                }

                if (tsfn)
                {
                    tsfn.BlockingCall([transcription](Napi::Env env, Napi::Function jsCallback)
                                      { jsCallback.Call({Napi::String::New(env, transcription)}); });
                }
            }
        }

        ++n_iter;

        if ((n_iter % n_new_line) == 0)
        {
            pcmf32_old = std::vector<float>(pcmf32.end() - n_samples_keep, pcmf32.end());

            if (!whisperInstance->params.no_context)
            {
                prompt_tokens.clear();

                const int n_segments = whisper_full_n_segments(whisperInstance->ctx);
                for (int i = 0; i < n_segments; ++i)
                {
                    const int token_count = whisper_full_n_tokens(whisperInstance->ctx, i);
                    for (int j = 0; j < token_count; ++j)
                    {
                        prompt_tokens.push_back(whisper_full_get_token_id(whisperInstance->ctx, i, j));
                    }
                }
            }
        }
    }
}

Napi::Value Initialize(const Napi::CallbackInfo &info)
{
    Napi::Env env = info.Env();

    if (global_whisper_state != nullptr)
    {
        Napi::TypeError::New(env, "Whisper is already initialized!").ThrowAsJavaScriptException();
        return env.Undefined();
    }

    if (info.Length() < 1 || !info[0].IsObject())
    {
        Napi::TypeError::New(env, "Expected an object as the first argument").ThrowAsJavaScriptException();
        return env.Undefined();
    }

    Napi::Object paramsObj = info[0].As<Napi::Object>();

    whisper_params params;

    if (paramsObj.Has("n_threads"))
        params.n_threads = paramsObj.Get("n_threads").As<Napi::Number>().Int32Value();
    if (paramsObj.Has("step_ms"))
        params.step_ms = paramsObj.Get("step_ms").As<Napi::Number>().Int32Value();
    if (paramsObj.Has("length_ms"))
        params.length_ms = paramsObj.Get("length_ms").As<Napi::Number>().Int32Value();
    if (paramsObj.Has("keep_ms"))
        params.keep_ms = paramsObj.Get("keep_ms").As<Napi::Number>().Int32Value();
    if (paramsObj.Has("capture_id"))
        params.capture_id = paramsObj.Get("capture_id").As<Napi::Number>().Int32Value();
    if (paramsObj.Has("max_tokens"))
        params.max_tokens = paramsObj.Get("max_tokens").As<Napi::Number>().Int32Value();
    if (paramsObj.Has("audio_ctx"))
        params.audio_ctx = paramsObj.Get("audio_ctx").As<Napi::Number>().Int32Value();

    if (paramsObj.Has("vad_thold"))
        params.vad_thold = paramsObj.Get("vad_thold").As<Napi::Number>().FloatValue();
    if (paramsObj.Has("freq_thold"))
        params.freq_thold = paramsObj.Get("freq_thold").As<Napi::Number>().FloatValue();

    if (paramsObj.Has("translate"))
        params.translate = paramsObj.Get("translate").As<Napi::Boolean>().Value();
    if (paramsObj.Has("no_fallback"))
        params.no_fallback = paramsObj.Get("no_fallback").As<Napi::Boolean>().Value();
    if (paramsObj.Has("print_special"))
        params.print_special = paramsObj.Get("print_special").As<Napi::Boolean>().Value();
    if (paramsObj.Has("no_context"))
        params.no_context = paramsObj.Get("no_context").As<Napi::Boolean>().Value();
    if (paramsObj.Has("no_timestamps"))
        params.no_timestamps = paramsObj.Get("no_timestamps").As<Napi::Boolean>().Value();
    if (paramsObj.Has("tinydiarize"))
        params.tinydiarize = paramsObj.Get("tinydiarize").As<Napi::Boolean>().Value();
    if (paramsObj.Has("save_audio"))
        params.save_audio = paramsObj.Get("save_audio").As<Napi::Boolean>().Value();
    if (paramsObj.Has("use_gpu"))
        params.use_gpu = paramsObj.Get("use_gpu").As<Napi::Boolean>().Value();
    if (paramsObj.Has("flash_attn"))
        params.flash_attn = paramsObj.Get("flash_attn").As<Napi::Boolean>().Value();

    if (paramsObj.Has("language"))
        params.language = paramsObj.Get("language").As<Napi::String>();
    if (paramsObj.Has("model"))
        params.model = paramsObj.Get("model").As<Napi::String>();
    if (paramsObj.Has("fname_out"))
        params.fname_out = paramsObj.Get("fname_out").As<Napi::String>();

    whisper_state *whisperInstance = new whisper_state(params.length_ms, params.capture_id);
    whisperInstance->params = params;

    struct whisper_context_params cparams = whisper_context_default_params();
    cparams.use_gpu = params.use_gpu;
    cparams.flash_attn = params.flash_attn;

    whisperInstance->ctx = whisper_init_from_file_with_params(params.model.c_str(), cparams);
    if (whisperInstance->ctx == nullptr)
    {
        delete whisperInstance;
        Napi::TypeError::New(env, "Failed to initialize Whisper model").ThrowAsJavaScriptException();
        return env.Undefined();
    }

    global_whisper_state = whisperInstance;

    return Napi::Boolean::New(env, true);
}

Napi::Value StartListening(const Napi::CallbackInfo &info)
{
    Napi::Env env = info.Env();

    if (global_whisper_state == nullptr)
    {
        Napi::TypeError::New(env, "Whisper is not initialized!").ThrowAsJavaScriptException();
        return env.Undefined();
    }

    if (info.Length() < 1 || !info[0].IsFunction())
    {
        Napi::TypeError::New(env, "Expected a function as the first argument").ThrowAsJavaScriptException();
        return env.Undefined();
    }

    whisper_state *whisperInstance = global_whisper_state;

    if (!whisperInstance->audio.init(whisperInstance->params.capture_id, WHISPER_SAMPLE_RATE))
    {
        whisper_free(whisperInstance->ctx);
        delete whisperInstance;
        Napi::TypeError::New(env, "Failed to initialize audio capture").ThrowAsJavaScriptException();
        return env.Undefined();
    }

    whisperInstance->audio.resume();

    if (whisperInstance->is_running.load())
    {
        whisperInstance->is_running.store(false);
        if (whisperInstance->processing_thread.joinable())
        {
            whisperInstance->processing_thread.join();
        }
    }

    Napi::Function callback = info[0].As<Napi::Function>();
    tsfn = Napi::ThreadSafeFunction::New(
        env,
        callback,
        "WhisperCallback",
        0,
        1);

    whisperInstance->is_running.store(true);
    whisperInstance->processing_thread = std::thread(ProcessAudioThread, whisperInstance);

    return Napi::Boolean::New(env, true);
}

Napi::Value StopListening(const Napi::CallbackInfo &info)
{
    Napi::Env env = info.Env();

    if (global_whisper_state == nullptr)
    {
        Napi::TypeError::New(env, "Whisper is not initialized!").ThrowAsJavaScriptException();
        return env.Undefined();
    }

    whisper_state *whisperInstance = global_whisper_state;

    whisperInstance->audio.pause();

    if (whisperInstance->is_running.load())
    {
        whisperInstance->is_running.store(false);
        if (whisperInstance->processing_thread.joinable())
        {
            whisperInstance->processing_thread.join();
        }
    }

    if (tsfn)
    {
        tsfn.Release();
    }

    return Napi::Boolean::New(env, true);
}

Napi::Value Cleanup(const Napi::CallbackInfo &info)
{
    Napi::Env env = info.Env();

    if (global_whisper_state == nullptr)
    {
        return Napi::Boolean::New(env, true);
    }

    whisper_state *whisperInstance = global_whisper_state;

    if (whisperInstance->is_running.load())
    {
        whisperInstance->is_running.store(false);
        if (whisperInstance->processing_thread.joinable())
        {
            whisperInstance->processing_thread.join();
        }
    }

    if (tsfn)
    {
        tsfn.Release();
    }

    whisperInstance->audio.pause();

    delete whisperInstance;
    global_whisper_state = nullptr;

    return Napi::Boolean::New(env, true);
}

Napi::Object Init(Napi::Env env, Napi::Object exports)
{
    exports.Set(Napi::String::New(env, "initialize"), Napi::Function::New(env, Initialize));
    exports.Set(Napi::String::New(env, "startListening"), Napi::Function::New(env, StartListening));
    exports.Set(Napi::String::New(env, "stopListening"), Napi::Function::New(env, StopListening));
    exports.Set(Napi::String::New(env, "cleanup"), Napi::Function::New(env, Cleanup));
    return exports;
}

NODE_API_MODULE(whisper, Init);
