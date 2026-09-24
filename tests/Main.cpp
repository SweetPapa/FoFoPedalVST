#include "TestHarness.h"
#include "Tests.h"
#include <cstdlib>

int main()
{
    std::printf ("\n\033[1mFoFoDriver test suite\033[0m\n");
    std::printf ("\033[2m48 kHz, 512-sample blocks\033[0m\n");

    // FOFO_IMPACT_ONLY=1 runs just the impact table — the fast loop while
    // voicing a pedal.
    if (std::getenv ("FOFO_IMPACT_ONLY") != nullptr)
    {
        runImpactTests();
        return t::report();
    }

    runKernelTests();
    runPedalRegressionTests();
    runSwayTests();
    runBackporchTests();
    runDoubleTests();
    runDaydreamTests();
    runFofopedalTests();
    runDreamRipperTests();
    runPresetTests();
    runImpactTests();

    return t::report();
}
