#pragma once  

#define SDL_MAIN_HANDLED
#include <SDL.h>  
#include <cstring> // Include for std::memcpy  
#include "input.hpp"


class Video {  
public:  
    Video() {  
        SDL_Init(SDL_INIT_VIDEO);  
        win = SDL_CreateWindow("GB", SDL_WINDOWPOS_CENTERED,  
            SDL_WINDOWPOS_CENTERED, 640, 576, 0);  
        tex = SDL_CreateTexture(SDL_CreateRenderer(win, -1, 0),  
            SDL_PIXELFORMAT_ARGB8888,  
            SDL_TEXTUREACCESS_STREAMING,  
            160, 144);  
    }  
    ~Video() { SDL_Quit(); }  

    void present(const uint32_t fb[144][160]) {  
        void* pixels; int pitch;  
        SDL_LockTexture(tex, nullptr, &pixels, &pitch);  

        // Fix for potential arithmetic overflow  
        size_t bufferSize = static_cast<size_t>(144) * 160 * sizeof(uint32_t);  
        std::memcpy(pixels, fb, bufferSize);  

        SDL_UnlockTexture(tex);  

        SDL_Renderer* ren = SDL_GetRenderer(win);  
        SDL_RenderClear(ren);  
        SDL_RenderCopy(ren, tex, nullptr, nullptr);  
        SDL_RenderPresent(ren);  
    }  
    bool ProcessInput(Input& input) {
        SDL_Event event;
        while (SDL_PollEvent(&event)) {
            // Quit if window is closed or ESC pressed
            if (event.type == SDL_QUIT) return false;
            if (event.type == SDL_KEYDOWN && event.key.keysym.sym == SDLK_ESCAPE) return false;

            bool pressed = (event.type == SDL_KEYDOWN);

            switch (event.key.keysym.sym) {
                // Direction keys
            case SDLK_RIGHT: input.set_button(GbButton::Right, pressed); break;
            case SDLK_LEFT:  input.set_button(GbButton::Left, pressed); break;
            case SDLK_UP:    input.set_button(GbButton::Up, pressed); break;
            case SDLK_DOWN:  input.set_button(GbButton::Down, pressed); break;

                // Action buttons
            case SDLK_TAB:     input.set_button(GbButton::A, pressed); break;
            case SDLK_x:     input.set_button(GbButton::B, pressed); break;
            case SDLK_RSHIFT:input.set_button(GbButton::Select, pressed); break;
            case SDLK_RETURN:input.set_button(GbButton::Start, pressed); break;
            }
        }

        return true; // keep running
    }
private:  
    SDL_Window* win = nullptr;  
    SDL_Texture* tex = nullptr;  
};
