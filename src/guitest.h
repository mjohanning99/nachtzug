// The window tests ("--guitest", "--check-tooltips"; see guitest.cpp).
#pragma once

class App;
class MainWindow;

void scheduleGuiTest(App& app, MainWindow& w);      // drives the window, then quits with 0 or 1
void scheduleTooltipCheck(App& app, MainWindow& w); // lists controls without a tooltip, then quits
