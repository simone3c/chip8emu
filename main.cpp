#include "chip8.hpp"
#include "SDL3/SDL.h"
#include "SDL3/SDL_main.h"

void SDLCALL callback(void *userdata, SDL_AudioStream *astream, int additional_amount, int total_amount){
    static int current_sine_sample = 0;
    additional_amount /= sizeof (float);  /* convert from bytes to samples */
    while (additional_amount > 0){
        float samples[128];  /* this will feed 128 samples each iteration until we have enough. */
        const int total = SDL_min(additional_amount, SDL_arraysize(samples));
        int square_wave_period = 8100 / 440;
        const float volume = .25;

        for (int i = 0; i < total; i++){
            samples[i] = ((current_sine_sample / (square_wave_period/2) % 2))
                ? volume : -volume;
            current_sine_sample++;
        }

        /* wrapping around to avoid floating-point errors */
        current_sine_sample %= 8000;

        /* feed the new data to the stream. It will queue at the end, and trickle out as the hardware needs more data. */
        SDL_PutAudioStreamData(astream, samples, total * sizeof (float));
        additional_amount -= total;  /* subtract what we've just fed the stream. */
    }
}

int main(int argc, char** argv){
    int ips = 700; // instruction per second. 700 should be good
    int refresh_rate = 60; // FPS
    Chip8 c;
    const auto& screen = c.get_screen();
    std::FILE* f = std::fopen(argv[1], "r");
    if(!f){
        SDL_Log("Couldn't open the file: %s", argv[1]);
        return SDL_APP_FAILURE;
    }

    std::vector<uint8_t> rom(1 << 10);
    std::fread(rom.data(), sizeof(uint8_t), rom.size(), f);

    c.load(rom);

    SDL_Window *window = NULL;
    SDL_Renderer *renderer = NULL;
    SDL_AudioStream *stream = NULL;
    const SDL_AudioSpec spec{
        .format = SDL_AUDIO_F32,
        .channels = 1,
        .freq = 8100,
    };

    // init video / audio
    if(!SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO)){
        SDL_Log("Couldn't initialize SDL: %s", SDL_GetError());
        return SDL_APP_FAILURE;
    }

    if(!SDL_CreateWindowAndRenderer("examples/renderer/primitives", 1280, 720, 0, &window, &renderer)){
        SDL_Log("Couldn't create window/renderer: %s", SDL_GetError());
        return SDL_APP_FAILURE;
    }

    stream = SDL_OpenAudioDeviceStream(SDL_AUDIO_DEVICE_DEFAULT_PLAYBACK, &spec, callback, 0);
    if(!stream){
        SDL_Log("Couldn't create audio stream: %s", SDL_GetError());
        return SDL_APP_FAILURE;
    }

    SDL_SetRenderDrawColor(renderer, 0, 0, 0, SDL_ALPHA_OPAQUE);  /* dark gray, full alpha */
    SDL_RenderClear(renderer);
    SDL_RenderPresent(renderer);

    while(4){
        // read key events and update keyboard
        SDL_Event event;
        while(SDL_PollEvent(&event)){
            if (event.type == SDL_EVENT_QUIT){
                goto exit;
            }
        }

        auto start = std::chrono::high_resolution_clock().now();
        for(int i = 0; i < ips / refresh_rate; ++i){
            c.cpu_next_instr();
        }
        auto end = std::chrono::high_resolution_clock().now();
        float active_time = std::chrono::duration<float, std::milli>(end - start).count();
        assert(active_time < 16.67);
        SDL_Delay(active_time < 16.67 ? static_cast<uint32_t>(16.67f - active_time) : 0);

        // SDL render frame
        SDL_SetRenderDrawColor(renderer, 0, 0, 0, SDL_ALPHA_OPAQUE);
        SDL_RenderClear(renderer);
        SDL_SetRenderDrawColor(renderer, 255, 255, 255, SDL_ALPHA_OPAQUE);

        for(int i = 0; i < screen.size(); ++i){
            const int x = i % 64;
            const int y = i / 64;
            SDL_FRect rect = {20.f * x, 20.f * y, 20, 20};
            if(screen.test(i)){
                SDL_RenderFillRect(renderer, &rect);

            }
        }
        SDL_RenderPresent(renderer);

        for(int i = 0; i < 32; ++i){
            for(int j = 0; j < 64; ++j){
                bool a = screen[i * 64 + j];
                LOG("{}", a?"1":" ");
            }
            LOGLN("");
        }
        LOGLN("\n\n");

        // decrement timers and play sound if necessary
        if(c.get_delay_timer() > 0){
            c.decrement_delay_timer();
        }

        if(c.get_sound_timer() > 0){
            c.decrement_sound_timer();

            // play sound
            if(c.get_sound_timer() > 0){
                SDL_ResumeAudioStreamDevice(stream);
            }
            else{
                SDL_PauseAudioStreamDevice(stream);
            }
        }
    }

exit:
    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);

    SDL_Quit();
}