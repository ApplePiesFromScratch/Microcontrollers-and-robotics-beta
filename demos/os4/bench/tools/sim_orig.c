/* sim.c  runs a real ATmega328P image in upstream simavr and decodes what goes down the SPI wire to an SH1106 (128x64, 132-column RAM):
   page commands 0xB0+p, column nibbles 0x0X / 0x1X, data bytes into RAM. A frame is emitted at each page-0 command after page 7 was written.
   usage: sim image.elf frames out.bin [colofs]     writes frames as 1024 bytes each (page-major, 128 columns from colofs), and cycles to out.bin.cyc
   ADC inputs held at mid rail (2.5 V); buttons left to their pull-ups. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "sim_avr.h"
#include "sim_elf.h"
#include "avr_spi.h"
#include "avr_ioport.h"
#include "avr_adc.h"
static uint8_t ram[8][132], dc, page, col; static int wrote7, frames, want; static FILE *fo, *fc; static int colofs = 2; static avr_t *avr;
static uint64_t last_cyc;
static void emit(void) { for (int p = 0; p < 8; p++) fwrite(&ram[p][colofs], 1, 128, fo); fprintf(fc, "%llu\n", (unsigned long long)(avr->cycle - last_cyc)); last_cyc = avr->cycle; frames++; wrote7 = 0; }
static void spi_hook(struct avr_irq_t *irq, uint32_t v, void *p) { (void)irq; (void)p; uint8_t b = (uint8_t)v;
  if (!dc) { if ((b & 0xF8) == 0xB0) { if ((b & 7) == 0 && wrote7) emit(); page = b & 7; } else if (b < 0x10) col = (col & 0xF0) | b; else if (b < 0x20) col = (col & 0x0F) | ((b & 0x0F) << 4); }
  else { if (col < 132) ram[page][col] = b; col++; if (page == 7) wrote7 = 1; } }
static void dc_hook(struct avr_irq_t *irq, uint32_t v, void *p) { (void)irq; (void)p; dc = (uint8_t)v; }
int main(int argc, char **argv) {
  elf_firmware_t f; memset(&f, 0, sizeof f);
  if (elf_read_firmware(argv[1], &f)) { fprintf(stderr, "elf?\n"); return 1; }
  want = atoi(argv[2]); fo = fopen(argv[3], "wb"); char cp[512]; snprintf(cp, sizeof cp, "%s.cyc", argv[3]); fc = fopen(cp, "w"); if (argc > 4) colofs = atoi(argv[4]);
  avr = avr_make_mcu_by_name("atmega328p"); avr_init(avr); avr_load_firmware(avr, &f); avr->frequency = 16000000; avr->vcc = avr->avcc = avr->aref = 5000;
  avr_irq_register_notify(avr_io_getirq(avr, AVR_IOCTL_SPI_GETIRQ(0), SPI_IRQ_OUTPUT), spi_hook, 0);
  avr_irq_register_notify(avr_io_getirq(avr, AVR_IOCTL_IOPORT_GETIRQ('B'), 1), dc_hook, 0);
  for (int c = 0; c < 6; c++) avr_raise_irq(avr_io_getirq(avr, AVR_IOCTL_ADC_GETIRQ, ADC_IRQ_ADC0 + c), 2500);
  uint64_t limit = (uint64_t)want * 16000000ull;   /* at most a second of chip time a frame */
  while (frames < want && avr->cycle < limit) { int st = avr_run(avr); if (st == cpu_Done || st == cpu_Crashed) { fprintf(stderr, "cpu stopped %d\n", st); break; } }
  fclose(fo); fclose(fc); fprintf(stderr, "frames %d, chip time %.1f s\n", frames, avr->cycle / 16e6); return 0; }
