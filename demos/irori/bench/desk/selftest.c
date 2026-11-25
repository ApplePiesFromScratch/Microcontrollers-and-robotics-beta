/* selftest.c  checks that can fail, each with its fault planted */
#include <stdio.h>
#include <string.h>
#include "../core/irori.h"
#include "../sim/hal_sim.h"
static int fails;
static void check(int ok, const char *what) { printf("  [%s] %s\n", ok ? "PASS" : "FAIL", what); if (!ok) fails++; }
static void run(int n) { for (int i = 0; i < n; i++) { sim_latch++; os_latch(); } }
static void press(int b, int n) { sim_down[b] = 1; sim_taps[b] = 1; run(n); sim_down[b] = 0; run(1); }      /* a press seen for n latches, then let go */
static void tap_between(int b) { sim_taps[b] = 1; run(1); }                                                 /* a press that began and ended between two latches */
static void fresh(void) { memset(sim_store, 0, sizeof sim_store); memset(sim_down, 0, 4); safe_mode = 0; os_begin(); ST.demo_on = 0; ghost_on = 0; fg = -1; kb_on = 0; ctl_trial_ms = 0; run(1); }
int main(void) {
  os_begin(); ST.demo_on = 0; ghost_on = 0;
  /* 1. the keeper: spoil one byte of copy A; within 64 latches it is mended from B */
  check(copy_ok(0) && copy_ok(1), "both copies good after the first boot");
  sim_store[100] ^= 0x5A; check(!copy_ok(0), "planted: a byte of copy A turned over is seen as bad");
  uint32_t m0 = mends; run(64); check(copy_ok(0) && mends == m0 + 1, "the keeper mended copy A from copy B");
  /* 2. both copies spoiled: the next boot starts from the factory store, not from garbage */
  sim_store[100] ^= 0x5A; sim_store[100 + sizeof(store_t)] ^= 0x5A; s_cpy(ST.notes[3], "garbage must not survive", 160);
  store_begin(); check(s_len(ST.notes[3]) == 0 && copy_ok(0) && copy_ok(1), "both copies bad: factory store, rewritten whole");
  /* 3. a note kept across a power cycle */
  s_cpy(ST.notes[2], "kept across power-off", 160); os_save(); memset(&ST, 0, sizeof ST); store_begin();
  check(!strcmp(ST.notes[2], "kept across power-off"), "a saved note survives a power cycle (store read back from flash)");
  /* 4. serial data while the user is in PINS: it goes to the terminal, it presses nothing */
  open_app(0); f_focus[1] = 4; uint8_t mode_before = ST.pin_mode[4]; sim_latch = 1399; run(30);
  check(ST.pin_mode[4] == mode_before, "a line arriving on serial in PINS presses no button (pin mode unchanged)");
  check(!strncmp(term[(term_n - 1 + TL) % TL], "hello irori", 11) || !strncmp(term[term_n % TL], "hello irori", 11), "the line is in the terminal's log");
  /* planted: the same line in keyboard-only mode is keys */
  controls_preset(&ST.ctl, 3); sim_rx_at = 0; eq_in = eq_out = 0; uint8_t pm[8]; memcpy(pm, ST.pin_mode, 8); run(30);
  check(eq_in >= 12 && memcmp(pm, ST.pin_mode, 8) != 0, "planted: in keyboard-only mode the same line is keys (12 events pushed, a pin mode changed)");
  controls_preset(&ST.ctl, 0); memcpy(ST.pin_mode, pm, 8);
  /* 5. a profile starts its apps and its rules */
  profile_start(1); check(running == ST.prof[1].apps && ST.rules[0].on && ST.rules[1].on && !ST.rules[2].on, "PLANT CARE starts its apps and rules 1-2");
  profile_start(0); check(running == ST.prof[0].apps && ST.rules[2].on && !ST.rules[0].on, "BENCH starts its apps and rule 3");
  /* 6. a reading's rate against a reading that did not move is theta, not a number */
  exact_t r = rate_of(5, 0); check(r.bad == 1, "rate(A, B) with B unchanged is THETA");
  r = rate_of(10, 8); check(!r.bad && r.n == 5 && r.d == 4, "rate(A, B) = 10/8 is kept exact as 5/4");
  exact_t big = ex_op(ex_make(EXACT_MAX, 1), ex_make(EXACT_MAX, 1), 'x'); check(big.bad == 2, "planted: a product too big to keep exact is WIDE, not wrapped");
  /* 7. rule 3 (every 2 s, flip GPIO 8) fires by the clock's change: 33 ms a latch, so 2 s is 61 latches */
  uint32_t f0 = rule_fired[2]; run(61 * 4); check(rule_fired[2] - f0 == 4, "rule 3 fires 4 times in 4 x 61 latches (the clock read as its change)");
  /* 8. no empty frame: the demo hand's whole tour, a frame whose working area has two colours or fewer is empty (opening an app showed one) */
  { store_factory(); os_save(); os_begin(); ghost_on = 1; g_at = 0; g_wait = 0; int empty = 0;
    for (uint32_t k = 0; k < 1400; k++) { sim_latch++; os_latch(); uint8_t seen[256] = { 0 }; int nc = 0;
      for (int y = 30; y < SH - 30; y++) for (int x = 0; x < SW; x++) if (!seen[FB[y * SW + x]]) { seen[FB[y * SW + x]] = 1; nc++; }
      if (nc <= 2) empty++; }
    check(empty == 0, "the demo tour shows no empty frame (an app opened is drawn in the same latch)");
    memset(FB + 30 * SW, 0, (SH - 60) * SW); uint8_t seen[256] = { 0 }; int nc = 0;
    for (int y = 30; y < SH - 30; y++) for (int x = 0; x < SW; x++) if (!seen[FB[y * SW + x]]) { seen[FB[y * SW + x]] = 1; nc++; }
    check(nc <= 2, "planted: a cleared working area is counted as empty"); }
  /* 9. the hands: a tap shorter than a latch, holds, repeats, and NEXT reaching everything */
  fresh(); controls_preset(&ST.ctl, 0); hands_reset(); run(2);
  fg = -1; home_focus = 2; tap_between(0); check(fg == 2, "a tap of A that began and ended between two latches still opens the app (presses are counted, not only seen)");
  fg = -1; home_focus = 2; run(3); check(fg == -1, "planted: no press counted and none seen: nothing opens");
  open_app(3); rule_edit = 0; run(1); press(1, 5); check(fg == 3 && rule_edit == -1, "a short press of B is BACK: the rule editor returns to the list");
  rule_edit = 0; run(1); sim_down[1] = 1; sim_taps[1] = 1; run(15); char hb[56]; int pr = hold_progress(hb, 56); run(10); sim_down[1] = 0; run(2);
  check(pr > 0 && pr < 100 && fg == -1, "holding B shows its progress, then goes HOME from inside an app");
  controls_preset(&ST.ctl, 1); hands_reset(); run(2); fg = -1; home_focus = 0;
  press(0, 2); int h1 = home_focus; press(1, 2); int opened = fg; int r0 = f_focus[fg + 1]; press(0, 2); int r1 = f_focus[fg + 1];
  check(h1 == 1 && opened == 1 && r1 != r0, "BUTTONS A B C: A (NEXT) moves right at home and down in an app; B selects on release");
  fg = -1; home_focus = 0; sim_down[0] = 1; sim_taps[0] = 1; run(30); sim_down[0] = 0; run(2);
  check(home_focus >= 4, "holding A (NEXT) repeats: 30 latches (990 ms) move the focus 4 or more times");
  fg = -1; home_focus = 0; os_input(1); os_push(IE_DOWN, 0); run(1); check(home_focus == 4, "planted: a plain DOWN at home moves a row (4 tiles), unlike NEXT");
  controls_preset(&ST.ctl, 2); hands_reset(); run(2); fg = -1; home_focus = 0;
  press(2, 3); int c1 = home_focus; press(2, 30); int c2 = fg; check(c1 == 1 && c2 == 1, "ONE BUTTON: a short C is NEXT, a 1 s hold of C (on release) is SELECT");
  sim_down[2] = 1; sim_taps[2] = 1; run(60); int c3 = fg; sim_down[2] = 0; run(2); check(c3 == -1 && fg == -1, "ONE BUTTON: a 2 s hold of C is BACK, reached while still held");
  { controls_preset(&ST.ctl, 1); hands_reset(); run(2); open_app(6); calc_r = 0; calc_c = 0; int seen[20] = { 0 }, nd = 0;
    for (int k = 0; k < 20; k++) { press(0, 1); int i = calc_r * 4 + calc_c; if (!seen[i]) { seen[i] = 1; nd++; } }
    check(nd == 20, "buttons only: NEXT reaches all 20 calculator keys");
    int seen2[20] = { 0 }, nd2 = 0; calc_r = 0; calc_c = 0; for (int k = 0; k < 20; k++) { os_input(1); os_push(IE_DOWN, 0); run(1); int i = calc_r * 4 + calc_c; if (!seen2[i]) { seen2[i] = 1; nd2++; } }
    check(nd2 == 5, "planted: NEXT read as DOWN (IRORI 1) reaches only the 5 keys of one column"); }
  { char buf[40] = ""; fg = -1; kb_open(buf, 40, "TEST"); kb_row = 0; kb_col = 0; int seen[50] = { 0 }, nd = 0;
    for (int k = 0; k < 44; k++) { press(0, 1); int i = kb_row * 10 + kb_col; if (!seen[i]) { seen[i] = 1; nd++; } }
    int seen2[50] = { 0 }, nd2 = 0; kb_row = 0; kb_col = 0; for (int k = 0; k < 44; k++) { os_input(1); os_push(IE_RIGHT, 0); run(1); int i = kb_row * 10 + kb_col; if (!seen2[i]) { seen2[i] = 1; nd2++; } }
    printf("    keyboard keys reached: %d by NEXT, %d by RIGHT\n", nd, nd2); kb_on = 0; check(nd == 44 && nd2 == 10, "buttons only: NEXT reaches all 44 keyboard keys (planted: RIGHT stays in one row, 10)"); }
  { open_app(10); f_focus[11] = 0; run(1); int bad = 0; for (int k = 0; k < 40; k++) { press(0, 1); if (!(f_can >> f_focus[11] & 1)) bad++; }
    check(bad == 0 && f_can != (((uint64_t)1 << f_n) - 1), "the focus never rests on an info row (and TASKS does have info rows to pass over)"); }
  /* 10. new controls are tried before they are kept */
  fresh(); controls_preset(&ST.ctl, 0); hands_reset(); run(2);
  ctl_edit = ST.ctl; controls_preset(&ctl_edit, 1); ctl_old = ST.ctl; controls_use(&ctl_edit); ctl_trial_ms = 15000;
  press(0, 3); run(470); check(ctl_trial_ms == 0 && ST.ctl.preset == 0, "a trial not confirmed with the new SELECT puts the old controls back after 15 s (A was the old SELECT)");
  ctl_old = ST.ctl; controls_use(&ctl_edit); ctl_trial_ms = 15000; press(1, 3); run(470);
  check(ST.ctl.preset == 1, "the same trial confirmed with B (the new SELECT) is kept");
  controls_preset(&ctl_edit, 0); sim_down[0] = 1; sim_taps[0] = 1; run(2); ctl_old = ST.ctl; controls_use(&ctl_edit); ctl_trial_ms = 15000; run(5); sim_down[0] = 0; run(470);
  check(ST.ctl.preset == 1, "a button still held from before the change confirms nothing when let go");
  /* 11. changes save themselves, and readings that flip every latch do not wear the flash */
  fresh(); s_cpy(ST.notes[1], "saved by itself", 160); run(200); { store_t keep = ST; memset(&ST, 0, sizeof ST); store_begin(); check(!strcmp(ST.notes[1], "saved by itself"), "a changed note is saved by itself within 7 s of clock change"); ST = keep; }
  ST.rules[0].on = 1; run(300); uint32_t s0 = saves, fr0 = rule_fired[0]; run(1500);
  check(saves == s0 && rule_fired[0] > fr0, "a rule whose condition keeps flipping causes no saves (its reading is kept out of flash)");
  s_cpy(ST.sendbox, "a real change", 40); run(250); check(saves == s0 + 1, "planted: a real change in that same stretch is saved once");
  /* 12. IRORI 1's settings carried over */
  { memset(sim_store, 0, sizeof sim_store); store1_t o; memset(&o, 0, sizeof o); o.magic = STORE_MAGIC; o.version = 1; o.size = sizeof o; o.input_mode = 1; o.invert = 1;
    s_cpy(o.notes[0], "written by irori 1", 160); s_cpy(o.prof[2].name, "MY ROBOT", 12); o.sum = fnv((uint8_t *)&o, offsetof(store1_t, sum));
    for (int c = 0; c < 2; c++) for (size_t i = 0; i < sizeof o; i++) sim_store[c * sizeof o + i] = ((uint8_t *)&o)[i];
    carried = 0; store_begin(); check(carried && !strcmp(ST.notes[0], "written by irori 1") && !strcmp(ST.prof[2].name, "MY ROBOT") && ST.ctl.preset == 1 && copy_ok(0) && copy_ok(1),
      "IRORI 1's notes, profiles and controls are carried into IRORI 2 and kept twice");
    memset(sim_store, 0, sizeof sim_store); o.sum ^= 1; for (int c = 0; c < 2; c++) for (size_t i = 0; i < sizeof o; i++) sim_store[c * sizeof o + i] = ((uint8_t *)&o)[i];
    carried = 0; store_begin(); check(!carried && strcmp(ST.notes[0], "written by irori 1") != 0, "planted: an IRORI 1 copy with a wrong sum is not carried (factory instead)"); }
  /* 13. one owner a pin */
  fresh(); profile_start(0); ST.servo_pin = 8; ST.servo_mode = 0; open_app(2); fg = -1; uint8_t o8 = sim_out[8]; uint32_t f2 = rule_fired[2]; run(61 * 2);
  check(rule_fired[2] > f2 && sim_out[8] == o8 && !strcmp(pin_user(8, U_PINS), "SERVO"), "a pin the SERVO holds is not switched by rule 3 (it fires, but leaves the pin), and PINS says SERVO has it");
  stop_app(2); run(61 * 2); check(sim_out[8] != o8 || rule_fired[2] > f2 + 1, "planted: with the SERVO stopped, rule 3 switches the pin");
  /* 14. a safe start */
  memset(sim_store, 0, sizeof sim_store); os_begin(); ST.demo_on = 0; ghost_on = 0; check(!safe_mode, "an ordinary start is not a safe start");
  sim_down[1] = 1; controls_preset(&ST.ctl, 3); os_save(); os_begin(); sim_down[1] = 0; check(safe_mode && running == 0 && !ST.rules[2].on && ST.ctl.preset == 0, "B held at power-on: safe start (no apps, rules paused, controls back to STICK + A B C)");
  safe_mode = 0; sim_trouble = 3; os_begin(); sim_trouble = 0; check(safe_mode, "three troubled starts in a row: safe start"); safe_mode = 0;
  /* 15. all twelve running, the demo touring, every app opened */
  { fresh(); running = 0x0FFF; ghost_on = 1; g_at = 0; g_wait = 0; int empty = 0;
    for (uint32_t k = 0; k < 1500; k++) { sim_latch++; os_latch(); uint8_t seen[256] = { 0 }; int nc = 0;
      for (int y = 30; y < SH - 30; y += 2) for (int x = 0; x < SW; x += 2) if (!seen[FB[y * SW + x]]) { seen[FB[y * SW + x]] = 1; nc++; } if (nc <= 2) empty++; }
    ghost_on = 0; int all = 1; for (int a = 0; a < NAPP; a++) if (APPS[a].tick && !work_all[a]) all = 0;
    for (int a = 0; a < NAPP; a++) { fg = a; run(4); }
    for (int pg = 1; pg <= 5; pg++) { fg = 11; set_page = pg; run(4); } set_page = 0; bt_test = 0;
    check(empty == 0 && all && running == 0x0FFF, "all twelve running: the tour shows no empty frame, every background app worked, every app and Settings page drew"); }
  /* 16. the look */
  { fresh(); uint8_t bg0 = PALRGB[K_BG][0]; int p0 = sim_palette_set; ST.theme = 1; theme_apply(1); hal_palette(); check(PALRGB[K_BG][0] != bg0 && sim_palette_set == p0 + 1, "a theme changes the colours and tells the panel");
    theme_apply(0); ST.theme = 0; ST.big_text = 1; fg = 11; set_page = 1; ctl_edit = ST.ctl; controls_preset(&ST.ctl, 1); hands_reset(); run(2); int lost = 0;
    for (int k = 0; k < 60; k++) { press(0, 1); int v = (f_y1 - f_y0) / ROWH; if (f_focus[12] - f_scroll[12] < 0 || f_focus[12] - f_scroll[12] >= v) lost++; }
    check(lost == 0, "large text: walking all the CONTROLS rows, the focused row is always on screen");
    f_scroll[12] = 0; f_focus[12] = f_n - 1; check(f_focus[12] - f_scroll[12] >= (f_y1 - f_y0) / ROWH, "planted: the last row with the scroll at the top is off screen (the check can see it)");
    ST.big_text = 0; set_page = 0; }
  printf("%s: %d failed\n", fails ? "SELFTEST FAILED" : "SELFTEST PASSED", fails);
  return fails != 0; }
