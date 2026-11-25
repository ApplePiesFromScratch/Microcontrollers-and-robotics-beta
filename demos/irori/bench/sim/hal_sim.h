/* hal_sim.h  a bench that answers like a board, the same on the desk and on the emulated chip: two analog triangles on GPIO 0
   and 1 (a stick's two axes, moving), BOOT on GPIO 9 pressed now and then, three I2C devices, a clock that reads 33 ms more each
   latch, and a line arriving on the serial port. Deterministic: nothing here depends on the host. */
static uint32_t sim_latch;
static uint8_t sim_out[22], sim_mode[22]; static int16_t sim_pwm[22];
static uint8_t sim_store[4096];
static char sim_serial_log[512]; static int sim_log_n;
static const char *SIM_RX = "hello irori\n"; static int sim_rx_at;
static int tri(uint32_t k, uint32_t period) { uint32_t p = k % period, h = period / 2; return (int)((p < h ? p : period - p) * 4095 / h); }
static int  hal_pin_read(int pin) { if (pin < 0 || pin > 21) return 0; if (sim_mode[pin] == 1) return sim_out[pin]; if (pin == 9) return (sim_latch / 90) % 5 == 0 ? 0 : 1; return 1; }
static void hal_pin_write(int pin, int v) { if (pin >= 0 && pin <= 21) sim_out[pin] = (uint8_t)(v != 0); }
static void hal_pin_mode(int pin, int mode) { if (pin >= 0 && pin <= 21) sim_mode[pin] = (uint8_t)mode; }
static int  hal_adc(int pin) { if (pin == 0) return 2048 + (tri(sim_latch, 400) - 2048) * 3 / 4; if (pin == 1) return tri(sim_latch + 100, 640); return 1800 + (int)(pin * 37); }
static void hal_pwm(int pin, int permille) { if (pin >= 0 && pin <= 21) sim_pwm[pin] = (int16_t)permille; }
static void hal_servo(int pin, int degrees) { if (pin >= 0 && pin <= 21) sim_pwm[pin] = (int16_t)(500 + degrees * 2000 / 180); }
static int  hal_i2c_probe(int addr) { return addr == 0x3C || addr == 0x76 || addr == 0x68; }
static int  hal_serial_read(void) { if (sim_latch >= 1400 && SIM_RX[sim_rx_at]) return (uint8_t)SIM_RX[sim_rx_at++]; return -1; }
static void hal_serial_write(const char *s) { while (*s && sim_log_n < 511) sim_serial_log[sim_log_n++] = *s++; sim_serial_log[sim_log_n] = 0; }
static uint32_t hal_clock_ms(void) { return sim_latch * 33u; }
static uint8_t hal_store_get(int addr) { return (addr >= 0 && addr < 4096) ? sim_store[addr] : 0; }
static void hal_store_put(int addr, uint8_t v) { if (addr >= 0 && addr < 4096) sim_store[addr] = v; }
static void hal_store_commit(void) {}
static int  hal_cardkb(void) { return -1; }
static void hal_panel_mode(int invert, int rot180, int bgr) { (void)invert; (void)rot180; (void)bgr; }
static uint32_t hal_free_memory(void) { return 123456u; }
static uint8_t sim_down[4], sim_taps[4]; static int sim_stick_on, sim_sx = 2048, sim_sy = 2048, sim_trouble, sim_inputs_set, sim_palette_set;
static void hal_buttons(uint8_t down[4], uint8_t presses[4]) { for (int b = 0; b < 4; b++) { down[b] = sim_down[b]; presses[b] = sim_taps[b]; sim_taps[b] = 0; } }
static int  hal_stick(int *x, int *y) { if (!sim_stick_on) return 0; *x = sim_sx; *y = sim_sy; return 1; }   /* no stick unless a test fits one: the bench's GPIO 0/1 waves are for the plotter */
static void hal_inputs(const controls_t *c) { (void)c; sim_inputs_set++; }
static void hal_palette(void) { sim_palette_set++; }
static int  hal_boot_trouble(void) { return sim_trouble + (sim_down[0] || sim_down[1] ? 100 : 0); }
