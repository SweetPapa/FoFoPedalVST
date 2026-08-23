// Presets — does the shared bank machinery behave, and is the data shippable?
//
// Two separate concerns here:
//
//   1. fofo::FactoryPresetHost, exercised against a stub processor. This is
//      the part every pedal's host program list runs through, and the part
//      with the sharp edge: a DAW re-asserting the current program after a
//      session restore must not overwrite the user's own edits.
//
//   2. The real banks that ship. Param IDs can't be typo'd (the tables use
//      the ParamID constants, so a bad name wouldn't compile), but values
//      out of range, duplicate names and empty blurbs all can.

#include "TestHarness.h"
#include "Tests.h"

#include "fofo/Presets.h"

#include "sway/Source/presets/PresetBank.h"
#include "backporch/Source/presets/PresetBank.h"
#include "double/Source/presets/PresetBank.h"
#include "daydream/Source/presets/PresetBank.h"

#include <set>

namespace
{
    // The smallest processor that can own an APVTS: two percentage knobs and
    // a three-way mode, which is the shape every Series B pedal has.
    class StubProcessor : public juce::AudioProcessor
    {
    public:
        StubProcessor()
            : apvts (*this, nullptr, juce::Identifier ("STUB"), makeLayout())
        {
        }

        static juce::AudioProcessorValueTreeState::ParameterLayout makeLayout()
        {
            juce::AudioProcessorValueTreeState::ParameterLayout layout;
            layout.add (std::make_unique<juce::AudioParameterFloat>(
                juce::ParameterID { "alpha", 1 }, "Alpha",
                juce::NormalisableRange<float> (0.0f, 100.0f, 0.01f), 50.0f));
            layout.add (std::make_unique<juce::AudioParameterFloat>(
                juce::ParameterID { "beta", 1 }, "Beta",
                juce::NormalisableRange<float> (0.0f, 100.0f, 0.01f), 50.0f));
            layout.add (std::make_unique<juce::AudioParameterChoice>(
                juce::ParameterID { "mode", 1 }, "Mode",
                juce::StringArray { "One", "Two", "Three" }, 0));
            return layout;
        }

        float valueOf (const char* id) const
        {
            auto* p = apvts.getParameter (id);
            return p != nullptr ? p->convertFrom0to1 (p->getValue()) : -1.0f;
        }

        juce::AudioProcessorValueTreeState apvts;

        // Boilerplate the base class demands and these tests never touch.
        const juce::String getName() const override      { return "STUB"; }
        void prepareToPlay (double, int) override        {}
        void releaseResources() override                 {}
        void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override {}
        juce::AudioProcessorEditor* createEditor() override { return nullptr; }
        bool hasEditor() const override                  { return false; }
        bool acceptsMidi() const override                { return false; }
        bool producesMidi() const override               { return false; }
        bool isMidiEffect() const override               { return false; }
        double getTailLengthSeconds() const override     { return 0.0; }
        int getNumPrograms() override                    { return 1; }
        int getCurrentProgram() override                 { return 0; }
        void setCurrentProgram (int) override            {}
        const juce::String getProgramName (int) override { return {}; }
        void changeProgramName (int, const juce::String&) override {}
        void getStateInformation (juce::MemoryBlock&) override {}
        void setStateInformation (const void*, int) override {}
    };

    const std::vector<fofo::FactoryPreset>& stubBank()
    {
        static const std::vector<fofo::FactoryPreset> bank = {
            { "First",  "the first one",  { { "alpha", 10.0f }, { "beta", 20.0f }, { "mode", 0.0f } } },
            { "Second", "the second one", { { "alpha", 30.0f }, { "beta", 40.0f }, { "mode", 1.0f } } },
            { "Third",  "the third one",  { { "alpha", 70.0f }, { "beta", 80.0f }, { "mode", 2.0f } } },
        };
        return bank;
    }

    // Every knob on the Series B pedals is a percentage; `mode` is a choice.
    void checkBank (const char* pedal,
                    const std::vector<fofo::FactoryPreset>& bank,
                    int modeChoiceCount)
    {
        const std::string who = pedal;

        t::ok (bank.size() >= 4, who + ": ships a usable number of presets",
               std::to_string (bank.size()) + " presets");

        std::set<std::string> names;
        bool allNamed = true, allBlurbed = true, allInRange = true, noDuplicates = true;
        std::string firstProblem;

        for (const auto& preset : bank)
        {
            if (preset.name.isEmpty())  allNamed   = false;
            if (preset.blurb.isEmpty()) allBlurbed = false;

            if (! names.insert (preset.name.toStdString()).second)
            {
                noDuplicates = false;
                if (firstProblem.empty()) firstProblem = "duplicate name " + preset.name.toStdString();
            }

            for (const auto& value : preset.values)
            {
                const bool isMode = juce::String (value.paramId) == "mode";
                const float top   = isMode ? (float) (modeChoiceCount - 1) : 100.0f;

                if (! std::isfinite (value.value) || value.value < 0.0f || value.value > top)
                {
                    allInRange = false;
                    if (firstProblem.empty())
                        firstProblem = preset.name.toStdString() + "." + value.paramId
                                     + " = " + std::to_string (value.value);
                }
            }
        }

        t::ok (allNamed,     who + ": every preset has a name");
        t::ok (noDuplicates, who + ": preset names are unique", firstProblem);
        t::ok (allBlurbed,   who + ": every preset has a blurb");
        t::ok (allInRange,   who + ": every value is inside its parameter's range", firstProblem);
    }

    // Each pedal reads its parameter defaults straight out of the preset the
    // bank nominates (see defaultFor() in the processors), so the two can no
    // longer disagree. What can still go wrong is the nominated preset not
    // naming every parameter, which would silently default it to zero.
    void checkDefaultPresetIsComplete (const char* pedal,
                                       const std::vector<fofo::FactoryPreset>& bank,
                                       int defaultIndex,
                                       size_t parameterCount)
    {
        const std::string who = pedal;

        if (defaultIndex < 0 || defaultIndex >= (int) bank.size())
        {
            t::ok (false, who + ": the nominated default preset exists",
                   "index " + std::to_string (defaultIndex));
            return;
        }

        const auto& preset = bank[(size_t) defaultIndex];

        t::ok (preset.values.size() == parameterCount,
               who + ": \"" + preset.name.toStdString() + "\" sets every parameter the pedal has",
               std::to_string (preset.values.size()) + " of " + std::to_string (parameterCount));
    }
}

void runPresetTests()
{
    t::section ("Presets — shared bank machinery");

    {
        StubProcessor proc;
        fofo::FactoryPresetHost host (proc, proc.apvts, stubBank());

        t::ok (host.getNumPrograms() == 3, "the bank size is what the host reports");
        t::ok (host.getProgramName (1) == "Second", "program names come back by index");
        t::ok (host.getProgramName (99).isEmpty(), "an out-of-range index yields no name");

        host.setCurrentProgram (2);
        t::ok (host.getCurrentProgram() == 2, "selecting a program sticks");
        t::ok (std::abs (proc.valueOf ("alpha") - 70.0f) < 0.01f, "a float parameter takes the preset's value",
               "alpha = " + std::to_string (proc.valueOf ("alpha")));
        t::ok (std::abs (proc.valueOf ("mode") - 2.0f) < 0.01f, "a choice parameter takes the preset's index",
               "mode = " + std::to_string (proc.valueOf ("mode")));

        // The sharp edge: a host re-asserting the program we are already on
        // must not throw away edits the user made after loading it.
        proc.apvts.getParameter ("alpha")->setValueNotifyingHost (
            proc.apvts.getParameter ("alpha")->convertTo0to1 (5.0f));
        host.setCurrentProgram (2);
        t::ok (std::abs (proc.valueOf ("alpha") - 5.0f) < 0.01f,
               "re-selecting the current program leaves the user's edits alone",
               "alpha = " + std::to_string (proc.valueOf ("alpha")));

        // ...but an explicit reload is still allowed to reset it.
        host.setCurrentProgram (2, true);
        t::ok (std::abs (proc.valueOf ("alpha") - 70.0f) < 0.01f,
               "a forced reload does restore the preset");
    }

    {
        StubProcessor proc;
        fofo::FactoryPresetHost host (proc, proc.apvts, stubBank());

        t::ok (host.getCurrentProgram() == 0, "a new host starts on the default program");
        t::ok (! host.isModified(), "a freshly constructed host is not modified");

        host.setCurrentProgram (1);
        t::ok (! host.isModified(), "a freshly loaded preset is not modified");

        proc.apvts.getParameter ("beta")->setValueNotifyingHost (
            proc.apvts.getParameter ("beta")->convertTo0to1 (99.0f));
        t::ok (host.isModified(), "moving a knob marks the preset as modified");

        host.setCurrentProgram (2);
        t::ok (! host.isModified(), "loading the next preset clears the modified mark");
    }

    {
        StubProcessor proc;
        fofo::FactoryPresetHost host (proc, proc.apvts, stubBank());

        host.setCurrentProgram (0);
        host.step (-1);
        t::ok (host.getCurrentProgram() == 2, "stepping back from the first preset wraps to the last");

        host.step (1);
        t::ok (host.getCurrentProgram() == 0, "stepping on from the last preset wraps to the first");

        host.step (1);
        t::ok (host.getCurrentProgram() == 1, "stepping forward advances by one");
    }

    {
        // A reopened session has to come back showing the preset it was left
        // on, which rides along in the APVTS state rather than being guessed.
        StubProcessor proc;
        fofo::FactoryPresetHost host (proc, proc.apvts, stubBank());
        host.setCurrentProgram (2);

        const auto saved = proc.apvts.copyState();

        StubProcessor reopened;
        fofo::FactoryPresetHost reopenedHost (reopened, reopened.apvts, stubBank());
        reopened.apvts.replaceState (saved);
        reopenedHost.restoreIndexFromState();

        t::ok (reopenedHost.getCurrentProgram() == 2,
               "the loaded preset survives a state save/restore round trip");
        t::ok (! reopenedHost.isModified(),
               "a restored session does not open up already marked as edited");
    }

    t::section ("Presets — the banks that ship");

    checkBank ("SWAY",      sway::getFactoryPresets(),     3);
    checkBank ("BACKPORCH", bkpr::getFactoryPresets(),     3);
    checkBank ("DOUBLE",    dbl::getFactoryPresets(),      3);
    checkBank ("DAYDREAM",  daydream::getFactoryPresets(), 1);

    // Every preset should set every knob, or stepping onto it leaves whatever
    // the previous one did to the parameters it forgot. The default preset is
    // the one that matters most, because the pedal's own defaults come from it.
    checkDefaultPresetIsComplete ("SWAY",      sway::getFactoryPresets(),     sway::kDefaultPresetIndex,     5);
    checkDefaultPresetIsComplete ("BACKPORCH", bkpr::getFactoryPresets(),     bkpr::kDefaultPresetIndex,     5);
    checkDefaultPresetIsComplete ("DOUBLE",    dbl::getFactoryPresets(),      dbl::kDefaultPresetIndex,      5);
    checkDefaultPresetIsComplete ("DAYDREAM",  daydream::getFactoryPresets(), daydream::kDefaultPresetIndex, 1);

    // DAYDREAM is one knob, so its presets are only meaningful if they are
    // actually spread along it rather than clustered in one zone.
    {
        const auto& bank = daydream::getFactoryPresets();
        float lowest = 1000.0f, highest = -1000.0f;
        bool ascending = true;
        float previous = -1.0f;

        for (const auto& preset : bank)
            for (const auto& value : preset.values)
            {
                lowest  = juce::jmin (lowest,  value.value);
                highest = juce::jmax (highest, value.value);
                if (value.value <= previous) ascending = false;
                previous = value.value;
            }

        t::ok (lowest < 15.0f && highest > 85.0f,
               "DAYDREAM: the presets span the whole knob",
               std::to_string (lowest) + " .. " + std::to_string (highest));
        t::ok (ascending, "DAYDREAM: the presets are ordered along the knob");
    }
}
