// SPDX-FileCopyrightText: 2026 Néo Foderé de Frutos
// SPDX-License-Identifier: MIT

#pragma once

#include "bus.hpp"
#include <array>
#include <iostream>

class Riot
{
public:
	explicit Riot(Bus* systemBus);

	uint8_t Read(uint16_t address);
	void Write(uint16_t address, uint8_t value);

	void UpdateInput(uint8_t keys);
	void UpdateConsole(uint8_t keys);
	void Tick();
	void Serialize(std::iostream& stream, bool isSaving);

private:
	Bus* bus;
	std::array<uint8_t, 128> ram;
	uint8_t swcha;
	uint8_t swchb;

	int timerCycle;
	int timerShift;
	uint8_t timerValue;
};