# ELECTROBOIS

ELECTROBOIS is an experimental [Atari 2600](https://en.wikipedia.org/wiki/Atari_2600) emulator developed by Néo Foderé de Frutos and written in C++.

It's not meant to be playable; it's an experimental project. It does not feature a dynamic recompiler (JIT) and instead relies on a simple pure interpreter driven by a large `switch` statement with dozens of cases. Although many games launch, they don't run properly. If you want to emulate Atari 2600 properly you can download the [Stella](https://github.com/stella-emu/stella) emulator.

The Atari 2600 is powered by a [MOS 6507 CPU](https://en.wikipedia.org/wiki/MOS_Technology_6507) running at 1.19 MHz, a stripped-down version of the 6502 with an 8-bit data bus and a limited 13-bit address bus that can only access up to 4 KB of external memory. It relies on two primary support chips: the TIA (Television Interface Adaptor), which generates video and audio on the fly directly to the screen because the system lacks a frame buffer, and the RIOT (RAM-IO-Timer) chip, which manages the console's tiny 128 bytes of RAM, input/output operations for the controllers, and system timers.

## Dependencies
- SDL2
- g++ (7.1.0 or later)

## Building
```
git clone https://github.com/neofodere/electrobois.git

cd electrobois

make
```

## Running

```
# Run with NTSC color palette
./electrobois <path_to_rom>
or
./electrobois <path_to_rom> -ntsc

# Run with PAL color palette
./electrobois <path_to_rom> -pal
 
```

*In Windows, replace "./electrobois" with ".\electrobois.exe"*

## Controls and hotkeys

| Action | Key |
|-|-|
| Move | Arrow keys / WASD |
| Fire | Space |
| Reset | R |
| Quit | Escape |


<hr>

**Atari 2600 and Atari are registered trademarks of Atari SA. This software is not affiliated in any way with Atari SA.**

**© 2026 Néo Foderé de Frutos.**
