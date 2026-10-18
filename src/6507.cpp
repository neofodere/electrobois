// SPDX-FileCopyrightText: 2026 Néo Foderé de Frutos
// SPDX-License-Identifier: MIT

#include "include/6507.hpp"
#include "include/tia.hpp"
#include "include/riot.hpp"

Cpu6507::Cpu6507(Bus* systemBus) :
	bus(systemBus), a(0), x(0), y(0), sp(0xFD), st(0x24), pc(0), pageCrossed(false) {}

void Cpu6507::Reset()
{
	// Hardware vector for reset.
	pc = bus->Read(0x1FFC) | (bus->Read(0x1FFD) << 8);
}

void Cpu6507::UpdateFlags(uint8_t value)
{
	st = (st & ~0x82) | (value == 0 ? 0x02 : 0) | (value & 0x80);
}

void Cpu6507::Push(uint8_t value)
{
	bus->Write(0x0100 + sp--, value);
}

uint8_t Cpu6507::Pop()
{
	return bus->Read(0x0100 + ++sp);
}

uint16_t Cpu6507::AddrImmediate()
{
	return pc++;
}

uint16_t Cpu6507::AddrZeroPage()
{
	return bus->Read(pc++);
}

uint16_t Cpu6507::AddrZeroPageX()
{
	return (bus->Read(pc++) + x) & 0xFF;
}

uint16_t Cpu6507::AddrZeroPageY()
{
	return (bus->Read(pc++) + y) & 0xFF;
}

uint16_t Cpu6507::AddrAbsolute()
{
	uint16_t adr = bus->Read(pc) | (bus->Read(pc + 1) << 8);
	pc += 2;
	return adr;
}

uint16_t Cpu6507::AddrAbsoluteX()
{
	// Games routinely rely on the extra read cycle this produces when the
	// index carries into the next page, so callers that read (rather than
	// write) must check pageCrossed and bill an additional cycle for it.
	uint16_t base = AddrAbsolute();
	uint16_t effective = base + x;
	pageCrossed = (base & 0xFF00) != (effective & 0xFF00);
	return effective;
}

uint16_t Cpu6507::AddrAbsoluteY()
{
	uint16_t base = AddrAbsolute();
	uint16_t effective = base + y;
	pageCrossed = (base & 0xFF00) != (effective & 0xFF00);
	return effective;
}

uint16_t Cpu6507::AddrIndirectX()
{
	uint8_t ptr = bus->Read(pc++) + x;
	return bus->Read(ptr) | (bus->Read((ptr + 1) & 0xFF) << 8);
}

uint16_t Cpu6507::AddrIndirectY()
{
	uint8_t ptr = bus->Read(pc++);
	uint16_t base = bus->Read(ptr) | (bus->Read((ptr + 1) & 0xFF) << 8);
	uint16_t effective = base + y;
	pageCrossed = (base & 0xFF00) != (effective & 0xFF00);
	return effective;
}

void Cpu6507::Step()
{
	if (bus->tia->wsync)
	{
		// 6507 clock is 1/3 of the TIA clock. Check per-tick to avoid skipping zero-crossing.
		for (int i = 0; i < 3; ++i)
		{
			bus->tia->Tick();
			if (bus->tia->x == 0)
				bus->tia->wsync = false;
		}

		bus->riot->Tick();
		return;
	}

	uint8_t op = bus->Read(pc++);
	int cycles = 2;
	uint16_t adr = 0;
	uint8_t val = 0;
	pageCrossed = false;

	// Kept local so every addressing-mode variant of an instruction shares
	// one authoritative implementation instead of drifting out of sync.
	auto doAdc = [&](uint8_t operand)
	{
		// The overflow flag always reflects the plain binary sum, even in
		// BCD mode, matching how the real 6502 derives it internally.
		uint16_t binSum = (uint16_t)a + operand + (st & 0x01);
		bool overflow = (~(a ^ operand) & (a ^ (uint8_t)binSum) & 0x80) != 0;

		if (st & 0x08)
		{
			// BCD mode: correct each nibble back into 0-9 range after summing.
			uint16_t low = (a & 0x0F) + (operand & 0x0F) + (st & 0x01);
			uint16_t high = (a >> 4) + (operand >> 4);
			if (low > 9)
			{
				low -= 10;
				high++;
			}
			bool carry = high > 9;
			if (carry)
				high -= 10;
			a = ((high & 0x0F) << 4) | (low & 0x0F);
			st = (st & ~0x41) | (carry ? 0x01 : 0) | (overflow ? 0x40 : 0);
		}
		else
		{
			a = (uint8_t)binSum;
			st = (st & ~0x41) | ((binSum > 0xFF) ? 0x01 : 0) | (overflow ? 0x40 : 0);
		}

		UpdateFlags(a);
	};

	auto doSbc = [&](uint8_t operand)
	{
		uint8_t borrowIn = (st & 0x01) ? 0 : 1;
		int16_t binResult = (int16_t)a - operand - borrowIn;
		bool overflow = ((a ^ operand) & (a ^ (uint8_t)binResult) & 0x80) != 0;
		bool carry = binResult >= 0;

		if (st & 0x08)
		{
			// BCD mode: correct each nibble back into 0-9 range after borrowing.
			int16_t low = (a & 0x0F) - (operand & 0x0F) - borrowIn;
			int16_t high = (a >> 4) - (operand >> 4);
			if (low < 0)
			{
				low += 10;
				high--;
			}
			if (high < 0)
				high += 10;
			a = ((high & 0x0F) << 4) | (low & 0x0F);
		}
		else
		{
			a = (uint8_t)binResult;
		}

		st = (st & ~0x41) | (carry ? 0x01 : 0) | (overflow ? 0x40 : 0);
		UpdateFlags(a);
	};

	auto doCompare = [&](uint8_t reg, uint8_t operand)
	{
		UpdateFlags(reg - operand);
		st = (reg >= operand) ? (st | 0x01) : (st & ~0x01);
	};

	auto doAsl = [&](uint8_t operand) -> uint8_t
	{
		st = (st & ~0x01) | ((operand & 0x80) >> 7);
		operand <<= 1;
		UpdateFlags(operand);
		return operand;
	};

	auto doLsr = [&](uint8_t operand) -> uint8_t
	{
		st = (st & ~0x01) | (operand & 0x01);
		operand >>= 1;
		UpdateFlags(operand);
		return operand;
	};

	auto doRol = [&](uint8_t operand) -> uint8_t
	{
		uint8_t carry = st & 0x01;
		st = (st & ~0x01) | ((operand & 0x80) >> 7);
		operand = (operand << 1) | carry;
		UpdateFlags(operand);
		return operand;
	};

	auto doRor = [&](uint8_t operand) -> uint8_t
	{
		uint8_t carry = st & 0x01;
		st = (st & ~0x01) | (operand & 0x01);
		operand = (operand >> 1) | (carry << 7);
		UpdateFlags(operand);
		return operand;
	};

	// A taken branch always costs one extra cycle, and a second if it lands
	// in a different page; both delays are visible to scanline kernels that
	// count cycles to stay in sync with the beam.
	auto doBranch = [&](bool condition)
	{
		uint8_t offset = bus->Read(pc++);
		if (!condition)
			return;

		uint16_t oldPc = pc;
		pc += (int8_t)offset;
		cycles++;
		if ((oldPc & 0xFF00) != (pc & 0xFF00))
			cycles++;
	};

	auto doMemAdd = [&](uint16_t address, int delta) -> uint8_t
	{
		val = bus->Read(address) + delta;
		bus->Write(address, val);
		UpdateFlags(val);
		return val;
	};

	switch (op)
	{
		case 0xEA: break;

		// --- Loads ---
		case 0xA9: a = bus->Read(AddrImmediate()); UpdateFlags(a); break;
		case 0xA5: a = bus->Read(AddrZeroPage()); UpdateFlags(a); cycles = 3; break;
		case 0xB5: a = bus->Read(AddrZeroPageX()); UpdateFlags(a); cycles = 4; break;
		case 0xAD: a = bus->Read(AddrAbsolute()); UpdateFlags(a); cycles = 4; break;
		case 0xBD: a = bus->Read(AddrAbsoluteX()); UpdateFlags(a); cycles = pageCrossed ? 5 : 4; break;
		case 0xB9: a = bus->Read(AddrAbsoluteY()); UpdateFlags(a); cycles = pageCrossed ? 5 : 4; break;
		case 0xA1: a = bus->Read(AddrIndirectX()); UpdateFlags(a); cycles = 6; break;
		case 0xB1: a = bus->Read(AddrIndirectY()); UpdateFlags(a); cycles = pageCrossed ? 6 : 5; break;

		case 0xA2: x = bus->Read(AddrImmediate()); UpdateFlags(x); break;
		case 0xA6: x = bus->Read(AddrZeroPage()); UpdateFlags(x); cycles = 3; break;
		case 0xB6: x = bus->Read(AddrZeroPageY()); UpdateFlags(x); cycles = 4; break;
		case 0xAE: x = bus->Read(AddrAbsolute()); UpdateFlags(x); cycles = 4; break;
		case 0xBE: x = bus->Read(AddrAbsoluteY()); UpdateFlags(x); cycles = pageCrossed ? 5 : 4; break;

		case 0xA0: y = bus->Read(AddrImmediate()); UpdateFlags(y); break;
		case 0xA4: y = bus->Read(AddrZeroPage()); UpdateFlags(y); cycles = 3; break;
		case 0xB4: y = bus->Read(AddrZeroPageX()); UpdateFlags(y); cycles = 4; break;
		case 0xAC: y = bus->Read(AddrAbsolute()); UpdateFlags(y); cycles = 4; break;
		case 0xBC: y = bus->Read(AddrAbsoluteX()); UpdateFlags(y); cycles = pageCrossed ? 5 : 4; break;

		// --- Stores ---
		case 0x85: bus->Write(AddrZeroPage(), a); cycles = 3; break;
		case 0x95: bus->Write(AddrZeroPageX(), a); cycles = 4; break;
		case 0x8D: bus->Write(AddrAbsolute(), a); cycles = 4; break;
		case 0x9D: bus->Write(AddrAbsoluteX(), a); cycles = 5; break;
		case 0x99: bus->Write(AddrAbsoluteY(), a); cycles = 5; break;
		case 0x81: bus->Write(AddrIndirectX(), a); cycles = 6; break;
		case 0x91: bus->Write(AddrIndirectY(), a); cycles = 6; break;

		case 0x86: bus->Write(AddrZeroPage(), x); cycles = 3; break;
		case 0x96: bus->Write(AddrZeroPageY(), x); cycles = 4; break;
		case 0x8E: bus->Write(AddrAbsolute(), x); cycles = 4; break;

		case 0x84: bus->Write(AddrZeroPage(), y); cycles = 3; break;
		case 0x94: bus->Write(AddrZeroPageX(), y); cycles = 4; break;
		case 0x8C: bus->Write(AddrAbsolute(), y); cycles = 4; break;

		// --- Logic ---
		case 0x29: a &= bus->Read(AddrImmediate()); UpdateFlags(a); break;
		case 0x25: a &= bus->Read(AddrZeroPage()); UpdateFlags(a); cycles = 3; break;
		case 0x35: a &= bus->Read(AddrZeroPageX()); UpdateFlags(a); cycles = 4; break;
		case 0x2D: a &= bus->Read(AddrAbsolute()); UpdateFlags(a); cycles = 4; break;
		case 0x3D: a &= bus->Read(AddrAbsoluteX()); UpdateFlags(a); cycles = pageCrossed ? 5 : 4; break;
		case 0x39: a &= bus->Read(AddrAbsoluteY()); UpdateFlags(a); cycles = pageCrossed ? 5 : 4; break;
		case 0x21: a &= bus->Read(AddrIndirectX()); UpdateFlags(a); cycles = 6; break;
		case 0x31: a &= bus->Read(AddrIndirectY()); UpdateFlags(a); cycles = pageCrossed ? 6 : 5; break;

		case 0x09: a |= bus->Read(AddrImmediate()); UpdateFlags(a); break;
		case 0x05: a |= bus->Read(AddrZeroPage()); UpdateFlags(a); cycles = 3; break;
		case 0x15: a |= bus->Read(AddrZeroPageX()); UpdateFlags(a); cycles = 4; break;
		case 0x0D: a |= bus->Read(AddrAbsolute()); UpdateFlags(a); cycles = 4; break;
		case 0x1D: a |= bus->Read(AddrAbsoluteX()); UpdateFlags(a); cycles = pageCrossed ? 5 : 4; break;
		case 0x19: a |= bus->Read(AddrAbsoluteY()); UpdateFlags(a); cycles = pageCrossed ? 5 : 4; break;
		case 0x01: a |= bus->Read(AddrIndirectX()); UpdateFlags(a); cycles = 6; break;
		case 0x11: a |= bus->Read(AddrIndirectY()); UpdateFlags(a); cycles = pageCrossed ? 6 : 5; break;

		case 0x49: a ^= bus->Read(AddrImmediate()); UpdateFlags(a); break;
		case 0x45: a ^= bus->Read(AddrZeroPage()); UpdateFlags(a); cycles = 3; break;
		case 0x55: a ^= bus->Read(AddrZeroPageX()); UpdateFlags(a); cycles = 4; break;
		case 0x4D: a ^= bus->Read(AddrAbsolute()); UpdateFlags(a); cycles = 4; break;
		case 0x5D: a ^= bus->Read(AddrAbsoluteX()); UpdateFlags(a); cycles = pageCrossed ? 5 : 4; break;
		case 0x59: a ^= bus->Read(AddrAbsoluteY()); UpdateFlags(a); cycles = pageCrossed ? 5 : 4; break;
		case 0x41: a ^= bus->Read(AddrIndirectX()); UpdateFlags(a); cycles = 6; break;
		case 0x51: a ^= bus->Read(AddrIndirectY()); UpdateFlags(a); cycles = pageCrossed ? 6 : 5; break;

		case 0x24:
			val = bus->Read(AddrZeroPage());
			st = (st & ~0xC2) | (val & 0xC0) | ((a & val) == 0 ? 0x02 : 0);
			cycles = 3;
			break;

		case 0x2C:
			val = bus->Read(AddrAbsolute());
			st = (st & ~0xC2) | (val & 0xC0) | ((a & val) == 0 ? 0x02 : 0);
			cycles = 4;
			break;

		// --- Compare ---
		case 0xC9: doCompare(a, bus->Read(AddrImmediate())); break;
		case 0xC5: doCompare(a, bus->Read(AddrZeroPage())); cycles = 3; break;
		case 0xD5: doCompare(a, bus->Read(AddrZeroPageX())); cycles = 4; break;
		case 0xCD: doCompare(a, bus->Read(AddrAbsolute())); cycles = 4; break;
		case 0xDD: doCompare(a, bus->Read(AddrAbsoluteX())); cycles = pageCrossed ? 5 : 4; break;
		case 0xD9: doCompare(a, bus->Read(AddrAbsoluteY())); cycles = pageCrossed ? 5 : 4; break;
		case 0xC1: doCompare(a, bus->Read(AddrIndirectX())); cycles = 6; break;
		case 0xD1: doCompare(a, bus->Read(AddrIndirectY())); cycles = pageCrossed ? 6 : 5; break;

		case 0xE0: doCompare(x, bus->Read(AddrImmediate())); break;
		case 0xE4: doCompare(x, bus->Read(AddrZeroPage())); cycles = 3; break;
		case 0xEC: doCompare(x, bus->Read(AddrAbsolute())); cycles = 4; break;

		case 0xC0: doCompare(y, bus->Read(AddrImmediate())); break;
		case 0xC4: doCompare(y, bus->Read(AddrZeroPage())); cycles = 3; break;
		case 0xCC: doCompare(y, bus->Read(AddrAbsolute())); cycles = 4; break;

		// --- Arithmetic ---
		case 0x69: doAdc(bus->Read(AddrImmediate())); break;
		case 0x65: doAdc(bus->Read(AddrZeroPage())); cycles = 3; break;
		case 0x75: doAdc(bus->Read(AddrZeroPageX())); cycles = 4; break;
		case 0x6D: doAdc(bus->Read(AddrAbsolute())); cycles = 4; break;
		case 0x7D: doAdc(bus->Read(AddrAbsoluteX())); cycles = pageCrossed ? 5 : 4; break;
		case 0x79: doAdc(bus->Read(AddrAbsoluteY())); cycles = pageCrossed ? 5 : 4; break;
		case 0x61: doAdc(bus->Read(AddrIndirectX())); cycles = 6; break;
		case 0x71: doAdc(bus->Read(AddrIndirectY())); cycles = pageCrossed ? 6 : 5; break;

		case 0xE9: doSbc(bus->Read(AddrImmediate())); break;
		case 0xE5: doSbc(bus->Read(AddrZeroPage())); cycles = 3; break;
		case 0xF5: doSbc(bus->Read(AddrZeroPageX())); cycles = 4; break;
		case 0xED: doSbc(bus->Read(AddrAbsolute())); cycles = 4; break;
		case 0xFD: doSbc(bus->Read(AddrAbsoluteX())); cycles = pageCrossed ? 5 : 4; break;
		case 0xF9: doSbc(bus->Read(AddrAbsoluteY())); cycles = pageCrossed ? 5 : 4; break;
		case 0xE1: doSbc(bus->Read(AddrIndirectX())); cycles = 6; break;
		case 0xF1: doSbc(bus->Read(AddrIndirectY())); cycles = pageCrossed ? 6 : 5; break;

		// --- Increment / decrement ---
		case 0xE8: x++; UpdateFlags(x); break;
		case 0xCA: x--; UpdateFlags(x); break;
		case 0xC8: y++; UpdateFlags(y); break;
		case 0x88: y--; UpdateFlags(y); break;

		case 0xC6: doMemAdd(AddrZeroPage(), -1); cycles = 5; break;
		case 0xD6: doMemAdd(AddrZeroPageX(), -1); cycles = 6; break;
		case 0xCE: doMemAdd(AddrAbsolute(), -1); cycles = 6; break;
		case 0xDE: doMemAdd(AddrAbsoluteX(), -1); cycles = 7; break;

		case 0xE6: doMemAdd(AddrZeroPage(), 1); cycles = 5; break;
		case 0xF6: doMemAdd(AddrZeroPageX(), 1); cycles = 6; break;
		case 0xEE: doMemAdd(AddrAbsolute(), 1); cycles = 6; break;
		case 0xFE: doMemAdd(AddrAbsoluteX(), 1); cycles = 7; break;

		// --- Shifts / rotates ---
		case 0x0A: a = doAsl(a); break;
		case 0x06: adr = AddrZeroPage(); bus->Write(adr, doAsl(bus->Read(adr))); cycles = 5; break;
		case 0x16: adr = AddrZeroPageX(); bus->Write(adr, doAsl(bus->Read(adr))); cycles = 6; break;
		case 0x0E: adr = AddrAbsolute(); bus->Write(adr, doAsl(bus->Read(adr))); cycles = 6; break;
		case 0x1E: adr = AddrAbsoluteX(); bus->Write(adr, doAsl(bus->Read(adr))); cycles = 7; break;

		case 0x4A: a = doLsr(a); break;
		case 0x46: adr = AddrZeroPage(); bus->Write(adr, doLsr(bus->Read(adr))); cycles = 5; break;
		case 0x56: adr = AddrZeroPageX(); bus->Write(adr, doLsr(bus->Read(adr))); cycles = 6; break;
		case 0x4E: adr = AddrAbsolute(); bus->Write(adr, doLsr(bus->Read(adr))); cycles = 6; break;
		case 0x5E: adr = AddrAbsoluteX(); bus->Write(adr, doLsr(bus->Read(adr))); cycles = 7; break;

		case 0x2A: a = doRol(a); break;
		case 0x26: adr = AddrZeroPage(); bus->Write(adr, doRol(bus->Read(adr))); cycles = 5; break;
		case 0x36: adr = AddrZeroPageX(); bus->Write(adr, doRol(bus->Read(adr))); cycles = 6; break;
		case 0x2E: adr = AddrAbsolute(); bus->Write(adr, doRol(bus->Read(adr))); cycles = 6; break;
		case 0x3E: adr = AddrAbsoluteX(); bus->Write(adr, doRol(bus->Read(adr))); cycles = 7; break;

		case 0x6A: a = doRor(a); break;
		case 0x66: adr = AddrZeroPage(); bus->Write(adr, doRor(bus->Read(adr))); cycles = 5; break;
		case 0x76: adr = AddrZeroPageX(); bus->Write(adr, doRor(bus->Read(adr))); cycles = 6; break;
		case 0x6E: adr = AddrAbsolute(); bus->Write(adr, doRor(bus->Read(adr))); cycles = 6; break;
		case 0x7E: adr = AddrAbsoluteX(); bus->Write(adr, doRor(bus->Read(adr))); cycles = 7; break;

		// --- Transfers ---
		case 0x9A: sp = x; break;
		case 0xBA: x = sp; UpdateFlags(x); break;
		case 0xAA: x = a; UpdateFlags(x); break;
		case 0x8A: a = x; UpdateFlags(a); break;
		case 0xA8: y = a; UpdateFlags(y); break;
		case 0x98: a = y; UpdateFlags(a); break;

		// --- Stack ---
		case 0x48: Push(a); cycles = 3; break;
		case 0x68: a = Pop(); UpdateFlags(a); cycles = 4; break;
		case 0x08: Push(st | 0x30); cycles = 3; break;
		case 0x28: st = (Pop() & 0xCF) | 0x20; cycles = 4; break;

		// --- Flags ---
		case 0x18: st &= ~0x01; break;
		case 0x38: st |= 0x01; break;
		case 0x58: st &= ~0x04; break;
		case 0x78: st |= 0x04; break;
		case 0xB8: st &= ~0x40; break;
		case 0xD8: st &= ~0x08; break;
		case 0xF8: st |= 0x08; break;

		// --- Jumps / calls ---
		case 0x4C: pc = AddrAbsolute(); cycles = 3; break;

		case 0x6C:
			adr = AddrAbsolute();
			// Hardware bug replication for page boundary crossing.
			pc = bus->Read(adr) | (bus->Read((adr & 0xFF00) | ((adr + 1) & 0x00FF)) << 8);
			cycles = 5;
			break;

		case 0x20:
			adr = AddrAbsolute();
			Push((pc - 1) >> 8);
			Push((pc - 1) & 0xFF);
			pc = adr;
			cycles = 6;
			break;

		case 0x60:
			pc = Pop();
			pc |= (Pop() << 8);
			pc++;
			cycles = 6;
			break;

		case 0x00:
			// BRK is essentially unused by cartridge software on this console,
			// but is still honoured so stray or intentional uses behave sanely
			// instead of silently desyncing the instruction stream.
			pc++;
			Push(pc >> 8);
			Push(pc & 0xFF);
			Push(st | 0x30);
			st |= 0x04;
			pc = bus->Read(0xFFFE) | (bus->Read(0xFFFF) << 8);
			cycles = 7;
			break;

		case 0x40:
			st = (Pop() & 0xCF) | 0x20;
			pc = Pop();
			pc |= (Pop() << 8);
			cycles = 6;
			break;

		// --- Branches ---
		case 0xD0: doBranch(!(st & 0x02)); break;
		case 0xF0: doBranch(st & 0x02); break;
		case 0x10: doBranch(!(st & 0x80)); break;
		case 0x30: doBranch(st & 0x80); break;
		case 0x90: doBranch(!(st & 0x01)); break;
		case 0xB0: doBranch(st & 0x01); break;
		case 0x50: doBranch(!(st & 0x40)); break;
		case 0x70: doBranch(st & 0x40); break;

		default: break;
	}

	// Maintain system lockstep.
	while (cycles--)
	{
		bus->tia->Tick();
		bus->tia->Tick();
		bus->tia->Tick();
		bus->riot->Tick();
	}
}

void Cpu6507::Serialize(std::iostream& stream, bool isSaving)
{
	if (isSaving)
	{
		stream.write(reinterpret_cast<const char*>(&a), sizeof(a));
		stream.write(reinterpret_cast<const char*>(&x), sizeof(x));
		stream.write(reinterpret_cast<const char*>(&y), sizeof(y));
		stream.write(reinterpret_cast<const char*>(&sp), sizeof(sp));
		stream.write(reinterpret_cast<const char*>(&st), sizeof(st));
		stream.write(reinterpret_cast<const char*>(&pc), sizeof(pc));
	}
	else
	{
		stream.read(reinterpret_cast<char*>(&a), sizeof(a));
		stream.read(reinterpret_cast<char*>(&x), sizeof(x));
		stream.read(reinterpret_cast<char*>(&y), sizeof(y));
		stream.read(reinterpret_cast<char*>(&sp), sizeof(sp));
		stream.read(reinterpret_cast<char*>(&st), sizeof(st));
		stream.read(reinterpret_cast<char*>(&pc), sizeof(pc));
	}
}
