#pragma once
// Ctrl+C (and closing the console window on Windows) while a script has swapped sources (spec 031, review S6): the
// handler only sets a flag; the session loop sees it, ends the script and puts the originals back on the main thread
// (finish_reload). On Windows a console close waits up to 4.5 s for that before the system ends the process.

namespace nnh::tool::reload {

/// From now on Ctrl+C / console close set the flag instead of ending nnh.
void install_interrupt();
/// The default behaviour again (after the swaps were put back).
void uninstall_interrupt();
/// True once Ctrl+C or a console close arrived.
bool interrupt_requested();
/// The main thread put everything back: a waiting console-close handler may return.
void interrupt_handled();
/// What the handler does (tests).
void simulate_interrupt();

} // namespace nnh::tool::reload
