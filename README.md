# Sweet Papa Pedals

Six free audio-effect pedals (VST3 / AU / Standalone) for indie-rock and bedroom-pop production, by **Sweet Papa Technologies**. Built with JUCE 8, designed around one rule: *every pedal is one opinionated sound with a handful of knobs that all matter.*

| Pedal | One-liner | Reach for it when… |
|---|---|---|
| **DOUBLE** | Every take you didn't record | A vocal or guitar needs to be thicker/wider without sounding chorused |
| **BACKPORCH** | Sounds produced, not wet | You want space that never swamps the performance (the tail ducks while you play) |
| **SWAY** | Makes static tracks move like a band | Keys, DI guitar, or synths feel frozen and lifeless |
| **VROOM** | The dirt pedal that lands in the mix | Tube warmth → crunch → fuzz → octave fuzz, with touch response |
| **DAYDREAM** | One knob, from warm tape to dream | You want instant lo-fi/ambient character without a manual |
| **FOFOPEDAL** | Twelve characters, one MIX knob | You want a produced sound fast: pick a character, ride MIX |

All wet paths are high-passed, pre-delayed, and ducked where it counts — the mix hygiene is built in, not a settings page.

## Install

### macOS (Apple Silicon + Intel, macOS 10.15+)
Run the installer: **`SweetPapaPedals.pkg`** — it puts the AU components and VST3s into `/Library/Audio/Plug-Ins`. Restart your DAW; first launch re-scans plugins.

- **GarageBand / Logic**: plug-in slot → Audio Units → *Sweet Papa Technologies*.
- **Ableton / Reaper / Cubase**: enable VST3 scanning; they appear under *Sweet Papa Technologies*.
- If macOS warns about an unidentified developer, right-click the pkg → **Open** (we're working on notarization).

### Windows (64-bit, Windows 10+)
Run **`SweetPapaPedals-Setup.exe`** — installs the VST3s to `C:\Program Files\Common Files\VST3`. Rescan plugins in your DAW.

## Quick-start per pedal

- **DOUBLE** — THICK = how many voices/how detuned · WIDE = stereo spread (mono-safe) · HUMAN = how much the takes wander · MIX = layer the doubles under the dry. Modes voice it for **Vox / Strings / Synth**. Start: vocal, all knobs at default, MIX to taste.
- **BACKPORCH** — SPACE = size+decay · TONE = darker/brighter tail · DUCK = how much the tail hides while you play · MIX (dry holds at unity until 70%). Modes: **Slap / Room / Plate**. Start: Room, defaults, on a lead vocal.
- **SWAY** — MOVE = total movement · RATE = speed · COLOR = flavor per mode (wow↔flutter / spread / trem shape) · MIX. Modes: **Tape / Ensemble / Pump**. Start: Tape mode on keys, MOVE at noon.
- **VROOM** — Four hero knobs: DRIVE / CHARACTER / TONE / LEVEL, four voices (**Smooth / Crunch / Fuzz / Octave**), source modes (**Electric / Acoustic / Bass**). DRIVE is loudness-compensated — it changes *texture*, not volume. The support row (Input/Body/Sag/Blend) is there when you want it; SAG makes the clipper respond to your picking.
- **DAYDREAM** — One knob. 0–35% warm tape, 35–65% wobble and width, 65–100% shimmering near-infinite wash that ducks under your playing. That's the manual.
- **FOFOPEDAL** — Pick one of 12 named characters (each is ONE vibe: *Front Porch* is a hall, *Dub Lounge* is a ping-pong tape echo, *Cassette Sunday* is a warbly cassette…). **MIX is "how much pedal"** — it scales the whole character, 0 = clean. The six block knobs are there for deeper tweaks.

## Building from source

Requirements: CMake ≥ 3.22, Xcode (macOS) or Visual Studio 2022 (Windows), Node 18+ (plugin UIs are Vite/React, compiled into the binaries).

```bash
# 1. Build the UIs (once per UI change)
for d in ui daydream/ui fofopedal/ui double/ui backporch/ui sway/ui; do
  (cd "$d" && npm install && npm run build)
done

# 2. Configure + build everything
cmake -B build
cmake --build build --config Release
```

Plugins are copied to your user plug-in folders automatically after each build (`COPY_PLUGIN_AFTER_BUILD`). JUCE 8.0.4 is fetched automatically via CPM.

- macOS installer: `scripts/build-installer-macos.sh` → `dist/SweetPapaPedals.pkg`
- Windows build + installer: see `windows/README.md` (run on a Windows machine)

## Architecture notes

All six plugins share a header-only DSP toolkit in `common/spt/`:
`FableVerb` (Dattorro plate/hall/room with a modulated tank), ADAA saturation primitives, tape hysteresis, a jittered dual-grain pitch shifter, and drifting LFOs (nothing in these pedals sits perfectly still).

## License

Source code: Apache-2.0 (see `LICENSE`). The pedals are free to download and use in any production, commercial or not.

> Note for redistributors building from source: this project uses the JUCE framework, which has its own license terms (JUCE 8 personal/commercial tiers) — see [juce.com/legal](https://juce.com/legal/juce-8-licence/).
