// SPDX-FileCopyrightText: 2026 Néo Foderé de Frutos
// SPDX-License-Identifier: MIT

#pragma once

#include <cstdint>
#include <string>
#include <vector>

class Cpu6507;
class Riot;
class Tia;

class Bus
{
public:
	Bus();
	~Bus();

	bool LoadRom(const std::string& path);
	void ExecuteFrame();

	uint8_t Read(uint16_t address);
	void Write(uint16_t address, uint8_t value);

	Cpu6507* cpu;
	Riot* riot;
	Tia* tia;

	std::vector<uint8_t> rom;
	int currentBank;
};