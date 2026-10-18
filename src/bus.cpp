// SPDX-FileCopyrightText: 2026 Néo Foderé de Frutos
// SPDX-License-Identifier: MIT

#include "include/bus.hpp"
#include "include/6507.hpp"
#include "include/riot.hpp"
#include "include/tia.hpp"
#include <algorithm>
#include <fstream>
#include <sstream>

Bus::Bus()
{
	cpu = new Cpu6507(this);
	riot = new Riot(this);
	tia = new Tia(this);
	currentBank = 0;
}

Bus::~Bus()
{
	delete cpu;
	delete riot;
	delete tia;
}

bool Bus::LoadRom(const std::string& path)
{
	std::ifstream file(path, std::ios::binary | std::ios::ate);

	if (!file)
		return false;

	std::streamsize size = file.tellg();
	file.seekg(0, std::ios::beg);

	rom.resize(size);
	file.read(reinterpret_cast<char*>(rom.data()), size);

	// 2K ROMs are mirrored to fill the 4K address space allocated by the Atari 2600.
	if (size == 2048)
	{
		rom.resize(4096);
		std::copy(rom.begin(), rom.begin() + 2048, rom.begin() + 2048);
	}

	currentBank = (rom.size() > 4096) ? 1 : 0;
	cpu->Reset();

	return true;
}

void Bus::ExecuteFrame()
{
	int linesExecuted = 0;
	int lastX = tia->x;

	// NTSC specifies 262 scanlines per frame.
	while (linesExecuted < 262)
	{
		cpu->Step();

		if (tia->x < lastX)
			linesExecuted++;

		lastX = tia->x;
	}
}

uint8_t Bus::Read(uint16_t address)
{
	address &= 0x1FFF;

	// Hotspots for standard Atari F8 bankswitching scheme.
	if (address == 0x1FF8)
		currentBank = 0;
	else if (address == 0x1FF9 && rom.size() > 4096)
		currentBank = 1;

	if (!(address & 0x1000))
	{
		if ((address & 0x0280) == 0x0280 || (address & 0x0080))
			return riot->Read(address);
		return tia->Read(address);
	}

	int offset = (currentBank * 4096);
	return rom[(address & 0x0FFF) + offset];
}

void Bus::Write(uint16_t address, uint8_t value)
{
	address &= 0x1FFF;

	if (address == 0x1FF8)
		currentBank = 0;
	else if (address == 0x1FF9 && rom.size() > 4096)
		currentBank = 1;

	if (!(address & 0x1000))
	{
		if ((address & 0x0280) == 0x0280 || (address & 0x0080))
			riot->Write(address, value);
		else
			tia->Write(address, value);
	}
}