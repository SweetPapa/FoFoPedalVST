#include "Drive.h"

namespace fofopedal
{

namespace
{
    // Grid-conduction soft limit on the positive half — the compressed top
    // of a pushed triode's transfer, applied before the main tube curve.
    inline float gridLimit (float x) noexcept
    {
        if (x <= 0.5f) return x;
        const float o = (x - 0.5f) * 2.0f;
        return 0.5f + (x - 0.5f) / (1.0f + o * o);
    }

    inline float shapeIron (float x) noexcept
    {
        // Blend of tanh and a mild cubic — "iron core" 3rd + 5th emphasis;
        // the pre/de-emphasis shelves around this aim it at the low band.
        const float linear = juce::jlimit (-1.5f, 1.5f, x);
        const float cubic  = linear - (linear * linear * linear) / 3.0f;
        const float th     = std::tanh (x);
        return 0.5f * cubic + 0.5f * th;
    }
}

void Drive::prepare (const juce::dsp::ProcessSpec& s)
{
    spec = s;
    rebuildOversampler();

    ironPreShelf .clear(); ironPreShelf .resize ((size_t) s.numChannels);
    ironPostShelf.clear(); ironPostShelf.resize ((size_t) s.numChannels);
    tapeRolloff  .clear(); tapeRolloff  .resize ((size_t) s.numChannels);
    toneLow      .clear(); toneLow      .resize ((size_t) s.numChannels);
    toneHigh     .clear(); toneHigh     .resize ((size_t) s.numChannels);

    {
        // Pre-emphasis: +7 dB below ~500 Hz drives the lows hard into the
        // shaper (transformer 3rd/5th harmonics). Post de-emphasis: -7 dB
        // restores tonal balance, leaving the generated harmonics behind.
        auto cPre  = juce::dsp::IIR::Coefficients<float>::makeLowShelf (s.sampleRate, 500.0, 0.707f, juce::Decibels::decibelsToGain ( 7.0f));
        auto cPost = juce::dsp::IIR::Coefficients<float>::makeLowShelf (s.sampleRate, 500.0, 0.707f, juce::Decibels::decibelsToGain (-7.0f));
        for (auto& f : ironPreShelf)  f.coefficients = cPre;
        for (auto& f : ironPostShelf) f.coefficients = cPost;
    }

    dryBuffer.setSize ((int) s.numChannels, (int) s.maximumBlockSize, false, true, true);

    adaa   .assign (s.numChannels, {});
    hyst   .assign (s.numChannels, {});
    dcBlock.assign (s.numChannels, {});
    for (auto& b : dcBlock) b.setCutoff (15.0f, s.sampleRate);
    touchEnv.prepare (5.0f, 150.0f, s.sampleRate);
    envScratch.assign (s.maximumBlockSize, 0.0f);

    toneDirty = true;
    rebuildFilters();
}

void Drive::reset()
{
    if (os) os->reset();
    for (auto& f : ironPreShelf)  f.reset();
    for (auto& f : ironPostShelf) f.reset();
    for (auto& f : tapeRolloff)   f.reset();
    for (auto& f : toneLow)       f.reset();
    for (auto& f : toneHigh)      f.reset();
    for (auto& a : adaa)    a.reset();
    for (auto& h : hyst)    h.reset();
    for (auto& b : dcBlock) b.reset();
    touchEnv.reset();
}

void Drive::setAlgo (Algo a) noexcept
{
    if (a == algo) return;
    algo = a;
    // Clear OS state on algo change — different shapers introduce different
    // DC/group-delay characteristics; a stale state would click.
    if (os) os->reset();
    for (auto& f : ironPreShelf)  f.reset();
    for (auto& f : ironPostShelf) f.reset();
}

void Drive::rebuildOversampler()
{
    if (spec.numChannels == 0 || spec.maximumBlockSize == 0) return;
    os = std::make_unique<juce::dsp::Oversampling<float>> (
        spec.numChannels, 2, // 4×
        juce::dsp::Oversampling<float>::filterHalfBandPolyphaseIIR, true);
    os->initProcessing (spec.maximumBlockSize);
    os->reset();
}

void Drive::rebuildFilters()
{
    if (spec.sampleRate <= 0.0) return;
    const double sr = spec.sampleRate;

    // Tape rolloff — gets darker as drive rises (16 kHz → 5 kHz).
    {
        const float hz = juce::jmap (drive01, 0.0f, 1.0f, 16000.0f, 5000.0f);
        if (std::abs (hz - lastTapeHz) > 50.0f)
        {
            auto c = juce::dsp::IIR::Coefficients<float>::makeLowPass (sr, hz, 0.6f);
            for (auto& f : tapeRolloff) f.coefficients = c;
            lastTapeHz = hz;
        }
    }

    // Post tone tilt — TONE 0..1 maps to ±3 dB shelves at 800 Hz.
    if (toneDirty || std::abs (tone01 - lastTone) > 1.0e-3f)
    {
        const float tiltDb = juce::jmap (tone01, 0.0f, 1.0f, -3.0f, 3.0f);
        auto cLo = juce::dsp::IIR::Coefficients<float>::makeLowShelf  (sr, 800.0, 0.707f, juce::Decibels::decibelsToGain (-tiltDb));
        auto cHi = juce::dsp::IIR::Coefficients<float>::makeHighShelf (sr, 800.0, 0.707f, juce::Decibels::decibelsToGain ( tiltDb));
        for (auto& f : toneLow)  f.coefficients = cLo;
        for (auto& f : toneHigh) f.coefficients = cHi;
        lastTone  = tone01;
        toneDirty = false;
    }
}

int Drive::getLatencySamples() const noexcept
{
    return os ? (int) std::ceil (os->getLatencyInSamples()) : 0;
}

void Drive::process (juce::AudioBuffer<float>& buffer) noexcept
{
    if (bypassed || ! os) return;

    rebuildFilters();

    const int nCh = buffer.getNumChannels();
    const int nS  = buffer.getNumSamples();

    // Snapshot dry for parallel blend.
    for (int ch = 0; ch < nCh; ++ch)
        dryBuffer.copyFrom (ch, 0, buffer, ch, 0, nS);

    // Iron pre-emphasis: boost lows into the shaper (all in-line, so the
    // oversampler latency applies uniformly — no comb-filtering recombine).
    const bool isIron = (algo == Algo::Iron);
    if (isIron)
    {
        for (int ch = 0; ch < juce::jmin (nCh, (int) ironPreShelf.size()); ++ch)
        {
            auto* d = buffer.getWritePointer (ch);
            for (int n = 0; n < nS; ++n)
                d[n] = ironPreShelf[(size_t) ch].processSample (d[n]);
        }
    }

    // Drive in the dB domain: more travel in the usable low half, and ~78%
    // loudness compensation so the knob changes texture, not just volume.
    const float driveDb = 30.0f * std::pow (drive01, 1.4f);
    const float preGain = juce::Decibels::decibelsToGain (driveDb);
    const float makeup  = juce::Decibels::decibelsToGain (-0.78f * driveDb);

    // Base-rate input envelope → bias drift (tube touch response). One value
    // per base sample, indexed from inside the oversampled loop.
    for (int n = 0; n < nS; ++n)
    {
        float mono = 0.0f;
        for (int ch = 0; ch < nCh; ++ch) mono += buffer.getReadPointer (ch)[n];
        envScratch[(size_t) n] = touchEnv.process (mono / (float) juce::jmax (1, nCh));
    }

    juce::dsp::AudioBlock<float> blk (buffer);
    auto upBlock = os->processSamplesUp (blk);
    const int osN   = (int) upBlock.getNumSamples();
    const int osCh  = (int) upBlock.getNumChannels();
    const int upPerBase = juce::jmax (1, osN / juce::jmax (1, nS));

    for (int ch = 0; ch < osCh; ++ch)
    {
        auto* d = upBlock.getChannelPointer ((size_t) ch);
        auto& ad = adaa   [(size_t) juce::jmin (ch, (int) adaa.size() - 1)];
        auto& hy = hyst   [(size_t) juce::jmin (ch, (int) hyst.size() - 1)];
        auto& dc = dcBlock[(size_t) juce::jmin (ch, (int) dcBlock.size() - 1)];

        for (int n = 0; n < osN; ++n)
        {
            const float env = juce::jmin (1.0f, envScratch[(size_t) juce::jmin (nS - 1, n / upPerBase)]);
            const float x = preGain * d[n];
            float y;
            switch (algo)
            {
                case Algo::Tube:
                {
                    // Bias rides the envelope: 0.12 at rest → ~0.30 dug in.
                    const float bias = 0.12f + 0.18f * env;
                    y = ad.process (gridLimit (x), bias);
                    break;
                }
                case Algo::Tape:
                {
                    // Hysteresis loop; k rises with drive for deeper history.
                    const float k = 0.10f + 0.20f * drive01;
                    y = hy.process (x, 1.0f, k);
                    y = dc.process (y);
                    break;
                }
                case Algo::Iron: y = shapeIron (x); break;
                case Algo::NumAlgos:
                default:         y = x;             break;
            }
            d[n] = y * makeup;
        }
    }

    os->processSamplesDown (blk);

    // Iron de-emphasis: restore tonal balance, keeping the added harmonics.
    if (isIron)
    {
        for (int ch = 0; ch < juce::jmin (nCh, (int) ironPostShelf.size()); ++ch)
        {
            auto* d = buffer.getWritePointer (ch);
            for (int n = 0; n < nS; ++n)
                d[n] = ironPostShelf[(size_t) ch].processSample (d[n]);
        }
    }

    // Tape rolloff (only on Tape algo).
    if (algo == Algo::Tape)
    {
        for (int ch = 0; ch < juce::jmin (nCh, (int) tapeRolloff.size()); ++ch)
        {
            auto* d = buffer.getWritePointer (ch);
            for (int n = 0; n < nS; ++n)
                d[n] = tapeRolloff[(size_t) ch].processSample (d[n]);
        }
    }

    // Post tone tilt.
    for (int ch = 0; ch < juce::jmin (nCh, (int) toneLow.size()); ++ch)
    {
        auto* d = buffer.getWritePointer (ch);
        for (int n = 0; n < nS; ++n)
        {
            float v = toneLow [(size_t) ch].processSample (d[n]);
            v       = toneHigh[(size_t) ch].processSample (v);
            d[n] = v;
        }
    }

    // Parallel blend (mix=1 → all wet, mix=0 → all dry).
    if (mix01 < 0.999f)
    {
        const float wet = mix01;
        const float dry = 1.0f - mix01;
        for (int ch = 0; ch < nCh; ++ch)
        {
            auto* d   = buffer.getWritePointer (ch);
            auto* dr  = dryBuffer.getReadPointer (ch);
            for (int n = 0; n < nS; ++n)
                d[n] = wet * d[n] + dry * dr[n];
        }
    }
}

} // namespace fofopedal
