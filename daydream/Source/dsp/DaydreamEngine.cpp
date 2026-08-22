#include "DaydreamEngine.h"

namespace daydream
{

using fofo::Lfo;
using fofo::Drift;
using fofo::EnvSource;

void DaydreamEngine::prepare (const juce::dsp::ProcessSpec& s)
{
    spec = { s.sampleRate, (int) s.maximumBlockSize, (int) juce::jmax (1u, s.numChannels) };

    for (auto& f : inputHp)
    {
        f.prepare (spec.sampleRate);
        f.set (fofo::Svf::Type::Highpass, 80.0f, 0.7f);
    }

    tapeSat.prepare (spec);
    transport.prepare (spec);
    transport.setCentreDelayMs (5.0f);

    early.prepare (spec);
    tank .prepare (spec);

    for (int ch = 0; ch < 2; ++ch)
    {
        // 80 ms sweep: the shimmer is feeding a reverb, where smear hides in
        // the tail and the crossfade rate wants to stay well below the
        // material. A short sweep would put its sidebands right in the way.
        shimmer[ch].prepare (spec.sampleRate, 80.0f);
        shimmer[ch].setRatio (2.0f);
        shimHp[ch].prepare (spec.sampleRate);
        shimHp[ch].set (fofo::Svf::Type::Highpass, 250.0f, 0.7f);
        shimLp[ch].prepare (spec.sampleRate);
        shimLp[ch].set (fofo::Svf::Type::Lowpass, 6500.0f, 0.6f);
        outputLp[ch].prepare (spec.sampleRate);
        outputLp[ch].set (fofo::Svf::Type::Lowpass, 20000.0f, 0.6f);
    }

    mod = fofo::ModMatrix {};
    sWow  = mod.addSource (std::make_unique<Lfo> (Lfo::Shape::Sine, 0.55f, 0x51A1u));
    sWowR = mod.addSource (std::make_unique<Lfo> (Lfo::Shape::Sine, 0.61f, 0x52B2u));
    sDuck = mod.addSource (std::make_unique<EnvSource> (10.0f, 350.0f));
    static_cast<Lfo*> (mod.source (sWow ))->setRateDrift (0.30f);
    static_cast<Lfo*> (mod.source (sWowR))->setRateDrift (0.30f);
    static_cast<Lfo*> (mod.source (sWowR))->setStartPhase (0.27f);

    const float maxWow = 0.004f * (float) spec.sampleRate;
    dWowL = mod.addDest ("wowL", 0.0f, -maxWow, maxWow);
    dWowR = mod.addDest ("wowR", 0.0f, -maxWow, maxWow);
    dDuck = mod.addDest ("duck", 1.0f, 0.0f, 1.0f);

    rWowL = mod.connect (sWow,  dWowL, 0.0f);
    rWowR = mod.connect (sWowR, dWowR, 0.0f);
    rDuck = mod.connect (sDuck, dDuck, 0.0f);
    mod.prepare (spec);

    transport.setSampleTick ([this]
    {
        float key = 0.0f;
        if (tickDryL != nullptr)
        {
            key = tickDryL[tickIndex];
            if (tickDryR != nullptr) key = 0.5f * (key + tickDryR[tickIndex]);
        }
        mod.tick (key);
        ++tickIndex;
    });
    transport.setDelayProvider ([this] (int ch) { return mod.get (ch == 0 ? dWowL : dWowR); });

    noiseRng.seed (0xD4D2EAu);
    noiseLp.prepare (spec.sampleRate);
    noiseLp.set (fofo::Svf::Type::Lowpass, 7000.0f, 0.6f);

    drySnap.setSize (2, spec.maxBlockSize, false, true, true);
    wetBus .setSize (2, spec.maxBlockSize, false, true, true);

    dryGainSm.reset (spec.sampleRate, 0.05);
    wetGainSm.reset (spec.sampleRate, 0.05);

    dreamSmoothed = dreamTarget;
    updateMacros (dreamSmoothed);
    reset();
}

void DaydreamEngine::reset()
{
    for (auto& f : inputHp) f.reset();
    tapeSat.reset();
    transport.reset();
    early.reset();
    tank.reset();
    for (int ch = 0; ch < 2; ++ch)
    {
        shimmer[ch].reset();
        shimHp[ch].reset();
        shimLp[ch].reset();
        outputLp[ch].reset();
    }
    shimFbL = shimFbR = 0.0f;
    mod.reset();
    noiseLp.reset();
    drySnap.clear();
    wetBus.clear();
}

void DaydreamEngine::updateMacros (float k01)
{
    // The three stages of the journey, overlapping so nothing arrives with a
    // hard edge.
    const float warm  = smoothstep (0.05f, 0.40f, k01);
    const float memory = smoothstep (0.32f, 0.70f, k01);
    const float dream  = smoothstep (0.62f, 1.00f, k01);

    // ── warm: tape ───────────────────────────────────────────────────────
    tapeAmt = warm;
    tapeSat.setDrive (0.10f + 0.45f * warm);
    tapeSat.setEmphasisDb (2.0f + 3.0f * warm);
    transport.setHeadBump (juce::jmap (warm, 90.0f, 62.0f), warm * 2.4f);
    transport.setGapLossHz (juce::jmap (memory, 16000.0f, 7000.0f));
    transport.setSelfErasure (0.15f + 0.35f * warm);
    transport.setDropoutAmount (memory * 0.35f);
    transport.setHissDb (k01 < 0.05f ? -120.0f : juce::jmap (warm, -88.0f, -74.0f));

    // ── memory: wow, width, and the room growing ─────────────────────────
    wowMs = memory * 1.1f;
    const float msToSamp = 0.001f * (float) spec.sampleRate;
    mod.setRouteDepth (rWowL, wowMs * msToSamp);
    mod.setRouteDepth (rWowR, wowMs * msToSamp * 0.88f);

    earlyAmt = 0.55f * smoothstep (0.05f, 0.55f, k01) * (1.0f - 0.5f * dream);
    early.setSize01 (juce::jmap (memory, 0.20f, 0.85f));
    early.setDampHz (juce::jmap (memory, 9000.0f, 5000.0f));
    early.setWidth01 (0.5f + 0.5f * memory);

    tank.setSize01 (juce::jmap (memory, 0.25f, 0.85f));
    // Capped short of runaway: full dream is lush, never a swamp.
    tank.setDecaySeconds (juce::jmap (k01, 0.5f, 14.0f));
    tank.setLowDecayRatio (0.45f);          // the wash never piles up down low
    tank.setHighDecayRatio (juce::jmap (dream, 0.55f, 0.85f));
    tank.setCrossovers (250.0f, 4200.0f);
    tank.setModDepth (4.0f + 8.0f * memory);
    tank.setWidth01 (0.6f + 0.4f * memory);

    // ── dream: shimmer, ducking, gauze ───────────────────────────────────
    shimAmt = dream * 0.62f;                // feedback cap, per the design rule
    duckAmt = dream * 0.55f;
    mod.setRouteDepth (rDuck, -duckAmt);

    noiseGain = (k01 < 0.05f) ? 0.0f : juce::Decibels::decibelsToGain (juce::jmap (warm, -90.0f, -72.0f));

    const float lpHz = juce::jmap (dream, 20000.0f, 8000.0f);
    for (auto& f : outputLp) f.set (fofo::Svf::Type::Lowpass, lpHz, 0.6f);

    // Dry holds up; wet climbs. The dry floor keeps the performance present
    // even at full dream.
    dryGainSm.setTargetValue (juce::jmap (dream, 1.0f, 0.75f));
    wetGainSm.setTargetValue (juce::jmap (k01, 0.0f, 0.82f));
}

void DaydreamEngine::process (juce::AudioBuffer<float>& buffer) noexcept
{
    const int nCh = juce::jmin (buffer.getNumChannels(), 2);
    const int nS  = buffer.getNumSamples();
    if (nCh == 0 || nS == 0) return;

    {
        float peak = 0.0f;
        for (int ch = 0; ch < nCh; ++ch) peak = juce::jmax (peak, buffer.getMagnitude (ch, 0, nS));
        float cur = inputPeakMax.load (std::memory_order_relaxed);
        while (peak > cur && ! inputPeakMax.compare_exchange_weak (cur, peak, std::memory_order_release, std::memory_order_relaxed)) {}
    }

    // One knob, smoothed, so a sweep is a journey and not a series of steps.
    dreamSmoothed += 0.15f * (dreamTarget - dreamSmoothed);
    updateMacros (dreamSmoothed);

    for (int ch = 0; ch < nCh; ++ch)
        drySnap.copyFrom (ch, 0, buffer, ch, 0, nS);

    // ── send: high-pass, tape saturation, then the transport ─────────────
    for (int ch = 0; ch < nCh; ++ch)
    {
        auto* d = buffer.getWritePointer (ch);
        for (int n = 0; n < nS; ++n) d[n] = inputHp[ch].process (d[n]);
    }

    tapeSat.process (buffer, nS);

    // The transport advances the matrix once per sample. The ducker keys off
    // the DRY signal — a wash that ducks under its own tail never gets out of
    // the way — so point the tick at the dry snapshot.
    tickIndex = 0;
    tickDryL = drySnap.getReadPointer (0);
    tickDryR = nCh > 1 ? drySnap.getReadPointer (1) : nullptr;
    transport.process (buffer, nS);

    // ── space, with the shimmer inside the tank's feedback ───────────────
    auto* wl = wetBus.getWritePointer (0);
    auto* wr = wetBus.getWritePointer (1);

    for (int n = 0; n < nS; ++n)
    {
        const float inL = buffer.getReadPointer (0)[n];
        const float inR = nCh > 1 ? buffer.getReadPointer (1)[n] : inL;

        float noise = 0.0f;
        if (noiseGain > 0.0f) noise = noiseLp.process (noiseRng.bipolar()) * noiseGain;

        float eL = 0.0f, eR = 0.0f;
        if (earlyAmt > 0.0f) early.process (0.5f * (inL + inR), eL, eR);

        // Shimmer: an octave up, band-limited and softly clipped inside the
        // loop so it can never build without bound.
        const float sL = shimAmt > 0.0f ? fofo::softClipCubic (shimLp[0].process (shimHp[0].process (shimmer[0].process (shimFbL)))) : 0.0f;
        const float sR = shimAmt > 0.0f ? fofo::softClipCubic (shimLp[1].process (shimHp[1].process (shimmer[1].process (shimFbR)))) : 0.0f;

        float tL = 0.0f, tR = 0.0f;
        tank.process (inL + eL * 0.4f + sL * shimAmt + noise,
                      inR + eR * 0.4f + sR * shimAmt + noise, tL, tR);

        shimFbL = tL;
        shimFbR = tR;

        wl[n] = tL + eL * earlyAmt;
        wr[n] = tR + eR * earlyAmt;
    }

    // ── duck the wash under the playing, then mix ────────────────────────
    for (int n = 0; n < nS; ++n)
    {
        const float duck = mod.get (dDuck);
        const float dg = dryGainSm.getNextValue();
        const float wg = wetGainSm.getNextValue();

        for (int ch = 0; ch < nCh; ++ch)
        {
            const float wet = outputLp[ch].process (wetBus.getReadPointer (ch)[n]) * duck;
            buffer.getWritePointer (ch)[n] = drySnap.getReadPointer (ch)[n] * dg + wet * wg;
        }
    }

    {
        float peak = 0.0f;
        for (int ch = 0; ch < nCh; ++ch) peak = juce::jmax (peak, buffer.getMagnitude (ch, 0, nS));
        float cur = outputPeakMax.load (std::memory_order_relaxed);
        while (peak > cur && ! outputPeakMax.compare_exchange_weak (cur, peak, std::memory_order_release, std::memory_order_relaxed)) {}
    }
}

} // namespace daydream
