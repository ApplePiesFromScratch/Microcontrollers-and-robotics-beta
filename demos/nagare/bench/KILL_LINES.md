# NAGARE kill lines (written and hashed before any run)

The claim: on one ESP32-C3 screen, the same heated box is run two ways side by side.
LEFT, the textbook: continuity, Navier-Stokes and the energy equation, discretised with a time step (the stable-fluids method:
semi-Lagrangian advection, pressure projection, Boussinesq lift), in fixed point.
RIGHT, ours: no equation imported. Places hold stuff, carried rate and warmth; every change is a hand-on between neighbours
(MOVE, PUSH, DRAG, LIFT) or a booked feed/vent. Mass, momentum and warmth are kept by construction and shown to the unit.

Fails, reported as failed, if:
N1  Ours: any book opens on any latch over 20,000 latches (stuff = start; warmth = start + fed - vented;
    each momentum component = start + given by the walls).
N2  Ours: no plume. With the heater on, the warmest tenth of the box has its centre above the box's middle by 2,000 latches.
N3  Ours: no turning. By 3,000 latches the turning reading (circulation round 4x4 loops) holds both signs, each at more than
    5% of places, with the heater still.
N4  Control: one line that SETS a quantity instead of moving it (a place's warmth written directly) must open the books.
N5  Textbook: its continuity residual and its heat-book gap are measured and printed, not judged (it is the reference method).
N6  Desk != emulated C3 on any printed line.
N7  The emulated C3 needs more than 66 ms for one frame (one step of each side and the drawing).
