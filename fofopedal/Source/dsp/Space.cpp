#include "Space.h"

namespace fofopedal
{

void Space::prepare (const juce::dsp::ProcessSpec& s)
{
    spec = { s.sampleRate, (int) s.maximumBlockSize, (int) juce::jmax (1u, s.numChannels) };

    for (auto& f : sendHp) f.prepare (spec.sampleRate);
    for (auto& d : pre)    d.prepare (spec.sampleRate, 0.260f * (float) spec.sampleRate);

    early.prepare (spec);
    tank .prepare (spec);

    for (int ch = 0; ch < 2; ++ch)
    {
        // 80 ms sweep — long, because this feeds a reverb where smear hides in
        // the tail and a short sweep would put its crossfade sidebands right
        // in the way of the material.
        shifter[ch].prepare (spec.sampleRate, 80.0f);
        shifter[ch].setRatio (2.0f);
        shimHp[ch].prepare (spec.sampleRate);
        shimHp[ch].set (fofo::Svf::Type::Highpass, 250.0f, 0.7f);
        shimLp[ch].prepare (spec.sampleRate);
        shimLp[ch].set (fofo::Svf::Type::Lowpass, 6500.0f, 0.6f);
    }

    duckMod = fofo::ModMatrix {};
    sDuck = duckMod.addSource (std::make_unique<fofo::EnvSource> (5.0f, 380.0f));
    dDuck = duckMod.addDest ("duck", 1.0f, 0.0f, 1.0f);
    rDuck = duckMod.connect (sDuck, dDuck, -0.30f);   // fixed gentle duck
    duckMod.prepare (spec);

    drySnap.setSize (2, spec.maxBlockSize, false, true, true);

    dirty = true;
    updateAll();
    reset();
}

void Space::reset()
{
    for (auto& f : sendHp) f.reset();
    for (auto& d : pre)    d.reset();
    early.reset();
    tank.reset();
    for (int ch = 0; ch < 2; ++ch)
    {
        shifter[ch].reset();
        shimHp[ch].reset();
        shimLp[ch].reset();
        shimFb[ch] = 0.0f;
    }
    duckMod.reset();
    drySnap.clear();
}

void Space::updateAll()
{
    if (! dirty || spec.sampleRate <= 0.0) return;
    dirty = false;

    for (auto& f : sendHp) f.set (fofo::Svf::Type::Highpass, sendHpHz, 0.7f);

    // Each algorithm is now a different space rather than a different voicing
    // of one tank: how much discrete early field there is, how the bands
    // decay, and how much the tank is modulated all move together.
    switch (algo)
    {
        case Algo::Plate:
            // A plate has no discrete early reflections — the sheet goes
            // dense immediately. Giving one an early field is the classic way
            // to make it sound like a cheap room.
            earlyGain = 0.0f;
            tank.setSize01 (juce::jmap (size01, 0.28f, 0.70f));
            tank.setDecaySeconds (juce::jmap (size01, 0.5f, 4.0f));
            tank.setLowDecayRatio (0.60f);
            tank.setHighDecayRatio (0.82f);      // bright
            tank.setCrossovers (300.0f, 5000.0f);
            tank.setModDepth (9.0f);
            tank.setWidth01 (0.90f);
            break;

        case Algo::Room:
            early.setSize01 (juce::jmap (size01, 0.15f, 0.60f));
            early.setDampHz (juce::jmap (size01, 9500.0f, 5500.0f));
            early.setWidth01 (0.80f);
            earlyGain = juce::jmap (size01, 0.80f, 0.55f);
            tank.setSize01 (juce::jmap (size01, 0.18f, 0.55f));
            tank.setDecaySeconds (juce::jmap (size01, 0.25f, 1.8f));
            tank.setLowDecayRatio (0.50f);
            tank.setHighDecayRatio (0.55f);
            tank.setCrossovers (260.0f, 3800.0f);
            tank.setModDepth (4.0f);
            tank.setWidth01 (0.75f);
            break;

        case Algo::Shimmer:
            early.setSize01 (juce::jmap (size01, 0.40f, 1.00f));
            early.setDampHz (6000.0f);
            early.setWidth01 (1.0f);
            earlyGain = 0.25f;
            tank.setSize01 (juce::jmap (size01, 0.45f, 1.00f));
            tank.setDecaySeconds (juce::jmap (size01, 2.0f, 12.0f));
            tank.setLowDecayRatio (0.42f);       // the wash must not silt up
            tank.setHighDecayRatio (0.80f);
            tank.setCrossovers (250.0f, 4500.0f);
            tank.setModDepth (11.0f);
            tank.setWidth01 (1.0f);
            break;

        case Algo::Hall:
        case Algo::NumAlgos:
        default:
            early.setSize01 (juce::jmap (size01, 0.45f, 1.00f));
            early.setDampHz (juce::jmap (size01, 8000.0f, 4500.0f));
            early.setWidth01 (0.95f);
            earlyGain = juce::jmap (size01, 0.55f, 0.35f);
            tank.setSize01 (juce::jmap (size01, 0.40f, 0.95f));
            tank.setDecaySeconds (juce::jmap (size01, 0.8f, 6.5f));
            tank.setLowDecayRatio (0.55f);
            tank.setHighDecayRatio (0.50f);      // air absorbs the top
            tank.setCrossovers (220.0f, 3200.0f);
            tank.setModDepth (7.0f);
            tank.setWidth01 (1.0f);
            break;
    }
}

void Space::process (juce::AudioBuffer<float>& buffer) noexcept
{
    if (bypassed || spec.sampleRate <= 0.0) return;

    const int nCh = juce::jmin (buffer.getNumChannels(), 2);
    const int nS  = buffer.getNumSamples();
    if (nCh == 0 || nS == 0) return;

    updateAll();

    for (int ch = 0; ch < nCh; ++ch)
        drySnap.copyFrom (ch, 0, buffer, ch, 0, nS);

    const float preSamp = juce::jlimit (2.0f, 0.250f * (float) spec.sampleRate,
                                        preDelayMs * 0.001f * (float) spec.sampleRate);
    const float shimAmt = (algo == Algo::Shimmer) ? juce::jmin (0.62f, shimmer01 * 0.62f)
                                                  : juce::jmin (0.35f, shimmer01 * 0.35f);

    for (int n = 0; n < nS; ++n)
    {
        // Key the duck off the block input, mono-summed.
        float key = 0.0f;
        for (int ch = 0; ch < nCh; ++ch) key += drySnap.getReadPointer (ch)[n];
        duckMod.tick (key / (float) nCh);

        // Send: high-pass so the tail can never accumulate low end, then
        // pre-delay so the space starts behind the transient.
        float sL = pre[0].processSample (sendHp[0].process (drySnap.getReadPointer (0)[n]), preSamp);
        float sR = pre[1].processSample (sendHp[1].process (nCh > 1 ? drySnap.getReadPointer (1)[n]
                                                                   : drySnap.getReadPointer (0)[n]), preSamp);

        float eL = 0.0f, eR = 0.0f;
        if (earlyGain > 0.0f) early.process (0.5f * (sL + sR), eL, eR);

        // Shimmer sits inside the tank's feedback: octave up, band-limited
        // and soft-clipped in the loop so it cannot build without bound.
        float shL = 0.0f, shR = 0.0f;
        if (shimAmt > 0.0f)
        {
            shL = fofo::softClipCubic (shimLp[0].process (shimHp[0].process (shifter[0].process (shimFb[0]))));
            shR = fofo::softClipCubic (shimLp[1].process (shimHp[1].process (shifter[1].process (shimFb[1]))));
        }

        float tL = 0.0f, tR = 0.0f;
        tank.process (sL + eL * 0.4f + shL * shimAmt,
                      sR + eR * 0.4f + shR * shimAmt, tL, tR);
        shimFb[0] = tL;
        shimFb[1] = tR;

        const float duck = duckMod.get (dDuck);
        const float wetL = (tL + eL * earlyGain) * duck;
        const float wetR = (tR + eR * earlyGain) * duck;

        // The block's own mix. No clipper on the sum.
        const float dry = 1.0f - mix01;
        buffer.getWritePointer (0)[n] = drySnap.getReadPointer (0)[n] * dry + wetL * mix01;
        if (nCh > 1)
            buffer.getWritePointer (1)[n] = drySnap.getReadPointer (1)[n] * dry + wetR * mix01;
    }
}

} // namespace fofopedal
