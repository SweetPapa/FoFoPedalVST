#include "SwayEngine.h"

namespace sway
{

using fofo::Lfo;
using fofo::Drift;
using fofo::ModMatrix;

namespace
{
    // RATE maps logarithmically — the useful settings are bunched at the slow
    // end, and a linear knob wastes most of its travel above 3 Hz.
    inline float mapRate (float r01, float lo, float hi) noexcept
    {
        return lo * std::pow (hi / lo, r01);
    }

    constexpr float kEnsBaseMs[3]  = { 6.5f, 9.0f, 12.5f };
    constexpr float kEnsRateMul[3] = { 1.00f, 1.19f, 1.41f };  // mutually irrational-ish
    constexpr float kEnsPhase[3]   = { 0.0f, 1.0f / 3.0f, 2.0f / 3.0f };
    constexpr float kEnsPan[3]     = { -1.0f, 0.0f, +1.0f };
}

SwayEngine::SwayEngine()  = default;
SwayEngine::~SwayEngine() = default;

// ─────────────────────────────────────────────────────────────────────────────
void SwayEngine::prepare (const juce::dsp::ProcessSpec& s)
{
    spec = { s.sampleRate, (int) s.maximumBlockSize, (int) juce::jmax (1u, s.numChannels) };

    // ── Tape ─────────────────────────────────────────────────────────────
    // Wow is reel and capstan eccentricity: slow, roughly sinusoidal, and
    // slightly different on each channel because the tape is not perfectly
    // flat against the head. Flutter is idler and scrape: faster, and drift
    // keeps neither of them mechanically exact.
    {
        tapeMod = ModMatrix {};

        sWowL = tapeMod.addSource (std::make_unique<Lfo> (Lfo::Shape::Sine, 0.7f, 0x5A11u));
        sWowR = tapeMod.addSource (std::make_unique<Lfo> (Lfo::Shape::Sine, 0.7f, 0x5B22u));
        sFltL = tapeMod.addSource (std::make_unique<Lfo> (Lfo::Shape::Triangle, 7.0f, 0x6C33u));
        sFltR = tapeMod.addSource (std::make_unique<Lfo> (Lfo::Shape::Triangle, 7.0f, 0x6D44u));
        sSlip = tapeMod.addSource (std::make_unique<Drift> (0.11f, 0x7E55u));

        // Quadrature start so the two sides never peak together.
        static_cast<Lfo*> (tapeMod.source (sWowR))->setStartPhase (0.25f);
        static_cast<Lfo*> (tapeMod.source (sFltR))->setStartPhase (0.37f);
        // A real transport is never mechanically exact.
        for (auto id : { sWowL, sWowR, sFltL, sFltR })
            static_cast<Lfo*> (tapeMod.source (id))->setRateDrift (0.35f);

        const float maxSamp = 0.004f * (float) spec.sampleRate;   // headroom
        dTapeDelayL = tapeMod.addDest ("delayL", 0.0f, -maxSamp, maxSamp);
        dTapeDelayR = tapeMod.addDest ("delayR", 0.0f, -maxSamp, maxSamp);

        // Routes are wired once here and only their depths change per block —
        // rebuilding the list every block would touch the heap on the audio
        // thread.
        rWowL  = tapeMod.connect (sWowL, dTapeDelayL, 0.0f);
        rFltL  = tapeMod.connect (sFltL, dTapeDelayL, 0.0f);
        rSlipL = tapeMod.connect (sSlip, dTapeDelayL, 0.0f);
        rWowR  = tapeMod.connect (sWowR, dTapeDelayR, 0.0f);
        rFltR  = tapeMod.connect (sFltR, dTapeDelayR, 0.0f);
        rSlipR = tapeMod.connect (sSlip, dTapeDelayR, 0.0f);

        tapeMod.prepare (spec);

        tapeSat.prepare (spec);
        tapeTransport.prepare (spec);
        tapeTransport.setSampleTick ([this] { tapeMod.tick (0.0f); });
        tapeTransport.setDelayProvider ([this] (int ch)
        {
            return tapeMod.get (ch == 0 ? dTapeDelayL : dTapeDelayR);
        });
    }

    // ── Ensemble ─────────────────────────────────────────────────────────
    {
        ensMod = ModMatrix {};
        const float maxSamp = 0.005f * (float) spec.sampleRate;

        for (int v = 0; v < kEnsVoices; ++v)
        {
            sEnsLfo[v]   = ensMod.addSource (std::make_unique<Lfo> (
                Lfo::Shape::Sine, 0.6f * kEnsRateMul[v], 0xE570u + (uint32_t) v * 7919u));
            sEnsDrift[v] = ensMod.addSource (std::make_unique<Drift> (
                0.23f, 0xD41Fu + (uint32_t) v * 104729u));

            static_cast<Lfo*> (ensMod.source (sEnsLfo[v]))->setStartPhase (kEnsPhase[v]);
            static_cast<Lfo*> (ensMod.source (sEnsLfo[v]))->setRateDrift (0.25f);

            dEnsDelay[v] = ensMod.addDest ("ensDelay", 0.0f, -maxSamp, maxSamp);
            rEnsLfo[v]   = ensMod.connect (sEnsLfo[v],   dEnsDelay[v], 0.0f);
            rEnsDrift[v] = ensMod.connect (sEnsDrift[v], dEnsDelay[v], 0.0f);

            ensLine[v].prepare (spec.sampleRate, (float) (0.030 * spec.sampleRate));
            ensDark[v].prepare (spec.sampleRate);
        }
        ensMod.prepare (spec);
        ensWet.setSize (2, spec.maxBlockSize, false, true, true);
    }

    // ── Pump ─────────────────────────────────────────────────────────────
    {
        pumpMod = ModMatrix {};
        sPumpLfo = pumpMod.addSource (std::make_unique<Lfo> (Lfo::Shape::Sine, 3.0f, 0x9F3Au));
        static_cast<Lfo*> (pumpMod.source (sPumpLfo))->setRateDrift (0.06f);

        dPumpGain   = pumpMod.addDest ("gain",   1.0f, 0.0f, 1.0f);
        dPumpCutoff = pumpMod.addDest ("cutoff", 18000.0f, 400.0f, 20000.0f);
        rPumpGain   = pumpMod.connect (sPumpLfo, dPumpGain,   0.0f);
        rPumpCutoff = pumpMod.connect (sPumpLfo, dPumpCutoff, 0.0f);
        pumpMod.prepare (spec);

        for (auto& f : pumpFilter) f.prepare (spec.sampleRate);
    }

    drySnap.setSize (2, spec.maxBlockSize, false, true, true);

    // One latency figure for every mode (see the header). Tape genuinely has
    // it; the other two get padded to match so switching modes never shifts
    // the track under a host that has already compensated.
    reportedLatency = tapeSat.latencySamples() + tapeTransport.latencySamples();
    outputPad.prepare (spec, (float) reportedLatency + 8.0f);
    outputPad.setDelay ((float) reportedLatency);

    reset();
}

void SwayEngine::reset()
{
    tapeMod.reset();
    tapeSat.reset();
    tapeTransport.reset();

    ensMod.reset();
    for (auto& l : ensLine) l.reset();
    for (auto& f : ensDark) f.reset();
    ensWet.clear();

    pumpMod.reset();
    for (auto& f : pumpFilter) f.reset();

    outputPad.reset();
    drySnap.clear();
}

int SwayEngine::getLatencySamples() const noexcept
{
    return reportedLatency;
}

// ─────────────────────────────────────────────────────────────────────────────
// Parameter mapping
// ─────────────────────────────────────────────────────────────────────────────

void SwayEngine::applyTapeParams()
{
    // MIX is the master "how much pedal" for a mode that is 100% wet: it
    // scales every departure from a clean signal at once.
    const float amt = mix01;

    // COLOR is one axis of machine condition. Everything that gets worse as a
    // deck ages moves together — that is what makes it a single musical knob
    // instead of four engineering ones.
    const float worn = color01;

    // ── transport ────────────────────────────────────────────────────────
    // Wow slows and deepens on a tired transport; flutter rises sharply.
    const float wowHz  = mapRate (rate01, 0.25f, 1.7f) * (1.0f - 0.25f * worn);
    const float fltHz  = mapRate (rate01, 4.5f, 15.0f) * (1.0f + 0.35f * worn);

    static_cast<Lfo*> (tapeMod.source (sWowL))->setRateHz (wowHz);
    static_cast<Lfo*> (tapeMod.source (sWowR))->setRateHz (wowHz * 1.06f);
    static_cast<Lfo*> (tapeMod.source (sFltL))->setRateHz (fltHz);
    static_cast<Lfo*> (tapeMod.source (sFltR))->setRateHz (fltHz * 1.13f);

    const float msToSamp = 0.001f * (float) spec.sampleRate;
    const float depth    = move01 * amt;

    // Calibrated against what real transports actually do, measured as RMS
    // pitch deviation at MOVE 1 (tests/SwayTests.cpp reports this directly):
    //   COLOR 0  →  ~3 cents   a serviced deck; movement you feel, not hear
    //   COLOR 1  →  ~20 cents  a cassette on its way out
    // Before this calibration every setting sat around 36-43 cents, which is
    // seasick rather than musical, and COLOR barely changed the amount at all.
    const float wear = juce::jmap (worn, 0.083f, 0.463f);

    // The balance shifts as well as the amount: a good machine's error is
    // slow wow, a dying one's is fast flutter.
    const float wowMs    = depth * wear * (1.0f - 0.55f * worn) * 3.2f;
    const float fltMs    = depth * wear * (0.10f + 0.55f * worn) * 0.55f;
    const float slipMs   = depth * wear * 0.9f;

    tapeMod.setRouteDepth (rWowL,  wowMs  * msToSamp);
    tapeMod.setRouteDepth (rFltL,  fltMs  * msToSamp);
    tapeMod.setRouteDepth (rSlipL, slipMs * msToSamp);
    tapeMod.setRouteDepth (rWowR,  wowMs  * msToSamp);
    tapeMod.setRouteDepth (rFltR,  fltMs  * msToSamp);
    tapeMod.setRouteDepth (rSlipR, slipMs * msToSamp * 0.85f);

    // ── record head ──────────────────────────────────────────────────────
    tapeSat.setDrive (amt * (0.16f + 0.62f * worn));
    tapeSat.setEmphasisDb (2.0f + 5.0f * worn);

    // ── replay head and tape ─────────────────────────────────────────────
    // The bump moves down and grows as the deck tires; gap loss closes in.
    tapeTransport.setCentreDelayMs (6.0f);
    tapeTransport.setHeadBump (juce::jmap (worn, 88.0f, 55.0f), amt * juce::jmap (worn, 1.2f, 3.4f));
    tapeTransport.setGapLossHz (juce::jmap (worn, 17000.0f, 5200.0f));
    tapeTransport.setSelfErasure (amt * (0.20f + 0.45f * worn));
    tapeTransport.setDropoutAmount (amt * juce::jmax (0.0f, worn - 0.35f) * 1.5f);
    tapeTransport.setHissDb (amt < 0.01f ? -120.0f : juce::jmap (worn, -86.0f, -66.0f));
}

void SwayEngine::applyEnsembleParams()
{
    const float rateHz = mapRate (rate01, 0.12f, 3.0f);
    const float swingMs = juce::jmap (move01, 0.15f, 3.0f);
    const float msToSamp = 0.001f * (float) spec.sampleRate;

    for (int v = 0; v < kEnsVoices; ++v)
    {
        static_cast<Lfo*> (ensMod.source (sEnsLfo[v]))->setRateHz (rateHz * kEnsRateMul[v]);
        ensMod.setRouteDepth (rEnsLfo[v],   swingMs * msToSamp);
        ensMod.setRouteDepth (rEnsDrift[v], swingMs * msToSamp * 0.35f);
    }

    // COLOR darkens the voices as it widens them — a bucket-brigade ensemble
    // is dark, and keeping the copies behind the dry is what stops a chorus
    // from sounding like a phaser.
    const float darkHz = juce::jmap (color01, 8000.0f, 3200.0f);
    for (auto& f : ensDark) f.set (fofo::Svf::Type::Lowpass, darkHz, 0.7f);
}

void SwayEngine::applyPumpParams()
{
    const float rateHz = mapRate (rate01, 0.4f, 11.0f);
    static_cast<Lfo*> (pumpMod.source (sPumpLfo))->setRateHz (rateHz);
    static_cast<Lfo*> (pumpMod.source (sPumpLfo))->setShape (
        color01 < 0.5f ? Lfo::Shape::Sine : Lfo::Shape::Triangle);

    const float depth = move01 * mix01 * 0.85f;

    pumpMod.setBase (dPumpGain, 1.0f - depth * 0.5f);
    pumpMod.setRouteDepth (rPumpGain, depth * 0.5f);

    // The filter breathing with the gain is what the old engine could not do —
    // it had no resonant filter to move. A tremolo that only changes level
    // reads as chopping; one where the tone closes as it ducks reads as
    // breathing.
    const float filtDepth = color01 * 0.9f;
    const float centre = juce::jmap (filtDepth, 19000.0f, 4200.0f);
    pumpMod.setBase (dPumpCutoff, centre);
    pumpMod.setRouteDepth (rPumpCutoff, filtDepth * centre * 0.62f);
}

// ─────────────────────────────────────────────────────────────────────────────
// Processing
// ─────────────────────────────────────────────────────────────────────────────

void SwayEngine::process (juce::AudioBuffer<float>& buffer) noexcept
{
    const int nCh = juce::jmin (buffer.getNumChannels(), 2);
    const int nS  = buffer.getNumSamples();
    if (nCh == 0 || nS == 0) return;

    {
        float peak = 0.0f;
        for (int ch = 0; ch < nCh; ++ch) peak = juce::jmax (peak, buffer.getMagnitude (ch, 0, nS));
        float cur = inputPeak.load (std::memory_order_relaxed);
        while (peak > cur && ! inputPeak.compare_exchange_weak (cur, peak, std::memory_order_release, std::memory_order_relaxed)) {}
    }

    switch (mode)
    {
        case Mode::Ensemble: applyEnsembleParams(); processEnsemble (buffer, nS); outputPad.process (buffer, nS); break;
        case Mode::Pump:     applyPumpParams();     processPump     (buffer, nS); outputPad.process (buffer, nS); break;
        case Mode::Tape:
        default:             applyTapeParams();     processTape     (buffer, nS); break;
    }

    {
        float peak = 0.0f;
        for (int ch = 0; ch < nCh; ++ch) peak = juce::jmax (peak, buffer.getMagnitude (ch, 0, nS));
        float cur = outputPeak.load (std::memory_order_relaxed);
        while (peak > cur && ! outputPeak.compare_exchange_weak (cur, peak, std::memory_order_release, std::memory_order_relaxed)) {}
    }
}

void SwayEngine::processTape (juce::AudioBuffer<float>& buffer, int nS) noexcept
{
    // No dry blend anywhere in here — that was the bug. A tape machine is
    // something you play through, so the signal is the tape's output.
    tapeSat.process (buffer, nS);

    // The transport advances the matrix itself, once per sample, via the tick
    // callback wired in prepare() — so the modulation and the read pointer
    // can't drift out of step with each other.
    tapeTransport.process (buffer, nS);
}

void SwayEngine::processEnsemble (juce::AudioBuffer<float>& buffer, int nS) noexcept
{
    const int nCh = juce::jmin (buffer.getNumChannels(), 2);

    for (int ch = 0; ch < nCh; ++ch)
        drySnap.copyFrom (ch, 0, buffer, ch, 0, nS);

    auto* wl = ensWet.getWritePointer (0);
    auto* wr = ensWet.getWritePointer (1);

    const float width = 0.4f + 0.6f * color01;
    const float msToSamp = 0.001f * (float) spec.sampleRate;

    for (int n = 0; n < nS; ++n)
    {
        ensMod.tick (0.0f);

        float mono = 0.0f;
        for (int ch = 0; ch < nCh; ++ch) mono += buffer.getReadPointer (ch)[n];
        mono /= (float) nCh;

        float accL = 0.0f, accR = 0.0f;
        for (int v = 0; v < kEnsVoices; ++v)
        {
            const float d = juce::jmax (2.0f, kEnsBaseMs[v] * msToSamp + ensMod.get (dEnsDelay[v]));
            float tap = ensDark[v].process (ensLine[v].processSample (mono, d));

            const float pan = kEnsPan[v] * width;
            accL += tap * std::sqrt (0.5f * (1.0f - pan));
            accR += tap * std::sqrt (0.5f * (1.0f + pan));
        }

        const float norm = 0.5773503f; // 1/sqrt(3)
        wl[n] = accL * norm;
        wr[n] = accR * norm;
    }

    // Ensemble is the one mode where dry and wet genuinely belong together —
    // the comb between them IS the chorus. Straight blend, dry untouched, and
    // no clipper on the sum.
    const float dry = 1.0f - mix01;
    const float wet = mix01;
    for (int ch = 0; ch < nCh; ++ch)
    {
        auto* d = buffer.getWritePointer (ch);
        const auto* w = ensWet.getReadPointer (nCh == 1 ? 0 : ch);
        const auto* wOther = ensWet.getReadPointer (nCh == 1 ? 1 : ch);
        const auto* dr = drySnap.getReadPointer (ch);
        for (int n = 0; n < nS; ++n)
        {
            const float wv = (nCh == 1) ? 0.5f * (w[n] + wOther[n]) : w[n];
            d[n] = dr[n] * dry + wv * wet;
        }
    }
}

void SwayEngine::processPump (juce::AudioBuffer<float>& buffer, int nS) noexcept
{
    const int nCh = juce::jmin (buffer.getNumChannels(), 2);

    for (int n = 0; n < nS; ++n)
    {
        pumpMod.tick (0.0f);
        const float g  = pumpMod.get (dPumpGain);
        const float fc = pumpMod.get (dPumpCutoff);

        // One gain and one cutoff for both channels — a tremolo whose two
        // sides breathe independently reads as broken, not wide.
        for (int ch = 0; ch < nCh; ++ch)
        {
            auto* d = buffer.getWritePointer (ch);
            d[n] = pumpFilter[ch].processModulated (d[n], fc) * g;
        }
    }
}

} // namespace sway
