# sidbasic
A collection of tools to convert C64 PSID files and play them on the Enterprise and Spectrum 128.

sid_dump: extracts raw SID register data from a PSID file, using 6502 CPU emulation from libplus4emu. Track lengths can optionally be read from the Songlengths.txt file included with HVSC. The output is the state of 25 SID registers at each interrupt, usually at 50.12 Hz, with the originally unused most significant bit of the pulse width registers (R3, R10, R17) indicating events where the envelope generator needs to be restarted by a 1->0->1 cycle of the gate bit within a single frame

sid_conv: converts the output of sid_dump to a simple format suitable for software synthesis on 8-bit computers. Envelopes are pre-rendered at a resolution of 5 bits per channel, only some of the features of the SID chip are implemented (basic waveforms, PWM and ring modulation). Combined waveforms, the sync effect, filtering and SID sample playback are not supported

sid_dump and sid_conv are both licensed under the terms of the GNU GPL, version 2 or later. Building sid_dump requires the libplus4emu library and header files.

sidbasicSP plays files converted by sid_conv on the Spectrum 128 and clones, the player and utilities were written by Noel Persa and Istvan Varga.
