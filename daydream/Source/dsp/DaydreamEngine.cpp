#include "DaydreamEngine.h"

namespace daydream
{

void DaydreamEngine::prepare (const juce::dsp::ProcessSpec& s)
{
    spec = s;

    inputHPF.clear(); inputHPF.resize (s.numChannels);
    const auto hp = juce::dsp::IIR::Coefficients<float>::makeHighPass (s.sampleRate, 80.0f);
    for (auto& f : inputHPF) f.coefficients = hp;

    tapeSat.prepare (s);
    drift  .prepare (s);

    chorus.prepare (s);
    chorus.setRate        (0.55f);
    chorus.setDepth       (0.35f);
    chorus.setCentreDelay (10.0f);
    chorus.setFeedback    (0.0f);
    chorus.setMix         (0.25f);

    reverb.prepare (s);

    outputLPF.clear(); outputLPF.resize (s.numChannels);
    lastLPFhz = 16000.0f;
    const auto lp = juce::dsp::IIR::Coefficients<float>::makeLowPass (s.sampleRate, lastLPFhz);
    for (auto& f : outputLPF) f.coefficients = lp;

    dryBuffer.setSize ((int) s.numChannels, (int) s.maximumBlockSize, false, true, true);

    duckEnv.prepare (10.0f, 350.0f, s.sampleRate);
    noiseSrc.prepare (1200.0f, s.sampleRate, 0xD15EA5Eu); // fast walk ≈ pinkish hiss
    noiseLP.setCutoff (5500.0f, s.sampleRate);

    const double smoothMs = 0.04;
    dryGainSmoothed.reset (s.sampleRate, smoothMs);
    wetGainSmoothed.reset (s.sampleRate, smoothMs);

    updateMacros (dreamTarget);
    dryGainSmoothed.setCurrentAndTargetValue (dryGainSmoothed.getTargetValue());
    wetGainSmoothed.setCurrentAndTargetValue (wetGainSmoothed.getTargetValue());
}

void DaydreamEngine::reset()
{
    for (auto& f : inputHPF)  f.reset();
    tapeSat.reset();
    drift.reset();
    chorus.reset();
    reverb.reset();
    for (auto& f : outputLPF) f.reset();
    duckEnv.reset();
}

void DaydreamEngine::updateMacros (float k)
{
    k = juce::jlimit (0.0f, 1.0f, k);

    // Stage 1 — warm tape. Saturation does most of the work low on the dial.
    tapeSat.setDrive01 (juce::jmap (k, 0.0f, 1.0f, 0.04f, 0.55f));

    // Stage 2 — memory. Wow arrives mid-dial, flutter a little later.
    drift.setWowDepth01     (smoothstep (0.25f, 0.90f, k) * 0.55f);
    drift.setFlutterDepth01 (smoothstep (0.45f, 1.00f, k) * 0.45f);

    chorus.setMix   (juce::jmap (smoothstep (0.20f, 0.80f, k), 0.0f, 1.0f, 0.08f, 0.38f));
    chorus.setDepth (juce::jmap (k, 0.0f, 1.0f, 0.25f, 0.42f));

    // Stage 3 — dream. Room → hall → near-infinite, shimmer climbs in.
    // Caps tuned down from v2: max dial should be lush, never swamp — the
    // dry signal stays the lead singer even at full dream.
    reverb.setSize01    (juce::jmap (k, 0.0f, 1.0f, 0.25f, 1.00f));
    reverb.setShimmer01 (smoothstep (0.55f, 1.00f, k) * 0.70f);

    // Ducking ramps with the dial — exactly when the wash gets big enough
    // to need to move out of the way of the playing.
    duckAmount = smoothstep (0.40f, 1.00f, k) * 0.65f;

    // Noise floor texture only in the dreamy half, capped ~−72 dB.
    noiseGain = smoothstep (0.45f, 1.00f, k) * 0.00025f;

    // Output gauze — floor raised to 8 kHz so full dream stays airy.
    const float toneHz = juce::jmap (smoothstep (0.55f, 1.0f, k), 0.0f, 1.0f, 16000.0f, 8000.0f);
    if (std::abs (toneHz - lastLPFhz) > 50.0f && spec.sampleRate > 0.0)
    {
        lastLPFhz = toneHz;
        const auto coefs = juce::dsp::IIR::Coefficients<float>::makeLowPass (spec.sampleRate, toneHz);
        for (auto& f : outputLPF) f.coefficients = coefs;
    }

    // Outer mix: dry holds up high on the dial (the ducker manages the
    // balance dynamically), wet swells from silence with a smoothstep so the
    // first 5% reads as bypass.
    const float dryGain = 1.0f - smoothstep (0.60f, 1.0f, k) * 0.25f;
    const float wetGain = smoothstep (0.02f, 1.0f, k) * 0.82f;
    dryGainSmoothed.setTargetValue (dryGain);
    wetGainSmoothed.setTargetValue (wetGain);
}

void DaydreamEngine::process (juce::AudioBuffer<float>& buffer) noexcept
{
    const int nCh = buffer.getNumChannels();
    const int nS  = buffer.getNumSamples();

    {
        float peak = 0.0f;
        for (int ch = 0; ch < nCh; ++ch)
            peak = juce::jmax (peak, buffer.getMagnitude (ch, 0, nS));
        float cur = inputPeakMax.load (std::memory_order_relaxed);
        while (peak > cur && ! inputPeakMax.compare_exchange_weak (cur, peak, std::memory_order_release, std::memory_order_relaxed)) {}
    }

    updateMacros (dreamTarget);

    // Dry tap before any processing — bypass position stays transparent.
    for (int ch = 0; ch < nCh; ++ch)
        dryBuffer.copyFrom (ch, 0, buffer, ch, 0, nS);

    // ── Wet chain ──────────────────────────────────────────────────────────
    for (int ch = 0; ch < juce::jmin ((int) inputHPF.size(), nCh); ++ch)
    {
        auto* d = buffer.getWritePointer (ch);
        for (int n = 0; n < nS; ++n)
            d[n] = inputHPF[(size_t) ch].processSample (d[n]);
    }

    tapeSat.process (buffer);
    drift  .process (buffer);

    {
        juce::dsp::AudioBlock<float> blk (buffer);
        juce::dsp::ProcessContextReplacing<float> ctx (blk);
        chorus.process (ctx);
    }

    // Noise floor texture into the reverb input — faint hiss that blooms
    // through the tail, slightly louder while you play.
    if (noiseGain > 0.0f)
    {
        for (int n = 0; n < nS; ++n)
        {
            float mono = 0.0f;
            for (int ch = 0; ch < nCh; ++ch) mono += dryBuffer.getReadPointer (ch)[n];
            const float env = juce::jmin (1.0f, std::abs (mono) * 2.0f);
            const float hiss = noiseLP.process (noiseSrc.next() * 20.0f) * noiseGain * (1.0f + 2.0f * env);
            for (int ch = 0; ch < nCh; ++ch)
                buffer.getWritePointer (ch)[n] += hiss;
        }
    }

    reverb.process (buffer); // buffer now holds the fully-wet wash

    // ── Outer mix with ducking ─────────────────────────────────────────────
    for (int n = 0; n < nS; ++n)
    {
        const float dg = dryGainSmoothed.getNextValue();
        const float wg = wetGainSmoothed.getNextValue();

        // Duck the wet against the dry envelope: playing pushes the wash
        // down (up to duckAmount), silence lets it swell back.
        float mono = 0.0f;
        for (int ch = 0; ch < nCh; ++ch) mono += dryBuffer.getReadPointer (ch)[n];
        const float env  = duckEnv.process (mono / (float) juce::jmax (1, nCh));
        const float duck = 1.0f - duckAmount * juce::jmin (1.0f, env * 2.5f);

        for (int ch = 0; ch < nCh; ++ch)
        {
            const float dry = dryBuffer.getReadPointer (ch)[n];
            const float wet = buffer  .getReadPointer (ch)[n];
            buffer.getWritePointer (ch)[n] = dry * dg + wet * wg * duck;
        }
    }

    // Output gauze + safety clip.
    for (int ch = 0; ch < nCh; ++ch)
    {
        auto* d = buffer.getWritePointer (ch);
        for (int n = 0; n < nS; ++n)
        {
            float v = (ch < (int) outputLPF.size()) ? outputLPF[(size_t) ch].processSample (d[n]) : d[n];
            d[n] = spt::softClipCubic (v);
        }
    }

    {
        float peak = 0.0f;
        for (int ch = 0; ch < nCh; ++ch)
            peak = juce::jmax (peak, buffer.getMagnitude (ch, 0, nS));
        float cur = outputPeakMax.load (std::memory_order_relaxed);
        while (peak > cur && ! outputPeakMax.compare_exchange_weak (cur, peak, std::memory_order_release, std::memory_order_relaxed)) {}
    }
}

} // namespace daydream
