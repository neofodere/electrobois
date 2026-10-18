// SPDX-FileCopyrightText: 2026 Néo Foderé de Frutos
// SPDX-License-Identifier: MIT

#include <SDL2/SDL.h>
#include <algorithm>
#include <cstdint>
#include <iostream>
#include <string>
#include <vector>
#include <cstring>
#include <cstdio>
#include "include/bus.hpp"
#include "include/riot.hpp"
#include "include/tia.hpp"


int main(int argc, char **argv)
{
	if (argc > 3 ||  argc <= 1)
	{
		std::cerr << "Usage: " << argv[0] << " <path_to_rom>\n";
		return 1;
	}
	if (argc == 3 && (std::strcmp(argv[2], "-ntsc") != 0 && std::strcmp(argv[2], "-pal") != 0))
	{
		std::cerr << "Usage: " << argv[0] << " <path_to_rom> -ntsc/-pal\n";
		return 1;
	}

	Bus systemBus;
	if (!systemBus.LoadRom(argv[1]))
		return 1;
	
	char *romName;
	char *lastSlash = std::strrchr(argv[1], '/');

	if (lastSlash != nullptr)
		romName = lastSlash + 1;
	else
		romName = argv[1];

	if (argc == 3 && std::strcmp(argv[2], "-pal") == 0)
		systemBus.tia->isPalColor = true;

	SDL_SetMainReady();
	if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO) < 0)
		return 1;

	SDL_Window* window = SDL_CreateWindow(("ELECTROBOIS: " + std::string(romName)).c_str(),
		SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, 640, 480,
		SDL_WINDOW_SHOWN | SDL_WINDOW_INPUT_FOCUS | SDL_WINDOW_RESIZABLE);

	if (!window)
	{
		SDL_Quit();
		return 1;
	}

	SDL_Surface* iconSurface = SDL_LoadBMP("img/icons/electrobois.bmp");
	if (iconSurface)
		SDL_SetWindowIcon(window, iconSurface);

	SDL_Renderer* renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED);
	if (!renderer)
		renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_SOFTWARE);
	if (!renderer)
	{
		if (iconSurface) SDL_FreeSurface(iconSurface);
		SDL_DestroyWindow(window);
		SDL_Quit();
		return 1;
	}

	SDL_Texture* displayTexture = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_ARGB8888,
		SDL_TEXTUREACCESS_STREAMING, 160, 192);

	SDL_AudioSpec desiredAudio{};
	desiredAudio.freq = Tia::AudioSampleRate;
	desiredAudio.format = AUDIO_S16SYS;
	desiredAudio.channels = 1;
	desiredAudio.samples = 1024;

	SDL_AudioDeviceID audioDevice = SDL_OpenAudioDevice(nullptr, 0, &desiredAudio, nullptr, 0);
	if (audioDevice != 0)
		SDL_PauseAudioDevice(audioDevice, 0);

	bool isRunning = true;
	uint8_t joystickState = 0xFF;
	bool resetHeld = false;
	bool selectHeld = false;
	SDL_Event event;

	while (isRunning)
	{

		uint32_t frameStartTime = SDL_GetTicks();

		while (SDL_PollEvent(&event))
		{
			if (event.type == SDL_QUIT)
				isRunning = false;
			else if (event.type == SDL_KEYDOWN || event.type == SDL_KEYUP)
			{
				bool isPress = (event.type == SDL_KEYDOWN);
				uint8_t mask = 0;

				switch (event.key.keysym.sym)
				{
					case SDLK_w: case SDLK_UP:    mask = 0x10; break;
					case SDLK_s: case SDLK_DOWN:  mask = 0x20; break;
					case SDLK_a: case SDLK_LEFT:  mask = 0x40; break;
					case SDLK_d: case SDLK_RIGHT: mask = 0x80; break;

					case SDLK_ESCAPE: isRunning = false; break;
					case SDLK_SPACE:  systemBus.tia->input4 = isPress ? 0x00 : 0xFF; break;

					case SDLK_r:   resetHeld = isPress; break;
					case SDLK_TAB: selectHeld = isPress; break;

					case SDLK_q:
						if (isPress && (SDL_GetModState() & KMOD_CTRL))
							isRunning = false;
						break;
				}

				if (mask != 0)
					joystickState = isPress ? (joystickState & ~mask) : (joystickState | mask);
			}
		}

		uint8_t consoleState = 0xFF;
		if (resetHeld) consoleState &= ~0x01;
		if (selectHeld) consoleState &= ~0x02;

		systemBus.riot->UpdateInput(joystickState);
		systemBus.riot->UpdateConsole(consoleState);

		SDL_SetTextureScaleMode(displayTexture, SDL_ScaleModeLinear);

		SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
		SDL_RenderClear(renderer);

		systemBus.ExecuteFrame();

		SDL_UpdateTexture(displayTexture, nullptr, systemBus.tia->buffer.data(), 160 * sizeof(uint32_t));
		SDL_RenderCopy(renderer, displayTexture, nullptr, nullptr);

		std::vector<int16_t> samples = systemBus.tia->DrainAudioSamples();

		if (audioDevice != 0 && !samples.empty())
		{
			Uint32 maxQueuedBytes = static_cast<Uint32>(Tia::AudioSampleRate * sizeof(int16_t) / 2);
			if (SDL_GetQueuedAudioSize(audioDevice) < maxQueuedBytes)
				SDL_QueueAudio(audioDevice, samples.data(),
					static_cast<Uint32>(samples.size() * sizeof(int16_t)));
		}

		SDL_RenderPresent(renderer);

		uint32_t elapsedTime = SDL_GetTicks() - frameStartTime;
		if (elapsedTime < 16)
			SDL_Delay(16 - elapsedTime);
	}

	if (audioDevice != 0)
		SDL_CloseAudioDevice(audioDevice);

	if (iconSurface) SDL_FreeSurface(iconSurface);

	SDL_DestroyTexture(displayTexture);
	SDL_DestroyRenderer(renderer);
	SDL_DestroyWindow(window);
	SDL_Quit();

	return 0;
}