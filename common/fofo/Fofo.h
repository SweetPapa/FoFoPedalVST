#pragma once

// FoFoDriver — umbrella header.
//
//   Spec.h        the shared process spec and the one control rate
//   Filters.h     TPT/ZDF state variable filter, one-pole, DC blocker
//   Delay.h       cubic-Hermite fractional delay, latency alignment delay
//   Mod.h         modulation sources, sparse routing matrix, destinations
//   Node.h        Node / Chain / Parallel / MixRule
//   Oversampler.h latency-reporting oversampled shaper node
//
// See Spec.h for why the kernel is shaped this way.

#include "Spec.h"
#include "Filters.h"
#include "Delay.h"
#include "Mod.h"
#include "Node.h"
#include "Oversampler.h"
