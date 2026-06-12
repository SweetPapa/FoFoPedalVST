#include "Saturator.h"

namespace vroom
{

namespace
{
    // Grid-conduction soft limit on the positive half: models the way a
    // triode's grid starts conducting and compresses the top of the wave
    // before the main clip — the "bark" of a pushed tube stage.
    inline float gridLimit (float x) noexcept
    {
        if (x <= 0.5f) return x;
        const float o = (x - 0.5f) * 2.0f;
        return 0.5f + (x - 0.5f) / (1.0f + o * o);
    }
}

void Saturator::prepare (const juce::dsp::ProcessSpec& s)
{
    spec = s;

    rebuildOversampler();

    preEmph .clear(); preEmph .resize (spec.numChannels);
    postEmph.clear(); postEmph.resize (spec.numChannels);
    interstageLPF.clear(); interstageLPF.resize (spec.numChannels);
    adaa.assign (spec.numChannels, {});

    builtVoice = -1;
    rebuildVoiceFilters();
    updateInterstageCoefficients();

    sagEnv.prepare (5.0f, 150.0f, spec.sampleRate);
    envScratch.assign (spec.maximumBlockSize, 0.0f);

    const double rampTime = 0.02;
    preGainSmoothed.reset (spec.sampleRate, rampTime);
    makeupSmoothed .reset (spec.sampleRate, rampTime);
    biasSmoothed   .reset (spec.sampleRate, rampTime);
}

void Saturator::reset()
{
    if (oversampler) oversampler->reset();
    for (auto& f : preEmph)  f.reset();
    for (auto& f : postEmph) f.reset();
    for (auto& f : interstageLPF) f.reset();
    for (auto& chs : adaa) { chs[0].reset(); chs[1].reset(); }
    sagEnv.reset();
}

void Saturator::setOversamplingFactorPower (int newPower)
{
    newPower = juce::jlimit (1, 3, newPower);
    if (newPower == factorPower) return;
    factorPower = newPower;
    if (spec.sampleRate > 0.0)
    {
        rebuildOversampler();
        updateInterstageCoefficients();
    }
}

void Saturator::rebuildOversampler()
{
    if (spec.numChannels == 0 || spec.maximumBlockSize == 0) return;

    oversampler = std::make_unique<juce::dsp::Oversampling<float>> (
        spec.numChannels,
        factorPower,
        juce::dsp::Oversampling<float>::filterHalfBandPolyphaseIIR,
        true /* maxQuality */);
    oversampler->initProcessing (spec.maximumBlockSize);
    oversampler->reset();
}

void Saturator::rebuildVoiceFilters()
{
    if (spec.sampleRate <= 0.0 || builtVoice == clipShape) return;

    const double sr = spec.sampleRate;
    juce::dsp::IIR::Coefficients<float>::Ptr pre, post;

    // The character of each voice lives mostly here: what gets tilted INTO
    // the clipper gets saturated hardest; the inverse tilt afterwards keeps
    // the net response flat-ish while the generated harmonics stay.
    switch (clipShape)
    {
        case Shape_Crunch:
            // Presence shelf +5 dB @ 1.1 kHz into the curve → bite up top,
            // lows stay cleaner (no intermod mud).
            pre  = juce::dsp::IIR::Coefficients<float>::makeHighShelf (sr, 1100.0, 0.707f, juce::Decibels::decibelsToGain ( 5.0f));
            post = juce::dsp::IIR::Coefficients<float>::makeHighShelf (sr, 1100.0, 0.707f, juce::Decibels::decibelsToGain (-5.0f));
            break;

        case Shape_Fuzz:
            // Slight low boost into the fuzz (wall of sound), post LPF tames
            // the wasp-in-a-jar fizz.
            pre  = juce::dsp::IIR::Coefficients<float>::makeLowShelf (sr, 350.0, 0.707f, juce::Decibels::decibelsToGain (3.0f));
            post = juce::dsp::IIR::Coefficients<float>::makeLowPass  (sr, 6500.0, 0.6f);
            break;

        case Shape_Octave:
            // Octave content reads best when mids dominate the rectifier.
            pre  = juce::dsp::IIR::Coefficients<float>::makeHighPass (sr, 180.0, 0.707f);
            post = juce::dsp::IIR::Coefficients<float>::makePeakFilter (sr, 1500.0, 0.9f, juce::Decibels::decibelsToGain (2.0f));
            break;

        case Shape_Smooth:
        default:
            // Gentle bright tilt in, inverse out — sheen without harshness.
            pre  = juce::dsp::IIR::Coefficients<float>::makeHighShelf (sr, 700.0, 0.5f, juce::Decibels::decibelsToGain ( 2.5f));
            post = juce::dsp::IIR::Coefficients<float>::makeHighShelf (sr, 700.0, 0.5f, juce::Decibels::decibelsToGain (-2.5f));
            break;
    }

    for (auto& f : preEmph)  { f.coefficients = pre;  f.reset(); }
    for (auto& f : postEmph) { f.coefficients = post; f.reset(); }
    builtVoice = clipShape;
}

void Saturator::updateInterstageCoefficients()
{
    const double overRate = spec.sampleRate * (double) (1 << factorPower);
    interstageCoefs = juce::dsp::IIR::Coefficients<float>::makeFirstOrderLowPass (overRate, 9000.0f);
    for (auto& f : interstageLPF) f.coefficients = interstageCoefs;
}

int Saturator::getLatencySamples() const noexcept
{
    return oversampler ? (int) std::ceil (oversampler->getLatencyInSamples()) : 0;
}

void Saturator::process (juce::AudioBuffer<float>& buffer) noexcept
{
    if (! oversampler) return;

    rebuildVoiceFilters(); // no-op unless the voice changed

    // ── drive mapping: dB-domain taper with loudness compensation ──────────
    // driveDB = ceiling · p^1.5 puts more travel in the usable lower half;
    // makeup undoes ~78% of it so the knob changes texture, not just volume.
    const float driveDb  = kMaxDriveDb * driveCeilingScale * std::pow (driveUI01, 1.5f);
    const float preGain  = juce::Decibels::decibelsToGain (driveDb);
    const float makeup   = juce::Decibels::decibelsToGain (-kMakeupComp * driveDb);

    preGainSmoothed.setTargetValue (preGain);
    makeupSmoothed .setTargetValue (makeup);
    biasSmoothed   .setTargetValue (kBiasMax * charUI01);

    const int nCh = (int) juce::jmin ((juce::uint32) buffer.getNumChannels(), spec.numChannels);
    const int nS  = buffer.getNumSamples();
    const int shape = clipShape;

    // ── pre-emphasis + envelope capture at base rate ───────────────────────
    // The envelope is taken from the mono input sum so both channels move
    // together (a stereo-divergent bias point sounds broken, not wide).
    // We store one env value per base sample for use inside the OS loop.
    for (int n = 0; n < nS; ++n)
    {
        float mono = 0.0f;
        for (int ch = 0; ch < nCh; ++ch)
        {
            auto* d = buffer.getWritePointer (ch);
            d[n] = preEmph[(size_t) ch].processSample (d[n]);
            mono += d[n];
        }
        envScratch[(size_t) n] = sagEnv.process (mono / (float) juce::jmax (1, nCh));
    }

    // ── oversampled nonlinear core ─────────────────────────────────────────
    juce::dsp::AudioBlock<float> block (buffer);
    auto upBlock = oversampler->processSamplesUp (block);

    const int numSamplesUp = (int) upBlock.getNumSamples();
    const int upPerBase    = 1 << factorPower;

    for (int n = 0; n < numSamplesUp; n += upPerBase)
    {
        const float drive  = preGainSmoothed.getNextValue();
        const float mk     = makeupSmoothed .getNextValue();
        const float bias0  = biasSmoothed   .getNextValue();

        const int   baseIdx = juce::jmin (nS - 1, n / upPerBase);
        const float env     = juce::jmin (1.0f, envScratch[(size_t) baseIdx]);

        // Program-dependence: playing harder shifts the bias point (more even
        // harmonics) and drops headroom (supply sag squashes the transient).
        const float bias     = bias0 + sagDepth01 * 0.22f * env;
        const float headroom = 1.0f / (1.0f + sagDepth01 * 0.9f * env);
        const float g        = drive * headroom;

        for (int k = 0; k < upPerBase && (n + k) < numSamplesUp; ++k)
        {
            for (int ch = 0; ch < nCh; ++ch)
            {
                auto* d = upBlock.getChannelPointer ((size_t) ch);
                const float x = d[n + k] * g;
                float y;

                switch (shape)
                {
                    case Shape_Crunch:
                    {
                        // Grid limit → asymmetric cubic, second gentler pass.
                        float a = spt::crunchShape (gridLimit (x + bias * 0.5f), 1.25f, 0.85f);
                        a = interstageLPF[(size_t) ch].processSample (a);
                        y = spt::crunchShape (a * 1.1f, 1.1f, 0.95f) * 1.4f;
                        break;
                    }

                    case Shape_Fuzz:
                    {
                        // Heavy squash + strong asymmetry. The env-driven bias
                        // makes decays sputter and clean up — fuzz that reacts.
                        // 0.95 trim level-matches Fuzz to the other voices
                        // (it was ~7 dB quieter — switching voices shouldn't
                        // read as a volume drop).
                        float a = std::tanh (3.5f * x + bias * 2.0f) - std::tanh (bias * 2.0f);
                        a = interstageLPF[(size_t) ch].processSample (a);
                        y = std::tanh (1.6f * a) * 0.95f;
                        break;
                    }

                    case Shape_Octave:
                    {
                        // Normal clip + full-wave-rectified clip. Rectification
                        // doubles the frequency → ghost octave; the blend rides
                        // the envelope so the octave sings on hard attacks.
                        const float normal = std::tanh (x + bias) - std::tanh (bias);
                        const float rect   = std::tanh (std::abs (x) * 1.2f);
                        const float blend  = 0.35f + 0.3f * env;
                        float a = normal * (1.0f - blend * 0.5f) + rect * blend;
                        a = interstageLPF[(size_t) ch].processSample (a);
                        y = std::tanh (a * 1.2f) * 1.1f; // level-match to Smooth
                        break;
                    }

                    case Shape_Smooth:
                    default:
                    {
                        // Two cascaded ADAA tanh stages, interstage LPF tames
                        // the harmonic buildup between them.
                        float a = adaa[(size_t) ch][0].process (x, bias);
                        a = interstageLPF[(size_t) ch].processSample (a);
                        y = adaa[(size_t) ch][1].process (a * 0.8f, bias * 0.6f) * 1.25f;
                        break;
                    }
                }

                d[n + k] = y * mk;
            }
        }
    }

    oversampler->processSamplesDown (block);

    // ── post de-emphasis / voicing at base rate ────────────────────────────
    for (int ch = 0; ch < nCh; ++ch)
    {
        auto* d = buffer.getWritePointer (ch);
        for (int n = 0; n < nS; ++n)
            d[n] = postEmph[(size_t) ch].processSample (d[n]);
    }
}

} // namespace vroom
