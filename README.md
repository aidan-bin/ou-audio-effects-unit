# STM32-Audio-Effects-Unit

![Board v1](https://raw.githubusercontent.com/ouras/STM32-Audio-Effects-Unit/main/board/STM32AudioEffects/STM32AudioEffectsUnit.jpg)

Audio effects unit designed for STM32. Includes:
- Library of effects implemented in C (including distortion/overdrive, reverb/echo, and compression)
- Python GUI demo/testbench for effects (based on PyQt, PyPlot, NumPy, and ctypes)
- PCB layout based on STM32F3RETx (plus LTSpice files for select sub-networks)
- CubeMX file for board (including FreeRTOS, peripheral, and pin configuration)
- main.c (FreeRTOS-based firmware)

Firmware features:
- FreeRTOS for task management
- ADC and DAC with DMA for efficient audio processing (note: sample rate = 40.5 kHz)
- I2C for EEPROM ~~and OLED display~~ *(coming soon)*
- Interrupts/handlers for user inputs

Board includes:
- 1/4" (6.35mm) input/output jacks
- OLED display
- 4 configuration potentiometers + 2 volume potentiometers
- 3 buttons
- 3 switches
- EEPROM (for user configuration backups)
- Input and output active gain and noise filter networks (with LTSpice schematics/simulation files)
- 9V DC wall adapter, 9V battery, and 5V USB power support (converted to 3.3V for Vcc)
- Status and power LEDs

Directory guide:
- /board contains LTSpice files and a KiCad project for the board
- /cubemx contains a CubeMX file for generating code for the MCU configuration and a main.c file containing the FreeRTOS port
- /demo contains files for running the demo
- /effects and /math contain the effects and math libraries
- /signal_gen_tools contains code for generating binary samples files

Running the demo:
1. Copy the contents of /demo to a folder.
2. Install the required modules (at the top of effects.py and main.py)
3. Copy the contents of /math and /effects to the folder from step 1.
4. Compile the C source files using:
  1. gcc -Wall -c fast_math.c effects.c
5. Create the shared library using:
  1. gcc -shared -o libeffects.dll \*.o
  2. Add “-m64” if using 64-bit Python
  3. Run main.py

A working document for this project (which include requirements, milestones, and technical notes) can be found [here](https://docs.google.com/document/d/13bzRhMHOJ_USe95iWY26JIxKYx1gLJ1tkpusW1AXwGo/edit?usp=sharing).
