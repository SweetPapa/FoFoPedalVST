#include "Mod.h"

namespace fofopedal
{

using fofo::Lfo;
using fofo::Drift;

namespace
{
    constexpr float kVoiceBaseMs[3]  = { 7.0f, 10.5f, 14.0f };
    constexpr float kVoiceRateMul[3] = { 1.00f, 1.21f, 1.43f };
    constexpr float kVoicePhase[3]   = { 0.0f, 1.0f / 3.0f, 2.0f / 3.0f };
    constexpr float kVoicePan[3]     = { -1.0f, 0.0f, +1.0f };

    inline float mapRate (float r01, float lo, float hi) noexcept
    {
        return lo * std::pow (hi / lo, r01);
    }
}

void Mod::prepare (const juce::dsp::ProcessSpec& s)
{
    spec = { s.sampleRate, (int) s.maximumBlockSize, (int) juce::jmax (1u, s.numChannels) };

    mod = fofo::ModMatrix {};

    const float maxSwing = 0.006f * (float) spec.sampleRate;
    for (int v = 0; v < kVoices; ++v)
    {
        chorusLine[v].prepare (spec.sampleRate, 0.035f * (float) spec.sampleRate);
        chorusDark[v].prepare (spec.sampleRate);

        sVoice[v] = mod.addSource (std::make_unique<Lfo> (Lfo::Shape::Sine, 0.5f * kVoiceRateMul[v], 0xC01u + (uint32_t) v * 7919u));
        sDrift[v] = mod.addSource (std::make_unique<Drift> (0.19f, 0xD02u + (uint32_t) v * 104729u));
        static_cast<Lfo*> (mod.source (sVoice[v]))->setStartPhase (kVoicePhase[v]);
        static_cast<Lfo*> (mod.source (sVoice[v]))->setRateDrift (0.22f);

        dVoice[v] = mod.addDest ("voice", 0.0f, -maxSwing, maxSwing);
        rVoice[v] = mod.connect (sVoice[v], dVoice[v], 0.0f);
        rDrift[v] = mod.connect (sDrift[v], dVoice[v], 0.0f);
    }

    for (int ch = 0; ch < 2; ++ch)
    {
        for (int st = 0; st < kStages; ++st) phaserAp[ch][st].prepare (spec.sampleRate);
        vibLine[ch].prepare (spec.sampleRate, 0.030f * (float) spec.sampleRate);
    }

    sPhaser = mod.addSource (std::make_unique<Lfo> (Lfo::Shape::Sine, 0.4f, 0x9A5u));
    static_cast<Lfo*> (mod.source (sPhaser))->setRateDrift (0.15f);
    dPhaser = mod.addDest ("phaserHz", 800.0f, 120.0f, 6000.0f);
    rPhaser = mod.connect (sPhaser, dPhaser, 0.0f);

    sTrem = mod.addSource (std::make_unique<Lfo> (Lfo::Shape::Sine, 4.0f, 0x7B6u));
    static_cast<Lfo*> (mod.source (sTrem))->setRateDrift (0.08f);
    dTrem = mod.addDest ("trem", 1.0f, 0.0f, 1.0f);
    rTrem = mod.connect (sTrem, dTrem, 0.0f);

    mod.prepare (spec);

    drySnap.setSize (2, spec.maxBlockSize, false, true, true);
    reset();
}

void Mod::reset()
{
    for (auto& l : chorusLine) l.reset();
    for (auto& f : chorusDark) f.reset();
    for (int ch = 0; ch < 2; ++ch)
    {
        for (auto& f : phaserAp[ch]) f.reset();
        phaserFb[ch] = 0.0f;
        vibLine[ch].reset();
    }
    mod.reset();
    drySnap.clear();
}

void Mod::applyParams()
{
    const float msToSamp = 0.001f * (float) spec.sampleRate;

    switch (algo)
    {
        case Algo::Chorus:
        {
            const float hz = mapRate (rate01, 0.08f, 2.5f);
            const float swingMs = juce::jmap (depth01, 0.1f, 3.2f);
            for (int v = 0; v < kVoices; ++v)
            {
                static_cast<Lfo*> (mod.source (sVoice[v]))->setRateHz (hz * kVoiceRateMul[v]);
                mod.setRouteDepth (rVoice[v], swingMs * msToSamp);
                mod.setRouteDepth (rDrift[v], swingMs * msToSamp * 0.30f);
                // SHAPE darkens the voices, which is what keeps a chorus from
                // reading as a phaser: the copies sit behind the dry.
                chorusDark[v].set (fofo::Svf::Type::Lowpass, juce::jmap (shape01, 9000.0f, 3000.0f), 0.7f);
            }
            break;
        }

        case Algo::Phaser:
        {
            static_cast<Lfo*> (mod.source (sPhaser))->setRateHz (mapRate (rate01, 0.05f, 3.0f));
            const float centre = juce::jmap (shape01, 350.0f, 1600.0f);
            mod.setBase (dPhaser, centre);
            mod.setRouteDepth (rPhaser, depth01 * centre * 0.85f);
            break;
        }

        case Algo::TremVib:
        case Algo::NumAlgos:
        default:
        {
            static_cast<Lfo*> (mod.source (sTrem))->setRateHz (mapRate (rate01, 0.3f, 12.0f));
            // SHAPE crossfades tremolo (amplitude) into vibrato (pitch).
            const float tremDepth = depth01 * (1.0f - shape01) * 0.85f;
            mod.setBase (dTrem, 1.0f - tremDepth * 0.5f);
            mod.setRouteDepth (rTrem, tremDepth * 0.5f);
            break;
        }
    }
}

void Mod::process (juce::AudioBuffer<float>& buffer) noexcept
{
    if (bypassed || spec.sampleRate <= 0.0) return;
    const int nCh = juce::jmin (buffer.getNumChannels(), 2);
    const int nS  = buffer.getNumSamples();
    if (nCh == 0 || nS == 0) return;

    applyParams();

    for (int ch = 0; ch < nCh; ++ch)
        drySnap.copyFrom (ch, 0, buffer, ch, 0, nS);

    const float dry = 1.0f - mix01;
    const float msToSamp = 0.001f * (float) spec.sampleRate;

    for (int n = 0; n < nS; ++n)
    {
        mod.tick (0.0f);

        float wl = 0.0f, wr = 0.0f;

        if (algo == Algo::Chorus)
        {
            float mono = 0.0f;
            for (int ch = 0; ch < nCh; ++ch) mono += drySnap.getReadPointer (ch)[n];
            mono /= (float) nCh;

            for (int v = 0; v < kVoices; ++v)
            {
                const float d = juce::jmax (2.0f, kVoiceBaseMs[v] * msToSamp + mod.get (dVoice[v]));
                const float tap = chorusDark[v].process (chorusLine[v].processSample (mono, d));
                const float pan = kVoicePan[v];
                wl += tap * std::sqrt (0.5f * (1.0f - pan));
                wr += tap * std::sqrt (0.5f * (1.0f + pan));
            }
            wl *= 0.5773503f;
            wr *= 0.5773503f;
        }
        else if (algo == Algo::Phaser)
        {
            const float fc = mod.get (dPhaser);
            // Resonance rises with FEEDBACK — this is the part the old
            // first-order stages could not do at all.
            const float q = 0.5f + 2.5f * feedback01;

            for (int ch = 0; ch < nCh; ++ch)
            {
                float x = drySnap.getReadPointer (ch)[n] + phaserFb[ch] * feedback01 * 0.7f;
                for (int st = 0; st < kStages; ++st)
                {
                    // Stagger the stages so the notches spread rather than
                    // stacking on one frequency.
                    phaserAp[ch][st].set (fofo::Svf::Type::Allpass,
                                          fc * (0.72f + 0.18f * (float) st), q);
                    x = phaserAp[ch][st].process (x);
                }
                phaserFb[ch] = x;
                // A phaser is the sum of dry and the all-passed copy: the
                // notches come from the cancellation, not from the filter.
                (ch == 0 ? wl : wr) = 0.5f * (drySnap.getReadPointer (ch)[n] + x);
            }
        }
        else // TremVib
        {
            const float g = mod.get (dTrem);
            // SHAPE past halfway swaps amplitude modulation for pitch.
            const float vibDepth = depth01 * shape01 * 2.0f;
            for (int ch = 0; ch < nCh; ++ch)
            {
                float x = drySnap.getReadPointer (ch)[n];
                if (vibDepth > 0.0f)
                {
                    const float d = juce::jmax (2.0f, (6.0f + vibDepth * (2.0f * g - 1.0f) * 3.0f) * msToSamp);
                    x = vibLine[ch].processSample (x, d);
                }
                (ch == 0 ? wl : wr) = x * g;
            }
        }

        buffer.getWritePointer (0)[n] = drySnap.getReadPointer (0)[n] * dry + wl * mix01;
        if (nCh > 1)
            buffer.getWritePointer (1)[n] = drySnap.getReadPointer (1)[n] * dry + wr * mix01;
    }
}

} // namespace fofopedal
