// The self-test: every feature driven without a window ("nachtzug --selftest"; see selftest.cpp).
#pragma once

int selfTest(); // 0 when everything works, 1 when something failed (each failure is printed)
