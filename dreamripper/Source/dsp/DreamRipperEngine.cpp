#include "DreamRipperEngine.h"

namespace drip
{

using Svf = fofo::Svf;

namespace
{
    inline float dbToGain (float db) noexcept { return std::pow (10.0f, db * 0.05f); }

    // The oversampling factor is a constant, not a per-mode choice. Every mode
    // therefore reports the same latency, which is the only way a mode switch
    // can be safe in a host that compensates once and never re-queries.
    constexpr int kOsPower  = 2;   // 4×
    constexpr int kOsFactor = 1 << kOsPower;

    // How the total drive is split between cascaded stages. The first stage
    // takes the largest share because it is the only one seeing an unclipped
    // signal; each later stage is shaping something already bounded, so equal
    // gains there would just square the same waveform repeatedly.
    constexpr float kSplit2[3] = { 0.62f, 0.38f, 0.0f };
    constexpr float kSplit3[3] = { 0.46f, 0.33f, 0.21f };
}

// ─────────────────────────────────────────────────────────────────────────────
// The four amplifiers.
//
// These are not four EQ curves over one distortion. The stage count, the
// coupling filters between the stages, the asymmetry of each stage and the
// stiffness of the supply all change, because those are the things that
// actually differ between a fuzz box, a cranked combo and a modern high-gain
// head.
//
// `makeupDb` and `ripCompDb` are measured, not guessed: the output level of
// each mode was swept against RIP and the two numbers read off the result.
// tests/DreamRipperTests.cpp re-measures both on every build, so if the
// voicing changes and the calibration is not redone, CI says so.
// ─────────────────────────────────────────────────────────────────────────────
const DreamRipperEngine::Voicing& DreamRipperEngine::voicingFor (Mode m) noexcept
{
    static const Voicing table[kNumModes] = {
        // ── Sludge — the big muff-shaped fuzz the Pacific Northwest ran on.
        // Nearly symmetric clipping (odd harmonics, square-ish), a low
        // coupling lowpass so the top never gets brittle, and enough gain
        // that a note simply does not stop.
        { "Sludge", 2,
          /*tight*/  42.0f, 165.0f, 0.70f,
          /*bite */  1700.0f, 2.0f,
          /*mid  */  560.0f,
          /*drive*/  42.0f, /*bias*/ 0.16f,
          /*inter*/  80.0f, 5200.0f,
          /*sag  */  0.26f,
          /*cab  */  78.0f, 104.0f, 2000.0f, 2900.0f, 5200.0f,
          /*trim */  -4.7f, 14.9f },

        // ── Grunge — a cranked combo on the edge of falling apart. The most
        // asymmetric of the four, which is what lets it clean up when the
        // guitar's volume comes down instead of just getting quieter.
        { "Grunge", 2,
          58.0f, 235.0f, 0.75f,
          2200.0f, 3.5f,
          650.0f,
          36.0f, 0.44f,
          120.0f, 6800.0f,
          0.22f,
          82.0f, 110.0f, 2500.0f, 3200.0f, 6000.0f,
          -4.4f, 15.0f },

        // ── Metal — three stages, tighter coupling, a stiffer supply. Fast
        // picking survives here because the low end is gone before the gain
        // and the sag barely moves.
        { "Metal", 3,
          78.0f, 320.0f, 0.85f,
          2700.0f, 5.0f,
          760.0f,
          46.0f, 0.26f,
          175.0f, 8200.0f,
          0.13f,
          86.0f, 118.0f, 3000.0f, 3400.0f, 6600.0f,
          -6.4f, 13.3f },

        // ── Djent — the modern rhythm sound: everything below the fundamental
        // of a drop-tuned low string removed before the gain, a nearly rigid
        // supply, and the most presence. Made to be gated hard.
        { "Djent", 3,
          105.0f, 420.0f, 1.00f,
          3100.0f, 6.0f,
          830.0f,
          48.0f, 0.18f,
          240.0f, 9200.0f,
          0.07f,
          92.0f, 126.0f, 3400.0f, 3600.0f, 7200.0f,
          -7.1f, 12.6f }
    };

    return table[juce::jlimit (0, kNumModes - 1, (int) m)];
}

DreamRipperEngine::DreamRipperEngine()  = default;
DreamRipperEngine::~DreamRipperEngine() = default;

// ─────────────────────────────────────────────────────────────────────────────
void DreamRipperEngine::buildGraph()
{
    auto chain = std::make_unique<fofo::Chain>();

    chain->add (fofo::makeFnNode ([this] (juce::AudioBuffer<float>& b, int n) { frontEnd (b, n); }));

    auto os = std::make_unique<fofo::Oversampled> (
        [this] (float x, int ch) { return cascadeSample (x, ch); }, kOsPower);
    cascade = os.get();
    chain->add (std::move (os));

    chain->add (fofo::makeFnNode ([this] (juce::AudioBuffer<float>& b, int n) { backEnd (b, n); }));

    // Blend, not Ducked or Additive: at MIX 100 you hear the amplifier and
    // nothing else, which is what an amplifier is. Parallel owns the dry
    // snapshot and holds it back by the oversampler's own reported latency, so
    // partial MIX is a genuine blend rather than a comb filter.
    graph = std::make_unique<fofo::Parallel> (std::move (chain), fofo::MixRule::Blend, mix01);
}

void DreamRipperEngine::prepare (const juce::dsp::ProcessSpec& s)
{
    spec = { s.sampleRate, (int) s.maximumBlockSize, (int) juce::jmax (1u, s.numChannels) };

    const double osRate = spec.sampleRate * (double) kOsFactor;

    for (auto& c : chan)
    {
        c.tightHp.prepare (spec.sampleRate);
        c.bite   .prepare (spec.sampleRate);
        c.midPush.prepare (spec.sampleRate);

        // The cascade runs inside the oversampled region, so everything it
        // touches has to be designed at the oversampled rate — a filter
        // prepared at base rate and run at 4× sits two octaves too high.
        for (int i = 0; i < kMaxStages; ++i)
        {
            c.interHp[i].prepare (osRate);
            c.interLp[i].prepare (osRate);
            c.stageDc[i].prepare (osRate, 12.0f);
            c.shaper[i].reset();
        }

        c.midScoop.prepare (spec.sampleRate);
        c.cabHp   .prepare (spec.sampleRate);
        c.cabRes  .prepare (spec.sampleRate);
        c.cabDip  .prepare (spec.sampleRate);
        c.cabPres .prepare (spec.sampleRate);
        c.cabLp1  .prepare (spec.sampleRate);
        c.cabLp2  .prepare (spec.sampleRate);
        c.cabFizz .prepare (spec.sampleRate);
        c.outDc   .prepare (spec.sampleRate, 18.0f);
    }

    gate.prepare (spec.sampleRate);

    sagAtk = std::exp (-1.0f / (0.012f * (float) spec.sampleRate));
    sagRel = std::exp (-1.0f / (0.260f * (float) spec.sampleRate));

    buildGraph();
    graph->prepare (spec);
    reportedLatency = graph->latencySamples();

    reset();
}

void DreamRipperEngine::reset()
{
    for (auto& c : chan)
    {
        c.tightHp.reset(); c.bite.reset(); c.midPush.reset();
        for (int i = 0; i < kMaxStages; ++i)
        {
            c.shaper[i].reset();
            c.interHp[i].reset();
            c.interLp[i].reset();
            c.stageDc[i].reset();
        }
        c.midScoop.reset();
        c.cabHp.reset(); c.cabRes.reset(); c.cabDip.reset(); c.cabPres.reset();
        c.cabLp1.reset(); c.cabLp2.reset(); c.cabFizz.reset();
        c.outDc.reset();
    }

    gate.reset();
    sagEnv = 0.0f;
    gateGain.store (1.0f, std::memory_order_relaxed);

    if (graph) graph->reset();
}

// ─────────────────────────────────────────────────────────────────────────────
// Parameter mapping — everything the audio path reads is set here, once per
// block, so nothing in the per-sample code has to branch on a knob.
// ─────────────────────────────────────────────────────────────────────────────
void DreamRipperEngine::applyParams() noexcept
{
    const auto& v = voicing();

    activeStages = juce::jlimit (1, kMaxStages, v.stages);
    sagDepth     = v.sagDepth;

    // ── gate ─────────────────────────────────────────────────────────────
    // Below 0.5% the gate leaves the circuit rather than sitting at a very low
    // threshold: an "off" that still runs a detector is an "off" that can
    // still surprise you on a quiet passage.
    gate.setThresholdDb (gate01 < 0.005f
                             ? -90.0f
                             : juce::jmap (std::pow (gate01, 0.75f), -78.0f, -26.0f));
    gate.setAggression (gate01);

    // ── front end ────────────────────────────────────────────────────────
    // The resonant highpass in front of the gain is the single most important
    // filter in the pedal. Low end that reaches a clipper intermodulates with
    // everything above it; removing it first is the whole difference between a
    // chug and a flub, and it is why real rigs are built this way.
    const float tightHz = juce::jmap (std::pow (tight01, 0.85f), v.tightLoHz, v.tightHiHz);

    // Pick attack going into the gain. It grows with TIGHT because a tight amp
    // with no bite is just a thin one.
    const float biteDb = v.biteDb * (0.60f + 0.80f * tight01);

    // SCOOP is one knob straddling two different jobs, which is why it works:
    // pushing mids INTO the gain is more saturation and more cut, scooping
    // them AFTER it is the metal "V". They are not the same operation and
    // cannot be swapped.
    const float push  = juce::jmax (0.0f, 0.5f - scoop01) * 2.0f;
    const float dig   = juce::jmax (0.0f, scoop01 - 0.5f) * 2.0f;
    const float preMidDb  = push * 10.0f - dig * 4.0f;
    const float postMidDb = push * 2.5f  - dig * 16.0f;

    for (auto& c : chan)
    {
        c.tightHp.set (Svf::Type::Highpass, tightHz, v.tightQ);
        c.bite   .set (Svf::Type::Bell,     v.biteHz, 1.1f, biteDb);
        c.midPush.set (Svf::Type::Bell,     v.midHz,  0.75f, preMidDb);
        c.midScoop.set (Svf::Type::Bell,    v.midHz * 1.05f, 0.90f, postMidDb);
    }

    // ── the cascade ──────────────────────────────────────────────────────
    // dB-tapered with a floor: RIP at zero is still an amplifier with its
    // volume up, not a clean DI. This pedal has no clean setting on purpose.
    const float totalDriveDb = v.driveDbMax * (0.16f + 0.84f * std::pow (rip01, 1.35f));
    const float* split = (activeStages >= 3) ? kSplit3 : kSplit2;

    // The first stage's gain is applied at base rate in the front end — it is
    // linear, so it costs nothing to move it out of the oversampled loop.
    preGain = dbToGain (totalDriveDb * split[0]);
    stageGain[0] = 1.0f;
    for (int i = 1; i < kMaxStages; ++i)
        stageGain[i] = dbToGain (totalDriveDb * split[i]);

    // Asymmetry rises with drive, the way a tube biases hotter as it is pushed.
    stageBias = v.bias * (0.35f + 0.65f * rip01);

    // Coupling filters. The highpass climbs stage by stage, which is what
    // keeps a three-stage cascade from turning into mud: each stage hands the
    // next one less low end than it was given.
    const float interHpBase = v.interLoHz * (0.70f + 0.60f * tight01);
    for (auto& c : chan)
        for (int i = 0; i < kMaxStages; ++i)
        {
            c.interHp[i].set (Svf::Type::Highpass, interHpBase * (1.0f + 0.35f * (float) i), 0.70f);
            c.interLp[i].set (Svf::Type::Lowpass,  v.interHiHz * (1.0f - 0.15f * (float) i), 0.70f);
        }

    // ── speaker ──────────────────────────────────────────────────────────
    // A filter stack, not a noise-seeded impulse. The load-bearing part is the
    // first lowpass being resonant (Q ≈ 1.6): the bump sitting on the edge of
    // the cliff is what reads as a speaker rather than as a blanket thrown
    // over the amp.
    const float lpHz    = juce::jmap (cab01, v.cabLpLoHz, v.cabLpHiHz);
    const float presDb  = juce::jmap (cab01, 1.5f, 6.0f);
    const float fizzDb  = juce::jmap (cab01, -2.0f, -4.5f);

    for (auto& c : chan)
    {
        c.cabHp  .set (Svf::Type::Highpass, v.cabHpHz,   0.80f);
        c.cabRes .set (Svf::Type::Bell,     v.cabResHz,  1.40f,  3.5f);
        c.cabDip .set (Svf::Type::Bell,     420.0f,      0.90f, -2.8f);
        c.cabPres.set (Svf::Type::Bell,     v.cabPresHz, 1.20f,  presDb);
        c.cabLp1 .set (Svf::Type::Lowpass,  lpHz,        1.60f);
        c.cabLp2 .set (Svf::Type::Lowpass,  lpHz * 1.35f, 0.60f);
        c.cabFizz.set (Svf::Type::Bell,     7600.0f,     1.00f,  fizzDb);
    }

    // ── output ───────────────────────────────────────────────────────────
    // RIP has to be a texture control. A cascade of tanh stages is bounded, so
    // the level does not run away the way an unbounded shaper's would — but it
    // does still rise by 13-15 dB across the sweep, because a squarer wave
    // carries more RMS for the same peak.
    //
    // That rise is not proportional to the drive in dB: it is steep at the
    // bottom of the knob and almost flat at the top, so a straight
    // "give back a fraction of the drive" compensation leaves ~5 dB of travel
    // whatever fraction you pick. The curve below is the measured shape,
    // normalised to 1 at full RIP and shared by all four modes (their measured
    // curves agree to within half a dB once normalised); `ripCompDb` is how
    // many dB of it each mode has to give back.
    const float r = rip01;
    const float ripCurve = r * (1.35f + r * (0.50f - 0.85f * r));

    // SCOOP has the same obligation. Cutting 12 dB out of the region a guitar
    // keeps most of its energy in costs about 6 dB of broadband level, so a
    // scooped preset next to a flat one would read as "quieter" rather than as
    // "scooped". Half the post-EQ bell's gain comes back, measured the same
    // way the RIP curve was.
    const float scoopCompDb = -0.50f * postMidDb;

    const float makeupDb = v.makeupDb - v.ripCompDb * ripCurve + scoopCompDb;

    // 50 is unity. LEVEL is an honest gain and nothing follows it: an output
    // clipper here would be a limiter pretending to be a volume control, and
    // on a partial MIX it would be sitting on the dry path as well.
    const float levelDb = (level01 - 0.5f) * 40.0f;

    postGain = dbToGain (makeupDb + levelDb);

    graph->setMix (mix01);
}

// ─────────────────────────────────────────────────────────────────────────────
// Processing
// ─────────────────────────────────────────────────────────────────────────────
void DreamRipperEngine::process (juce::AudioBuffer<float>& buffer) noexcept
{
    const int nCh = juce::jmin (buffer.getNumChannels(), 2);
    const int nS  = buffer.getNumSamples();
    if (nCh == 0 || nS == 0 || graph == nullptr) return;

    {
        float p = 0.0f;
        for (int ch = 0; ch < nCh; ++ch) p = juce::jmax (p, buffer.getMagnitude (ch, 0, nS));
        float cur = inputPeak.load (std::memory_order_relaxed);
        while (p > cur && ! inputPeak.compare_exchange_weak (cur, p, std::memory_order_release, std::memory_order_relaxed)) {}
    }

    applyParams();
    graph->process (buffer, nS);

    {
        float p = 0.0f;
        for (int ch = 0; ch < nCh; ++ch) p = juce::jmax (p, buffer.getMagnitude (ch, 0, nS));
        float cur = outputPeak.load (std::memory_order_relaxed);
        while (p > cur && ! outputPeak.compare_exchange_weak (cur, p, std::memory_order_release, std::memory_order_relaxed)) {}
    }
}

void DreamRipperEngine::frontEnd (juce::AudioBuffer<float>& buffer, int nS) noexcept
{
    const int nCh = juce::jmin (buffer.getNumChannels(), 2);
    float lastGate = 1.0f;

    for (int n = 0; n < nS; ++n)
    {
        // One detector fed the louder side. A gate or a sagging supply that
        // acts on one channel only is a stereo image that slides around when
        // you dig in, which is the same defect a per-channel compressor has.
        float key = 0.0f;
        for (int ch = 0; ch < nCh; ++ch)
            key = juce::jmax (key, std::abs (buffer.getReadPointer (ch)[n]));

        const float g = gate.processGain (key);
        lastGate = g;

        const float a = key > sagEnv ? sagAtk : sagRel;
        sagEnv = a * sagEnv + (1.0f - a) * key;

        // Supply droop: dig in and the stages get less to work with, so the
        // amp compresses and then blooms back as the note decays. Modern
        // high-gain modes are nearly rigid here; the fuzz is not.
        const float sagGain = 1.0f - sagDepth * juce::jlimit (0.0f, 1.0f, sagEnv * 1.8f);
        const float drive   = preGain * sagGain * g;

        for (int ch = 0; ch < nCh; ++ch)
        {
            auto& c = chan[(size_t) ch];
            auto* d = buffer.getWritePointer (ch);
            float x = d[n];
            x = c.tightHp.process (x);
            x = c.bite   .process (x);
            x = c.midPush.process (x);
            d[n] = x * drive;
        }
    }

    gateGain.store (lastGate, std::memory_order_relaxed);
}

float DreamRipperEngine::cascadeSample (float x, int channel) noexcept
{
    auto& c = chan[(size_t) juce::jlimit (0, 1, channel)];

    for (int i = 0; i < activeStages; ++i)
    {
        // Stage 0's low end was already shaped by TIGHT at base rate; from
        // there on, each coupling stage hands the next one less.
        if (i > 0) x = c.interHp[i].process (x);

        x = c.shaper[i].process (x * stageGain[i], stageBias * (1.0f - 0.30f * (float) i));

        // An asymmetric shaper leaves a DC offset behind. Left in place it
        // would eat the next stage's headroom on one side only, so it comes
        // off between every pair of stages rather than once at the end.
        x = c.stageDc[i].process (x);
        x = c.interLp[i].process (x);
    }

    return x;
}

void DreamRipperEngine::backEnd (juce::AudioBuffer<float>& buffer, int nS) noexcept
{
    const int nCh = juce::jmin (buffer.getNumChannels(), 2);

    for (int ch = 0; ch < nCh; ++ch)
    {
        auto& c = chan[(size_t) ch];
        auto* d = buffer.getWritePointer (ch);

        for (int n = 0; n < nS; ++n)
        {
            float x = c.outDc.process (d[n]);
            x = c.midScoop.process (x);

            x = c.cabHp  .process (x);
            x = c.cabRes .process (x);
            x = c.cabDip .process (x);
            x = c.cabPres.process (x);
            x = c.cabLp1 .process (x);
            x = c.cabLp2 .process (x);
            x = c.cabFizz.process (x);

            d[n] = x * postGain;
        }
    }
}

} // namespace drip
