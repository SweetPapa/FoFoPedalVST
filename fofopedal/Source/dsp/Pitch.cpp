#include "Pitch.h"

namespace fofopedal
{

void Pitch::prepare (const juce::dsp::ProcessSpec& s)
{
    spec = { s.sampleRate, (int) s.maximumBlockSize, (int) juce::jmax (1u, s.numChannels) };

    // Detune and harmony want different sweep ranges: a few cents wants a
    // short range so the taps stay close, while an octave wants a long one so
    // the crossfade sidebands stay clear of the material. Prepared long and
    // reused, since the range only sets the sweep, not the shift.
    for (auto& sh : shifter) sh.prepare (spec.sampleRate, 45.0f);

    ringSize = (int) (2.5 * spec.sampleRate);
    ring.setSize (2, ringSize, false, true, true);
    ring.clear();
    loopLen = (int) (0.5 * spec.sampleRate);
    xfade   = (int) (0.03 * spec.sampleRate);

    mod = fofo::ModMatrix {};
    sEnv   = mod.addSource (std::make_unique<fofo::EnvSource> (8.0f, 220.0f));
    sDrift = mod.addSource (std::make_unique<fofo::Drift> (0.22f, 0xF0F0Au));
    dEnv   = mod.addDest ("env",   0.0f, 0.0f, 1.0f);
    dDrift = mod.addDest ("drift", 0.0f, -8.0f, 8.0f);   // cents
    mod.connect (sEnv,   dEnv,   1.0f);
    mod.connect (sDrift, dDrift, 4.0f);
    mod.prepare (spec);

    drySnap.setSize (2, spec.maxBlockSize, false, true, true);
    reset();
}

void Pitch::reset()
{
    for (auto& sh : shifter) sh.reset();
    ring.clear();
    writeHead = 0;
    frozen = false;
    fadeIn = 0.0f;
    readPos = 0.0f;
    mod.reset();
    drySnap.clear();
}

void Pitch::process (juce::AudioBuffer<float>& buffer) noexcept
{
    if (bypassed || spec.sampleRate <= 0.0) return;
    const int nCh = juce::jmin (buffer.getNumChannels(), 2);
    const int nS  = buffer.getNumSamples();
    if (nCh == 0 || nS == 0) return;

    for (int ch = 0; ch < nCh; ++ch)
        drySnap.copyFrom (ch, 0, buffer, ch, 0, nS);

    switch (algo)
    {
        case Algo::OctaveHarm:
        {
            // SHAPE picks the interval: down an octave, a fifth, or up an
            // octave, so one knob covers the three that are actually musical.
            const float cents = shape01 < 0.33f ? -1200.0f
                              : shape01 < 0.66f ?   700.0f
                                                 :  1200.0f;
            const float r = fofo::centsToRatio (cents);
            processShift (buffer, nS, r, r);
            break;
        }

        case Algo::Freeze:
            processFreeze (buffer, nS);
            break;

        case Algo::MicroDetune:
        case Algo::NumAlgos:
        default:
        {
            // ±3..18 cents, opposite directions per channel, with a slow
            // drift on top so it never sits still.
            const float cents = 3.0f + 15.0f * amount01;
            processShift (buffer, nS, fofo::centsToRatio (-cents), fofo::centsToRatio (cents));
            break;
        }
    }
}

void Pitch::processShift (juce::AudioBuffer<float>& buffer, int nS, float ratioL, float ratioR) noexcept
{
    const int nCh = juce::jmin (buffer.getNumChannels(), 2);
    const float dry = 1.0f - mix01;

    for (int n = 0; n < nS; ++n)
    {
        float key = 0.0f;
        for (int ch = 0; ch < nCh; ++ch) key += drySnap.getReadPointer (ch)[n];
        mod.tick (key / (float) nCh);
        const float driftCents = mod.get (dDrift);

        shifter[0].setRatio (ratioL * fofo::centsToRatio (driftCents));
        shifter[1].setRatio (ratioR * fofo::centsToRatio (-driftCents));

        for (int ch = 0; ch < nCh; ++ch)
        {
            const float w = shifter[ch].process (drySnap.getReadPointer (ch)[n]);
            buffer.getWritePointer (ch)[n] = drySnap.getReadPointer (ch)[n] * dry + w * mix01;
        }
    }
}

void Pitch::processFreeze (juce::AudioBuffer<float>& buffer, int nS) noexcept
{
    const int nCh = juce::jmin (buffer.getNumChannels(), 2);
    const float dry = 1.0f - mix01;

    // SHAPE sets how long a slice is held; AMOUNT is the freeze threshold, so
    // the block grabs and holds when the input drops away.
    loopLen = juce::jlimit (2048, ringSize - 1, (int) ((0.15 + 0.85 * shape01) * spec.sampleRate));

    for (int n = 0; n < nS; ++n)
    {
        float key = 0.0f;
        for (int ch = 0; ch < nCh; ++ch) key += drySnap.getReadPointer (ch)[n];
        mod.tick (key / (float) nCh);
        const float env = mod.get (dEnv);

        for (int ch = 0; ch < nCh; ++ch)
            ring.getWritePointer (ch)[writeHead] = drySnap.getReadPointer (ch)[n];

        const bool wantFrozen = env < (0.02f + 0.20f * (1.0f - amount01));
        if (wantFrozen && ! frozen)
        {
            frozen = true;
            fadeIn = 0.0f;
            readPos = (float) ((writeHead - loopLen + ringSize) % ringSize);
        }
        else if (! wantFrozen && frozen && env > 0.10f)
        {
            frozen = false;
        }

        float wl = 0.0f, wr = 0.0f;
        if (frozen)
        {
            fadeIn = juce::jmin (1.0f, fadeIn + 1.0f / (0.05f * (float) spec.sampleRate));

            const int i0 = (int) readPos % ringSize;
            const int i1 = (i0 + 1) % ringSize;
            const float fr = readPos - std::floor (readPos);

            auto tap = [&] (int ch)
            {
                return ring.getReadPointer (ch)[i0] * (1.0f - fr) + ring.getReadPointer (ch)[i1] * fr;
            };

            wl = tap (0) * fadeIn;
            wr = tap (nCh > 1 ? 1 : 0) * fadeIn;

            // Loop with a crossfade so the seam never clicks.
            readPos += 1.0f;
            const float start = (float) ((writeHead - loopLen + ringSize) % ringSize);
            if (readPos - start >= (float) loopLen || readPos >= (float) ringSize)
                readPos = start;
        }

        for (int ch = 0; ch < nCh; ++ch)
        {
            const float w = (ch == 0) ? wl : wr;
            buffer.getWritePointer (ch)[n] = drySnap.getReadPointer (ch)[n] * dry + w * mix01;
        }

        writeHead = (writeHead + 1) % ringSize;
    }
    juce::ignoreUnused (xfade);
}

} // namespace fofopedal
