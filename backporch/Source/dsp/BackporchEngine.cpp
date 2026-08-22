#include "BackporchEngine.h"

namespace bkpr
{

void BackporchEngine::prepare (const juce::dsp::ProcessSpec& s)
{
    spec = s;
    juce::dsp::ProcessSpec mono { s.sampleRate, s.maximumBlockSize, 1 };

    const auto hp = juce::dsp::IIR::Coefficients<float>::makeHighPass (s.sampleRate, 150.0f);
    for (auto& f : sendHP) { f.coefficients = hp; f.reset(); }

    preMaxSamp = (int) std::ceil (0.080 * s.sampleRate);
    for (auto& d : preDelay)
    {
        d.reset();
        d.prepare (mono);
        d.setMaximumDelayInSamples (preMaxSamp + 8);
    }

    verb.prepare (s.sampleRate, s.maximumBlockSize);

    slapMaxSamp = (int) std::ceil (0.180 * s.sampleRate);
    for (auto& d : slapLine)
    {
        d.reset();
        d.prepare (mono);
        d.setMaximumDelayInSamples (slapMaxSamp + 8);
    }
    const auto lp = juce::dsp::IIR::Coefficients<float>::makeLowPass (s.sampleRate, 4200.0f, 0.6f);
    for (auto& f : slapLP) { f.coefficients = lp; f.reset(); }
    slapTimeSm.reset (s.sampleRate, 0.12); // tape-style swoop if SPACE moves in slap mode
    slapTimeSm.setCurrentAndTargetValue (0.100f * (float) s.sampleRate);

    for (auto& t : tailTilt) { t.prepare (850.0f, s.sampleRate); t.reset(); }

    duckEnv.prepare (5.0f, 400.0f, s.sampleRate);

    drySnap.setSize (2, (int) s.maximumBlockSize, false, true, true);
    wet    .setSize (2, (int) s.maximumBlockSize, false, true, true);
    verbScratch.setSize (2, (int) s.maximumBlockSize, false, true, true);
}

void BackporchEngine::reset()
{
    for (auto& f : sendHP)   f.reset();
    for (auto& d : preDelay) d.reset();
    verb.reset();
    for (auto& d : slapLine) d.reset();
    for (auto& f : slapLP)   f.reset();
    for (auto& t : tailTilt) t.reset();
    duckEnv.reset();
}

void BackporchEngine::process (juce::AudioBuffer<float>& buffer) noexcept
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

    for (int ch = 0; ch < nCh; ++ch)
        drySnap.copyFrom (ch, 0, buffer, ch, 0, nS);

    // ── per-mode voicing ────────────────────────────────────────────────────
    float preMs, slapGain = 0.0f;
    switch (mode)
    {
        case Mode::Slap:
            verb.setVoicing (spt::FableVerb::Voicing::Room);
            verb.setSize01  (0.25f);
            verb.setDecay01 (juce::jmap (space01, 0.15f, 0.45f));
            verb.setDamp01  (0.55f);
            verb.setWidth01 (0.7f);
            preMs = 8.0f;
            slapGain = 1.0f;
            slapTimeSm.setTargetValue (juce::jmap (space01, 0.070f, 0.150f) * (float) spec.sampleRate);
            break;
        case Mode::Plate:
            verb.setVoicing (spt::FableVerb::Voicing::Plate);
            verb.setSize01  (space01);
            verb.setDecay01 (juce::jmap (space01, 0.25f, 0.80f));
            verb.setDamp01  (juce::jmap (space01, 0.45f, 0.30f));
            verb.setWidth01 (0.9f);
            preMs = juce::jmap (space01, 20.0f, 60.0f);
            break;
        case Mode::Room:
        default:
            verb.setVoicing (spt::FableVerb::Voicing::Room);
            verb.setSize01  (space01);
            verb.setDecay01 (juce::jmap (space01, 0.18f, 0.65f));
            verb.setDamp01  (0.50f);
            verb.setWidth01 (0.8f);
            preMs = juce::jmap (space01, 12.0f, 32.0f);
            break;
    }

    const float tiltDb = juce::jmap (tone01, -4.5f, 4.5f);
    for (auto& t : tailTilt) t.setTiltDb (tiltDb);

    // ── build the send: HPF → pre-delay (→ slap voice) ─────────────────────
    const float preSamp = juce::jlimit (1.0f, (float) preMaxSamp, preMs * 0.001f * (float) spec.sampleRate);
    for (int ch = 0; ch < nCh; ++ch)
    {
        const auto* in = drySnap.getReadPointer (ch);
        auto* w = wet.getWritePointer (ch);
        for (int n = 0; n < nS; ++n)
        {
            float v = sendHP[ch].processSample (in[n]);
            preDelay[ch].pushSample (0, v);
            v = preDelay[ch].popSample (0, preSamp, true);

            if (slapGain > 0.0f)
            {
                // One dark repeat (low feedback for a hint of a second).
                const float t = slapTimeSm.getNextValue();
                slapLine[ch].pushSample (0, v + slapLP[ch].processSample (slapLine[ch].popSample (0, t, false)) * 0.22f);
                const float echo = slapLine[ch].popSample (0, t, true);
                // The echo IS the voice; the small room just glues it.
                v = echo + v * 0.12f;
            }
            w[n] = v;
        }
        if (nCh == 1) wet.copyFrom (1, 0, wet, 0, 0, nS);
    }

    // ── reverb (fully wet) ──────────────────────────────────────────────────
    {
        auto* l = wet.getWritePointer (0);
        auto* r = wet.getWritePointer (1);
        if (mode == Mode::Slap)
        {
            // Slap: the room runs at low blend beside the echo.
            auto* vl = verbScratch.getWritePointer (0);
            auto* vr = verbScratch.getWritePointer (1);
            verb.processBlock (l, r, vl, vr, nS);
            const float roomAmt = juce::jmap (space01, 0.10f, 0.35f);
            for (int n = 0; n < nS; ++n)
            {
                l[n] = l[n] * 0.85f + vl[n] * roomAmt;
                r[n] = r[n] * 0.85f + vr[n] * roomAmt;
            }
        }
        else
        {
            verb.processBlock (l, r, l, r, nS);
        }
    }

    // ── tail tilt + duck + mix ──────────────────────────────────────────────
    // Soundtoys curve: wet rises to unity at 70% MIX, then dry comes down.
    float wetGain, dryGain;
    if (mix01 <= 0.70f) { wetGain = mix01 / 0.70f; dryGain = 1.0f; }
    else                { wetGain = 1.0f; dryGain = 1.0f - (mix01 - 0.70f) / 0.30f; }

    for (int n = 0; n < nS; ++n)
    {
        float mono = 0.0f;
        for (int ch = 0; ch < nCh; ++ch) mono += drySnap.getReadPointer (ch)[n];
        const float env  = duckEnv.process (mono / (float) nCh);
        const float duck = 1.0f - duck01 * juce::jmin (1.0f, env * 3.0f);

        for (int ch = 0; ch < nCh; ++ch)
        {
            const float tail = tailTilt[ch].process (wet.getReadPointer (ch)[n]) * duck;
            const float dry  = drySnap.getReadPointer (ch)[n];
            // No output clipper: the dry path stays pristine. A base-rate
            // cubic clip on the sum added 3rd-harmonic distortion and
            // aliasing to the dry signal whenever the source ran hot.
            buffer.getWritePointer (ch)[n] = dry * dryGain + tail * wetGain;
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

} // namespace bkpr
