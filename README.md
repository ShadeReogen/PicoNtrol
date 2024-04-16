
# NES Developement branch
## Overview
In the end, the NES is probably not coming anytime soon.
Why?
I can't for the love of me understand what's going on in Bluepads code.
The NES sends a 12us long signal on the clock (or sometimes called "latch") line, after that the Data line is set high, and read 8 times using synchronized 6us pulses on the pulse line.
**(The NES uses active LOW logic. A pressed -> A = FALSE)**
Well, I just can't find a way to make the Bluepad library work with this curcially synced timeframes. Either the program crashes, I get a \*PANIC\* message or it freezes...
