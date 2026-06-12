#include "Pitch.h"

namespace fofopedal
{

namespace
{
    constexpr float kTwoPi = 6.28318530717958647692f;

    inline int nextPow2 (int n)
    {
        int p = 1; while (p < n) p <<= 1; return p;
    }

    // Map shape01 to a musically useful interval-ratio for OctaveHarm.
    // 0       → -1 oct  (0.5)
    // 0..0.4  → minor/major 3rd-down (0.84, 0.79)
    // 0.4..0.6→ unison-ish (1.0)
    // 0.6..0.8→ +3rd / +5th (1.26, 1.5)
    // 0.8..1  → +1 oct (2.0)
    inline float harmonyRatio (float s)
    {
        // Quantize into 5 buckets for musical predictability.
        if (s < 0.20f) return 0.5f;
        if (s < 0.40f) return 0.7937f; // -major 3rd
        if (s < 0.60f) return 1.0f;
        if (s < 0.80f) return 1.2599f; // +major 3rd
        return 2.0f;
    }
}

void Pitch::prepare (const juce::dsp::ProcessSpec& s)
{
    spec = s;

    // Grain size ~50 ms is the sweet spot — short enough for transient
    // articulation, long enough that pitch artefacts stay sub-perceptible.
    grainSize = juce::jmax (256, (int) std::round (0.050 * s.sampleRate));
    grainBufSize = nextPow2 (grainSize * 4);
    grainBuffer.setSize ((int) s.numChannels, grainBufSize, false, true, true);
    grainBuffer.clear();

    grains.clear();
    grains.resize ((size_t) s.numChannels);
    grainWriteHead.assign ((size_t) s.numChannels, 0);
    for (auto& gp : grains)
    {
        gp[0].readPos = 0.0f; gp[0].phase = 0.0f;
        gp[1].readPos = 0.0f; gp[1].phase = 0.5f; // 180° out of phase
    }

    // Freeze ring — 2 seconds of capture.
    freezeRingSize = juce::jmax (4096, (int) std::ceil (2.0 * s.sampleRate));
    freezeRing.setSize ((int) s.numChannels, freezeRingSize, false, true, true);
    freezeRing.clear();
    freezeWriteHead.assign ((size_t) s.numChannels, 0);

    loopXfade = juce::jmax (256, (int) std::round (0.040 * s.sampleRate)); // 40 ms crossfade
    loopStart = 0;
    loopEnd   = juce::jmax (loopXfade * 2, (int) std::round (1.5 * s.sampleRate));
    freezeReadPos = 0.0f;
    frozen = false;
    frozenFadeIn = 0.0f;

    const auto coefFromMs = [&] (float ms) {
        return std::exp (-1.0f / (0.001f * ms * (float) s.sampleRate));
    };
    envAtk = coefFromMs (15.0f);
    envRel = coefFromMs (220.0f);
    envFollow = 0.0f;
    allpassState = 0.0f;
    allpassLfoPhase = 0.0f;

    dryBuffer.setSize ((int) s.numChannels, (int) s.maximumBlockSize, false, true, true);
}

void Pitch::reset()
{
    grainBuffer.clear();
    for (auto& gp : grains)
    {
        gp[0].readPos = 0.0f; gp[0].phase = 0.0f;
        gp[1].readPos = 0.0f; gp[1].phase = 0.5f;
    }
    std::fill (grainWriteHead.begin(), grainWriteHead.end(), 0);
    freezeRing.clear();
    std::fill (freezeWriteHead.begin(), freezeWriteHead.end(), 0);
    envFollow = 0.0f;
    frozen = false;
    frozenFadeIn = 0.0f;
    freezeReadPos = 0.0f;
    allpassState = 0.0f;
    allpassLfoPhase = 0.0f;
}

void Pitch::process (juce::AudioBuffer<float>& buffer) noexcept
{
    if (bypassed || spec.sampleRate <= 0.0) return;

    const int nCh = buffer.getNumChannels();
    const int nS  = buffer.getNumSamples();

    // Snapshot dry — every algo here mixes wet against dry at the boundary.
    for (int ch = 0; ch < nCh; ++ch)
        dryBuffer.copyFrom (ch, 0, buffer, ch, 0, nS);

    if (algo == Algo::MicroDetune)
    {
        // Amount maps 0..1 → 0..25 cents. L channel shifts up, R shifts down,
        // mono input picks the spread internally for an instant "double".
        const float cents = juce::jmap (amount01, 0.0f, 1.0f, 0.0f, 25.0f);
        const float ratioUp   = std::pow (2.0f, +cents / 1200.0f);
        const float ratioDown = std::pow (2.0f, -cents / 1200.0f);
        processGrainShifter (buffer, ratioUp, ratioDown);
    }
    else if (algo == Algo::OctaveHarm)
    {
        const float ratio = harmonyRatio (shape01);
        processGrainShifter (buffer, ratio, ratio);
    }
    else // Freeze
    {
        processFreeze (buffer);
    }

    // Wet/dry mix.
    const float wet = mix01;
    const float dry = 1.0f - mix01;
    for (int ch = 0; ch < nCh; ++ch)
    {
        auto* w = buffer.getWritePointer (ch);
        auto* d = dryBuffer.getReadPointer (ch);
        for (int n = 0; n < nS; ++n)
            w[n] = wet * w[n] + dry * d[n];
    }
}

void Pitch::processGrainShifter (juce::AudioBuffer<float>& buffer, float ratioL, float ratioR) noexcept
{
    const int nCh = buffer.getNumChannels();
    const int nS  = buffer.getNumSamples();
    const float twoPi = kTwoPi;
    const float phaseInc = 1.0f / (float) grainSize;

    for (int ch = 0; ch < nCh; ++ch)
    {
        const float ratio = (ch == 0) ? ratioL : ratioR;
        auto* in  = buffer.getWritePointer (ch);
        auto* cb  = grainBuffer.getWritePointer (ch);
        auto& gp  = grains[(size_t) ch];
        int& wh   = grainWriteHead[(size_t) ch];

        for (int n = 0; n < nS; ++n)
        {
            // Write current input into circular buffer.
            cb[wh] = in[n];

            float out = 0.0f;
            for (auto& g : gp)
            {
                // Hann window: 0.5 - 0.5*cos(2π * phase)
                const float w = 0.5f - 0.5f * std::cos (twoPi * g.phase);

                int rInt = (int) g.readPos;
                rInt = ((rInt % grainBufSize) + grainBufSize) % grainBufSize;
                const float rFrac = g.readPos - std::floor (g.readPos);
                const int rNext = (rInt + 1) % grainBufSize;
                const float sample = cb[rInt] * (1.0f - rFrac) + cb[rNext] * rFrac;

                out += sample * w;

                g.readPos += ratio;
                if (g.readPos >= (float) grainBufSize) g.readPos -= (float) grainBufSize;
                if (g.readPos < 0.0f)                  g.readPos += (float) grainBufSize;

                g.phase += phaseInc;
                if (g.phase >= 1.0f)
                {
                    g.phase -= 1.0f;
                    // Restart grain at writeHead - grainSize so the grain has
                    // a full window's worth of recent material to traverse.
                    int restart = wh - grainSize;
                    while (restart < 0) restart += grainBufSize;
                    g.readPos = (float) (restart % grainBufSize);
                }
            }

            in[n] = out;

            wh = (wh + 1) % grainBufSize;
        }
    }
}

void Pitch::processFreeze (juce::AudioBuffer<float>& buffer) noexcept
{
    const int nCh = buffer.getNumChannels();
    const int nS  = buffer.getNumSamples();

    // amount01 — input threshold (linear), 0..1 → -50..-10 dB.
    const float threshDb = juce::jmap (amount01, 0.0f, 1.0f, -50.0f, -10.0f);
    const float threshLin = juce::Decibels::decibelsToGain (threshDb);

    // Allpass modulation rate stays slow regardless of shape; depth tracks shape01.
    const float allpassRate = 0.35f;
    const float allpassPhaseInc = allpassRate / (float) spec.sampleRate;
    const float allpassDepth = juce::jmap (shape01, 0.0f, 1.0f, 0.0f, 0.6f);

    for (int n = 0; n < nS; ++n)
    {
        // Mono envelope from the channel sum.
        float abss = 0.0f;
        for (int ch = 0; ch < nCh; ++ch) abss += std::abs (buffer.getReadPointer (ch)[n]);
        if (nCh > 0) abss /= (float) nCh;

        const float coef = (abss > envFollow) ? envAtk : envRel;
        envFollow = coef * envFollow + (1.0f - coef) * abss;

        const bool wantFrozen = envFollow < threshLin;
        if (wantFrozen && ! frozen)
        {
            // Just entered frozen state — capture the most recent 1.5s.
            frozen = true;
            frozenFadeIn = 0.0f;
            const int look = juce::jmin (freezeRingSize, (int) std::round (1.5 * spec.sampleRate));
            loopXfade = juce::jmax (256, (int) std::round (0.040 * spec.sampleRate));
            // Use channel 0's write head as the snapshot anchor.
            const int wh0 = freezeWriteHead[0];
            loopEnd   = wh0;
            loopStart = ((wh0 - look) % freezeRingSize + freezeRingSize) % freezeRingSize;
            freezeReadPos = (float) loopStart;
        }
        else if (! wantFrozen && frozen)
        {
            frozen = false;
            frozenFadeIn = 0.0f;
        }

        // Fade between dry and frozen so transitions don't click.
        const float targetFade = frozen ? 1.0f : 0.0f;
        const float fadeCoef = 0.9990f; // ~30ms at 44.1k
        frozenFadeIn = fadeCoef * frozenFadeIn + (1.0f - fadeCoef) * targetFade;

        // Capture path: always write input into the ring.
        for (int ch = 0; ch < nCh; ++ch)
        {
            auto* ring = freezeRing.getWritePointer (ch);
            const int wh = freezeWriteHead[(size_t) ch];
            ring[wh] = buffer.getReadPointer (ch)[n];
            freezeWriteHead[(size_t) ch] = (wh + 1) % freezeRingSize;
        }

        // Read path: looped capture with crossfade at loop boundaries.
        float frozenSamp[2] = { 0.0f, 0.0f };
        if (frozenFadeIn > 1.0e-4f)
        {
            // Determine crossfade weights when read is near loopEnd.
            const float distFromEnd = (float) ((loopEnd - (int) freezeReadPos + freezeRingSize) % freezeRingSize);
            float xfA = 1.0f, xfB = 0.0f;
            float readPosB = freezeReadPos;
            if (distFromEnd < (float) loopXfade)
            {
                xfB = 1.0f - (distFromEnd / (float) loopXfade);
                xfA = 1.0f - xfB;
                readPosB = (float) loopStart + ((float) loopXfade - distFromEnd);
            }

            for (int ch = 0; ch < juce::jmin (nCh, freezeRing.getNumChannels()); ++ch)
            {
                auto* ring = freezeRing.getReadPointer (ch);
                const int aInt = ((int) freezeReadPos) % freezeRingSize;
                const int bInt = ((int) readPosB) % freezeRingSize;
                frozenSamp[ch] = xfA * ring[aInt] + xfB * ring[bInt];
            }

            // Slow all-pass for "alive" feel.
            const float lfoSin = std::sin (kTwoPi * allpassLfoPhase);
            const float a = allpassDepth * lfoSin * 0.5f; // -0.3..0.3
            for (int ch = 0; ch < juce::jmin (nCh, 2); ++ch)
            {
                const float x  = frozenSamp[ch];
                const float yn = -a * x + allpassState;
                allpassState   =  x + a * yn;
                frozenSamp[ch] = yn;
            }
            allpassLfoPhase += allpassPhaseInc;
            if (allpassLfoPhase >= 1.0f) allpassLfoPhase -= 1.0f;

            freezeReadPos += 1.0f;
            if ((int) freezeReadPos >= loopEnd + loopXfade)
            {
                freezeReadPos -= (float) (loopEnd - loopStart);
            }
            // Wrap into ring.
            while (freezeReadPos >= (float) freezeRingSize) freezeReadPos -= (float) freezeRingSize;
        }

        const float fade = frozenFadeIn;
        for (int ch = 0; ch < nCh; ++ch)
        {
            const float dryS = buffer.getReadPointer (ch)[n];
            const float frS  = (ch < 2) ? frozenSamp[ch] : frozenSamp[0];
            buffer.getWritePointer (ch)[n] = (1.0f - fade) * dryS + fade * frS;
        }
    }
}

} // namespace fofopedal
