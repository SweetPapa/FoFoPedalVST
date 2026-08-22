#include "BackporchEngine.h"

namespace bkpr
{

void BackporchEngine::prepare (const juce::dsp::ProcessSpec& s)
{
    spec = { s.sampleRate, (int) s.maximumBlockSize, (int) juce::jmax (1u, s.numChannels) };

    for (auto& f : sendHp)
    {
        f.prepare (spec.sampleRate);
        f.set (fofo::Svf::Type::Highpass, 150.0f, 0.7f);
    }

    const float preMax = 0.120f * (float) spec.sampleRate;
    for (auto& d : preDelay) d.prepare (spec.sampleRate, preMax + 8.0f);

    early.prepare (spec);
    tank .prepare (spec);

    const float slapMax = 0.220f * (float) spec.sampleRate;
    for (auto& d : slapLine) d.prepare (spec.sampleRate, slapMax + 8.0f);
    for (auto& f : slapDark)
    {
        f.prepare (spec.sampleRate);
        f.set (fofo::Svf::Type::Lowpass, 4200.0f, 0.6f);
    }

    for (auto& f : tailTiltLow)  f.prepare (spec.sampleRate);
    for (auto& f : tailTiltHigh) f.prepare (spec.sampleRate);

    // The ducker keys off the mono-summed dry. Routing it through the matrix
    // rather than a private follower means it is stereo-linked by
    // construction — one detector, not one per channel.
    duckMod = fofo::ModMatrix {};
    sDuckEnv = duckMod.addSource (std::make_unique<fofo::EnvSource> (4.0f, 320.0f));
    dDuck    = duckMod.addDest ("duck", 1.0f, 0.0f, 1.0f);
    rDuck    = duckMod.connect (sDuckEnv, dDuck, 0.0f);
    duckMod.prepare (spec);

    drySnap.setSize (2, spec.maxBlockSize, false, true, true);
    wet    .setSize (2, spec.maxBlockSize, false, true, true);

    applyParams();
    reset();
}

void BackporchEngine::reset()
{
    for (auto& f : sendHp)   f.reset();
    for (auto& d : preDelay) d.reset();
    early.reset();
    tank .reset();
    for (auto& d : slapLine) d.reset();
    for (auto& f : slapDark) f.reset();
    for (auto& f : tailTiltLow)  f.reset();
    for (auto& f : tailTiltHigh) f.reset();
    duckMod.reset();
    drySnap.clear();
    wet.clear();
}

void BackporchEngine::applyParams()
{
    const float srf = (float) spec.sampleRate;

    switch (mode)
    {
        case Mode::Slap:
        {
            // The echo is the effect; the room only glues it. Barely any
            // early field, because a slap in a small live space IS the early
            // field.
            early.setSize01 (0.12f);
            early.setDampHz (6500.0f);
            early.setWidth01 (0.55f);
            earlyGain = 0.18f;

            tank.setSize01 (0.18f);
            tank.setDecaySeconds (juce::jmap (space01, 0.25f, 0.75f));
            tank.setLowDecayRatio (0.55f);
            tank.setHighDecayRatio (0.42f);
            tank.setCrossovers (280.0f, 3200.0f);
            tank.setModDepth (3.0f);
            tank.setWidth01 (0.65f);
            tankGain = juce::jmap (space01, 0.10f, 0.30f);

            preSamp  = juce::jmax (2.0f, 8.0f * 0.001f * srf);
            slapSamp = juce::jmap (space01, 0.070f, 0.155f) * srf;
            slapFeedback = 0.22f;
            slapGain = 1.0f;
            break;
        }

        case Mode::Plate:
        {
            // A plate has no discrete early reflections — the whole point of
            // the design is that the sheet goes dense immediately. Modelling
            // one with an early field is the classic way to make a plate sound
            // like a cheap room.
            earlyGain = 0.0f;

            tank.setSize01 (juce::jmap (space01, 0.30f, 0.72f));
            tank.setDecaySeconds (juce::jmap (space01, 0.6f, 4.5f));
            tank.setLowDecayRatio (0.62f);      // plates are thin down low
            tank.setHighDecayRatio (0.80f);     // and bright up top
            tank.setCrossovers (300.0f, 5000.0f);
            tank.setModDepth (9.0f);
            tank.setWidth01 (0.95f);
            tankGain = 1.0f;

            preSamp = juce::jmax (2.0f, juce::jmap (space01, 18.0f, 55.0f) * 0.001f * srf);
            slapGain = 0.0f;
            slapFeedback = 0.0f;
            break;
        }

        case Mode::Room:
        default:
        {
            // The early field carries the mode. It is what makes this read as
            // a place rather than a wash, and it is what v1 had none of.
            early.setSize01 (juce::jmap (space01, 0.18f, 0.85f));
            early.setDampHz (juce::jmap (space01, 9500.0f, 5200.0f));
            early.setWidth01 (0.85f);
            earlyGain = juce::jmap (space01, 0.75f, 0.50f);

            tank.setSize01 (juce::jmap (space01, 0.22f, 0.68f));
            tank.setDecaySeconds (juce::jmap (space01, 0.35f, 2.6f));
            // Lows die noticeably sooner than mids — this is the whole
            // "produced, not wet" identity, expressed as a number.
            tank.setLowDecayRatio (0.50f);
            tank.setHighDecayRatio (0.55f);
            tank.setCrossovers (260.0f, 3800.0f);
            tank.setModDepth (5.0f);
            tank.setWidth01 (0.82f);
            tankGain = juce::jmap (space01, 0.55f, 0.95f);

            preSamp = juce::jmax (2.0f, juce::jmap (space01, 10.0f, 30.0f) * 0.001f * srf);
            slapGain = 0.0f;
            slapFeedback = 0.0f;
            break;
        }
    }

    // TONE tilts the tail without changing its level: a shelf pair pivoting
    // at 850 Hz, one up as the other goes down.
    const float tiltDb = juce::jmap (tone01, -5.0f, 5.0f);
    for (auto& f : tailTiltLow)  f.set (fofo::Svf::Type::LowShelf,  850.0f, 0.7f, -tiltDb);
    for (auto& f : tailTiltHigh) f.set (fofo::Svf::Type::HighShelf, 850.0f, 0.7f,  tiltDb);

    // DUCK: the envelope pulls the destination down from 1 toward 0.
    duckMod.setBase (dDuck, 1.0f);
    duckMod.setRouteDepth (rDuck, -duck01);
}

void BackporchEngine::process (juce::AudioBuffer<float>& buffer) noexcept
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

    for (int ch = 0; ch < nCh; ++ch)
        drySnap.copyFrom (ch, 0, buffer, ch, 0, nS);

    auto* wl = wet.getWritePointer (0);
    auto* wr = wet.getWritePointer (1);

    for (int n = 0; n < nS; ++n)
    {
        // ── build the send: high-pass, then pre-delay ─────────────────────
        float sL = sendHp[0].process (drySnap.getReadPointer (0)[n]);
        float sR = nCh > 1 ? sendHp[1].process (drySnap.getReadPointer (1)[n]) : sL;

        sL = preDelay[0].processSample (sL, preSamp);
        sR = preDelay[1].processSample (sR, preSamp);

        // ── slap: one dark discrete repeat ────────────────────────────────
        if (slapGain > 0.0f)
        {
            const float fbL = slapDark[0].process (slapLine[0].read (slapSamp));
            const float fbR = slapDark[1].process (slapLine[1].read (slapSamp));
            slapLine[0].push (sL + fbL * slapFeedback);
            slapLine[1].push (sR + fbR * slapFeedback);
            sL = slapLine[0].read (slapSamp) + sL * 0.12f;
            sR = slapLine[1].read (slapSamp) + sR * 0.12f;
        }

        // ── early field, then the late field it feeds ─────────────────────
        float eL = 0.0f, eR = 0.0f;
        if (earlyGain > 0.0f)
            early.process (0.5f * (sL + sR), eL, eR);

        float tL = 0.0f, tR = 0.0f;
        tank.process (sL + eL * 0.5f, sR + eR * 0.5f, tL, tR);

        wl[n] = eL * earlyGain + tL * tankGain;
        wr[n] = eR * earlyGain + tR * tankGain;
    }

    // ── tail tone, duck, and one recombine ────────────────────────────────
    // Soundtoys curve: wet rises to unity at 70% MIX, then the dry comes down.
    float wetGain, dryGain;
    if (mix01 <= 0.70f) { wetGain = mix01 / 0.70f; dryGain = 1.0f; }
    else                { wetGain = 1.0f;          dryGain = 1.0f - (mix01 - 0.70f) / 0.30f; }

    for (int n = 0; n < nS; ++n)
    {
        float key = 0.0f;
        for (int ch = 0; ch < nCh; ++ch) key += drySnap.getReadPointer (ch)[n];
        duckMod.tick (key / (float) nCh);
        const float duck = duckMod.get (dDuck);

        for (int ch = 0; ch < nCh; ++ch)
        {
            float t = wet.getReadPointer (ch)[n];
            t = tailTiltLow[ch].process (t);
            t = tailTiltHigh[ch].process (t);

            // No clipper on the sum. The dry path stays exactly as it arrived.
            buffer.getWritePointer (ch)[n] =
                drySnap.getReadPointer (ch)[n] * dryGain + t * duck * wetGain;
        }
    }

    {
        float peak = 0.0f;
        for (int ch = 0; ch < nCh; ++ch) peak = juce::jmax (peak, buffer.getMagnitude (ch, 0, nS));
        float cur = outputPeak.load (std::memory_order_relaxed);
        while (peak > cur && ! outputPeak.compare_exchange_weak (cur, peak, std::memory_order_release, std::memory_order_relaxed)) {}
    }
}

} // namespace bkpr
