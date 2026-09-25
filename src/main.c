#include "app.h"
#include "draw.h"
#if PLATFORM_DEVICE
#include <eadk.h>
const char eadk_app_name[] __attribute__((section(".rodata.eadk_app_name")))="NumDash";
const uint32_t eadk_api_level __attribute__((section(".rodata.eadk_api_level")))=0;
#endif

int main(int argc,char **argv) {
  (void)argc;(void)argv;
  if(!platform_init())return 1;
  app_init();
  uint32_t previous=platform_millis(),accumulator=0,render_acc=1000,fps_start=previous,frames=0;
  uint32_t last_keys=0,pending_keys=0;
  while(app.running) {
    uint32_t now=platform_millis(),elapsed=now-previous;previous=now;
    /* An OS stall must not simulate unseen hazards. Ordinary rendering
     * overruns catch up without changing the movement speed. */
    if(elapsed>100) {elapsed=0;if(app.screen==PLAY&&!app.player.dead){app.screen=PAUSE;app.menu=2;}}
    accumulator+=elapsed*ND_HZ;render_acc+=elapsed*60;
    uint32_t keys=platform_keys();
    pending_keys|=keys&~last_keys;
    last_keys=keys;
    while(accumulator>=1000){app_tick(keys|pending_keys);pending_keys=0;accumulator-=1000;}
    if(render_acc>=1000){app_render();platform_present(frame,palette);render_acc%=1000;frames++;}
    if(now-fps_start>=1000){app.fps=frames*1000/(now-fps_start);frames=0;fps_start=now;}
    platform_sleep(1);
  }
  platform_close();return 0;
}
