// SPDX-FileCopyrightText: 2026 Néo Foderé de Frutos
// SPDX-License-Identifier: MIT

#include "include/tia.hpp"

Tia::Tia(Bus* systemBus) :
	bus(systemBus), lines(0), wsync(false), x(0), input4(0xFF),
	background(0), colupf(0), colup0(0), colup1(0), pf0(0), pf1(0), pf2(0),
	grp0(0), grp1(0), ctrlpf(0), vsyncReg(0), posP0(0), posP1(0),
	posM0(0), posM1(0), posBl(0), nusiz0(0), nusiz1(0),
	enaM0(false), enaM1(false), enaBl(false), reflectP0(false), reflectP1(false),
	cxm0p(0), cxm1p(0), cxp0fb(0), cxp1fb(0), cxm0fb(0), cxm1fb(0), cxblpf(0), cxppmm(0)
{
	buffer.fill(0xFF000000);
}

uint8_t Tia::Read(uint16_t address)
{
	address &= 0x0F;
	switch (address)
	{
		case 0x00: return cxm0p;
		case 0x01: return cxm1p;
		case 0x02: return cxp0fb;
		case 0x03: return cxp1fb;
		case 0x04: return cxm0fb;
		case 0x05: return cxm1fb;
		case 0x06: return cxblpf;
		case 0x07: return cxppmm;
		case 0x0C: return input4;
		case 0x0D: return 0xFF; // Second joystick button; no controller wired to it.
	}
	return 0;
}

void Tia::Write(uint16_t address, uint8_t value)
{
	address &= 0x3F;
	switch (address)
	{
		case 0x00:
			if ((vsyncReg & 0x02) && !(value & 0x02))
				lines = 0;
			vsyncReg = value;
			break;

		case 0x02: wsync = true; break;
		case 0x04: nusiz0 = value; break;
		case 0x05: nusiz1 = value; break;
		case 0x06: colup0 = value; break;
		case 0x07: colup1 = value; break;
		case 0x08: colupf = value; break;
		case 0x09: background = value; break;
		case 0x0A: ctrlpf = value; break;
		case 0x0B: reflectP0 = (value & 0x08) != 0; break;
		case 0x0C: reflectP1 = (value & 0x08) != 0; break;
		case 0x0D: pf0 = value; break;
		case 0x0E: pf1 = value; break;
		case 0x0F: pf2 = value; break;
		case 0x10: posP0 = x; break;
		case 0x11: posP1 = x; break;
		case 0x12: posM0 = x; break;
		case 0x13: posM1 = x; break;
		case 0x14: posBl = x; break;
		case 0x15: audio0.control = value & 0x0F; break;
		case 0x16: audio1.control = value & 0x0F; break;
		case 0x17: audio0.frequency = value & 0x1F; break;
		case 0x18: audio1.frequency = value & 0x1F; break;
		case 0x19: audio0.volume = value & 0x0F; break;
		case 0x1A: audio1.volume = value & 0x0F; break;
		case 0x1B: grp0 = value; break;
		case 0x1C: grp1 = value; break;
		case 0x1D: enaM0 = (value & 0x02) != 0; break;
		case 0x1E: enaM1 = (value & 0x02) != 0; break;
		case 0x1F: enaBl = (value & 0x02) != 0; break;
		case 0x20: hmp0 = value >> 4; break;
		case 0x21: hmp1 = value >> 4; break;
		case 0x22: hmm0 = value >> 4; break;
		case 0x23: hmm1 = value >> 4; break;
		case 0x24: hmbl = value >> 4; break;

		case 0x2A:
		{
			// HMOVE nudges every movable object by its own signed motion
			// value; the wrap uses 228 because that is the width of the
			// horizontal counter positions are actually measured against.
			auto ApplyMotion = [](int& pos, uint8_t motionVal)
			{
				// TIA applies left shift for 0-7, and right shift for 8-15 due to late clocking.
				if (motionVal < 8)
					pos -= motionVal;
				else
					pos += (16 - motionVal);

				pos = ((pos % 228) + 228) % 228;
			};

			ApplyMotion(posP0, hmp0);
			ApplyMotion(posP1, hmp1);
			ApplyMotion(posM0, hmm0);
			ApplyMotion(posM1, hmm1);
			ApplyMotion(posBl, hmbl);
			break;
		}

		case 0x2B:
			hmp0 = hmp1 = hmm0 = hmm1 = hmbl = 0;
			break;

		case 0x2C:
			cxm0p = cxm1p = cxp0fb = cxp1fb = 0;
			cxm0fb = cxm1fb = cxblpf = cxppmm = 0;
			break;
	}
}


static const uint32_t kNtscPalette[128] =
{
	0x000000, 0x445400, 0x673900, 0x7b2504,
	0x7d122c, 0x730871, 0x5d0b92, 0x401599,
	0x252593, 0x0f3480, 0x04425a, 0x044f30,
	0x0f550a, 0x1f5100, 0x344600, 0x463e00,
	0x505050, 0x586800, 0x7b4d00, 0x8f3918,
	0x912640, 0x871c85, 0x711fa6, 0x5429ad,
	0x3939a7, 0x234894, 0x18566e, 0x186344,
	0x23691e, 0x336505, 0x485a00, 0x5a5205,
	0x646464, 0x6c7c00, 0x8f6100, 0xa34d2c,
	0xa53a54, 0x9b3099, 0x8533ba, 0x683dc1,
	0x4d4dbb, 0x375ca8, 0x2c6a82, 0x2c7758,
	0x377d32, 0x477919, 0x5c6e14, 0x6e6619,
	0x787878, 0x809000, 0xa37513, 0xb76140,
	0xb94e68, 0xaf44ad, 0x9947ce, 0x7c51d5,
	0x6161cf, 0x4b70bc, 0x407e96, 0x408b6c,
	0x4b9146, 0x5b8d2d, 0x708228, 0x827a2d,
	0x8c8c8c, 0x94a414, 0xb78927, 0xcb7554,
	0xcd627c, 0xc358c1, 0xad5be2, 0x9065e9,
	0x7575e3, 0x5f84d0, 0x5492aa, 0x549f80,
	0x5fa55a, 0x6fa141, 0x84963c, 0x968e41,
	0xa0a0a0, 0xa8b828, 0xcb9d3b, 0xdf8968,
	0xe17690, 0xd76cd5, 0xc16ff6, 0xa479fd,
	0x8989f7, 0x7398e4, 0x68a6be, 0x68b394,
	0x73b96e, 0x83b555, 0x98aa50, 0xaaa255,
	0xb4b4b4, 0xbccc3c, 0xdfb14f, 0xf39d7c,
	0xf58aa4, 0xeb80e9, 0xd583ff, 0xb88dff,
	0x9d9dff, 0x87acf8, 0x7bbad2, 0x7cc7a8,
	0x87cd82, 0x97c969, 0xacbe64, 0xbeb669,
	0xc8c8c8, 0xd0e050, 0xf3c563, 0xffb190,
	0xff9eb8, 0xff94fd, 0xe997ff, 0xcca1ff,
	0xb1b1ff, 0x9bc0ff, 0x90cee6, 0x90dbbc,
	0x9be196, 0xabdd7d, 0xc0d278, 0xd2ca7d
};

static const uint32_t kPalPalette[128] =
{
	0x000000, 0x000000, 0x533a00, 0x1b5800,
	0x6a2900, 0x075b00, 0x741b2f, 0x00572e,
	0x6d165f, 0x014c5e, 0x5f1588, 0x123b87,
	0x451e9d, 0x2a2b9e, 0x000000, 0x000000,
	0x4c4c4c, 0x4c4c4c, 0x674e00, 0x2f6c00,
	0x7e3d12, 0x1b6f11, 0x882f43, 0x106b42,
	0x812a73, 0x156072, 0x73299c, 0x264f9b,
	0x5932b1, 0x3e3fb2, 0x4c4c4c, 0x4c4c4c,
	0x606060, 0x606060, 0x7b6203, 0x438001,
	0x925126, 0x2f8325, 0x9c4357, 0x247f56,
	0x953e87, 0x297486, 0x873db0, 0x3a63af,
	0x6d46c5, 0x5253c6, 0x606060, 0x606060,
	0x747474, 0x747474, 0x8f7617, 0x579415,
	0xa6653a, 0x439739, 0xb0576b, 0x38936a,
	0xa9529b, 0x3d889a, 0x9b51c4, 0x4e77c3,
	0x815ad9, 0x6667da, 0x747474, 0x747474,
	0x888888, 0x888888, 0xa38a2b, 0x6ba829,
	0xba794e, 0x57ab4d, 0xc46b7f, 0x4ca77e,
	0xbd66af, 0x519cae, 0xaf65d8, 0x628bd7,
	0x956eed, 0x7a7bee, 0x888888, 0x888888,
	0x9c9c9c, 0x9c9c9c, 0xb79e3f, 0x7fbc3d,
	0xce8d62, 0x6bbf61, 0xd87f93, 0x60bb92,
	0xd17ac3, 0x65b0c2, 0xc379ec, 0x769feb,
	0xa982ff, 0x8e8fff, 0x9c9c9c, 0x9c9c9c,
	0xb0b0b0, 0xb0b0b0, 0xcbb253, 0x93d051,
	0xe2a176, 0x7fd375, 0xec93a7, 0x74cfa6,
	0xe58ed7, 0x79c4d6, 0xd78dff, 0x8ab3ff,
	0xbd96ff, 0xa2a3ff, 0xb0b0b0, 0xb0b0b0,
	0xc4c4c4, 0xc4c4c4, 0xdfc667, 0xa7e465,
	0xf6b58a, 0x93e789, 0xffa7bb, 0x88e3ba,
	0xf9a2eb, 0x8dd8ea, 0xeba1ff, 0x9ec7ff,
	0xd1aaff, 0xb6b7ff, 0xc4c4c4, 0xc4c4c4
};



uint32_t Tia::DecodeColor(uint8_t colorByte, bool isPalColor)
{
	uint8_t hue = (colorByte >> 4) & 0x0F;
	uint8_t lum = (colorByte >> 1) & 0x07;
	uint32_t rgb = isPalColor ? kPalPalette[lum * 16 + hue] : kNtscPalette[lum * 16 + hue];
	return 0xFF000000 | rgb;
}

int Tia::ObjectWidth(uint8_t nusiz) const
{
	// Shared by NUSIZ0/1 (missile width) and CTRLPF (ball width): both
	// encode the same "1/2/4/8 pixels" choice in bits 4-5.
	return 1 << ((nusiz >> 4) & 0x03);
}

void Tia::Tick()
{
	if (x >= 68 && x < 228 && lines >= 40 && lines < 232)
	{
		int px = x - 68;
		int py = lines - 40;
		int pfBit = px / 4;
		bool drawPlayfield = false;

		// Reflection logic governed by CTRLPF bit 0.
		if (pfBit >= 20)
			pfBit = (ctrlpf & 0x01) ? (39 - pfBit) : (pfBit - 20);

		if (pfBit < 4) drawPlayfield = (pf0 >> (4 + pfBit)) & 1;
		else if (pfBit < 12) drawPlayfield = (pf1 >> (7 - (pfBit - 4))) & 1;
		else drawPlayfield = (pf2 >> (pfBit - 12)) & 1;

		// NUSIZx bits 0-2 of 5 or 7 select double/quad-width sprites; other
		// values would additionally draw extra copies, which this emulator
		// does not reproduce.
		int stretchP0 = ((nusiz0 & 0x07) == 0x05) ? 2 : (((nusiz0 & 0x07) == 0x07) ? 4 : 1);
		int stretchP1 = ((nusiz1 & 0x07) == 0x05) ? 2 : (((nusiz1 & 0x07) == 0x07) ? 4 : 1);

		bool drawP0 = false;
		bool drawP1 = false;

		if (x >= posP0 && x < posP0 + 8 * stretchP0)
		{
			int bitIndex = (x - posP0) / stretchP0;
			drawP0 = (grp0 >> (reflectP0 ? bitIndex : (7 - bitIndex))) & 1;
		}

		if (x >= posP1 && x < posP1 + 8 * stretchP1)
		{
			int bitIndex = (x - posP1) / stretchP1;
			drawP1 = (grp1 >> (reflectP1 ? bitIndex : (7 - bitIndex))) & 1;
		}

		int widthM0 = ObjectWidth(nusiz0);
		int widthM1 = ObjectWidth(nusiz1);
		int widthBl = ObjectWidth(ctrlpf);

		bool drawM0 = enaM0 && x >= posM0 && x < posM0 + widthM0;
		bool drawM1 = enaM1 && x >= posM1 && x < posM1 + widthM1;
		bool drawBl = enaBl && x >= posBl && x < posBl + widthBl;

		// Collision latches accumulate until software clears them via CXCLR,
		// so games can sample "did this happen anywhere on the line" once
		// per frame instead of racing the beam pixel by pixel.
		if (drawM0 && drawP1) cxm0p |= 0x80;
		if (drawM0 && drawP0) cxm0p |= 0x40;
		if (drawM1 && drawP0) cxm1p |= 0x80;
		if (drawM1 && drawP1) cxm1p |= 0x40;
		if (drawP0 && drawPlayfield) cxp0fb |= 0x80;
		if (drawP0 && drawBl) cxp0fb |= 0x40;
		if (drawP1 && drawPlayfield) cxp1fb |= 0x80;
		if (drawP1 && drawBl) cxp1fb |= 0x40;
		if (drawM0 && drawPlayfield) cxm0fb |= 0x80;
		if (drawM0 && drawBl) cxm0fb |= 0x40;
		if (drawM1 && drawPlayfield) cxm1fb |= 0x80;
		if (drawM1 && drawBl) cxm1fb |= 0x40;
		if (drawBl && drawPlayfield) cxblpf |= 0x80;
		if (drawP0 && drawP1) cxppmm |= 0x80;
		if (drawM0 && drawM1) cxppmm |= 0x40;

		bool priorityMode = (ctrlpf & 0x04) != 0;
		bool scoreMode = (ctrlpf & 0x02) != 0;
		bool drawGroup0 = drawP0 || drawM0;
		bool drawGroup1 = drawP1 || drawM1;

		// Score mode paints each half of the playfield in the matching
		// player's colour instead of COLUPF, the classic two-digit-score trick.
		uint8_t playfieldColor = scoreMode ? (px < 80 ? colup0 : colup1) : colupf;

		uint8_t finalColor = background;

		if (priorityMode)
		{
			if (drawPlayfield) finalColor = playfieldColor;
			else if (drawBl) finalColor = colupf;
			else if (drawGroup0) finalColor = colup0;
			else if (drawGroup1) finalColor = colup1;
		}
		else
		{
			if (drawGroup0) finalColor = colup0;
			else if (drawGroup1) finalColor = colup1;
			else if (drawBl) finalColor = colupf;
			else if (drawPlayfield) finalColor = playfieldColor;
		}

		if (isPalColor)
			buffer[py * 160 + px] = DecodeColor(finalColor, true);
		else
			buffer[py * 160 + px] = DecodeColor(finalColor, false);
	}

	// The audio section runs off the same colour clock, divided down to the
	// chip's native ~31.4kHz sample rate (3579545 / 114).
	if (++audioClockAccumulator >= 114)
	{
		audioClockAccumulator = 0;
		TickAudioChannel(audio0);
		TickAudioChannel(audio1);
		GenerateAudioSample();
	}

	if (++x >= 228)
	{
		x = 0;
		lines++;
	}
}

void Tia::TickAudioChannel(TiaAudioChannel& channel)
{
	if (channel.divCounter-- > 0)
		return;

	channel.divCounter = channel.frequency;

	// All three generators advance together; AUDC just picks which one
	// drives the output, matching the chip's habit of always clocking every
	// polynomial counter and only muxing the result.
	auto stepPoly = [](uint32_t& poly, int bits, int tapA, int tapB) -> bool
	{
		bool feedback = (((poly >> tapA) ^ (poly >> tapB)) & 1) == 0;
		poly = ((poly << 1) | (feedback ? 1u : 0u)) & ((1u << bits) - 1);
		return (poly & 1) != 0;
	};

	bool poly4Bit = stepPoly(channel.poly4, 4, 0, 1);
	bool poly5Bit = stepPoly(channel.poly5, 5, 0, 2);
	bool poly9Bit = stepPoly(channel.poly9, 9, 0, 4);

	// AUDC has sixteen documented waveforms on real silicon; several of them
	// are variations on the same three generator families below, so this
	// approximation folds each code onto the closest family rather than
	// reproducing every hardware quirk bit-for-bit.
	switch (channel.control)
	{
		case 0x00:
			channel.outputHigh = true;
			break;

		case 0x04: case 0x05: case 0x0C: case 0x0D:
			channel.outputHigh = !channel.outputHigh;
			break;

		case 0x08:
			channel.outputHigh = poly9Bit;
			break;

		case 0x01: case 0x03: case 0x0B:
			channel.outputHigh = poly4Bit;
			break;

		default:
			channel.outputHigh = poly5Bit;
			break;
	}
}

void Tia::GenerateAudioSample()
{
	// Each channel contributes a signed +/-volume swing rather than an
	// unsigned level, so silence sits at zero instead of leaving a DC
	// offset that would otherwise thump the speakers on every state change.
	int contribution0 = audio0.outputHigh ? audio0.volume : -(int)audio0.volume;
	int contribution1 = audio1.outputHigh ? audio1.volume : -(int)audio1.volume;
	int mixed = contribution0 + contribution1;

	audioSamples.push_back((int16_t)(mixed * (32767 / 30)));
}

std::vector<int16_t> Tia::DrainAudioSamples()
{
	std::vector<int16_t> drained;
	drained.swap(audioSamples);
	return drained;
}

void TiaAudioChannel::Serialize(std::iostream& stream, bool isSaving)
{
	auto HandleData = [&](auto& data)
	{
		if (isSaving) stream.write(reinterpret_cast<const char*>(&data), sizeof(data));
		else stream.read(reinterpret_cast<char*>(&data), sizeof(data));
	};

	HandleData(control); HandleData(frequency); HandleData(volume);
	HandleData(divCounter); HandleData(poly4); HandleData(poly5);
	HandleData(poly9); HandleData(outputHigh);
}

void Tia::Serialize(std::iostream& stream, bool isSaving)
{
	auto HandleData = [&](auto& data)
	{
		if (isSaving) stream.write(reinterpret_cast<const char*>(&data), sizeof(data));
		else stream.read(reinterpret_cast<char*>(&data), sizeof(data));
	};

	HandleData(lines); HandleData(x); HandleData(wsync); HandleData(input4);
	HandleData(background); HandleData(colupf); HandleData(colup0); HandleData(colup1);
	HandleData(pf0); HandleData(pf1); HandleData(pf2);
	HandleData(grp0); HandleData(grp1); HandleData(ctrlpf); HandleData(vsyncReg);
	HandleData(posP0); HandleData(posP1); HandleData(posM0); HandleData(posM1); HandleData(posBl);
	HandleData(nusiz0); HandleData(nusiz1);
	HandleData(enaM0); HandleData(enaM1); HandleData(enaBl);
	HandleData(reflectP0); HandleData(reflectP1);
	HandleData(hmp0); HandleData(hmp1); HandleData(hmm0); HandleData(hmm1); HandleData(hmbl);
	HandleData(cxm0p); HandleData(cxm1p); HandleData(cxp0fb); HandleData(cxp1fb);
	HandleData(cxm0fb); HandleData(cxm1fb); HandleData(cxblpf); HandleData(cxppmm);
	HandleData(audioClockAccumulator);
	audio0.Serialize(stream, isSaving);
	audio1.Serialize(stream, isSaving);
}
