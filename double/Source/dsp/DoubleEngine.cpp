#include "DoubleEngine.h"

namespace dbl
{

namespace
{
    // Base offsets, staggered like separate takes rather than like delay taps.
    constexpr float kVoiceOffsetMs[DoubleEngine::kVoices] = { 0.0f, 7.0f, 3.5f, 11.0f };
    // Detune polarity and scale: pair 1 at ±1×, pair 2 at ±2.1×.
    constexpr float kVoiceDetuneMul[DoubleEngine::kVoices] = { -1.0f, +1.0f, -2.1f, +2.1f };
    // Pan at full WIDE.
    constexpr float kVoicePan[DoubleEngine::kVoices] = { -1.0f, +1.0f, +0.6f, -0.6f };
}

void DoubleEngine::prepare (const juce::dsp::ProcessSpec& s)
{
    spec = { s.sampleRate, (int) s.maximumBlockSize, (int) juce::jmax (1u, s.numChannels) };

    human = fofo::ModMatrix {};

    for (int v = 0; v < kVoices; ++v)
    {
        // 20 ms sweep range: the taps sit about 10 ms apart, which is where
        // two real takes sit, and at the few cents of detune used here they
        // hand over only about once every 2.5 seconds.
        shifter[v].prepare (spec.sampleRate, 20.0f);
        voiceDelay[v].prepare (spec.sampleRate, 0.080f * (float) spec.sampleRate);

        // Three independent walks per voice. Different rates as well as
        // different seeds: a player's pitch, timing and level do not drift on
        // the same clock.
        auto mk = [&] (float hz, uint32_t seed)
        {
            auto d = std::make_unique<fofo::Drift> (hz, seed);
            return human.addSource (std::move (d));
        };
        sPitch[v] = mk (0.30f, 0xA11CE5u + (uint32_t) v * 104729u);
        sTime [v] = mk (0.20f, 0xB22DF6u + (uint32_t) v * 104729u);
        sLevel[v] = mk (0.15f, 0xC33EA7u + (uint32_t) v * 104729u);

        dPitch[v] = human.addDest ("pitchCents", 0.0f, -30.0f, 30.0f);
        dTime [v] = human.addDest ("timeMs",     0.0f, -12.0f, 12.0f);
        dLevel[v] = human.addDest ("levelMul",   1.0f,   0.6f,  1.4f);

        rPitch[v] = human.connect (sPitch[v], dPitch[v], 0.0f);
        rTime [v] = human.connect (sTime [v], dTime [v], 0.0f);
        rLevel[v] = human.connect (sLevel[v], dLevel[v], 0.0f);

        ratioSm[v].reset (spec.sampleRate, 0.05);
        ratioSm[v].setCurrentAndTargetValue (1.0f);
        gainSm[v].reset (spec.sampleRate, 0.05);
        gainSm[v].setCurrentAndTargetValue (0.0f);
    }

    human.prepare (spec);

    busNormSm.reset (spec.sampleRate, 0.05);
    busNormSm.setCurrentAndTargetValue (1.0f / std::sqrt (2.0f));

    for (int ch = 0; ch < 2; ++ch)
    {
        wetHp [ch].prepare (spec.sampleRate);
        wetDip[ch].prepare (spec.sampleRate);
        wetLp [ch].prepare (spec.sampleRate);
    }

    wetBus .setSize (2, spec.maxBlockSize, false, true, true);
    monoSrc.setSize (1, spec.maxBlockSize, false, true, true);

    modeDirty = true;
    applyParams();
    reset();
}

void DoubleEngine::reset()
{
    for (int v = 0; v < kVoices; ++v)
    {
        shifter[v].reset();
        voiceDelay[v].reset();
    }
    human.reset();
    for (int ch = 0; ch < 2; ++ch)
    {
        wetHp [ch].reset();
        wetDip[ch].reset();
        wetLp [ch].reset();
    }
    wetBus.clear();
    monoSrc.clear();
}

void DoubleEngine::applyParams()
{
    if (modeDirty)
    {
        float hpHz, lpHz, dipHz, dipDb;
        switch (mode)
        {
            // Keep the doubles behind the pick attack.
            case Mode::Strings: hpHz = 130.0f; lpHz = 11000.0f; dipHz = 2800.0f; dipDb = -1.5f; break;
            case Mode::Synth:   hpHz = 110.0f; lpHz = 14000.0f; dipHz = 1000.0f; dipDb =  0.0f; break;
            // Dip the presence so the doubles sit behind the lead vocal.
            case Mode::Vox:
            default:            hpHz = 160.0f; lpHz = 12000.0f; dipHz = 3200.0f; dipDb = -2.0f; break;
        }
        for (int ch = 0; ch < 2; ++ch)
        {
            wetHp [ch].set (fofo::Svf::Type::Highpass, hpHz, 0.7f);
            wetLp [ch].set (fofo::Svf::Type::Lowpass,  lpHz, 0.6f);
            wetDip[ch].set (fofo::Svf::Type::Bell,     dipHz, 0.9f, dipDb);
        }
        modeDirty = false;
    }

    // HUMAN drives all three kinds of wander at once. These depths are what
    // separates a doubler from a chorus, so they are set here in the units
    // they are heard in: cents, milliseconds, and a level multiplier.
    for (int v = 0; v < kVoices; ++v)
    {
        human.setRouteDepth (rPitch[v], human01 * 6.0f);    // ±6 cents
        human.setRouteDepth (rTime [v], human01 * 8.0f);    // ±8 ms
        human.setRouteDepth (rLevel[v], human01 * 0.16f);   // ±1.3 dB
    }
}

void DoubleEngine::process (juce::AudioBuffer<float>& buffer) noexcept
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

    applyParams();

    // A second take tracks the performance, so the doubling source is a mono
    // sum rather than the stereo image.
    {
        auto* m = monoSrc.getWritePointer (0);
        for (int n = 0; n < nS; ++n)
        {
            float acc = 0.0f;
            for (int ch = 0; ch < nCh; ++ch) acc += buffer.getReadPointer (ch)[n];
            m[n] = acc / (float) nCh;
        }
    }

    const float detCents = 4.0f + 10.0f * thick01;                                 // ±4..14 cents
    const float pair2In  = juce::jlimit (0.0f, 1.0f, (thick01 - 0.5f) * 2.5f);     // 3 and 4 fade in past half
    const float baseMs   = (mode == Mode::Vox ? 18.0f : mode == Mode::Strings ? 24.0f : 14.0f);
    const float msToSamp = 0.001f * (float) spec.sampleRate;

    // Voice count is continuous, so the bus normalisation never steps.
    busNormSm.setTargetValue (1.0f / std::sqrt (2.0f + 2.0f * pair2In));

    for (int v = 0; v < kVoices; ++v)
    {
        ratioSm[v].setTargetValue (fofo::centsToRatio (detCents * kVoiceDetuneMul[v]));
        gainSm [v].setTargetValue (v < 2 ? 1.0f : pair2In);
    }

    auto* wl = wetBus.getWritePointer (0);
    auto* wr = wetBus.getWritePointer (1);
    const auto* src = monoSrc.getReadPointer (0);

    for (int n = 0; n < nS; ++n)
    {
        human.tick (src[n]);
        const float bn = busNormSm.getNextValue();

        float accL = 0.0f, accR = 0.0f;
        for (int v = 0; v < kVoices; ++v)
        {
            const float g     = gainSm[v].getNextValue() * human.get (dLevel[v]);
            const float ratio = ratioSm[v].getNextValue() * fofo::centsToRatio (human.get (dPitch[v]));

            shifter[v].setRatio (ratio);
            const float pitched = shifter[v].process (src[n]);
            if (g < 0.001f) { voiceDelay[v].push (pitched); continue; }  // keep state warm

            const float dMs = baseMs + kVoiceOffsetMs[v] + human.get (dTime[v]);
            const float tap = voiceDelay[v].processSample (pitched, juce::jmax (2.0f, dMs * msToSamp)) * g;

            const float pan = kVoicePan[v] * wide01;
            accL += tap * std::sqrt (0.5f * (1.0f - pan));
            accR += tap * std::sqrt (0.5f * (1.0f + pan));
        }

        wl[n] = accL * bn;
        wr[n] = accR * bn;
    }

    // Wet-bus voicing. The high-pass is unconditional: doubles never add mud.
    for (int ch = 0; ch < 2; ++ch)
    {
        auto* w = wetBus.getWritePointer (ch);
        for (int n = 0; n < nS; ++n)
        {
            float x = wetHp [ch].process (w[n]);
            x       = wetDip[ch].process (x);
            w[n]    = wetLp [ch].process (x);
        }
    }

    // Additive: the dry is never attenuated to make room for the doubles, and
    // there is no clipper on the sum.
    const float wetGain = mix01 * 0.95f;
    for (int ch = 0; ch < nCh; ++ch)
    {
        auto* d = buffer.getWritePointer (ch);
        const auto* w      = wetBus.getReadPointer (nCh == 1 ? 0 : ch);
        const auto* wOther = wetBus.getReadPointer (nCh == 1 ? 1 : ch);
        for (int n = 0; n < nS; ++n)
        {
            const float wv = (nCh == 1) ? 0.5f * (w[n] + wOther[n]) : w[n];
            d[n] += wv * wetGain;
        }
    }

    {
        float peak = 0.0f;
        for (int ch = 0; ch < nCh; ++ch) peak = juce::jmax (peak, buffer.getMagnitude (ch, 0, nS));
        float cur = outputPeak.load (std::memory_order_relaxed);
        while (peak > cur && ! outputPeak.compare_exchange_weak (cur, peak, std::memory_order_release, std::memory_order_relaxed)) {}
    }
}

} // namespace dbl
