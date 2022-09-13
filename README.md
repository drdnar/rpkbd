# rpkbd
This is simple keyboard firmware for RP2040-based devices.

### Rational

The old controller board for my Unicomp Model M was getting flaky, so I removed the old microcontroller and soldered some wires to bridge the board to a new controller.
I used a [PGA2040 board from Pimoroni](https://shop.pimoroni.com/products/pga2040) because it exposes every single GPIO.
The Model M uses matrix with 16 columns and 8 rows, so it needs 24 pins to drive the matrix, plus three more for LEDs.
I wrote my own firmware because it's more instructive to do it myself.

### Design

The USB side is based on the Tiny USB example from the Pico SDK.

The matrix scan code scans columns by asserting each column one-by-one and then looking for output on rows.
When no keys are being pressed, all columns are asserted together.
This means that the moment any single key is pressed, one or more rows will go high.
This triggers a scan cycle to begin, and scanning continues for as long as any keys are pressed.
When no keys are being pressed, no scanning is done.

Because capacitive effects limit scanning speed, the scan cycle is interrupt-driven so the CPU need not busy-wait.
The interrupt handler produces a suitable delay between column scans, and caches the data in RAM.
A separate routine processes raw scan data.

Key ghosting is actively detected by using the tri-state capabilities of the GPIO array:
any time key ghosting is happening, the active column must be shorted to one or more other columns.
By placing all other columns in input mode, they can be checked for this effect.
To prevent key ghosting from being reported, the last ghost-free data for each column is cached.
When ghosting is detected in a column, the ghost-free cache is not updated, so the keyboard does not report any potentional phantom key presses.
