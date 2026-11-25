/* screens.c  screenshots of the screens the demo does not visit: Settings pages, the button test, a trial, themes, large text */
#include <stdio.h>
#include <string.h>
#include "../core/irori.h"
#include "../sim/hal_sim.h"
static void shot(const char *n) { char fn[64]; sprintf(fn, "shots/s_%s.ppm", n); FILE *f = fopen(fn, "wb"); fprintf(f, "P6\n%d %d\n255\n", SW, SH); for (int i = 0; i < SW * SH; i++) fwrite(PALRGB[FB[i]], 1, 3, f); fclose(f); }
static void run(int n) { for (int i = 0; i < n; i++) { sim_latch++; os_latch(); } }
static void open_(int a, int page) { os_input(1); fg = -1; kb_on = 0; home_focus = a; os_push(IE_OK, 0); run(1); if (a == 11) { set_page = page; if (page == 1) ctl_edit = ST.ctl; if (page == 2) bt_test = 1; } run(2); }
int main(void) {
  os_begin(); ST.demo_on = 0; ghost_on = 0; sim_stick_on = 1; run(200);
  open_(11, 0); shot("settings");
  open_(11, 1); shot("controls");
  f_focus[12] = 2; run(2); shot("controls_rows");
  open_(11, 2); sim_down[1] = 1; sim_taps[1] = 1; sim_sx = 3600; sim_sy = 1500; run(3); sim_down[1] = 0; run(1); sim_down[0] = 1; sim_taps[0] = 1; run(20); shot("test"); sim_down[0] = 0; sim_sx = 2048; sim_sy = 2048; run(2);
  set_page = 0; bt_test = 0; open_(11, 3); shot("look");
  open_(11, 4); shot("home_screen");
  open_(11, 5); shot("system");
  ctl_edit = ST.ctl; controls_preset(&ctl_edit, 1); ctl_old = ST.ctl; controls_use(&ctl_edit); ctl_trial_ms = 15000; run(120); shot("trial");
  ctl_trial_ms = 0; controls_use(&ctl_old); run(2);
  running = 0x0FFF; open_(10, 0); shot("tasks_all");
  fg = 3; rule_edit = -1; run(1); sim_down[1] = 1; sim_taps[1] = 1; run(14); shot("hold_b"); sim_down[1] = 0; run(2);
  for (int t = 1; t < NTHEME; t++) { ST.theme = (uint8_t)t; theme_apply(t); fg = -1; home_focus = 6; run(2); char n[16]; sprintf(n, "theme%d_home", t); shot(n); open_(0, 0); sprintf(n, "theme%d_pins", t); shot(n); }
  ST.theme = 0; theme_apply(0); ST.big_text = 1; open_(10, 0); shot("big_tasks"); open_(3, 0); shot("big_rules"); ST.big_text = 0;
  ST.home_hide = (1 << 4) | (1 << 5) | (1 << 7) | (1 << 9); fg = -1; run(2); shot("home_hidden"); ST.home_hide = 0;
  controls_preset(&ST.ctl, 1); hands_reset(); open_(6, 0); shot("buttons_calc"); controls_preset(&ST.ctl, 2); fg = -1; run(2); shot("onebutton_home");
  controls_preset(&ST.ctl, 0); { memset(sim_store, 0, sizeof sim_store); sim_down[0] = 1; os_begin(); sim_down[0] = 0; ST.demo_on = 0; ghost_on = 0; run(10); shot("safe"); }
  return 0; }
