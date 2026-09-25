#include "app.h"
#include "draw.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static uint8_t saved[15000];static size_t saved_size;
bool platform_load(uint8_t *d,size_t cap,size_t *n){if(!saved_size||saved_size>cap)return false;memcpy(d,saved,saved_size);*n=saved_size;return true;}
bool platform_save(const uint8_t *d,size_t n){if(n>sizeof(saved))return false;memcpy(saved,d,n);saved_size=n;return true;}
static void tap(uint32_t k){app_tick(0);app_tick(k);app_tick(0);}
static void ppm(const char *name){FILE *f=fopen(name,"wb");assert(f);fprintf(f,"P6\n320 240\n255\n");for(int i=0;i<320*240;i++){unsigned c=palette[gfx_index(frame,i)];uint8_t rgb[3]={(uint8_t)(((c>>11)&31)*255/31),(uint8_t)(((c>>5)&63)*255/63),(uint8_t)((c&31)*255/31)};fwrite(rgb,1,3,f);}fclose(f);}
static void shot(const char *name){app.time+=100;app_render();ppm(name);}
static void test_levels(void){for(int i=0;i<ND_BUILTINS;i++)assert(level_valid(&nd_levels[i]));assert(nd_levels[0].count==2291);assert(nd_levels[1].count==1545);assert(nd_levels[2].count==1531);assert(nd_levels[3].count==1444);}
static void test_physics(void){
  Object objects[4]={{525,15,8,0,0,0,0}};Level l={"TEST","",objects,1,1000,0,0,1,120};Player p;
  player_start(&p,&l);for(int i=0;i<600&&!p.dead;i++)player_step(&p,&l,false);assert(p.dead&&p.death_object==0);
  player_start(&p,&l);for(int i=0;i<500&&!p.dead;i++)player_step(&p,&l,i==380);assert(!p.dead&&p.x>600);
  l.count=0;player_start(&p,&l);float peak=0;for(int i=0;i<200;i++){player_step(&p,&l,i<110);if(p.y>peak)peak=p.y;}assert(peak>76&&peak<82);assert(!p.dead);
  /* Deterministic replay, and checkpoints copy all consumed triggers. */
  Player a,b;player_start(&a,&l);for(int i=0;i<99;i++)player_step(&a,&l,i>40);b=a;
  for(int i=0;i<100;i++){player_step(&a,&l,i%19<6);player_step(&b,&l,i%19<6);}assert(!memcmp(&a,&b,sizeof(a)));
  objects[0]=(Object){120,2,35,0,0,0,0};l.count=1;player_start(&p,&l);for(int i=0;i<120;i++)player_step(&p,&l,false);assert(p.y>120&&player_used(&p,0));
  objects[0]=(Object){120,40,36,0,0,0,0};player_start(&p,&l);for(int i=0;i<110;i++)player_step(&p,&l,i==60||i>=85);assert(player_used(&p,0));
  objects[0]=(Object){120,60,11,0,0,0,0};player_start(&p,&l);for(int i=0;i<110;i++)player_step(&p,&l,false);assert(p.inverted&&p.y>15);
  objects[0]=(Object){120,40,13,0,0,0,0};player_start(&p,&l);for(int i=0;i<160;i++)player_step(&p,&l,i>90);assert(p.mode&&p.y>20);
  /* Side contact kills; top landing is stable at exact contact. */
  objects[0]=(Object){150,15,1,0,0,0,0};player_start(&p,&l);for(int i=0;i<160&&!p.dead;i++)player_step(&p,&l,false);assert(p.dead);
  player_start(&p,&l);p.x=145;p.y=55;p.vy=-5;for(int i=0;i<10;i++)player_step(&p,&l,false);assert(!p.dead&&p.y==45&&p.grounded);
}
static void test_save(void){
  static SaveData a,b;static uint8_t buf[15000],arena[16000];save_defaults(&a);a.best[3]=84;a.attempts[1]=20000;
  assert(editor_put(&a.custom[0],(Object){555,15,8,0,0,0,0}));assert(editor_put(&a.custom[0],(Object){315,15,1,0,0,0,0}));
  size_t n=save_encode(&a,buf,sizeof(buf));assert(n);assert(save_decode(&b,buf,n));assert(!memcmp(&a,&b,sizeof(a)));
  for(size_t i=0;i<n;i++){buf[i]^=1;assert(!save_decode(&b,buf,n));buf[i]^=1;}assert(!save_decode(&b,buf,n-1));
  memset(arena,0,sizeof(arena));uint8_t canary[]={7,8,9};assert(storage_write(arena,sizeof(arena),"other.py",canary,3));assert(storage_write(arena,sizeof(arena),"numdash.ndd",buf,n));assert(storage_write(arena,sizeof(arena),"after.py",canary,3));
  const uint8_t *p;size_t len;assert(storage_read(arena,sizeof(arena),"numdash.ndd",&p,&len)&&len==n&&!memcmp(p,buf,n));
  assert(storage_write(arena,sizeof(arena),"numdash.ndd",buf,n-12));assert(storage_read(arena,sizeof(arena),"after.py",&p,&len)&&len==3&&!memcmp(p,canary,3));
  assert(storage_write(arena,sizeof(arena),"numdash.ndd",buf,n));assert(storage_read(arena,sizeof(arena),"other.py",&p,&len)&&len==3&&!memcmp(p,canary,3));
  uint32_t hash=nd_crc32(arena,sizeof(arena));assert(!storage_write(arena,20,"numdash.ndd",buf,n));assert(hash==nd_crc32(arena,sizeof(arena)));
  arena[0]=255;arena[1]=255;assert(!storage_write(arena,sizeof(arena),"numdash.ndd",buf,n));
  for(int j=0;j<2000;j++){size_t size=(unsigned)j%sizeof(arena);for(size_t i=0;i<size;i++)arena[i]=(uint8_t)(i*31+j);storage_read(arena,size,"numdash.ndd",&p,&len);storage_write(arena,size,"numdash.ndd",buf,n);}
}
static void test_editor(void){
  CustomLevel c={.length=1800,.bpm=128};for(int i=ND_CUSTOM_MAX-1;i>=0;i--)assert(editor_put(&c,(Object){(int16_t)(i*30+15),15,8,0,0,0,0}));
  assert(!editor_put(&c,(Object){13000,15,8,0,0,0,0}));assert(editor_put(&c,(Object){15,15,1,0,0,0,0}));assert(c.count==ND_CUSTOM_MAX);assert(c.objects[0].id==1);
  assert(editor_remove(&c,15,15));assert(c.count==ND_CUSTOM_MAX-1);Level l=custom_level(&c,0);assert(level_valid(&l));
}
static void test_ui(void){
  app_init();shot("build/menu.ppm");tap(K_OK);assert(app.screen==SELECT);shot("build/select.ppm");tap(K_OK);assert(app.screen==PLAY);
  for(int i=0;i<370;i++)app_tick(0);shot("build/game.ppm");tap(K_BACK);assert(app.screen==PAUSE);Player frozen=app.player;for(int i=0;i<100;i++)app_tick(0);assert(!memcmp(&frozen,&app.player,sizeof(frozen)));shot("build/pause.ppm");
  tap(K_LEFT);tap(K_OK);assert(app.practice);tap(K_CHECK);assert(app.has_checkpoint);Player cp=app.checkpoint;for(int i=0;i<2000;i++)app_tick(0);assert(app.deaths>1&&app.checkpoint.x==cp.x);
  app_init();tap(K_RIGHT);tap(K_OK);assert(app.screen==SLOTS);tap(K_OK);assert(app.screen==EDITOR);tap(K_RIGHT);tap(K_TOOL);tap(K_OK);assert(app.save.custom[0].count==1);tap(K_CHECK);assert(app.edit_mode==1);tap(K_OK);assert(app.save.custom[0].objects[0].rot==1);tap(K_UNDO);assert(app.save.custom[0].objects[0].rot==0);tap(K_CHECK);assert(app.edit_mode==2);tap(K_OK);assert(app.save.custom[0].count==0);tap(K_UNDO);assert(app.save.custom[0].count==1);tap(K_CHECK);assert(app.edit_mode==0);tap(K_SAVE);assert(app.save_ok);shot("build/editor.ppm");tap(K_EXE);assert(app.testing);tap(K_BACK);tap(K_RIGHT);tap(K_OK);assert(app.screen==EDITOR);tap(K_BACK);assert(app.screen==SLOTS);app_init();assert(app.loaded&&app.save.custom[0].count==1);
  app_start(6,false,false);app.player.x=8400;app.player.y=150;app.player.mode=1;app.player.tick=500;shot("build/ship.ppm");
  app.player.complete=true;app.player.coins=2;app.player.jumps=46;app_tick(0);assert(app.screen==COMPLETE);app.time+=130;shot("build/complete.ppm");
  app_init();tap(K_RIGHT);tap(K_RIGHT);tap(K_OK);assert(app.screen==SETTINGS);shot("build/settings.ppm");tap(K_DOWN);tap(K_OK);assert(!app.save.percent);tap(K_BACK);assert(app.screen==HOME);app_init();assert(!app.save.percent);
}
static int replay(int index,const char *path){
  FILE *f=fopen(path,"r");if(!f)return 2;int ticks[5000],values[5000],n=0,t,v;char linebuf[256];
  while(fgets(linebuf,sizeof(linebuf),f)&&n<5000){char *s=strstr(linebuf,"input=");if(s&&sscanf(s,"input=%d,%d",&t,&v)==2){ticks[n]=t;values[n++]=v;}}
  fclose(f);Player p;player_start(&p,&nd_levels[index]);int pos=0;bool held=false;
  for(int i=1;i<30000&&!p.dead&&!p.complete;i++){while(pos<n&&ticks[pos]<=i)held=values[pos++];player_step(&p,&nd_levels[index],held);}
  printf("%s: %s at tick %u x=%.2f y=%.2f object=%u id=%u\n",nd_levels[index].name,p.complete?"COMPLETE":p.dead?"DEAD":"TIMEOUT",p.tick,p.x,p.y,p.death_object,p.death_object<nd_levels[index].count?nd_levels[index].objects[p.death_object].id:0);
  return p.complete?0:1;
}
int main(int argc,char **argv){if(argc==4&&!strcmp(argv[1],"--replay"))return replay(atoi(argv[2]),argv[3]);test_levels();test_physics();test_save();test_editor();test_ui();puts("PASS: levels, physics, checkpoints, save corruption, storage bounds, editor capacity, menus, pause, restart, editor playtest, persistence, render snapshots");return 0;}
