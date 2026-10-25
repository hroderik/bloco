#include "nds.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <math.h>

typedef struct { float m[16]; } M4;   // coluna principal: m[col*4+row]
static M4 proj, mv; static int mode;
static M4 mul(M4 a, M4 b){ M4 r; for(int c=0;c<4;c++)for(int rr=0;rr<4;rr++){float s=0;for(int k=0;k<4;k++)s+=a.m[k*4+rr]*b.m[c*4+k];r.m[c*4+rr]=s;} return r; }
static M4 ident(void){ M4 r={{0}}; r.m[0]=r.m[5]=r.m[10]=r.m[15]=1; return r; }
static M4 *cur(void){ return mode==GL_PROJECTION?&proj:&mv; }
static void multCur(M4 b){ *cur()=mul(*cur(),b); }

#define SW 256
#define SH 192
static float zb[SH][SW]; static u16 fb[SH][SW], shown[SH][SW]; static u16 clearCol;
static u16 color;
static float qv[4][4]; static int qn;

void videoSetMode(u32 m){(void)m;} int glInit(void){mode=GL_MODELVIEW;proj=mv=ident();return 1;} void glEnable(int e){(void)e;}
void glClearColor(u8 r,u8 g,u8 b,u8 a){(void)a;clearCol=RGB15(r,g,b);}
void glClearPolyID(u8 i){(void)i;} void glClearDepth(u16 d){(void)d;} void glViewport(u8 a,u8 b,u8 c,u8 d){(void)a;(void)b;(void)c;(void)d;}
void glMatrixMode(int m){mode=m;} void glLoadIdentity(void){*cur()=ident();}
void gluPerspective(float fovy,float asp,float n,float f){ M4 r={{0}}; float t=1.0f/tanf(fovy*(float)M_PI/360.0f);
  r.m[0]=t/asp; r.m[5]=t; r.m[10]=(f+n)/(n-f); r.m[11]=-1; r.m[14]=2*f*n/(n-f); multCur(r); }
void gluLookAtf32(int32 ex,int32 ey,int32 ez,int32 lx,int32 ly,int32 lz,int32 ux,int32 uy,int32 uz){
  float e[3]={ex/4096.f,ey/4096.f,ez/4096.f}, f[3]={lx/4096.f-e[0],ly/4096.f-e[1],lz/4096.f-e[2]}, u[3]={ux/4096.f,uy/4096.f,uz/4096.f};
  float l=sqrtf(f[0]*f[0]+f[1]*f[1]+f[2]*f[2]); for(int i=0;i<3;i++)f[i]/=l;
  float s[3]={f[1]*u[2]-f[2]*u[1], f[2]*u[0]-f[0]*u[2], f[0]*u[1]-f[1]*u[0]};
  l=sqrtf(s[0]*s[0]+s[1]*s[1]+s[2]*s[2]); for(int i=0;i<3;i++)s[i]/=l;
  float uu[3]={s[1]*f[2]-s[2]*f[1], s[2]*f[0]-s[0]*f[2], s[0]*f[1]-s[1]*f[0]};
  M4 r=ident(); for(int i=0;i<3;i++){ r.m[i*4+0]=s[i]; r.m[i*4+1]=uu[i]; r.m[i*4+2]=-f[i]; }
  r.m[12]=-(s[0]*e[0]+s[1]*e[1]+s[2]*e[2]); r.m[13]=-(uu[0]*e[0]+uu[1]*e[1]+uu[2]*e[2]); r.m[14]=(f[0]*e[0]+f[1]*e[1]+f[2]*e[2]);
  multCur(r); }
void glScalef32(int32 s){ M4 r=ident(); r.m[0]=r.m[5]=r.m[10]=s/4096.f; multCur(r); }
void glPolyFmt(u32 p){(void)p;}
void glBegin(int m){(void)m;qn=0;} void glEnd(void){} void glColor(u16 c){color=c;}

static void tri(float a[4],float b[4],float c[4]){
  float *v[3]={a,b,c}; float sx[3],sy[3],sz[3];
  for(int i=0;i<3;i++){ sx[i]=(v[i][0]/v[i][3]*0.5f+0.5f)*SW; sy[i]=(0.5f-v[i][1]/v[i][3]*0.5f)*SH; sz[i]=v[i][2]/v[i][3]; }
  int x0=(int)floorf(fminf(sx[0],fminf(sx[1],sx[2]))), x1=(int)ceilf(fmaxf(sx[0],fmaxf(sx[1],sx[2])));
  int y0=(int)floorf(fminf(sy[0],fminf(sy[1],sy[2]))), y1=(int)ceilf(fmaxf(sy[0],fmaxf(sy[1],sy[2])));
  if(x0<0)x0=0;
  if(y0<0)y0=0;
  if(x1>SW)x1=SW;
  if(y1>SH)y1=SH;
  float area=(sx[1]-sx[0])*(sy[2]-sy[0])-(sx[2]-sx[0])*(sy[1]-sy[0]); if(fabsf(area)<1e-6f)return;
  for(int y=y0;y<y1;y++)for(int x=x0;x<x1;x++){ float px=x+0.5f,py=y+0.5f;
    float w0=((sx[1]-px)*(sy[2]-py)-(sx[2]-px)*(sy[1]-py))/area, w1=((sx[2]-px)*(sy[0]-py)-(sx[0]-px)*(sy[2]-py))/area, w2=1-w0-w1;
    if(w0<0||w1<0||w2<0)continue; float z=w0*sz[0]+w1*sz[1]+w2*sz[2];
    if(z<zb[y][x]){zb[y][x]=z;fb[y][x]=color;} } }

static void polyClip(float in[][4],int n){   // recorta contra o plano proximo (z > -w)
  float out[8][4]; int m=0;
  for(int i=0;i<n;i++){ float *a=in[i],*b=in[(i+1)%n]; float da=a[2]+a[3], db=b[2]+b[3];
    if(da>=0) memcpy(out[m++],a,16);
    if((da>=0)!=(db>=0)){ float t=da/(da-db); for(int k=0;k<4;k++) out[m][k]=a[k]+(b[k]-a[k])*t; m++; } }
  for(int i=1;i+1<m;i++) tri(out[0],out[i],out[i+1]); }

void glVertex3v16(v16 x,v16 y,v16 z){
  M4 t=mul(proj,mv); float p[4]={x/4096.f,y/4096.f,z/4096.f,1};
  for(int r=0;r<4;r++){ float s=0; for(int k=0;k<4;k++) s+=t.m[k*4+r]*p[k]; qv[qn][r]=s; }
  if(++qn==4){ polyClip(qv,4); qn=0; } }
void glFlush(u32 f){(void)f; memcpy(shown,fb,sizeof fb); for(int y=0;y<SH;y++)for(int x=0;x<SW;x++){fb[y][x]=clearCol;zb[y][x]=1e9f;} }
s16 sinLerp(s16 a){ return (s16)lrintf(sinf(a*(float)M_PI/16384.f)*4096); }
s16 cosLerp(s16 a){ return (s16)lrintf(cosf(a*(float)M_PI/16384.f)*4096); }

// console da tela de baixo: grade 32x24
static char con[24][33]; static int crow,ccol;
void consoleDemoInit(void){ consoleClear(); } void consoleClear(void){ memset(con,' ',sizeof con); for(int r=0;r<24;r++)con[r][32]=0; crow=ccol=0; }
int iprintf(const char *fmt,...){ char buf[256]; va_list ap; va_start(ap,fmt); vsnprintf(buf,sizeof buf,fmt,ap); va_end(ap);
  for(char *p=buf;*p;p++){ if(*p=='\x1b'&&p[1]=='['){ int r,c,n; if(sscanf(p+2,"%d;%dH%n",&r,&c,&n)==2){crow=r;ccol=c;p+=1+n;continue;} }
    if(*p=='\n'){crow++;ccol=0;continue;} if(crow<24&&ccol<32)con[crow][ccol]=*p; ccol++; }
  return 0; }

// ---- motor 2D secundario ----
u32 h_dispcntSub; u16 h_bgcntSub[4]; u16 h_subPal[512]; u16 h_subOam[512];
u16 h_subBg[0x20000/2]; u16 h_subObj[0x20000/2];
void videoSetModeSub(u32 m){ h_dispcntSub = m; } void vramSetBankC(int b){(void)b;} void vramSetBankD(int b){(void)b;}
static int sub4(u16 *base, int tile, int x, int y){ u32 *t=(u32*)base; return (t[tile*8+y]>>(x*4))&15; }
static int subBgPix(int n,int x,int y){ u16 cnt=h_bgcntSub[n]; int sbb=(cnt>>8)&31, cbb=(cnt>>2)&15;
  u16 e=h_subBg[sbb*0x400+(y>>3)*32+(x>>3)]; int p=sub4(h_subBg+cbb*0x2000, e&1023, x&7, y&7); return p ? (e>>12)*16+p : 0; }
static void renderSub(u16 out[192][256]){
  for(int y=0;y<192;y++)for(int x=0;x<256;x++){
    u16 c=h_subPal[0]; int v;
    // prioridade: BG de prioridade maior desenha primeiro
    for(int pr=3;pr>=0;pr--){
      for(int n=1;n>=0;n--) if((h_bgcntSub[n]&3)==pr && (v=subBgPix(n,x,y))) c=h_subPal[v];
      if(pr==0) for(int i=127;i>=0;i--){ u16 a0=h_subOam[i*4],a1=h_subOam[i*4+1],a2=h_subOam[i*4+2];
        if(a0&0x200) continue; int sy=a0&255, sx=a1&511; if(sy>=192) sy-=256; if(sx>=256) sx-=512;
        int sz=(a1>>14)&3, w=sz==1?16:8; int lx=x-sx, ly=y-sy; if(lx<0||ly<0||lx>=w||ly>=w) continue;
        int tile=(a2&1023)+(ly>>3)*(w/8)+(lx>>3); int p=sub4(h_subObj,tile,lx&7,ly&7); if(p) c=h_subPal[256+(a2>>12)*16+p]; }
    }
    out[y][x]=c; } }

// ---- entradas controladas pelo gerador de prints ----
u32 h_held, h_prev; int h_tx = 128, h_ty = 96;
void scanKeys(void){}
u32 keysHeld(void){ return h_held; }
u32 keysDown(void){ return h_held & ~h_prev; }
void touchRead(touchPosition *t){ t->px = (u16)h_tx; t->py = (u16)h_ty; }
void swiWaitForVBlank(void){}

void h_dump(const char *name){
  char fn[64]; snprintf(fn,sizeof fn,"%s_top.ppm",name); FILE *f=fopen(fn,"wb"); fprintf(f,"P6 256 192 255\n");
  for(int y=0;y<SH;y++)for(int x=0;x<SW;x++){u16 c=shown[y][x];u8 p[3]={(c&31)*255/31,((c>>5)&31)*255/31,((c>>10)&31)*255/31};fwrite(p,1,3,f);} fclose(f);
  static u16 sub[192][256]; renderSub(sub);
  snprintf(fn,sizeof fn,"%s_bottom.ppm",name); f=fopen(fn,"wb"); fprintf(f,"P6 256 192 255\n");
  for(int y=0;y<192;y++)for(int x=0;x<256;x++){u16 c=sub[y][x];u8 p[3]={(c&31)*255/31,((c>>5)&31)*255/31,((c>>10)&31)*255/31};fwrite(p,1,3,f);} fclose(f); }
