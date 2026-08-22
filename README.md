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
- **SWAY** — MOVE = total movement · RATE = speed · COLOR = flavour per mode (**Tape**: machine condition, from serviced deck to dying cassette · **Ensemble**: width · **Pump**: shape) · MIX = how much pedal. Modes: **Tape / Ensemble / Pump**. Tape is a real machine — saturation, head bump, gap loss, dropouts and hiss, not just a wobble. Start: Tape mode on keys, MOVE at noon.
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

**FoFoDriver** (`common/fofo/`) is the shared DSP kernel the pedals are being rebuilt onto — SWAY is the first. It exists to make a class of bug unrepresentable rather than merely fixed: a `Node` never sees dry signal, so it cannot mis-mix it; `Parallel` owns the dry snapshot, the latency compensation and the one canonical mix rule; and a single `ModMatrix` at a single control rate drives every modulation source. It provides a TPT/ZDF state variable filter with real resonance, cubic-Hermite fractional delays, a tape machine (hysteresis, head bump, gap loss, self-erasure, dropouts, hiss), and latency-reporting oversampled shapers.

The pedals not yet ported share a header-only toolkit in `common/spt/`:
`FableVerb` (Dattorro plate/hall/room with a modulated tank), ADAA saturation primitives, tape hysteresis, a jittered dual-grain pitch shifter, and drifting LFOs.

`tests/` holds the DSP test suite (`cmake --build build --target FOFO_TESTS`, then run
`build/FOFO_TESTS_artefacts/Release/FOFO_TESTS`). It runs on every CI build and covers
the kernel, SWAY's voicing calibration, and regression guards for defects that have
shipped before.

## License

Source code: Apache-2.0 (see `LICENSE`). The pedals are free to download and use in any production, commercial or not.

> Note for redistributors building from source: this project uses the JUCE framework, which has its own license terms (JUCE 8 personal/commercial tiers) — see [juce.com/legal](https://juce.com/legal/juce-8-licence/).

## Development workflow

`main` is protected: all changes land via pull request, and both platform
builds (macOS + Windows) must pass before merging.

```bash
git checkout -b feat/my-change   # branch
# ...work, commit...
git push -u origin feat/my-change
gh pr create --fill              # open the PR
gh pr merge --auto --squash      # merges itself when CI goes green
```

Releases are cut from main by tag: `git tag vX.Y.Z && git push origin vX.Y.Z`
(see RELEASING.md). The site auto-deploys on any merge to main touching `site/`.
