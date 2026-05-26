# VROOM — Harmonic Saturator / Dirt Pedal
### Build specification for Claude Code

> **Working title:** `VROOM` (rename freely — it's just a placeholder that matches the "thick but clean vroom" target sound). **Vendor:** Sweet Papa Technologies.

This document is the complete, agent-ready spec. It is written so Claude Code can build the plugin start to finish in ordered phases. Everything is concrete: parameter ranges, signal flow, DSP math, source modes, presets with exact values, the preset save system, the project layout, and a phased build plan. Where a value is "to taste," it's marked so — those are the things FoFo will dial in once it makes sound.

---

## 1. What this pedal is

A warm, harmonically rich saturation/dirt pedal that lives in the space between overdrive, distortion, and fuzz. The defining character is **thick but clean** — full low-mid body and grind, without harshness or mud — achieved through three core design choices:

1. **Asymmetric soft clipping** for even-harmonic (octave) warmth instead of brittle odd-harmonic fizz.
2. **A parallel clean blend** so the driven signal keeps note definition and articulation.
3. **Oversampling** around the nonlinearity so there's no aliasing "digital wasp" buzz.

It ships in three **Source Modes** — Electric Guitar, Acoustic Guitar, and Bass — that reconfigure the voicing, the gain ceiling, the cabinet stage, and the low-end handling so the same engine sounds correct for each instrument.

---

## 2. Tech stack and formats

| Concern | Choice |
|---|---|
| Framework | JUCE 8 (C++ audio core) |
| Build system | CMake (JUCE pulled via CPM or git submodule) |
| Plugin formats | VST3, AU (macOS), Standalone |
| UI | WebView via JUCE 8 `WebBrowserComponent`, front end built in React + Tailwind (Vite), bundled into `BinaryData` |
| Parameters / state | `AudioProcessorValueTreeState` (APVTS) |
| Antialiasing | `juce::dsp::Oversampling` (wraps the nonlinear stage only) |
| Cabinet sim | `juce::dsp::Convolution` (IR loader) |
| Presets | JSON files + APVTS state; native DAW preset support via `get/setStateInformation` |

Plugin identity (use these so DAWs track it consistently):
- Manufacturer name: `Sweet Papa Technologies`
- Manufacturer code: `SwPa`
- Plugin code: `Vrm1`
- Category: distortion / effect

---

## 3. Signal flow

Order matters. The dry blend is tapped **before** the drive so it stays clean, then recombined **before** the cabinet so dry and wet share the same speaker coloration and don't sound disjointed.

```
                 ┌──────────────────── WET PATH ─────────────────────┐
Input → Input    │  Pre-HPF → [ OVERSAMPLED: asym soft-clip ×2 ]      │
trim → SPLIT ──→ │  → DC blocker → Sag/Comp → Body EQ → Tone (LPF)    │ ──┐
        │        └────────────────────────────────────────────────────┘   │
        │                                                                   ├─→ BLEND → Cab IR → Output
        └────────── DRY PATH (clean, latency-compensated) ─────────────────┘    (bypassable)   Level
```

Critical implementation notes:
- **Latency compensation:** oversampling and convolution add latency. The dry path must be delayed to match the wet path's total latency or the blend will phase-cancel. Report total latency to the host with `setLatencySamples`.
- **DC blocker:** the asymmetric clipper introduces a DC offset by design. A high-pass at ~20 Hz right after the clipper removes it. Do not skip this.
- **Oversampling scope:** only the nonlinear clipping stage runs oversampled. Filters and EQ run at base rate — oversampling them wastes CPU for no benefit.
- **Parameter smoothing:** every continuous parameter uses `juce::SmoothedValue` (≈20 ms ramp) to avoid zipper noise.

---

## 4. User controls (knobs)

All continuous knobs are `0–100` in the UI for a clean feel; the mapping column is what the DSP uses internally.

| Knob | UI range | Default | Maps to (internal) | Purpose |
|---|---|---|---|---|
| **Input** | -12…+12 dB | 0 dB | linear gain | Level-match the source before drive (esp. across modes) |
| **Drive** | 0–100 | 45 | clip gain `1.0 … 50.0` (logarithmic) | Amount of saturation |
| **Character** | 0–100 | 60 | asymmetry bias `0.0 … 0.40` | 0 = symmetric/aggressive (odd harmonics), 100 = warm/tube (even harmonics) |
| **Body** | 0–100 | 55 | peaking EQ gain `-6 … +12 dB` @ mode-set center | Low-mid thickness |
| **Tone** | 0–100 | 50 | post-LPF cutoff `1.5 kHz … 12 kHz` (log) | Dark ↔ bright |
| **Sag** | 0–100 | 35 | compression/bloom amount | Tube-amp dynamic sag, sustain |
| **Blend** | 0–100 | 70 | % wet (100 = all wet, 0 = all clean) | The "thick but clean" control |
| **Level** | -24…+12 dB | 0 dB | output gain | Makeup gain |
| **Gate** *(optional)* | 0–100 | 0 (off) | downward gate threshold | Tame hiss/noise on high drive |

Discrete controls:

| Control | Options | Default | Notes |
|---|---|---|---|
| **Source Mode** | Electric / Acoustic / Bass | Electric | Reconfigures the engine (see §5) |
| **Cab** | On / Off | On (Electric) | Toggles the convolution stage |
| **Cab IR** | dropdown of bundled IRs + "Load custom…" | per mode | See §6 |
| **Oversampling** | 2× / 4× / 8× | 4× | Quality vs CPU |

---

## 5. Source Modes

Source Mode is the macro that re-voices the whole engine. These values are engine config, **not** user knobs — switching mode resets the hidden voicing below (and loads that mode's default cab), while the user knobs keep their positions.

### Electric Guitar (default)
- Pre-HPF: ~90 Hz (tightens the low end before clipping)
- Drive path: full-range
- Body EQ center: ~300 Hz
- Sag: normal response
- Default Cab: `1x12 Warm`, enabled
- Oversampling: 4×

### Acoustic Guitar
Acoustic guitars are full-range, dynamic, and depend on natural top-end air — you want gentle harmonic warmth, **not** heavy dirt, and you do **not** want a guitar speaker cab smearing the top end.
- Pre-HPF: ~60 Hz (preserve body), plus a gentle **piezo-quack tamer**: a small dip (~-3 dB, wide Q) around 3 kHz to soften the harsh upper-mid "quack" of piezo/under-saddle pickups
- **Air shelf:** gentle high shelf (+2…+3 dB above ~8 kHz) to restore sparkle
- Drive ceiling: reduced (effective max gain scaled down ~40%) — this mode is for warmth/excitement, not distortion
- More headroom, lighter Sag response (preserve dynamics)
- Body EQ center: ~200 Hz
- Default Cab: **Off / Full-Range (DI)** — preserve natural tone
- Blend: this mode defaults to a lower wet % (more clean) so the acoustic character stays intact
- Oversampling: 4×

### Bass
The classic bass-dirt problem: distorting the lows turns them to mud and kills the thump. Solution is **band-split drive** — only the mids/highs get driven, the lows stay clean and punchy, then recombine.
- **Band split** at ~180 Hz. Low band passes clean. High band → drive path.
- Recombine driven-highs + clean-lows, **then** apply the Blend knob.
- Body EQ center: ~120–150 Hz (growl)
- Sub-preserve: protect content below ~60 Hz from any gain change
- Default Cab: **Off / DI** (or a bass cab IR if loaded)
- Oversampling: 4× or 8× (driven highs over a low fundamental generate lots of harmonics — antialiasing matters)

---

## 6. Cabinet IRs

Use `juce::dsp::Convolution`. Bundle a small set of IR slots; let users load their own (`.wav` IRs). **Ship only freely/CC-licensed or originally-captured IRs** — do not bundle copyrighted commercial IR packs.

Suggested bundled slots:
- `1x12 Warm` — rounded, smaller speaker (default for Electric clean-ish tones)
- `4x12 Modern` — bigger, tighter (default for heavier Electric presets)
- `Bass 1x15` — bass cab (optional default for Bass mode)
- `Full-Range / DI` — effectively flat / near-bypass (default for Acoustic and Bass-DI)

"Full-Range / DI" can be implemented as either a flat IR or simply by bypassing the convolution; expose it as a selectable option either way so the UI is consistent.

---

## 7. Core DSP reference

Pseudocode for the saturation stage (runs **inside** the oversampled block, per sample):

```cpp
// Inputs already through Input trim + Pre-HPF.
const float drive = mapDrive(driveParam);      // 1.0 .. 50.0, log
const float bias  = mapBias(characterParam);   // 0.0 .. 0.40

// Asymmetric soft clip: the +bias offset makes the +/- halves clip
// differently, which is what generates the even (octave) harmonics.
auto clipStage = [&](float x)
{
    return std::tanh(drive * (x + bias)) - std::tanh(drive * bias);
};

// Two cascaded gentle stages sound smoother and bigger than one hard stage.
// A mild low-pass between stages tames harmonic buildup before the 2nd stage.
float a = clipStage(x);
a = interstageLPF.processSample(a);   // 1st-order LPF ~8 kHz
float y = clipStage(a * 0.8f);

// Remove the DC the asymmetry introduced.
y = dcBlocker.processSample(y);       // HPF ~20 Hz
return y;
```

Sag / compression (post-clip, base rate): a simple feed-forward compressor with a program-dependent release driven by an envelope follower. Map the `Sag` knob to both ratio and how much the "supply" dips on transients — higher Sag = more bloom and sustain, slower recovery.

Blend (after Sag → Body → Tone on the wet path):
```cpp
float wet = tone.process(body.process(sag.process(saturated)));
float mix = wet * blend + dryDelayed * (1.0f - blend);   // dryDelayed = latency-compensated clean
float out = level * cab.process(mix);                    // cab bypassable
```

EQ blocks: Pre-HPF and DC blocker are 1st/2nd-order high-pass; Body is a peaking (bell) filter at the mode's center frequency with gain from the Body knob; Tone is a 1st/2nd-order low-pass with cutoff from the Tone knob. Use `juce::dsp::IIR` filters.

---

## 8. Presets (factory)

At least 5 was the ask — here are 9, organized by mode, designed to show the full range and all sound musical. Values are the user-knob positions (`0–100` unless a dB control). `Input` defaults to 0 dB everywhere unless noted.

### Electric Guitar

| Preset | Drive | Character | Body | Tone | Sag | Blend | Level | Cab | Feel |
|---|---|---|---|---|---|---|---|---|---|
| **Vroom** *(signature/default)* | 45 | 70 | 65 | 45 | 40 | 70 | 0 dB | 1x12 Warm | Thick, warm, clean grind — the namesake |
| **Velvet OD** | 25 | 65 | 45 | 60 | 25 | 55 | +1 dB | 1x12 Warm | Low-gain transparent overdrive, touch-sensitive |
| **Stacked Wall** | 85 | 30 | 60 | 40 | 55 | 90 | -1 dB | 4x12 Modern | Fuzz-leaning, aggressive, big sustain |
| **Crunch** | 60 | 45 | 50 | 55 | 35 | 80 | 0 dB | 4x12 Modern | Classic mid-forward distortion |
| **Lead Bloom** | 70 | 75 | 60 | 42 | 65 | 78 | +2 dB | 1x12 Warm | Singing, sustaining lead tone |

### Acoustic Guitar

| Preset | Drive | Character | Body | Tone | Sag | Blend | Level | Cab | Feel |
|---|---|---|---|---|---|---|---|---|---|
| **Acoustic Warmth** | 15 | 80 | 40 | 70 | 15 | 45 | 0 dB | Full-Range/DI | Gentle tube warmth + air, dynamics intact |
| **Acoustic Body** | 12 | 70 | 70 | 62 | 10 | 40 | +1 dB | Full-Range/DI | Adds low-mid body to thin-sounding acoustics |

### Bass

| Preset | Drive | Character | Body | Tone | Sag | Blend | Level | Cab | Feel |
|---|---|---|---|---|---|---|---|---|---|
| **Bass Growl** | 50 | 55 | 55 | 50 | 30 | 50 | 0 dB | DI | Clean punchy lows, growly driven mids |
| **Bass Fuzz** | 80 | 40 | 60 | 45 | 45 | 55 | -1 dB | DI | Heavy bass dirt, low end still preserved |

---

## 9. Preset save system

Goal: dead-simple to save, share, and version. Presets are human-readable JSON (FoFo can `git` them).

### Format
```json
{
  "schemaVersion": 1,
  "name": "Vroom",
  "category": "Electric",
  "author": "Factory",
  "parameters": {
    "input": 0.0,
    "drive": 45,
    "character": 70,
    "body": 65,
    "tone": 45,
    "sag": 40,
    "blend": 70,
    "level": 0.0,
    "gate": 0,
    "sourceMode": "Electric",
    "cabEnable": true,
    "cabIR": "1x12 Warm",
    "oversampling": "4x"
  }
}
```

### Storage
- **Factory presets:** compiled into `BinaryData` (read-only). Optionally copied into the user dir on first launch so they appear alongside user presets.
- **User presets:** written to a platform-standard folder, abstracted behind a `PresetManager`:
  - macOS: `~/Library/Audio/Presets/Sweet Papa Technologies/VROOM/`
  - Windows: `%USERPROFILE%\Documents\Sweet Papa Technologies\VROOM\Presets\`

### UI (in the WebView)
- Preset name display with **◀ / ▶** arrows to step through presets
- Dropdown browser grouped by **Factory / User** and by **mode category**
- Buttons: **Save**, **Save As…**, **Rename**, **Delete** (user presets only)
- A small **modified** dot when the current state differs from the loaded preset
- Optional nicety: **A/B** compare slots

### Plumbing
- Save = serialize current APVTS state → JSON via the schema above.
- Load = parse JSON → set APVTS parameters (with smoothing).
- Also implement `getStateInformation` / `setStateInformation` so the DAW's native preset system and session recall work independently of the JSON browser.

---

## 10. Project structure

```
VROOM/
├── CMakeLists.txt
├── cmake/                      # CPM.cmake, JUCE fetch
├── Source/
│   ├── PluginProcessor.h/.cpp  # APVTS, DSP graph, latency reporting
│   ├── PluginEditor.h/.cpp     # hosts WebBrowserComponent + JS<->native bridge
│   ├── dsp/
│   │   ├── Saturator.h/.cpp    # oversampled asymmetric cascaded soft-clip
│   │   ├── ToneStack.h/.cpp    # Pre-HPF, Body EQ, Tone LPF, DC blocker
│   │   ├── Sag.h/.cpp          # compression/bloom
│   │   ├── CabSim.h/.cpp       # convolution IR loader
│   │   └── BandSplit.h/.cpp    # bass-mode low/high split
│   ├── presets/
│   │   ├── PresetManager.h/.cpp
│   │   └── factory/*.json
│   └── modes/SourceMode.h      # mode → engine config table
├── ui/                         # React + Tailwind (Vite)
│   ├── src/ (knobs, preset browser, mode selector)
│   └── dist/  (built, bundled to BinaryData)
└── Resources/IRs/*.wav         # freely-licensed cabinet IRs
```

---

## 11. Phased build plan

Build in increments so each phase produces something testable (ship-prototype-iterate). Validate sound **through a cab IR** from Phase 2 on — raw dirt with no cab sounds like a hornet and that's expected, not a bug.

- **Phase 0 — Scaffold.** JUCE 8 CMake project building VST3 + AU + Standalone. Empty processor passing audio straight through. Confirm it loads in a DAW.
- **Phase 1 — Round-trip.** APVTS with all params declared. WebView UI shell with one working knob (Drive) wired end-to-end through a trivial gain. Nail the JS↔native bridge here before anything else.
- **Phase 2 — Core drive.** Input trim → Pre-HPF → oversampled asymmetric cascaded soft-clip → DC blocker. Test through a bundled cab IR. This is the heart — get it sounding right.
- **Phase 3 — Voicing + blend.** Body EQ, Tone LPF, Sag, and the parallel dry Blend with latency compensation. Verify no phase cancellation in the blend.
- **Phase 4 — Cabinet.** Convolution IR loader, bundled IR slots, custom IR loading, on/off.
- **Phase 5 — Source Modes.** Electric / Acoustic / Bass engine reconfiguration, including the Acoustic piezo-tamer + air shelf and the Bass band-split.
- **Phase 6 — Presets.** PresetManager, factory JSON presets, user save/load, the browser UI, native DAW state.
- **Phase 7 — Polish.** Oversampling quality selector, optional Gate, UI refinement, all 9 presets dialed in, latency reporting verified, parameter smoothing throughout.

---

## 12. Dial-to-taste list

These are the knobs FoFo will tune by ear once it makes sound — the spec's starting values are sensible defaults, not gospel:
- Asymmetry bias max (`0.40`) — how warm vs. how aggressive the Character knob can go
- Drive gain ceiling (`50.0`) and the Acoustic-mode gain reduction
- Interstage LPF and Tone LPF ranges (fizz vs. presence)
- Body EQ center frequencies per mode
- Bass band-split frequency (`180 Hz`) and sub-preserve corner
- Sag response curve (subtle vs. obvious bloom)
- The exact preset values in §8
