// SPDX-FileCopyrightText: 2026 Néo Foderé de Frutos
// SPDX-License-Identifier: MIT

#pragma once

#include "bus.hpp"
#include <iostream>

class Cpu6507
{
public:
	explicit Cpu6507(Bus* systemBus);

	void Reset();
	void Step();
	void Serialize(std::iostream& stream, bool isSaving);

private:
	Bus* bus;
	uint8_t a, x, y, sp, st;
	uint16_t pc;

	// Set by the indexed addressing helpers below so Step() can add the
	// extra read cycle the real 6502 spends when an index crosses a page.
	bool pageCrossed;

	void UpdateFlags(uint8_t value);
	void Push(uint8_t value);
	uint8_t Pop();

	uint16_t AddrImmediate();
	uint16_t AddrZeroPage();
	uint16_t AddrZeroPageX();
	uint16_t AddrZeroPageY();
	uint16_t AddrAbsolute();
	uint16_t AddrAbsoluteX();
	uint16_t AddrAbsoluteY();
	uint16_t AddrIndirectX();
	uint16_t AddrIndirectY();
};
