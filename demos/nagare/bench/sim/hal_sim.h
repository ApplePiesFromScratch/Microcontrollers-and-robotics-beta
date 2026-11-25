/* hal_sim.h  the bench: buttons and stick as a test or the desk sets them; no clock */
static uint8_t sim_down[3]; static int sim_stick_on, sim_sx = 2048, sim_sy = 2048, sim_press;
static void hal_buttons(uint8_t down[3]) { for (int k = 0; k < 3; k++) down[k] = sim_down[k]; }
static int  hal_stick(int *x, int *y, int *press) { if (!sim_stick_on) return 0; *x = sim_sx; *y = sim_sy; *press = sim_press; return 1; }
static uint32_t hal_cycles(void) { return 0; }
