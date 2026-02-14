# ClaudeAmp

A professional 3-band EQ AudioUnit plugin for Logic Pro with master volume control.

## Features

- **3-Band EQ:**
  - **Bass:** Low-shelf filter @ 200 Hz (±24 dB)
  - **Mid:** Peaking filter @ 1 kHz (±24 dB)
  - **Treble:** High-shelf filter @ 5 kHz (±24 dB)
- **Master Volume:** -60 to +6 dB
- **Real-time processing** with parameter smoothing (no clicks or pops)
- **Professional DSP** using JUCE IIR biquad filters
- **Graphical interface** with 4 rotary knobs
- **Supports:** AudioUnit, VST3, and Standalone formats

## Build Requirements

- **macOS:** 12.0 or later (tested on macOS Sequoia 15.0)
- **Xcode:** 14.0 or later (with Command Line Tools)
- **CMake:** 3.22 or later
- **JUCE:** 8.0.4 (included in build process)

## Building from Source

1. **Clone the repository:**
   ```bash
   git clone https://github.com/yourusername/ClaudeAmp.git
   cd ClaudeAmp
   ```

2. **Ensure JUCE is available:**
   The project expects JUCE 8.x to be located at `/Users/anguslamb/code/JUCE`
   ```bash
   cd /Users/anguslamb/code
   git clone --depth 1 --branch 8.0.4 https://github.com/juce-framework/JUCE.git
   ```

3. **Configure the build:**
   ```bash
   cd /path/to/ClaudeAmp
   cmake -B build -DCMAKE_BUILD_TYPE=Release
   ```

4. **Build the plugin:**
   ```bash
   cmake --build build --config Release
   ```

5. **Installation:**
   The plugin is automatically installed to:
   - **AU:** `~/Library/Audio/Plug-Ins/Components/ClaudeAmp.component`
   - **VST3:** `~/Library/Audio/Plug-Ins/VST3/ClaudeAmp.vst3`
   - **Standalone:** `build/ClaudeAmp_artefacts/Release/Standalone/ClaudeAmp.app`

## Usage

### In Logic Pro

1. Open Logic Pro
2. Create or open a project
3. Add an audio track
4. Insert ClaudeAmp:
   - Click the Audio FX slot
   - Navigate to: Audio Units > ClaudeAudio > ClaudeAmp
5. Adjust the EQ controls:
   - **Bass knob:** Boost/cut low frequencies
   - **Mid knob:** Boost/cut midrange frequencies
   - **Treble knob:** Boost/cut high frequencies
   - **Master knob:** Overall volume control

### Automation

All parameters support DAW automation:
1. In Logic Pro, click the "Automation" button on a track
2. Select "ClaudeAmp" from the automation menu
3. Choose the parameter to automate (Bass, Mid, Treble, or Master)
4. Draw automation curves in the automation lane

### Default State

All controls default to 0 dB (unity gain), meaning the plugin is transparent when first loaded with no changes to your audio.

## Parameter Specifications

| Parameter | Type | Frequency | Q | Range | Default |
|-----------|------|-----------|---|-------|---------|
| Bass | Low-shelf | 200 Hz | 0.707 | ±24 dB | 0 dB |
| Mid | Peaking | 1 kHz | 0.7 | ±24 dB | 0 dB |
| Treble | High-shelf | 5 kHz | 0.707 | ±24 dB | 0 dB |
| Master | Gain | - | - | -60 to +6 dB | 0 dB |

## Technical Details

- **DSP:** IIR biquad filters (low-shelf, peaking, high-shelf)
- **Parameter smoothing:** 20ms ramp time to prevent audio artifacts
- **CPU usage:** < 1% on modern systems
- **Latency:** Zero (no look-ahead processing)
- **Thread-safe:** Uses JUCE AudioProcessorValueTreeState
- **Audio quality:** 32-bit floating-point processing

## Troubleshooting

### Plugin doesn't appear in Logic Pro

1. **Reset AudioUnit cache:**
   ```bash
   killall -9 AudioComponentRegistrar
   ```

2. **Verify installation:**
   ```bash
   ls ~/Library/Audio/Plug-Ins/Components/ClaudeAmp.component
   ```

3. **Check Logic's Plugin Manager:**
   - Logic Pro > Preferences > Plugin Manager
   - Ensure ClaudeAmp is enabled

### Build errors

1. **CMake not found:**
   ```bash
   brew install cmake
   ```

2. **JUCE not found:**
   - Ensure JUCE is at `/Users/anguslamb/code/JUCE`
   - Or update CMakeLists.txt with correct path

3. **Xcode Command Line Tools:**
   ```bash
   xcode-select --install
   ```

## Development

Built with:
- [JUCE Framework](https://juce.com/) - Cross-platform C++ framework for audio applications
- C++17
- CMake build system

For development documentation, see [CLAUDE.md](CLAUDE.md).

## License

[Add your license here]

## Credits

Developed with Claude Code - Anthropic's AI coding assistant.
