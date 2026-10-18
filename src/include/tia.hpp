// SPDX-FileCopyrightText: 2026 Néo Foderé de Frutos
// SPDX-License-Identifier: MIT

#pragma once

#include "bus.hpp"
#include <array>
#include <cstdint>
#include <iostream>
#include <vector>

// One channel of the TIA's sound generator: a clock divider feeding a
// polynomial (or plain square) waveform, exactly as the real chip pairs
// AUDF/AUDC/AUDV per channel.
struct TiaAudioChannel
{
	uint8_t control = 0;
	uint8_t frequency = 0;
	uint8_t volume = 0;

	int divCounter = 0;
	uint32_t poly4 = 0;
	uint32_t poly5 = 0;
	uint32_t poly9 = 0;
	bool outputHigh = false;

	void Serialize(std::iostream& stream, bool isSaving);
};

class Tia
{
public:
	explicit Tia(Bus* systemBus);

	uint8_t Read(uint16_t address);
	void Write(uint16_t address, uint8_t value);
	void Tick();
	void Serialize(std::iostream& stream, bool isSaving);

	// Pulls and clears the audio samples accumulated since the last call,
	// letting main.cpp hand a whole frame's worth to SDL in one go.
	std::vector<int16_t> DrainAudioSamples();

	Bus* bus;
	int lines;
	std::array<uint32_t, 160 * 192> buffer;
	bool wsync;
	bool isPalColor = false;
	int x;
	uint8_t input4;

	static constexpr int AudioSampleRate = 31400;

private:
	uint8_t background, colupf, colup0, colup1;
	uint8_t pf0, pf1, pf2;
	uint8_t grp0, grp1;
	uint8_t ctrlpf;
	uint8_t vsyncReg;
	int posP0, posP1;
	int posM0, posM1, posBl;
	uint8_t nusiz0, nusiz1;
	bool enaM0, enaM1, enaBl;
	bool reflectP0, reflectP1;
	uint8_t hmp0 = 0, hmp1 = 0, hmm0 = 0, hmm1 = 0, hmbl = 0;

	// Collision latches, one bit pair per object combination, matching the
	// eight CXxx read addresses the hardware exposes.
	uint8_t cxm0p, cxm1p, cxp0fb, cxp1fb, cxm0fb, cxm1fb, cxblpf, cxppmm;

	TiaAudioChannel audio0, audio1;
	int audioClockAccumulator = 0;
	std::vector<int16_t> audioSamples;

	uint32_t DecodeColor(uint8_t colorByte, bool isPal = false);
	int ObjectWidth(uint8_t nusiz) const;
	void TickAudioChannel(TiaAudioChannel& channel);
	void GenerateAudioSample();
};
