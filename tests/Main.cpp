#include "TestHarness.h"
#include "Tests.h"

int main()
{
    std::printf ("\n\033[1mFoFoDriver test suite\033[0m\n");
    std::printf ("\033[2m48 kHz, 512-sample blocks\033[0m\n");

    runKernelTests();
    runPedalRegressionTests();
    runSwayTests();
    runBackporchTests();
    runDoubleTests();
    runDaydreamTests();
    runFofopedalTests();
    runPresetTests();

    return t::report();
}
