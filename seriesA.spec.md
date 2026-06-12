# FoFoPedal Series A — Product Specification
*A Sweet Papa Technologies brief for Claude Code*

**TL;DR**
- Build a **six-block, semi-reorderable serial multi-effect** with a global "Sweet Papa" character/glue bus tying everything together — the right lineup is **Character → Drive → Mod → Pitch/Texture → Delay → Space**, with **MIDI Learn, sidechain ducking, and a constrained Dice (randomize-with-locks)** as the three highest-leverage stretch features for V1.
- Ship **12 hand-tuned named characters** (e.g. *Front Porch*, *Cassette Sunday*, *Cathedral Larynx*, *Dub Lounge*, *Shoebox Shoegaze*), not a 200-preset bank — each names a vibe, is voiced for a primary source, and earns its slot by being instantly usable on the first knob turn.
- The differentiator vs Chase Bliss, Strymon, Meris, Output and Soundtoys is **immediate musicality on the first turn across vocals AND guitar AND bass AND acoustic** plus a warm, hand-drawn aesthetic — opinionated curation rather than a modular sandbox. That space is unclaimed.

---

## A. Sonic identity — what FoFoPedal actually is

### The block lineup (recommended, opinionated)

Six blocks is the sweet spot. Five feels thin for "Swiss Army"; eight or more is where products like the Eventide H90, Meris LVX, and Empress ZOIA start to lose the "easy to dial in" promise. Six covers every canonical signal-chain need with one dedicated experimental slot.

**1 — CHARACTER (input conditioner / global voice).** A hybrid compressor + tilt EQ + tape-style HF rolloff + gentle low-shape. This is FoFoPedal's secret sauce: a subtle, always-on coloration stage (defeatable) that gives the whole plugin a unified voice. Conceptually adjacent to Goodhertz's LA-210, which they describe as "a fixed signal chain built from four modules, each pulled from an existing Goodhertz paid plugin … compression-into-saturation-into-degradation-into-limiting" — but tuned for instrument front-end use rather than mastering. Why it's first: it tames pickup spikes, evens out vocal dynamics, and feeds the drive block a "produced" signal — the trick behind why pro records sound pro even on cheap mics.

**2 — DRIVE / SATURATION.** Three algorithms selectable via toggle:
- **Tube** — asymmetric, even-harmonic-heavy, gentle compression. Tubes "often distort asymmetrically and therefore produce significant amounts of even order distortion" (Audient HMX article), making this the right pick for vocals and acoustic.
- **Tape** — symmetric, hysteresis-modeled, with HF rolloff that scales with drive: "as the tape reaches its limits, it introduces subtle compression, high-frequency roll-off, and a characteristic 'warble'" (Record/Mix/Master). Bass and full-mix glue mode.
- **Transformer/Iron** — 3rd & 5th harmonic emphasis below 500 Hz; per Audient HMX, "Transformers provide most of their saturation below 500Hz … therefore lots of 3rd and 5th harmonic are added to the lower mid-range and low end making instruments seem punchier and often easier to be heard on smaller speakers." Guitar/drum-friendly.

Controls: one DRIVE, one TONE (post-saturation tilt), one MIX (parallel saturation in one knob — the single highest-leverage trick borrowed from Soundtoys Decapitator and Baby Audio Parallel Aggressor).

Mandatory under the hood: **4× polyphase oversampling around the nonlinear stage with a post-LPF**, plus first-order ADAA on the static waveshapers. Per Jatin Chowdhury's ADAA primer, "the processing time for an effect will be multiplied by the oversampling factor … with that in mind, let us look for other anti-aliasing methods" — ADAA + modest oversampling is the right cost/quality balance. This is the single biggest "pro vs amateur" tell.

**3 — MODULATION.** One block, three algorithms: **Chorus** (BBD-style true tri-chorus with three voices at -120°/0°/+120° LFO phase relationships, per Eventide's TriceraChorus design — "the modulated signals for the Left and Right voices are -120 and 120 degrees out of phase relative to the Center modulation signal"); **Phaser** (4- or 6-stage all-pass cascade with feedback, with a dry-mix "vibey" mode); **Tremolo/Vibrato** (one knob crossfades from amplitude trem to pitch vibrato).

Notable exclusion: **flanger is dropped.** A chorus with extreme feedback covers most flanger use cases; saves a slot.

**4 — PITCH / TEXTURE (the experimental slot, but tasteful).** Three modes:
- **Micro-detune** — ±25 cents stereo spread; this is "MicroPitch" territory, which Eventide describes for its H9 as "you get detuning and dual delay for fattening and sweetening effects." The single highest-leverage "expensive vocal" trick in modern records.
- **Octave/Harmony** — clean ±1 octave or scale-locked 3rd/5th.
- **Freeze** — input-threshold-triggered freeze pad, inspired by Red Panda Particle 2's CHOP-based auto-freeze (where "above 12:00 it doubles as freeze threshold — input above threshold plays dry, below threshold reads from delay buffer"), feeding optionally into Space for instant ambient pads.

**5 — DELAY.** Three algorithms: **Digital** (the clean workhorse, ≤2 s, cubic interpolation), **BBD/Analog** (darker repeats, gentle modulation, self-oscillation at high feedback), **Tape** (wow/flutter, head-blend feel — the UAFX Galaxy '74 / Roland Space Echo lineage). TIME / FEEDBACK / MIX as primary; ping-pong, note division and HF-cut on repeats as shift functions.

**6 — SPACE (reverb).** Four algorithms:
- **Plate** — Dattorro-style modified Schroeder allpass loop; fast diffusion, slight metallic ring, rapid HF rolloff. Per Relab: "the Plate/Room algorithm has a higher initial reverb density when compared to the Hall algorithm … with smaller rooms and plate reverbs, there is a much higher initial 'echo density.'"
- **Hall** — Feedback Delay Network with longer initial reflections (30–120 ms). Per Valhalla's Sean Costello, Hall is characterized by "long delays in the initial sound (30~120mS), slow build, poor high end response, moderate diffusion build, moderate bass response."
- **Room/Ambience** — short FDN, dense, "you're in a recording studio." Per Pro Audio Files, "the shortest ambiences are great for any source that you want to sound dry, but still have a sense of three-dimensionality (like rap vocals)."
- **Shimmer** — FDN feeding a phase-vocoder pitch shifter at +12 (default) feeding back into the tank, per the Valhalla Shimmer playbook: "the basic foundation of the Brian Eno / Daniel Lanois shimmer sound is fairly simple: Create a feedback loop, incorporating a pitch shifter set to +1 octave, and a reverb with a fairly long decay time." Hard-cap feedback ≤0.7 to prevent runaway; randomize grain timing to dodge comb-filtering.

### Notable exclusions (and why)

- **No dedicated granular block.** Hologram Microcosm (11 algorithms with 44 preset variations) and Red Panda Particle/Tensor own this lane; competing here dilutes identity. Block 4's Freeze covers the 80/20 use case.
- **No looper.** Different product category.
- **No amp/cab sim.** UAFX, Neural DSP, Helix, and Tonex saturate this lane. FoFoPedal sits in front of those.
- **No vocoder / formant block in V1.** Soundtoys Little AlterBoy ("vocal formant and pitch shifting, hard tune FX, robot/vocoder mode, tube drive") owns it.
- **No standalone bit-crush.** Folded into Drive (Tape) and Character (low-rate flavor as a hidden parameter).

### Signal-chain ordering — opinionated stance

**Serial with two reorder toggles, not free routing.** After studying Strymon's fixed order, Meris LVX's full modularity, and the ZOIA/Beebo modular sandboxes, the right answer for a "fun, fast" pedal is mostly fixed with two tactical swaps:

```
[Character] → [Drive] → [Mod] ↔ [Pitch] → [Delay] ↔ [Space]
```

Two switches let users swap Mod↔Pitch and Delay↔Space. That covers every musically defensible signal chain (pitch-before-mod for arpeggiated chorus; reverb-into-delay for Cocteau Twins wash) without offering 720 permutations no one wants. Character is always first; Drive is always second — this is the BOSS/Strymon/Roland canonical ordering that the BOSS pedal-order guide states matter-of-factly: dynamics/dirt first, modulation after drive ("modulation … placed after the drive section … aggressive pedals will push the signal waveform hard and let modulation units deal with extreme harmonic content"), delays and reverb last.

The lesson from Empress ZOIA's modularity reception is direct: per Engadget, ZOIA "won't make any sound when you load up a blank patch: You have to actually place and connect audio-in and audio-out modules first" — a steep blank-slate problem we should not replicate. Free routing is a power-user feature that punishes everyone else.

---

## B. Signal chain and routing architecture

**Topology.** Serial core with parallel dry-mix at each block (per-block wet/dry knob in the hidden layer). Global wet/dry at the output. Stereo-in / stereo-out, true-stereo processing throughout. Mono-in auto-detected and split before the Mod block to keep effects stereo-native — borrowing UAFX's "dual-engine processing" approach where, per Sound on Sound, "all UAFX pedals also run dual instances of their processes, so delays and verbs that have no real stereo element to them audibly seem to have some kind of subtle 'imaging' going on when you run them in stereo." One internal feedback loop (Space → Pitch) enables Shimmer-style tails; user-defeatable, soft-limited.

**Source-aware "Voicing" toggle (recommended, with reservations).** A four-way toggle: **VOX / GTR / BASS / ACOUSTIC**. Rather than re-routing blocks, Voicing biases default parameter ranges and frequency targets behind the scenes — VOX has slower compression attack, darker drive tone, longer reverb pre-delay (40–60 ms, in the range Sound on Sound suggests for distant sources); BASS engages a hidden ~80 Hz dry-bass-pass and disables high-rate vibrato by default; ACOUSTIC favors Hall with long pre-delay and locks detune to ±10 cents max. Pros: "good on everything" is true on the first knob turn. Cons: hides state from the user — mitigate by surfacing the active voicing visibly and audibly when toggled.

**Stereo handling.** Mono-in/stereo-out auto-detected. Mod block creates stereo from mono via micro-detune + Haas, per the Sound on Sound vocal-production playbook ("vocal doublers … usually two or more pitch shifters (+/- 5 cents), followed by a very short delay (5 to 25 ms), panned hard left and right. This adds width to the original source in three dimensions"). Ping-pong lives inside Delay as a shift-toggle. Width control at the global output, 0–150% (above 100% uses M/S widening); mono-compatibility metering shows on hover, because (per Izotope) "as much as we all love to be immersed in our audio … mono is here to stay and should always be considered."

**Wet/dry handling.** Every block has an internal mix (hidden/shift). Global master wet/dry and output trim. A **"Defeat Character"** button bypasses Block 1 entirely — for users who want to test how raw the blocks sound, building trust.

---

## C. The 12 curated characters

Each character is a saved snapshot of block on/off state, algorithm selections, parameter values, and source voicing default. Users still tweak; the character is just the starting point. Twelve, not sixteen: sixteen is one too many to remember.

1. **Front Porch** — Acoustic guitar / piano. Voicing: ACOUSTIC. Character on (gentle), Drive off, Mod off, Pitch on (micro-detune ±8c), Delay off, Space on (Hall, 1.8 s, pre-delay 45 ms). Makes a strummed cowboy chord sound like a record.
2. **Cassette Sunday** — Lo-fi vocal/guitar. Voicing: VOX. Character on (tape rolloff strong), Drive on (Tape, 35%), Mod on (Chorus, slow, deep — wow/flutter feel), Pitch off, Delay on (BBD, dotted-⅛, 25% feedback, dark), Space on (Plate, short, dark). The house Sweet Papa sound.
3. **Cathedral Larynx** — Massive vocal. Voicing: VOX. Character on, Drive off, Mod on (Phaser, slow, subtle), Pitch on (micro-detune wide), Delay on (Digital, ¼ note, low mix), Space on (Shimmer, long, +12 feedback ~0.4). Worship / post-rock vocal pad-machine.
4. **Dub Lounge** — Bass / guitar / vocal. Voicing: GTR (works on all). Drive on (Tube, light), Mod off, Pitch off, Delay on (Tape, ⅛, high feedback, runaway-friendly), Space on (Plate, short).
5. **Shoebox Shoegaze** — Guitar wall-of-sound. Voicing: GTR. Drive on (Transformer, heavy), Mod on (Chorus, fast, wide), Pitch on (octave up + dry), Delay on (Digital, ⅛, modulated), Space on (Hall, very long, pre-delay 0).
6. **Nylon Velvet** — Acoustic / classical. Voicing: ACOUSTIC. Character on (transparent), Drive off, Mod on (Phaser, ultra-slow, subtle), Pitch off, Delay off, Space on (Room, short, bright). Adds air without adding "effects."
7. **Garage Vox** — Rock vocal. Voicing: VOX. Character on (compressed harder), Drive on (Tube, moderate), Mod off, Pitch on (octave-down, low mix), Delay on (Digital, slapback 90 ms, low mix), Space on (Plate, medium).
8. **Pillow Bass** — Bass. Voicing: BASS. Character on (transparent comp), Drive on (Tape, gentle), Mod off, Pitch off, Delay off, Space on (Room, very short, low-cut high). Glue, not effect. Mono-safe.
9. **Synth Bath** — Keys, also guitar pads. Voicing: GTR. Character on (sub-boost), Drive on (Tube, subtle), Mod on (Chorus, slow, wide), Pitch on (Freeze armed), Delay on (BBD, ½ note), Space on (Shimmer, modulated).
10. **Tin-Can Telephone** — Lo-fi extreme. Voicing: VOX. Character on (heavy HF rolloff, low-cut up), Drive on (Transformer, heavy), Mod on (Tremolo, fast square), Pitch off, Delay off, Space on (Room, tiny). The transition/bridge effect.
11. **Slap and Float** — Funk guitar / clavinet. Voicing: GTR. Character on (fast compression), Drive on (Tube, hot), Mod on (Phaser, medium, vibey), Pitch off, Delay on (Digital, ⅛, low mix), Space on (Plate, short, bright). 70s funk in a box.
12. **Vapor Trail** — Synth/voice/experimental. Voicing: VOX. Character on, Drive off, Mod on (Vibrato, slow), Pitch on (Freeze + harmony 5th), Delay on (Tape reverse-style, ½ note), Space on (Shimmer, long, modulated). The "no other plugin sounds like this" character.

**Naming philosophy.** Evocative, slightly silly, never SHOUTY-CAPS. Two words preferred. No "EXTREME 808 SLAYER vol. III" energy. Sweet Papa Technologies = the producer at the cookout, not the marketing department.

---

## D. Control surface / knob layout

### Top-level (always visible): 6 knobs + 4 toggles + 2 buttons

**Knobs (left to right):**
1. **CHARACTER** — macro for Block 1 (compression amount + tape color blended on one knob). At zero, Block 1 is bypassed. Always behaves the same; this is the anchor.
2. **DRIVE** — drive amount with auto-makeup gain. Drive type via toggle below.
3. **SHAPE** — *context-sensitive macro*. On VOX: depth of micro-detune + width. On GTR: mod depth. On BASS: a multi-band compressor tilt. On ACOUSTIC: brightness/air. The GUI label changes with voicing.
4. **TIME** — delay time (host-synced when host transport runs, otherwise free in ms).
5. **SPACE** — reverb decay + mix combined on one knob (the macro philosophy Hologram Microcosm's "Activity knob" embodies: a single macro that shapes the whole space).
6. **MIX** — global wet/dry. Use a Soundtoys-style mix curve (per Soundtoys' Little Plate manual: "when you start at 0 percent and fade up, it is mostly increasing the level of the reverb, and doing very little to the level of the dry signal, similar to how you would 'bring up' the reverb on an aux send. As you pass about 70 percent, the dry signal will quickly and smoothly begin to drop").

**Toggles (the "where am I" row):**
- **VOICING**: VOX / GTR / BASS / ACOUSTIC (illuminated)
- **DRIVE TYPE**: Tube / Tape / Iron
- **DELAY TYPE**: Digital / BBD / Tape
- **SPACE TYPE**: Plate / Hall / Room / Shimmer

**Buttons:** **TAP** (bypassable, doubles as tempo input) and **DICE** (constrained randomize — see §H).

### Secondary (Shift / Hold / Right-Click)

Hold-shift on a top-level knob reveals its hidden buddy:
- CHARACTER + shift → low-cut (0–200 Hz)
- DRIVE + shift → tone (post-drive tilt)
- SHAPE + shift → algorithm-secondary (e.g. phaser stages, chorus voice count)
- TIME + shift → feedback amount
- SPACE + shift → pre-delay
- MIX + shift → width / stereo spread

### Menu (expandable drawer)

Per-block on/off and mix; routing toggles; per-block algorithm selectors with audible preview; sidechain input routing and ducking depth; MIDI Learn + preset CC mapping; one global LFO assignable to up to 4 parameters; A/B compare.

**Macro-vs-parameter philosophy.** Knobs 1, 2, 5, 6 are pure parameter knobs (their meaning never changes). Knob 3 (SHAPE) is the one context-sensitive macro. Knob 4 (TIME) is a parameter knob but its musical meaning shifts subtly with delay type. This is predictable enough to muscle-memorize and surprising enough to invite exploration.

---

## E. Sound engineering / DSP approach

### Per-block algorithm recommendations

**Character.** Series chain: (a) soft-knee opto-style compressor with program-dependent release, (b) tilt-EQ around an 800 Hz pivot, (c) gentle low-shelf below 100 Hz and high-shelf rolloff above 8 kHz that scales with the tape sub-parameter. Tilt EQ via the Wilson/Quad approach (one shelf low + one shelf high ganged in opposite directions) is cheaper and more musical than parametric.

**Drive.** Three algorithms as in §A. All three: 4× polyphase oversampling, ADAA first-order on shapers, antialiasing filter post-shape, latency reported correctly. Per Universal Audio's own description of how this works on UAD plugins: "Some UAD plug-ins process audio at high sample rates internally, allowing replication of complex nonlinear behaviors such as distortion, saturation, and other classic analog characteristics. These UAD plug-ins incorporate an antialiasing filter that removes audio artifacts far above the audio spectrum in order to deliver the highest possible sound quality."

**Modulation.** True tri-chorus with three modulated delay lines (L/C/R) at -120°/0°/+120° phase per Eventide's documented design. Phaser: 4- or 6-stage all-pass cascade with feedback; "vibey" sub-mode mixes dry, "extreme" opens resonance. Trem/Vib: single LFO; one knob crossfades amplitude → fractional-delay-line vibrato; square/sine/triangle/random waveforms via shift. Depth 0–15 ms, rate 0.05–8 Hz.

**Pitch.** Phase-vocoder micro-detune (small shift, panned L/R for width). PSOLA for monophonic octave/harmony for low latency; phase-vocoder fallback for polyphonic. Freeze: threshold-triggered ~2 s circular-buffer capture with crossfaded loop points and slow LFO-modulated all-pass for "alive" feel (the Red Panda Particle 2 CHOP-threshold idea).

**Delay.** Digital uses cubic interpolation on the read pointer. BBD models a cascade of all-pass filters + companding + soft-clip in the feedback loop, with slow LFO drift on read pointer. Tape adds wow/flutter on read pointer + head-bump EQ + tape saturation in the feedback loop. Feedback path always goes through Character-style tone shaping so repeats degrade musically.

**Space.** Plate via Dattorro modified Schroeder allpass loop; Hall and Room via FDN (8×8 Hadamard or Householder matrix); Shimmer via FDN feeding a phase-vocoder pitch shifter (+12 default) feeding back. Per Sean Costello's notes on ValhallaShimmer: "the core pitch shifting algorithm uses randomization to avoid the comb filtering artifacts that can be heard in simpler pitch shifters. … The reverberation algorithm has been designed to work in conjunction with the pitch shifting, to allow for high levels of feedback without compromising" — adopt this approach. Pre-delay 0–250 ms across all spaces.

### Psychoacoustics — making things sound big, expensive, emotional

The competitive research lines up on the moves that separate "pro" from "amateur":

1. **Stereo width comes from micro-detune + short-delay imaging**, not extreme L/R panning. Per Sound on Sound: "vocal doublers… usually two or more pitch shifters (+/- 5 cents), followed by a very short delay (5 to 25 ms), panned hard left and right."
2. **Pre-delay matters more than reverb decay** for vocal intelligibility. Per ProSoundWeb's psychoacoustic mixing notes, "longer pre-delays on the drum reverb help reinforce the distance effect … my pre-delay values range from 5 to 10 milliseconds (ms) for near sources and from 40 to 60 ms for far sources."
3. **Frequency-dependent reverb decay** (longer lows, shorter highs) sells "real space" — implement via 3-band tail control in the FDN.
4. **Parallel saturation** sounds more produced than serial saturation; bake this into every Drive algorithm via the MIX knob, which is Decapitator's "secret weapon" framing.
5. **The dry signal is sacred for low end.** All reverbs and delays should default to a 60–120 Hz high-pass on the send (defeatable). This is what keeps Pillow Bass usable.
6. **Cohesion comes from a shared color** — the argument for the always-on Character block and the Output Glue stage below.

### What makes a multi-effect feel cohesive vs disconnected

**A unified post-stage.** Even when each block has its own algorithm, running the entire wet output through a subtle shared "voice" — tape-style HF rolloff at ~12 kHz, slight 3rd-harmonic saturation, a touch of bus compression — makes the whole pedal feel like one instrument rather than six. Add a hidden post-bus called **OUTPUT GLUE**, on by default at ~30%, with one parameter (amount, 0–100%). This is the trick behind why Goodhertz and Soundtoys plugins sound "of a piece" even across algorithmically diverse products.

### Sample rate / oversampling / antialiasing decisions

- Native processing at host sample rate for everything except Drive's nonlinear stage and Delay's BBD feedback path.
- 4× polyphase oversampling locally around those stages.
- ADAA on the static waveshapers inside Drive.
- Phase-vocoder pitch shifter latency (1024–2048 samples) reported correctly; PSOLA monophonic mode is near-zero latency.
- Freeze block processes at 48 kHz internal regardless of host rate, with SRC at the boundary, for predictable buffer behavior.
- Never use sample-and-hold for modulation; always interpolate the LFO between samples to avoid zipper noise.

---

## F. Aesthetic and brand identity

### Visual direction — playful but premium

Think *warm hardware sketch in a Moleskine*, not skeuomorphic chrome or modular Bauhaus. Borrows the friendliness of Chase Bliss's MOOD MKII (organic, hand-drawn cues) and Goodhertz's UI honesty — per Internet Tattoo's writeup, Goodhertz "plugins don't try to look like hardware. No skeuomorphic knobs, no fake VU meters. They look like software, and that's been a consistent design stance across everything they've released." FoFoPedal sits in between: warm and friendly, premium and clean, but not pretending to be wood.

**Color palette.** Background warm off-white / paper. Accents in muted terracotta, mustard, sage — not the saturated rainbow of Hologram Microcosm. Active LEDs/glow in soft amber, not RGB-pop. Text uses a soft serif for character names (Recoleta or Tiempos) and monospaced for parameter values.

**Layout.** Resizable. Six big knobs front-and-center; voicing-toggle row prominent; character name large and obvious in the center. Tape-reel animation in the Drive block when Tape is selected (homage to Baby Audio TAIP, but tasteful — not the central feature).

**Animation.** Minimal. A subtle "warmth bloom" pulse when audio is hot through the drive stage. Knobs have a soft physical feel (slight overshoot on release). No fireworks.

### Naming conventions

- **Blocks**: CHARACTER, DRIVE, MOD, PITCH, DELAY, SPACE. Plain English nouns.
- **Parameters**: SHAPE, DEPTH, TIME, FEEDBACK, TONE, MIX. Plain words. Unique sub-parameters lean evocative: SAG (compression release), BLOOM (reverb modulation), HAZE (pitch grain randomization), DRIFT (wow).
- **Presets**: two-word, evocative, slightly homespun (see §C). Never "INIT," never "Factory 01," never "EXTREME."
- **Voice**: all UI copy should sound like FoFo would actually say it. "Make it bigger" is fine; "Optimize spatial harmonic dispersion" is not.

---

## G. Competitive landscape — what to learn from, what to avoid

### What's working (and worth borrowing from, not copying)

- **Chase Bliss MOOD MKII** (looper + spatial effects, $399, Drolo FX × Old Blood Noise Endeavours collab, per Synth Anatomy: "Chase Bliss has released Mood MKII … designed in collaboration with Drolo FX and Old Blood Noise Endeavours"). Lesson: a clear sonic identity beats feature count.
- **Hologram Microcosm** ($449, 11 algorithms with 44 preset variations, per Hologram's official page: "11 unique granular and looping effects with 44 preset variations"). Engadget framing — "it's more than just a one-trick pony, which is important given the $449 price." Lesson: every preset should sound great with no knob-twiddling.
- **Strymon Mobius (~$429) and TimeLine (~$459)** (per Thomann US pricing; the original BigSky has been superseded by the BigSky MX flagship). Lesson: algorithm switching should be a confident gesture, not a sub-menu.
- **Soundtoys Decapitator / EchoBoy / Little Plate.** Parallel/dry-blend mix knobs that feel like analog; non-linear MIX curve (cited above). Lesson: the MIX knob curve matters more than people think.
- **Baby Audio Spaced Out's "Generate" button** — per MusicRadar: "this one, called Generate, randomises all Spaced Out parameters but, it promises, in a musical way, and we can certainly vouch for that — we were reaching for the Save button after just two rolls." Lesson: a constrained random button is a feature, not a gimmick.
- **Goodhertz Lossy / Lohi / Tupe.** Opinionated, narrow tools that became beloved. Lesson: opinion is a feature.
- **Output Movement / Portal / Thermal.** Macro-driven UX with XY pads, but per Image-Line forum users, "these plugins above all else absolutely destroy my cpu." Lesson: aim for the visual flair without the CPU bloat.
- **Eventide H9 / H90 / SpaceTime.** Per Equipboard's H9 Max review, "of the algorithms tailored for the H9, my favorite is SpaceTime. While most other algorithms run one effect at a time, SpaceTime gives two delays, modulation, and a reverb combined." (This is reviewer preference, not a poll, but it matches a broader pattern.) Lesson: a few well-tuned combined effects beat many disconnected ones.
- **UAFX (Astra, Galaxy, Starlight).** Dual-engine processing for spillover; per UA's docs, "Simple Live/Preset modes allow instant recall of your favorite sounds, with silent switching, true/buffered bypass, and spillover/trails." Lesson: spillover on preset change is non-negotiable for serious users.
- **Meris LVX / MercuryX.** Modular architecture, but per GuitarPedalX, "while it's relatively tiny screen and nature of operation / manipulation - means that it renders much more as a tabletop device vs a floor-based stompbox. I did not / do not find the screen / interface quite as intuitive as prescribed - it involves a lot of menu deep-diving." Lesson: don't go modular unless you commit to UI to match.
- **Empress ZOIA / Poly Effects Beebo.** Full modular, hard to dial in. ZOIA, per Engadget, "won't make any sound when you load up a blank patch." Beebo, per the same source, "is more conducive to on-the-fly virtual rewiring," but per Gearspace users, "you kind of get one complex-ish stereo routing or multiple more basic fx chains. Can't have your cake and eat it too." Lesson: avoid the blank-slate trap.

### What's missing in the market

1. **No multi-effect voiced equally well for vocals AND instruments.** Most pedals are guitar-first; most vocal-focused plugins are weak on bass. This is the FoFoPedal opening.
2. **No warm-aesthetic multi-effect.** The space is dominated by pro-mixer-blue (Soundtoys/UAD) or experimental-rainbow (Chase Bliss/Hologram). A warm, friendly, premium-but-not-sterile aesthetic is unclaimed.
3. **Curated-but-deep is rare.** Most products are curated-and-shallow (UAFX) or deep-and-uncurated (LVX, ZOIA). Twelve amazing presets + hidden depth is a sweet spot.

### Unique angle for FoFoPedal Series A

**"A multi-effect that's actually good on your vocal, your guitar, your bass, AND your acoustic — and gets there in three knob turns."** Differentiators: source-aware voicing, 12 hand-tuned characters with vibe-led names, unified Character + Output Glue stages for cohesion, sidechain ducking as a first-class feature (rare in guitar-oriented multi-FX), constrained parameter-lockable Dice.

---

## H. Nice-to-haves and stretch features — ranked for V1

### Tier 1 — Ship in V1 (high leverage)

1. **MIDI Learn + preset change via PC/CC.** Table-stakes. Per Neural DSP's docs, the workflow is industry standard: "right-click a parameter… click 'Enable MIDI Learn'. Then, press the button or move the pedal."
2. **Sidechain ducking input** with selectable target (Delay, Space, or both). Genuinely high-leverage and rare in guitar-oriented multi-FX. The use case ("echo only between vocal phrases") is something producers actively want and can't easily get without complex DAW routing.
3. **Constrained Dice (randomize) with parameter lock.** Baby Audio's randomize is genuinely beloved (see Spaced Out quote above); the design lesson is that chaos randomize gets used once, constrained randomize with locks gets used forever. Implementation: clicking Dice randomizes only non-locked, non-CHARACTER, non-MIX knobs, respecting musically sensible ranges (no 5-second feedback runaway, no detune > 50 cents). Each parameter has a small lock icon.
4. **Tap tempo + host sync.** Both. Default to host sync; tap overrides until host transport restarts.
5. **A/B compare.** One button, two snapshots. Industry standard.

### Tier 2 — Strong consideration (medium leverage)

6. **One global LFO with up to 4 modulation destinations.** A simple mod matrix. User picks parameter, depth, sync. Inspired by Chase Bliss's pedal-wide internal modulation. Earns the "deeper than it looks" reputation.
7. **Expression / CC pedal mapping** for any parameter — live performers use this even in a plugin via a MIDI expression pedal.
8. **Preset import/export as small JSON files** — for community sharing, the way patchstorage.com keeps ZOIA alive.

### Tier 3 — Defer to V2

9. **Audio-reactive visualizer.** Better as a content-creation tool (TikTok/Reels) than a mixing aid.
10. **CV input.** It's a plugin.
11. **Looper.** Different product.
12. **Randomize-the-block-order.** Too much chaos for a curated product.

---

## Recommendations — staged build order for Claude Code

**Phase 1 — MVP, prove the sonic identity.** All 6 blocks at baseline algorithm count (1 algorithm per block: Tube drive, Chorus mod, Micro-detune pitch, Digital delay, Plate reverb). Fixed signal chain. 4 characters (Front Porch, Cassette Sunday, Cathedral Larynx, Garage Vox). Voicing toggle with default parameter biasing. Global wet/dry + MIDI Learn. **Decision gate:** does the plugin sound musically credible on all four source types in blind tests? If no, fix Character + Drive + Space before adding more.

**Phase 2 — expand sonic palette.** All algorithm variants per block (3 drives, 3 mods, 3 pitches, 3 delays, 4 spaces). All 12 characters. The two reorder toggles. A/B compare, tap tempo, host sync. **Decision gate:** can a producer load a character and have a usable sound in under 10 seconds? If no, simplify presets or rename them.

**Phase 3 — stretch features.** Dice with parameter locks. Sidechain ducking. Global LFO mod matrix. Per-block wet/dry hidden controls. Preset import/export. **Decision gate:** is CPU under ~5% on a modern Mac/PC with everything active? If no, Output Glue and oversampling are the first things to make defeatable.

### Benchmarks that would change the recommendations

- **If CPU exceeds 8%** at default: drop Shimmer's pitch shifter to a 1024-sample window (from 2048), defeat Output Glue by default.
- **If users don't understand Voicing**: make it more visual (four big colored zones rather than a discreet toggle); show active biases on hover.
- **If presets all sound similar in testing**: Character stage is too dominant; weaken its default contribution from ~30% to ~15%.
- **If beta testers ignore the Dice**: it's not constrained enough; tighten musical ranges further.
- **If beta testers complain about Voicing as "hidden state"**: ship an inspection panel that lists what biases are active.

---

## Caveats

1. **Voicing-as-bias vs Voicing-as-EQ-curve** is a design call worth A/B testing. Biasing is more flexible but harder to explain; fixed-EQ "input mode" might be easier to reason about. Recommendation: ship biasing, but make it inspectable.
2. **Shimmer-in-feedback-loop is a runaway risk.** Hard limiter and feedback ceiling are mandatory; test exhaustively before shipping.
3. **PSOLA vs phase-vocoder for the pitch block** has CPU/quality trade-offs that should be tested with the actual target sources (saxophone, distorted guitar, polyphonic piano).
4. **"Output Glue always on" is opinionated.** Some users will hate it; surface the defeat button prominently to keep trust.
5. **Twelve presets is a recommendation, not a constraint.** If during voicing it becomes clear that two presets really do the same thing, kill one. If a thirteenth obvious banger emerges, add it. Goal: every preset earns its name.
6. **The competitive landscape moves fast** — Strymon, Chase Bliss, Universal Audio, and Meris all ship pedals frequently. The differentiator has to be *aesthetic and curation*, not feature parity; that race is unwinnable as a small operation.
7. **AI-based DSP is viable but not for V1.** Baby Audio TAIP (which Baby Audio describes on its product page as built on a "neural network" trained on analog hardware) is a named example worth benchmarking against, but is heavier-CPU and harder to debug. Stick with classical DSP for V1.
8. **"Most-loved Eventide algorithm" framing** about SpaceTime is reviewer preference (Equipboard), not a ranked survey. It still points the right direction (combined algorithms are loved), but the spec doesn't depend on it being literally #1.