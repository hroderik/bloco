// Emulacao minima da memoria do GBA para gerar prints no PC
#include <stdint.h>
extern uint16_t h_regs[0x200];
extern uint16_t h_pal[512];
extern uint16_t h_vram[0x18000 / 2];
extern uint16_t h_oam[512];
extern uint8_t  h_sram[0x10000];
#define REG_DISPCNT  h_regs[0x00]
#define REG_VCOUNT   h_regs[0x03]
#define REG_BG0CNT   h_regs[0x04]
#define REG_BG1CNT   h_regs[0x05]
#define REG_BG2CNT   h_regs[0x06]
#define REG_BG0HOFS  h_regs[0x08]
#define REG_BG0VOFS  h_regs[0x09]
#define REG_BG1HOFS  h_regs[0x0A]
#define REG_BG1VOFS  h_regs[0x0B]
#define REG_KEYINPUT h_regs[0x98]
#define PAL_BG    (h_pal)
#define PAL_OBJ   (h_pal + 256)
#define VRAM      (h_vram)
#define OBJ_TILES ((uint32_t *)((uint8_t *)h_vram + 0x10000))
#define OAM       (h_oam)
#define SRAM      (h_sram)
#define EWRAM_BSS
