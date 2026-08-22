#include "DoubleEngine.h"

namespace dbl
{

namespace
{
    // Per-voice base delays (ms) — staggered like takes, not like taps.
    constexpr float kVoiceOffsetMs[DoubleEngine::kVoices] = { 0.0f, 7.0f, 3.5f, 11.0f };
    // Detune polarity/scale per voice: pair 1 = ±1×, pair 2 = ±2.1×.
    constexpr float kVoiceDetuneMul[DoubleEngine::kVoices] = { -1.0f, +1.0f, -2.1f, +2.1f };
    // Pan position per voice (-1..+1 at full WIDE).
    constexpr float kVoicePan[DoubleEngine::kVoices] = { -1.0f, +1.0f, +0.6f, -0.6f };
}

void DoubleEngine::prepare (const juce::dsp::ProcessSpec& s)
{
    spec = s;

    juce::dsp::ProcessSpec mono { s.sampleRate, s.maximumBlockSize, 1 };

    for (int v = 0; v < kVoices; ++v)
    {
        // 35 ms window: the dry-use sweet spot (short enough not to smear,
        // long enough not to warble).
        shifter[v].prepare (s.sampleRate, 35.0f, 0xD0B1E5u + (uint32_t) v * 7919u);

        voiceDelay[v].reset();
        voiceDelay[v].prepare (mono);
        voiceDelay[v].setMaximumDelayInSamples ((int) std::ceil (0.060 * s.sampleRate));

        pitchDrift[v].prepare (0.30f, s.sampleRate, 0xA11CE5u + (uint32_t) v * 104729u);
        timeDrift [v].prepare (0.20f, s.sampleRate, 0xB22DF6u + (uint32_t) v * 104729u);
        levelDrift[v].prepare (0.15f, s.sampleRate, 0xC33EA7u + (uint32_t) v * 104729u);

        ratioSm[v].reset (s.sampleRate, 0.05);
        ratioSm[v].setCurrentAndTargetValue (1.0f);
        gainSm[v].reset (s.sampleRate, 0.05);
        gainSm[v].setCurrentAndTargetValue (0.0f);
    }

    busNormSm.reset (s.sampleRate, 0.05);
    busNormSm.setCurrentAndTargetValue (1.0f / std::sqrt (2.0f));

    wetBus .setSize (2, (int) s.maximumBlockSize, false, true, true);
    monoSrc.setSize (1, (int) s.maximumBlockSize, false, true, true);

    modeDirty = true;
    updateModeVoicing();
}

void DoubleEngine::reset()
{
    for (int v = 0; v < kVoices; ++v)
    {
        shifter[v].reset();
        voiceDelay[v].reset();
    }
    for (auto& f : wetHP)  f.reset();
    for (auto& f : wetLP)  f.reset();
    for (auto& f : wetDip) f.reset();
}

void DoubleEngine::updateModeVoicing()
{
    if (! modeDirty || spec.sampleRate <= 0.0) return;
    const double sr = spec.sampleRate;

    float hpHz, lpHz, dipHz, dipDb;
    switch (mode)
    {
        case Mode::Strings: hpHz = 130.0f; lpHz = 11000.0f; dipHz = 2800.0f; dipDb = -1.5f; break; // keep doubles behind the pick attack
        case Mode::Synth:   hpHz = 110.0f; lpHz = 14000.0f; dipHz = 1000.0f; dipDb =  0.0f; break;
        case Mode::Vox:
        default:            hpHz = 160.0f; lpHz = 12000.0f; dipHz = 3200.0f; dipDb = -2.0f; break; // dip the presence so doubles sit behind the lead
    }

    auto hp  = juce::dsp::IIR::Coefficients<float>::makeHighPass (sr, hpHz);
    auto lp  = juce::dsp::IIR::Coefficients<float>::makeLowPass  (sr, lpHz, 0.6f);
    auto dip = dipDb != 0.0f
        ? juce::dsp::IIR::Coefficients<float>::makePeakFilter (sr, dipHz, 0.9f, juce::Decibels::decibelsToGain (dipDb))
        : juce::dsp::IIR::Coefficients<float>::makeAllPass (sr, 1000.0);

    for (auto& f : wetHP)  f.coefficients = hp;
    for (auto& f : wetLP)  f.coefficients = lp;
    for (auto& f : wetDip) f.coefficients = dip;
    modeDirty = false;
}

void DoubleEngine::process (juce::AudioBuffer<float>& buffer) noexcept
{
    const int nCh = juce::jmin (buffer.getNumChannels(), 2);
    const int nS  = buffer.getNumSamples();
    if (nCh == 0 || nS == 0) return;

    {
        float peak = 0.0f;
        for (int ch = 0; ch < nCh; ++ch)
            peak = juce::jmax (peak, buffer.getMagnitude (ch, 0, nS));
        float cur = inputPeak.load (std::memory_order_relaxed);
        while (peak > cur && ! inputPeak.compare_exchange_weak (cur, peak, std::memory_order_release, std::memory_order_relaxed)) {}
    }

    updateModeVoicing();

    // ── doubling source: mono sum (a second take tracks the performance) ──
    {
        auto* m = monoSrc.getWritePointer (0);
        for (int n = 0; n < nS; ++n)
        {
            float s = 0.0f;
            for (int ch = 0; ch < nCh; ++ch) s += buffer.getReadPointer (ch)[n];
            m[n] = s / (float) nCh;
        }
    }

    // ── voice setup for this block ─────────────────────────────────────────
    const float detCents = 4.0f + 10.0f * thick01;          // ±4..14 cents
    const float pair2In  = juce::jlimit (0.0f, 1.0f, (thick01 - 0.5f) * 2.5f); // voices 3/4 fade in past half
    const float baseMs   = (mode == Mode::Vox ? 18.0f : mode == Mode::Strings ? 24.0f : 14.0f);
    const float msToSamp = 0.001f * (float) spec.sampleRate;

    // Voice count fades continuously rather than flipping 2→4 at thick01 0.5 —
    // the old integer count made busNorm jump 0.707→0.5 at a block boundary,
    // which is an audible step (and a click when THICK is automated).
    const float effVoices = 2.0f + 2.0f * pair2In;
    busNormSm.setTargetValue (1.0f / std::sqrt (effVoices));

    // Base (drift-free) targets. The wander itself is applied per-sample
    // below — DriftWalk's one-pole coefficient is computed from the sample
    // rate, so ticking it once per block divided its effective corner by the
    // block size and froze it into a static per-voice offset.
    for (int v = 0; v < kVoices; ++v)
    {
        ratioSm[v].setTargetValue (std::pow (2.0f, (detCents * kVoiceDetuneMul[v]) / 1200.0f));
        gainSm [v].setTargetValue (v < 2 ? 1.0f : pair2In);
    }

    // ── render voices into the wet bus ─────────────────────────────────────
    auto* wl = wetBus.getWritePointer (0);
    auto* wr = wetBus.getWritePointer (1);
    const auto* src = monoSrc.getReadPointer (0);

    // 2^(c/1200) ≈ 1 + c·ln2/1200 — exact to well under 0.01% over the few
    // cents the drift covers, and saves a pow() per voice per sample.
    constexpr float kCentsToRatio = 0.0005776227f;

    for (int n = 0; n < nS; ++n)
    {
        const float bn = busNormSm.getNextValue();
        float accL = 0.0f, accR = 0.0f;

        for (int v = 0; v < kVoices; ++v)
        {
            // Tick every walk before the early-out so all three stay
            // free-running at the same rate whatever the voice is doing.
            const float driftCents = juce::jlimit (-12.0f, 12.0f,
                                        pitchDrift[v].next() * 240.0f * human01);
            const float lvlWob   = juce::jlimit (0.7f, 1.3f,
                                        1.0f + levelDrift[v].next() * 1.8f * human01);
            const float wanderMs = timeDrift[v].next() * 10.0f * human01 * 8.0f;

            const float g     = gainSm[v].getNextValue() * lvlWob;
            const float ratio = ratioSm[v].getNextValue() * (1.0f + driftCents * kCentsToRatio);

            if (g < 0.001f) { shifter[v].process (src[n], ratio); continue; } // keep state warm

            // pitch
            float tap = shifter[v].process (src[n], ratio);

            // timing: base offset + slow wander (±8 ms at full HUMAN)
            const float dSamp = juce::jmax (1.0f, (baseMs + kVoiceOffsetMs[v] + wanderMs) * msToSamp);
            voiceDelay[v].pushSample (0, tap);
            tap = voiceDelay[v].popSample (0, dSamp, true);

            tap *= g;

            // constant-power-ish pan, folded toward centre by (1-WIDE)
            const float pan = kVoicePan[v] * wide01; // -1..1
            accL += tap * std::sqrt (0.5f * (1.0f - pan));
            accR += tap * std::sqrt (0.5f * (1.0f + pan));
        }

        wl[n] = accL * bn;
        wr[n] = accR * bn;
    }

    // ── wet-bus voicing (mode EQ + always-on HPF) ──────────────────────────
    for (int ch = 0; ch < 2; ++ch)
    {
        auto* w = wetBus.getWritePointer (ch);
        for (int n = 0; n < nS; ++n)
        {
            float x = w[n];
            x = wetHP [ch].processSample (x);
            x = wetDip[ch].processSample (x);
            x = wetLP [ch].processSample (x);
            w[n] = x;
        }
    }

    // ── additive mix: dry untouched, doubles layered on top ───────────────
    const float wetGain = mix01 * 0.95f;
    for (int ch = 0; ch < nCh; ++ch)
    {
        auto* d = buffer.getWritePointer (ch);
        const auto* w = wetBus.getReadPointer (nCh == 1 ? 0 : ch);
        const auto* wOther = wetBus.getReadPointer (nCh == 1 ? 1 : ch);
        for (int n = 0; n < nS; ++n)
        {
            const float wet = (nCh == 1) ? 0.5f * (w[n] + wOther[n]) : w[n];
            // No output clipper here: this is an additive doubler and the dry
            // path has to stay pristine. A cubic clip on the sum imposed a
            // hard ±1.0 ceiling and added 3rd-harmonic distortion (plus
            // aliasing — it ran at base rate) to the dry signal on hot input.
            d[n] = d[n] + wet * wetGain;
        }
    }

    {
        float peak = 0.0f;
        for (int ch = 0; ch < nCh; ++ch)
            peak = juce::jmax (peak, buffer.getMagnitude (ch, 0, nS));
        float cur = outputPeak.load (std::memory_order_relaxed);
        while (peak > cur && ! outputPeak.compare_exchange_weak (cur, peak, std::memory_order_release, std::memory_order_relaxed)) {}
    }
}

} // namespace dbl
