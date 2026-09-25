#include "game.h"
#include <string.h>

static float ab(float x) { return x < 0 ? -x : x; }
static float clamp(float x, float lo, float hi) { return x < lo ? lo : x > hi ? hi : x; }

Shape object_shape(const Object *o) {
  Shape s = {30, 30, DECOR};
  switch (o->id) {
    case 1: case 2: case 3: case 4: case 6: case 7: s.kind = SOLID; break;
    case 40: s = (Shape){30, 14, SOLID}; break;
    case 62: case 65: case 66: case 68: s = (Shape){30, 16, SOLID}; break;
    case 8: s = (Shape){6, 12, HAZARD}; break;
    case 9: s = (Shape){9, 10.8f, HAZARD}; break;
    case 39: s = (Shape){6, 5.6f, HAZARD}; break;
    case 10: case 11: s = (Shape){25, 75, GRAVITY}; break;
    case 12: case 13: s = (Shape){34, 86, PORTAL}; break;
    case 35: s = (Shape){25, 4, PAD}; break;
    case 67: s = (Shape){25, 6, PAD}; break;
    case 36: s = (Shape){36, 36, ORB}; break;
    case 1329: case 142: s = (Shape){22, 22, COIN}; break;
    case 29: case 30: s.kind = COLOR; break;
    default: break;
  }
  if (o->rot & 1) { float w = s.w; s.w = s.h; s.h = w; }
  return s;
}

bool level_valid(const Level *l) {
  if (!l || !l->objects || l->count > ND_MAX_OBJECTS || l->length < 300 || l->length > 32000) return false;
  for (unsigned i=0; i<l->count; i++) {
    const Object *o=&l->objects[i];
    if (o->x < 0 || o->x > l->length || o->y < -300 || o->y > 1500 || o->rot > 3 ||
        (i && o->x < l->objects[i-1].x)) return false;
  }
  return true;
}
unsigned level_lower_bound(const Level *l, float x) {
  unsigned a=0, b=l->count;
  while(a<b) { unsigned m=(a+b)/2; if(l->objects[m].x<x) a=m+1; else b=m; }
  return a;
}
bool player_used(const Player *p, unsigned i) { return i<ND_MAX_OBJECTS && (p->used[i>>3] & (1u<<(i&7))); }
static void use(Player *p, unsigned i) { p->used[i>>3] |= (uint8_t)(1u<<(i&7)); }

uint16_t color_mix(uint16_t a, uint16_t b, unsigned n, unsigned d) {
  if (!d || n>=d) return b;
  unsigned r=(((a>>11)&31)*(d-n)+((b>>11)&31)*n)/d;
  unsigned g=(((a>>5)&63)*(d-n)+((b>>5)&63)*n)/d;
  unsigned bl=((a&31)*(d-n)+(b&31)*n)/d;
  return (uint16_t)((r<<11)|(g<<5)|bl);
}
void player_start(Player *p, const Level *l) {
  memset(p,0,sizeof(*p));
  p->y=15; p->grounded=true; p->ceiling=300;
  p->bg=p->bg_target=p->bg_from=l->background;
  p->ground=p->ground_target=p->ground_from=l->ground;
  p->death_object=UINT16_MAX;
}
int player_progress(const Player *p, const Level *l) {
  if (p->complete) return 100;
  int progress=(int)(p->x*100/l->length);
  return progress<0?0:progress>99?99:progress;
}
static void die(Player *p, unsigned i) { p->dead=true; p->death_object=(uint16_t)i; }

void player_step(Player *p, const Level *l, bool down) {
  if(p->dead || p->complete) return;
  bool pressed=down&&!p->held;
  if(pressed) p->orb_armed=true;
  if(!down) p->orb_armed=false;
  float old_y=p->y;
  float sign=p->inverted?-1.0f:1.0f;
  bool jumped=false;
  if(!p->mode && p->grounded && down) {
    p->vy=11.18f*sign; p->grounded=false; jumped=true;
    /* Ground jump consumes the press; rings require a second press. */
    p->orb_armed=false;
  }
  p->x+=1.29825044f;
  p->tick++;
  if(p->mode) {
    float v=p->vy*sign;
    v += down ? (v < -1.916398f ? 0.108f : 0.086f) : (v > 1.916398f ? -0.103f : -0.069f);
    p->vy=clamp(v,-6.4f,8.0f)*sign;
    p->y+=p->vy*0.225f;
  } else if(!jumped || !pressed) {
    if(!jumped) p->vy=clamp(p->vy*sign-0.216f,-15.0f,16.0f)*sign;
    p->y+=p->vy*0.225f;
  }
  p->grounded=false;
  if(p->y<15) { p->y=15; if(p->vy<0) p->vy=0; p->grounded=!p->inverted; }
  if(p->mode) {
    p->y=clamp(p->y,p->floor+15,p->ceiling-15);
    if((p->y<=p->floor+15 && p->vy<0)||(p->y>=p->ceiling-15 && p->vy>0)) p->vy=0;
  }
  while(p->first<l->count && l->objects[p->first].x < p->x-100) p->first++;
  /* Trigger phase before solids, independent of authoring/object order. */
  for(unsigned i=p->first;i<l->count && l->objects[i].x<p->x+100;i++) {
    const Object *o=&l->objects[i]; Shape s=object_shape(o);
    if(s.kind==COLOR && !player_used(p,i) && p->x>=o->x) {
      use(p,i);
      if(o->id==29) { p->bg_from=p->bg; p->bg_target=o->color; p->bg_time=o->duration; p->bg_elapsed=0; }
      else { p->ground_from=p->ground; p->ground_target=o->color; p->ground_time=o->duration; p->ground_elapsed=0; }
    }
    if(s.kind< PAD || s.kind==COLOR || player_used(p,i)) continue;
    float trigger_x=(s.kind==ORB&&pressed)?p->x-1.29825044f:p->x;
    if(ab(trigger_x-o->x)>=15+s.w/2 || ab(p->y-o->y)>=15+s.h/2) continue;
    switch(s.kind) {
      case PAD:
        use(p,i); p->grounded=false;
        if(o->id==67) { p->inverted=!p->inverted; p->vy=(p->inverted?-1:1)*7.0f; }
        else p->vy=(p->inverted?-1:1)*16;
        break;
      case ORB:
        if(down && p->orb_armed) { use(p,i); p->vy=(p->inverted?-1:1)*11.18f; p->grounded=false; p->orb_armed=false; }
        break;
      case GRAVITY:
        use(p,i);
        if(p->inverted!=(o->id==11)) { p->inverted=o->id==11; p->vy*=0.5f; p->grounded=false; }
        break;
      case PORTAL:
        use(p,i);
        if(p->mode!=(o->id==13)) {
          p->mode=o->id==13; p->vy=0; p->inverted=false;
          if(p->mode) { p->floor=(float)((o->y/30)*30-150); if(p->floor<0) p->floor=0; p->ceiling=p->floor+300; }
        }
        break;
      case COIN: use(p,i); if(p->coins<3) p->coins++; break;
      default: break;
    }
  }
  for(unsigned i=p->first;i<l->count && l->objects[i].x<p->x+60;i++) {
    const Object *o=&l->objects[i]; Shape s=object_shape(o);
    if(s.kind!=SOLID && s.kind!=HAZARD) continue;
    float left=o->x-s.w/2, right=o->x+s.w/2, bottom=o->y-s.h/2, top=o->y+s.h/2;
    if(p->x+15<=left || p->x-15>=right || p->y+15<=bottom || p->y-15>=top) continue;
    if(s.kind==HAZARD) { die(p,i); return; }
    if(p->vy<=0 && old_y-15>=top-0.1f) {
      p->y=top+15; p->vy=0; p->grounded=!p->inverted;
    } else if(p->vy>=0 && old_y+15<=bottom+0.1f && (p->inverted||p->mode)) {
      p->y=bottom-15; p->vy=0; p->grounded=p->inverted;
    } else {
      /* GD uses the inner cube for lethal solid-side collisions. */
      if(p->x+4.5f>left && p->x-4.5f<right && p->y+4.5f>bottom && p->y-4.5f<top) { die(p,i); return; }
    }
  }
  if(p->y>1500 || p->y<-100) { die(p,UINT16_MAX); return; }
  if(p->x>=l->length) p->complete=true;
  if(!p->grounded && !p->mode) {int step=p->tick%4?1:2;p->rotation=(uint8_t)(p->rotation+(p->inverted?-step:step));}
  else if(p->grounded) p->rotation=(uint8_t)((p->rotation+32)&0xC0);
  float target=p->mode?p->floor:clamp(p->y-150,0,1100);
  p->camera_y+=(target-p->camera_y)*0.025f;
  if(p->bg_elapsed<p->bg_time) p->bg_elapsed++;
  if(p->ground_elapsed<p->ground_time) p->ground_elapsed++;
  p->bg=color_mix(p->bg_from,p->bg_target,p->bg_elapsed,p->bg_time);
  p->ground=color_mix(p->ground_from,p->ground_target,p->ground_elapsed,p->ground_time);
  p->held=down;
}

static const char *custom_names[]={"MY LEVEL 1","MY LEVEL 2","MY LEVEL 3"};
static const uint16_t themes[]={0x2199,0x90b3,0x0474,0xb9c7,0x4354,0x526d};
Level custom_level(const CustomLevel *c,unsigned slot) {
  return (Level){custom_names[slot%ND_SLOTS],"YOUR CREATION",c->objects,c->count,c->length,themes[c->theme%6],(uint16_t)(themes[c->theme%6]&0xdefb),2,c->bpm};
}
bool editor_put(CustomLevel *c,Object o) {
  if(o.x<0 || o.x>31000 || o.y<0 || o.y>1200 || o.rot>3) return false;
  for(unsigned i=0;i<c->count;i++) if(c->objects[i].x==o.x && c->objects[i].y==o.y) { c->objects[i]=o; return true; }
  if(c->count>=ND_CUSTOM_MAX) return false;
  unsigned i=c->count++;
  while(i && c->objects[i-1].x>o.x) { c->objects[i]=c->objects[i-1]; i--; }
  c->objects[i]=o;
  if(c->length<o.x+300) c->length=o.x+300;
  return true;
}
bool editor_remove(CustomLevel *c,int x,int y) {
  for(unsigned i=0;i<c->count;i++) if(c->objects[i].x==x && c->objects[i].y==y) {
    memmove(c->objects+i,c->objects+i+1,(c->count-i-1)*sizeof(Object)); c->count--; return true;
  }
  return false;
}
