#include "draw.h"
#include <string.h>

uint8_t frame[320*240/2];
uint16_t palette[16];
static const uint16_t player_colors[]={0xafe0,0x07ff,0xf81f,0xffe0,0xffff,0xfb00};
void gfx_palette(uint16_t bg,uint16_t ground,unsigned player) {
  palette[C_BG]=bg; palette[C_BG2]=color_mix(bg,0,1,7); palette[C_BG3]=color_mix(bg,0xffff,1,11);
  palette[C_GROUND]=ground; palette[C_GROUND2]=color_mix(ground,0,2,5);
  palette[C_GLOW]=color_mix(bg,0xffff,1,2);
  palette[C_BLACK]=0; palette[C_WHITE]=0xffff; palette[C_MUTED]=0xbdf7;
  palette[C_LIME]=0xafe0; palette[C_CYAN]=0x07ff; palette[C_YELLOW]=0xff60; palette[C_PINK]=0xf8b6;
  palette[C_RED]=0xf986; palette[C_INK]=0x0844;
  palette[C_PLAYER]=player_colors[player%6];
}
void rect(int x,int y,int w,int h,int c) {
  if(x<0) {w+=x;x=0;} if(y<0) {h+=y;y=0;}
  if(x+w>320) w=320-x; if(y+h>240) h=240-y;
  if(w<=0||h<=0) return;
  uint8_t cc=(uint8_t)(c&15),pair=(uint8_t)(cc*17);
  for(int j=y;j<y+h;j++) {
    uint8_t *p=frame+(j*320+x)/2;int width=w;
    if(x&1) { *p=(uint8_t)((*p&15)|(cc<<4));p++;width--; }
    if(width>=2){memset(p,pair,(size_t)(width/2));p+=width/2;}
    if(width&1)*p=(uint8_t)((*p&0xf0)|cc);
  }
}
void outline(int x,int y,int w,int h,int c) { rect(x,y,w,1,c);rect(x,y+h-1,w,1,c);rect(x,y,1,h,c);rect(x+w-1,y,1,h,c); }
void line(int x,int y,int xx,int yy,int c) {
  int dx=xx>x?xx-x:x-xx,sx=x<xx?1:-1,dy=yy>y?y-yy:yy-y,sy=y<yy?1:-1,e=dx+dy;
  for(;;) { rect(x,y,1,1,c); if(x==xx&&y==yy) break; int e2=e*2;if(e2>=dy){e+=dy;x+=sx;}if(e2<=dx){e+=dx;y+=sy;} }
}
void triangle(int x,int y,int xx,int yy,int xxx,int yyy,int c) {
  int xs[3]={x,xx,xxx},ys[3]={y,yy,yyy};
  for(int i=0;i<2;i++) for(int j=i+1;j<3;j++) if(ys[i]>ys[j]) {int t=ys[i];ys[i]=ys[j];ys[j]=t;t=xs[i];xs[i]=xs[j];xs[j]=t;}
  int from=ys[0]<0?0:ys[0],to=ys[2]>239?239:ys[2];
  for(int row=from;row<=to;row++) {
    int a=xs[0]+(ys[2]==ys[0]?0:(xs[2]-xs[0])*(row-ys[0])/(ys[2]-ys[0]));
    int b;
    if(row<ys[1]) b=xs[0]+(ys[1]==ys[0]?0:(xs[1]-xs[0])*(row-ys[0])/(ys[1]-ys[0]));
    else b=xs[1]+(ys[2]==ys[1]?0:(xs[2]-xs[1])*(row-ys[1])/(ys[2]-ys[1]));
    if(a>b){int t=a;a=b;b=t;} rect(a,row,b-a+1,1,c);
  }
}
void circle(int x,int y,int r,int c,bool fill) {
  int a=r,b=0,err=1-r;
  while(a>=b) {
    if(fill) {rect(x-a,y+b,a*2+1,1,c);rect(x-a,y-b,a*2+1,1,c);rect(x-b,y+a,b*2+1,1,c);rect(x-b,y-a,b*2+1,1,c);}
    else {rect(x+a,y+b,1,1,c);rect(x-a,y+b,1,1,c);rect(x+a,y-b,1,1,c);rect(x-a,y-b,1,1,c);rect(x+b,y+a,1,1,c);rect(x-b,y+a,1,1,c);rect(x+b,y-a,1,1,c);rect(x-b,y-a,1,1,c);}
    b++; if(err<0)err+=2*b+1;else{a--;err+=2*(b-a)+1;}
  }
}
/* Five-column, seven-row pixel lettering; uppercase by design at 320x240. */
static const uint8_t glyphs[][5]={
 {0x3e,0x51,0x49,0x45,0x3e},{0,0x42,0x7f,0x40,0},{0x42,0x61,0x51,0x49,0x46},{0x21,0x41,0x45,0x4b,0x31},{0x18,0x14,0x12,0x7f,0x10},
 {0x27,0x45,0x45,0x45,0x39},{0x3c,0x4a,0x49,0x49,0x30},{1,0x71,9,5,3},{0x36,0x49,0x49,0x49,0x36},{6,0x49,0x49,0x29,0x1e},
 {0x7e,0x11,0x11,0x11,0x7e},{0x7f,0x49,0x49,0x49,0x36},{0x3e,0x41,0x41,0x41,0x22},{0x7f,0x41,0x41,0x22,0x1c},{0x7f,0x49,0x49,0x49,0x41},
 {0x7f,9,9,9,1},{0x3e,0x41,0x49,0x49,0x7a},{0x7f,8,8,8,0x7f},{0,0x41,0x7f,0x41,0},{0x20,0x40,0x41,0x3f,1},{0x7f,8,0x14,0x22,0x41},
 {0x7f,0x40,0x40,0x40,0x40},{0x7f,2,0xc,2,0x7f},{0x7f,4,8,0x10,0x7f},{0x3e,0x41,0x41,0x41,0x3e},{0x7f,9,9,9,6},{0x3e,0x41,0x51,0x21,0x5e},
 {0x7f,9,0x19,0x29,0x46},{0x46,0x49,0x49,0x49,0x31},{1,1,0x7f,1,1},{0x3f,0x40,0x40,0x40,0x3f},{0x1f,0x20,0x40,0x20,0x1f},{0x3f,0x40,0x38,0x40,0x3f},
 {0x63,0x14,8,0x14,0x63},{7,8,0x70,8,7},{0x61,0x51,0x49,0x45,0x43}};
void text(int x,int y,const char *s,int scale,int c) {
  for(;*s;s++,x+=6*scale) {
    unsigned char ch=(unsigned char)*s;if(ch>='a'&&ch<='z')ch-=32;
    const uint8_t *g=ch>='0'&&ch<='9'?glyphs[ch-'0']:ch>='A'&&ch<='Z'?glyphs[10+ch-'A']:NULL;
    if(g) {for(int i=0;i<5;i++)for(int j=0;j<7;j++)if(g[i]&(1<<j))rect(x+i*scale,y+j*scale,scale,scale,c);}
    else if(ch=='-')rect(x,y+3*scale,5*scale,scale,c);
    else if(ch=='+'){rect(x,y+3*scale,5*scale,scale,c);rect(x+2*scale,y+scale,scale,5*scale,c);}
    else if(ch==':'){rect(x+2*scale,y+scale,scale,scale,c);rect(x+2*scale,y+5*scale,scale,scale,c);}
    else if(ch=='/'||ch=='%') {line(x,y+6*scale,x+4*scale,y,c);if(ch=='%'){rect(x,y,2*scale,2*scale,c);rect(x+3*scale,y+5*scale,2*scale,2*scale,c);}}
    else if(ch=='.')rect(x+2*scale,y+6*scale,scale,scale,c);
    else if(ch=='<'){line(x+4*scale,y,x,y+3*scale,c);line(x,y+3*scale,x+4*scale,y+6*scale,c);}
    else if(ch=='>'){line(x,y,x+4*scale,y+3*scale,c);line(x+4*scale,y+3*scale,x,y+6*scale,c);}
    else if(ch=='!'){rect(x+2*scale,y,scale,5*scale,c);rect(x+2*scale,y+6*scale,scale,scale,c);}
  }
}
void centered(int y,const char *s,int scale,int c) {text((320-(int)strlen(s)*6*scale+scale)/2,y,s,scale,c);}
void number(int x,int y,unsigned v,int scale,int c) {char s[11];unsigned n=10;s[n]=0;do{s[--n]=(char)('0'+v%10);v/=10;}while(v&&n);text(x,y,s+n,scale,c);}
static const int8_t sine[16]={0,6,11,15,16,15,11,6,0,-6,-11,-15,-16,-15,-11,-6};
void player_icon(int x,int y,unsigned rotation,bool ship,int color) {
  if(ship) {
    triangle(x-13,y-2,x+14,y+1,x-7,y+8,C_BLACK);triangle(x-11,y-1,x+11,y+1,x-6,y+5,color);
    rect(x-7,y-9,11,9,C_BLACK);rect(x-5,y-7,7,7,color);rect(x-2,y-5,3,3,C_DETAIL);return;
  }
  int s=sine[(rotation>>4)&15],c=sine[((rotation>>4)+4)&15];
  for(int yy=-13;yy<=13;yy++)for(int xx=-13;xx<=13;xx++) {
    int u=(xx*c+yy*s)/16,v=(-xx*s+yy*c)/16;
    if(u>=-9&&u<=9&&v>=-9&&v<=9) {
      int col=(u==-9||u==9||v==-9||v==9)?C_BLACK:color;
      if(v>=-4&&v<=0&&((u>=-5&&u<=-2)||(u>=2&&u<=5)))col=C_BLACK;
      if(v>=-3&&v<=-2&&((u>=-4&&u<=-3)||(u>=3&&u<=4)))col=C_CYAN;
      if(v>=3&&v<=5&&u>=-5&&u<=5)col=C_BLACK;
      if(v==4&&u>=-3&&u<=3)col=C_DETAIL;
      rect(x+xx,y+yy,1,1,col);
    }
  }
}
static void spike(int x,int y,int r,int rot,int color) {
  int ax=-r,ay=r,bx=r,by=r,cx=0,cy=-r;
  for(int i=0;i<rot;i++){int t=ax;ax=-ay;ay=t;t=bx;bx=-by;by=t;t=cx;cx=-cy;cy=t;}
  triangle(x+ax,y+ay,x+bx,y+by,x+cx,y+cy,C_BLACK);
  line(x+ax,y+ay,x+cx,y+cy,color);line(x+bx,y+by,x+cx,y+cy,color);line(x+ax,y+ay,x+bx,y+by,color);
}
void object_draw(const Object *o,int x,int y,bool used) {
  Shape sh=object_shape(o);
  if(sh.kind==COLOR) return;
  if(sh.kind==SOLID) {
    int w=(int)(sh.w*.6f),h=(int)(sh.h*.6f);int left=x-w/2,top=y-h/2;
    rect(left-1,top-1,w+2,h+2,C_GLOW);rect(left,top,w,h,C_BLACK);
    if(w>6&&h>6){rect(left+2,top+h/2,w-4,h/2-2,C_INK);if(o->id==2||o->id==6||o->id==7){for(int yy=top+5;yy<top+h-1;yy+=6)line(left+2,yy,left+w-3,yy,C_BG3);for(int xx=left+5;xx<left+w-1;xx+=6)line(xx,top+2,xx,top+h-3,C_BG3);}}
    outline(left,top,w,h,C_WHITE);return;
  }
  if(sh.kind==HAZARD) {
    if(o->id==9)spike(x,y-6,8,o->rot,C_WHITE);
    else spike(x,y,o->id==39?5:9,o->rot,C_WHITE);return;
  }
  if(sh.kind==PORTAL||sh.kind==GRAVITY) {
    int c=o->id==13?C_PINK:o->id==12?C_LIME:o->id==11?C_YELLOW:C_BLUE;
    for(int k=0;k<3;k++) {line(x-5-k,y-22+k,x-9-k,y-12,c);line(x-9-k,y-12,x-9-k,y+12,c);line(x-9-k,y+12,x-5-k,y+22-k,c);line(x+5+k,y-22+k,x+9+k,y-12,C_WHITE);line(x+9+k,y-12,x+9+k,y+12,C_WHITE);line(x+9+k,y+12,x+5+k,y+22-k,C_WHITE);}
    rect(x-3,y-25,7,3,c);rect(x-3,y+23,7,3,c);return;
  }
  if(sh.kind==PAD){int c=o->id==67?C_CYAN:C_YELLOW;rect(x-8,y-2,17,4,C_BLACK);rect(x-7,y-3,15,3,used?C_MUTED:c);line(x-5,y-5,x+5,y-5,c);return;}
  if(sh.kind==ORB){circle(x,y,10,used?C_GLOW:C_YELLOW,false);circle(x,y,6,used?C_BG3:C_YELLOW,true);circle(x,y,4,C_WHITE,false);return;}
  if(sh.kind==COIN){if(used)return;circle(x,y,7,C_ORANGE,true);circle(x,y,7,C_YELLOW,false);text(x-2,y-3,"C",1,C_YELLOW);return;}
  /* Original structural decoration: inset blocks, chains and direction marks. */
  if(o->id==5||o->id==41) {outline(x-8,y-8,17,17,C_GLOW);outline(x-5,y-5,11,11,C_BG3);}
  else if(o->id==15||o->id==16||o->id==17) {rect(x-7,y-1,15,2,C_GLOW);}
  else if(o->id>=18&&o->id<=21) {line(x-4,y+3,x,y-3,C_GLOW);line(x,y-3,x+4,y+3,C_GLOW);}
  else if(o->id==103||o->id==110) {circle(x,y,7,C_GLOW,false);}
}
void backdrop(int scroll,unsigned t,bool grid) {
  rect(0,0,320,240,C_BG);
  static const uint8_t widths[6]={74,49,86,57,69,91};
  int offset=((scroll/5)%426+426)%426;
  for(int row=0;row<4;row++){
    int x=-(int)offset-(row&1?43:0);
    unsigned k=0;
    while(x<320){int w=widths[(k+row*2)%6];rect(x+3,row*64+3,w-6,58,C_BG2);outline(x+4,row*64+4,w-8,56,C_BG3);x+=w;k++;}
  }
  if(grid)for(int y=0;y<240;y+=16)line(0,y,319,y,C_BG2);
  (void)t;
}
