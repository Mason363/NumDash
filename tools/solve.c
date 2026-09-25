/* Desktop-only reachability search. Produces real input replays, never bypasses
 * collisions. Used to verify every shipped course is completable. */
#include "game.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#define WIDTH 4096
#define LAYERS 7000
#define STEP 4
#define HASH 32768
typedef struct {uint16_t parent;uint8_t down;} Edge;
typedef struct {Player p;Edge e;} Node;
static Node a[WIDTH],b[WIDTH*2];
static uint64_t keys[HASH];static uint8_t occupied[HASH];
static uint64_t key(const Player *p){
  uint64_t y=(unsigned)(p->y*2),v=(unsigned)((p->vy+20)*8);
  uint64_t k=y|(v<<12)|((uint64_t)p->mode<<22)|((uint64_t)p->inverted<<23)|((uint64_t)p->grounded<<24)|((uint64_t)p->held<<25)|((uint64_t)p->orb_armed<<26);
  /* Distinguish rings/pads already consumed near the player. */
  for(unsigned i=p->first/8;i<ND_USED_BYTES&&i<p->first/8+8;i++)k=(k^p->used[i])*1099511628211ull;
  return k;
}
static bool insert(uint64_t k){unsigned i=(unsigned)((k^(k>>32))*2654435761u)&(HASH-1);while(occupied[i]){if(keys[i]==k)return false;i=(i+1)&(HASH-1);}occupied[i]=1;keys[i]=k;return true;}
int main(int argc,char **argv){
  int index=argc>1?atoi(argv[1]):0;if(index<0||index>=ND_BUILTINS)return 2;
  const Level *l=&nd_levels[index];Edge *history=calloc((size_t)LAYERS*WIDTH,sizeof(Edge));if(!history)return 2;
  player_start(&a[0].p,l);int count=1,finish=-1,layer=0;
  for(;layer<LAYERS;layer++){
    memset(occupied,0,sizeof(occupied));int n=0;
    for(int i=0;i<count;i++)for(int down=0;down<2;down++){
      Player p=a[i].p;for(int t=0;t<STEP;t++)player_step(&p,l,down!=0);
      if(p.dead||!insert(key(&p)))continue;
      b[n]=(Node){p,{(uint16_t)i,(uint8_t)down}};
      if(p.complete){finish=n;n++;goto complete;}
      n++;
    }
    if(!n){fprintf(stderr,"NO PATH: %s at x=%.2f tick=%u layer=%d\n",l->name,a[0].p.x,a[0].p.tick,layer);free(history);return 1;}
complete:
    if(finish>=0){history[(size_t)layer*WIDTH]=b[finish].e;finish=0;break;}
    int keep=n>WIDTH?WIDTH:n;
    for(int i=0;i<keep;i++){int chosen=(int)((int64_t)i*n/keep);a[i]=b[chosen];history[(size_t)layer*WIDTH+i]=a[i].e;}
    count=keep;
    if(layer%1000==0)fprintf(stderr,"%s: %d%%, %d reachable states\n",l->name,player_progress(&a[0].p,l),count);
  }
  if(finish<0){free(history);return 1;}
  uint8_t inputs[LAYERS];int node=finish;
  for(int j=layer;j>=0;j--){Edge e=history[(size_t)j*WIDTH+node];inputs[j]=e.down;node=e.parent;}
  char path[256];snprintf(path,sizeof(path),"tests/replays/level%d.txt",index+1);FILE *f=fopen(path,"w");if(!f){free(history);return 2;}
  int previous=0;for(int j=0;j<=layer;j++)if(inputs[j]!=previous){fprintf(f,"input=%d,%d\n",j*STEP+1,inputs[j]);previous=inputs[j];}fclose(f);
  printf("COMPLETE: %s, %d ticks -> %s\n",l->name,(layer+1)*STEP,path);free(history);return 0;
}
