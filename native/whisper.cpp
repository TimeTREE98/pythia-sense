#include "napi.h"
#include "common-sdl.h"
#include "common.h"
#include "whisper.h"

#include <string>
#include <vector>
#include <thread>
#include <iostream>

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

    whisper_state(int32_t length_ms, int32_t capture_id) : audio(length_ms)
    {
        this->ctx = nullptr;
    }
};

static whisper_state *global_whisper_state = nullptr;

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

Napi::Value ProcessAudio(const Napi::CallbackInfo &info)
{
    Napi::Env env = info.Env();

    if (global_whisper_state == nullptr)
    {
        Napi::TypeError::New(env, "Whisper is not initialized!").ThrowAsJavaScriptException();
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

    return Napi::Boolean::New(env, true);
}

Napi::Object Init(Napi::Env env, Napi::Object exports)
{
    exports.Set(Napi::String::New(env, "initialize"), Napi::Function::New(env, Initialize));
    exports.Set(Napi::String::New(env, "processAudio"), Napi::Function::New(env, ProcessAudio));
    return exports;
}

NODE_API_MODULE(whisper, Init);
