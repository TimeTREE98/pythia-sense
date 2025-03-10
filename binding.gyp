{
    "targets": [
        {
            "target_name": "whisper",
            "sources": ["native/whisper.cpp", "./whisper.cpp/examples/common-sdl.cpp"],
            "cflags_cc": ["-fexceptions", "-std=c++14"],
            "cflags": ["-fexceptions"],
            "include_dirs": [
                "<!@(node -p \"require('node-addon-api').include\")",
                "./whisper.cpp/examples",
                "./whisper.cpp/include",
                "./whisper.cpp/ggml/include",
                "<!@(pkg-config --cflags-only-I sdl2 | sed 's/-I//g')",  # needs to be installed: sdl2
            ],
            "dependencies": ["<!(node -p \"require('node-addon-api').gyp\")"],
            "defines": ["NAPI_CPP_EXCEPTIONS"],
            "libraries": [
                "<(module_root_dir)/whisper.cpp/build/src/libwhisper.dylib",
                "<!@(pkg-config --libs sdl2)",
            ],
            "conditions": [
                [
                    "OS=='mac'",
                    {
                        "xcode_settings": {
                            "GCC_ENABLE_CPP_EXCEPTIONS": "YES",
                            "OTHER_LDFLAGS": [
                                "-Wl,-rpath,<(module_root_dir)/whisper.cpp/build/src/",
                            ],
                        }
                    },
                ]
            ],
        }
    ]
}
