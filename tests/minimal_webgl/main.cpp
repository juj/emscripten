#include "webgl.h"
#include <math.h>

#define WIDTH 1024
#define HEIGHT 768

// Per-frame animation tick.
void draw_frame(double t, double dt)
{
  clear_screen(0.1f, 0.2f, 0.3f, 1.f);

  // snow background
#define NUM_FLAKES 100
#define FLAKE_SIZE 10
#define FLAKE_SPEED 0.05f
#define SNOWINESS 0.998
  static struct { float x, y; } flakes[NUM_FLAKES] = {};

#define SIM do { \
      flakes[i].y -= dt*(FLAKE_SPEED+i*0.05f/NUM_FLAKES); \
      flakes[i].x += dt*(fmodf(i*345362.f, 0.02f) - 0.01f); \
      float c = 0.5f + i*0.5/NUM_FLAKES; \
      if (flakes[i].y > -FLAKE_SIZE) fill_solid_rectangle(flakes[i].x, flakes[i].y, flakes[i].x+FLAKE_SIZE, flakes[i].y+FLAKE_SIZE, c, c, c, 1.f); \
      else if (rand01() > SNOWINESS) flakes[i].y = HEIGHT, flakes[i].x = WIDTH*rand01(); \
    } while(0)
  for(int i = 0; i < NUM_FLAKES/2; ++i) SIM;

  // flagpole x,y,width,height
#define FPX 50.f
#define FPW 25.f
#define FPH (HEIGHT-75.f)
  fill_solid_rectangle(FPX, 0.f, FPX+FPW, FPH, 0, 0, 0, 1.f);
  // flag x,y,width,height
#define FX 80
#define FH 200
#define FY (FPH-FH)
#define FW (18.f/11.f*FH)
  // flag grid
#define GX 18
#define GY 11
  // block size
#define BX ((double)FW/GX)
#define BY ((double)FH/GY)
  // color
#define COLOR(gx, gy) (((gx >= 5 && gx <= 7) || (gy >= 4 && gy <= 6)) ? 1 : 0)
#define MIN(x, y) (((x) < (y)) ? (x) : (y))
  for(int y = 0; y < GY; ++y)
    for(int x = 0; x < GX; ++x)
    {
      int c = COLOR(x, y);
      float wy = sinf(0.3f*(x+t*0.01f))*GY*0.5f*(MIN((float)x*3.f/GX,1.f));
      fill_solid_rectangle(FX+x*BX, FY+y*BY+wy, FX+(x+1)*BX, FY+(y+1)*BY+wy, c?0.f:1.f, c?47/255.f:1.f, c?108/255.f:1.f, 1.f);
    }

  // ground
  fill_solid_rectangle(0.f, 0.f, WIDTH, 20.f, 0.8f, 0.8f, 0.8f, 1.f);

  // tree & reindeer
  fill_image(WIDTH-320.f, 0.f, 2.f, 1.f, 1.f, 1.f, 1.f, "tree.png");
  fill_image(250.f, 10.f, 1.f, 1.f, 1.f, 1.f, 1.f, "reindeer.png");

  // snow foreground
  for(int i = NUM_FLAKES/2; i < NUM_FLAKES; ++i) SIM;

  // text
  fill_text(190.f, HEIGHT*2/5+80.f, 1.f, 0.f, 0.f, 1.f, "MERRY XMAS", 64.f, 64, true);
}

int main()
{
  init_webgl(WIDTH, HEIGHT);
  set_animation_frame_callback(&draw_frame);
  play_audio("song.mp3", /*loop=*/1);
}
