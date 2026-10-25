// Implementacao minima (PC) das funcoes da libnds usadas pelo BlocoDS, para gerar prints
#pragma once
#include <stdint.h>
typedef uint8_t u8; typedef uint16_t u16; typedef uint32_t u32; typedef int16_t s16; typedef int16_t v16; typedef int32_t int32;
#define RGB15(r,g,b) ((u16)((r)|((g)<<5)|((b)<<10)))
#define POLY_ALPHA(n) ((u32)(n)<<16)
#define POLY_CULL_NONE (3<<6)
#define POLY_ID(n) ((u32)(n)<<24)
enum { GL_PROJECTION=0, GL_POSITION=1, GL_MODELVIEW=2, GL_QUADS=1, GL_ANTIALIAS=16 };
#define GL_MAX_DEPTH 0x7FFF
#define MODE_0_3D 0x10100
#define floattov16(n) ((v16)((n)*4096))
enum {KEY_A=1,KEY_B=2,KEY_SELECT=4,KEY_START=8,KEY_RIGHT=16,KEY_LEFT=32,KEY_UP=64,KEY_DOWN=128,KEY_R=256,KEY_L=512,KEY_X=1024,KEY_Y=2048,KEY_TOUCH=4096};
typedef struct {u16 rawx,rawy,px,py,z1,z2;} touchPosition;
void videoSetMode(u32); void consoleDemoInit(void); void consoleClear(void); int glInit(void); void glEnable(int);
void glClearColor(u8,u8,u8,u8); void glClearPolyID(u8); void glClearDepth(u16); void glViewport(u8,u8,u8,u8);
void glMatrixMode(int); void glLoadIdentity(void); void gluPerspective(float,float,float,float);
void gluLookAtf32(int32,int32,int32,int32,int32,int32,int32,int32,int32); void glScalef32(int32,int32,int32); void glPolyFmt(u32);
void glBegin(int); void glEnd(void); void glColor(u16); void glVertex3v16(v16,v16,v16); void glFlush(u32);
s16 sinLerp(s16); s16 cosLerp(s16); void scanKeys(void); u32 keysHeld(void); u32 keysDown(void);
void touchRead(touchPosition*); void swiWaitForVBlank(void); int iprintf(const char*,...);
// motor 2D secundario (tela de baixo)
extern u32 h_dispcntSub; extern u16 h_bgcntSub[4];
extern u16 h_subPal[512]; extern u16 h_subOam[512];
extern u16 h_subBg[0x20000/2]; extern u16 h_subObj[0x20000/2];
#define REG_DISPCNT_SUB h_dispcntSub
#define REG_BG0CNT_SUB h_bgcntSub[0]
#define REG_BG1CNT_SUB h_bgcntSub[1]
#define BG_PALETTE_SUB (h_subPal)
#define SPRITE_PALETTE_SUB (h_subPal + 256)
#define OAM_SUB (h_subOam)
#define BG_GFX_SUB (h_subBg)
#define SPRITE_GFX_SUB (h_subObj)
#define MODE_0_2D 0x10000
#define DISPLAY_BG0_ACTIVE (1<<8)
#define DISPLAY_BG1_ACTIVE (1<<9)
#define DISPLAY_SPR_ACTIVE (1<<12)
#define DISPLAY_SPR_1D (1<<4)
#define VRAM_C_SUB_BG 4
#define VRAM_D_SUB_SPRITE 4
void videoSetModeSub(u32); void vramSetBankC(int); void vramSetBankD(int);
