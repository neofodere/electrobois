// SPDX-FileCopyrightText: 2026 Néo Foderé de Frutos
// SPDX-License-Identifier: MIT

#include "include/riot.hpp"

Riot::Riot(Bus* systemBus) :
	bus(systemBus), swcha(0xFF), swchb(0x0B), timerCycle(1), timerShift(1), timerValue(0)
{
	ram.fill(0);
}

uint8_t Riot::Read(uint16_t address)
{
	if ((address & 0x0280) == 0x0280)
	{
		uint8_t reg = address & 0x0F;

		if (reg == 0x00) return swcha;
		if (reg == 0x02) return swchb;
		if (reg == 0x04) return timerValue;

		return 0;
	}

	// Internal 128-byte RAM mirror logic.
	return ram[address & 0x7F];
}

void Riot::Write(uint16_t address, uint8_t value)
{
	if ((address & 0x0280) == 0x0280)
	{
		// Timer configuration based on specific address line mapping.
		if ((address & 0x17) == 0x14)
		{
			timerValue = value;
			timerShift = 1;
			timerCycle = 1;
		}
		else if ((address & 0x17) == 0x15)
		{
			timerValue = value;
			timerShift = 8;
			timerCycle = 8;
		}
		else if ((address & 0x17) == 0x16)
		{
			timerValue = value;
			timerShift = 64;
			timerCycle = 64;
		}
		else if ((address & 0x17) == 0x17)
		{
			timerValue = value;
			timerShift = 1024;
			timerCycle = 1024;
		}
	}
	else
	{
		ram[address & 0x7F] = value;
	}
}

void Riot::UpdateInput(uint8_t keys)
{
	swcha = keys;
}

void Riot::UpdateConsole(uint8_t keys)
{
	swchb = keys;
}

void Riot::Tick()
{
	if (--timerCycle <= 0)
	{
		timerValue--;

		// Timer shift logic dictates it drops to shift 1 post-wrap.
		if (timerValue == 0xFF)
			timerShift = 1;

		timerCycle = timerShift;
	}
}

void Riot::Serialize(std::iostream& stream, bool isSaving)
{
	if (isSaving)
	{
		stream.write(reinterpret_cast<const char*>(ram.data()), ram.size());
		stream.write(reinterpret_cast<const char*>(&swcha), sizeof(swcha));
		stream.write(reinterpret_cast<const char*>(&swchb), sizeof(swchb));
		stream.write(reinterpret_cast<const char*>(&timerCycle), sizeof(timerCycle));
		stream.write(reinterpret_cast<const char*>(&timerShift), sizeof(timerShift));
		stream.write(reinterpret_cast<const char*>(&timerValue), sizeof(timerValue));
	}
	else
	{
		stream.read(reinterpret_cast<char*>(ram.data()), ram.size());
		stream.read(reinterpret_cast<char*>(&swcha), sizeof(swcha));
		stream.read(reinterpret_cast<char*>(&swchb), sizeof(swchb));
		stream.read(reinterpret_cast<char*>(&timerCycle), sizeof(timerCycle));
		stream.read(reinterpret_cast<char*>(&timerShift), sizeof(timerShift));
		stream.read(reinterpret_cast<char*>(&timerValue), sizeof(timerValue));
	}
}
