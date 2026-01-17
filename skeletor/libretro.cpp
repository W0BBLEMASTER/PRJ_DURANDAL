#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <math.h>
#include <vector>
#include <string>
#include <fstream>
#include <new>

#include "libretro.h"

// SDL Includes
#include <SDL2/SDL.h>

// Aleph One Includes
#include "cseries.h"
#include "shell.h"
#include "shell_options.h"
#include "Logging.h"
#include "screen.h"
#include "interface.h"
#include "player.h"

#define VIDEO_WIDTH 640
#define VIDEO_HEIGHT 480

// Sized delete operator fallback for older MinGW/C++ runtimes
#if __cplusplus >= 201402L
void operator delete(void* ptr, std::size_t size) noexcept { 
    (void)size; 
    ::operator delete(ptr);
}
#endif

// --- Callback Declarations ---
static struct retro_log_callback logging;
static retro_log_printf_t log_cb;
static retro_video_refresh_t video_cb;
static retro_audio_sample_t audio_cb;
static retro_audio_sample_batch_t audio_batch_cb;
static retro_input_poll_t input_poll_cb;
static retro_input_state_t input_state_cb;
static retro_environment_t environ_cb;

// Externs from Aleph One
SDL_Surface *MainScreenSurface();
void increment_heartbeat_count(int value);
extern SDL_Surface *world_pixels;
extern SDL_Surface *Intro_Buffer;

extern "C" {
    extern void (*libretro_log_hook)(int level, const char *fmt, ...);
}

// --- Embedded Data (REMOVED FOR STANDALONE) ---
// extern "C" {
//     extern const char m2d_zip_start[];
//     extern const char m2d_zip_end[];
// }

static void fallback_log(enum retro_log_level level, const char *fmt, ...)
{
   (void)level;
   va_list va;
   va_start(va, fmt);
   vfprintf(stderr, fmt, va);
   va_end(va);
}

RETRO_API void retro_init(void) 
{
    #ifdef _WIN32
    _putenv("SDL_VIDEODRIVER=dummy");
    _putenv("SDL_AUDIODRIVER=dummy");
    #else
    setenv("SDL_VIDEODRIVER", "dummy", 1);
    setenv("SDL_AUDIODRIVER", "dummy", 1);
    #endif

    if (log_cb) log_cb(RETRO_LOG_INFO, "PRJ_WRKN: Standalone Aleph One Core Initialized.\n");
}

RETRO_API void retro_deinit()
{
   try {
      shutdown_application();
   } catch (...) {}
}

RETRO_API unsigned retro_api_version(void) { return RETRO_API_VERSION; }
RETRO_API void retro_set_controller_port_device(unsigned port, unsigned device) { (void)port; (void)device; }

RETRO_API void retro_get_system_info(struct retro_system_info *info)
{
   memset(info, 0, sizeof(*info));
   info->library_name     = "DURANDAL (Standalone)";
   info->library_version  = "1.0-STND";
   info->need_fullpath    = true;
   info->valid_extensions = "zip|sceA|wad";
}

RETRO_API void retro_get_system_av_info(struct retro_system_av_info *info)
{
   info->geometry.base_width   = VIDEO_WIDTH;
   info->geometry.base_height  = VIDEO_HEIGHT;
   info->geometry.max_width    = VIDEO_WIDTH;
   info->geometry.max_height   = VIDEO_HEIGHT;
   info->geometry.aspect_ratio = 4.0f / 3.0f;
   info->timing.fps            = 60.0;
   info->timing.sample_rate    = 44100.0;
}

RETRO_API void retro_set_environment(retro_environment_t cb)
{
   environ_cb = cb;
   bool no_game = true; // Allow 'Start Core' without external content
   cb(RETRO_ENVIRONMENT_SET_SUPPORT_NO_GAME, &no_game);
   if (cb(RETRO_ENVIRONMENT_GET_LOG_INTERFACE, &logging)) {
      log_cb = logging.log;
      libretro_log_hook = (void (*)(int, const char*, ...))log_cb;
   }
   else log_cb = fallback_log;
}

RETRO_API void retro_set_audio_sample(retro_audio_sample_t cb) { audio_cb = cb; }
RETRO_API void retro_set_audio_sample_batch(retro_audio_sample_batch_t cb) { audio_batch_cb = cb; }
RETRO_API void retro_set_input_poll(retro_input_poll_t cb) { input_poll_cb = cb; }
RETRO_API void retro_set_input_state(retro_input_state_t cb) { input_state_cb = cb; }
RETRO_API void retro_set_video_refresh(retro_video_refresh_t cb) { video_cb = cb; }
RETRO_API void retro_reset(void) {}

struct joy_map {
    int retro_id;
    SDL_Scancode scancode;
};

static joy_map input_map[] = {
    { RETRO_DEVICE_ID_JOYPAD_UP,    SDL_SCANCODE_UP },
    { RETRO_DEVICE_ID_JOYPAD_DOWN,  SDL_SCANCODE_DOWN },
    { RETRO_DEVICE_ID_JOYPAD_LEFT,  SDL_SCANCODE_LEFT },
    { RETRO_DEVICE_ID_JOYPAD_RIGHT, SDL_SCANCODE_RIGHT },
    { RETRO_DEVICE_ID_JOYPAD_A,     SDL_SCANCODE_RETURN },
    { RETRO_DEVICE_ID_JOYPAD_B,     SDL_SCANCODE_ESCAPE },
    { RETRO_DEVICE_ID_JOYPAD_X,     SDL_SCANCODE_SPACE },
    { RETRO_DEVICE_ID_JOYPAD_Y,     SDL_SCANCODE_LSHIFT },
    { RETRO_DEVICE_ID_JOYPAD_START, SDL_SCANCODE_RETURN },
};

static bool last_state[RETRO_DEVICE_ID_JOYPAD_R3 + 1] = {false};
static bool last_mouse_l = false;
static bool last_mouse_r = false;

// Global key state shared with Aleph One VBL
extern "C" uint8_t libretro_keys[512] = {0};

// EMULATED CURSOR STATE
extern "C" bool libretro_cursor_visible = false;
static float emu_cursor_x = VIDEO_WIDTH / 2.0f;
static float emu_cursor_y = VIDEO_HEIGHT / 2.0f;

// GHOSTING FIX STATE
static uint32_t cursor_backup[100] = {0}; 
static int last_drawn_x = -1;
static int last_drawn_y = -1;
static SDL_Surface* last_drawn_surface = NULL;

extern "C" void libretro_trace(const char* msg)
{
    if (log_cb) log_cb(RETRO_LOG_INFO, "[TRACE] %s\n", msg);
}

extern "C" void retro_loop_tick(void);

static void render_software_cursor(SDL_Surface* surface, int x, int y)
{
    if (!surface || !surface->pixels) return;
    if (surface->format->BytesPerPixel != 4) return;

    uint32_t* pixels = (uint32_t*)surface->pixels;
    int pitch = surface->pitch / 4;

    // 1. RESTORE previous area
    if (last_drawn_surface == surface && last_drawn_x != -1) {
        for (int i = 0; i < 10; i++) {
            for (int j = 0; j < 10; j++) {
                int px = last_drawn_x + j;
                int py = last_drawn_y + i;
                if (px >= 0 && px < surface->w && py >= 0 && py < surface->h) {
                    pixels[py * pitch + px] = cursor_backup[i * 10 + j];
                }
            }
        }
    }

    // 2. SAVE current area
    for (int i = 0; i < 10; i++) {
        for (int j = 0; j < 10; j++) {
            int px = x + j;
            int py = y + i;
            if (px >= 0 && px < surface->w && py >= 0 && py < surface->h) {
                cursor_backup[i * 10 + j] = pixels[py * pitch + px];
            }
        }
    }
    last_drawn_x = x;
    last_drawn_y = y;
    last_drawn_surface = surface;

    // 3. DRAW the arrow
    uint32_t white = 0xFFFFFFFF;
    uint32_t black = 0xFF000000;
    for (int i = 0; i < 10; i++) {
        for (int j = 0; j <= i; j++) {
            int px = x + j;
            int py = y + i;
            if (px >= 0 && px < surface->w && py >= 0 && py < surface->h) {
                if (j == i || j == 0 || i == 9)
                    pixels[py * pitch + px] = black;
                else
                    pixels[py * pitch + px] = white;
            }
        }
    }
}

// NON-BLOCKING YIELD HOOK
extern "C" void libretro_yield(void)
{
    static bool yielding = false;
    if (yielding) return; // Prevent recursive yielding
    yielding = true;

    if (input_poll_cb) input_poll_cb();
    
    // Process input changes for SDL events
    if (input_state_cb) {
        for (int i = 0; i < 9; i++) {
            bool down = input_state_cb(0, RETRO_DEVICE_JOYPAD, 0, input_map[i].retro_id);
            if (input_map[i].scancode < 512) libretro_keys[input_map[i].scancode] = down ? 1 : 0;
            if (down != last_state[input_map[i].retro_id]) {
                SDL_Event event;
                memset(&event, 0, sizeof(event));
                event.type = down ? SDL_KEYDOWN : SDL_KEYUP;
                event.key.keysym.scancode = input_map[i].scancode;
                event.key.state = down ? SDL_PRESSED : SDL_RELEASED;
                SDL_PushEvent(&event);
                last_state[input_map[i].retro_id] = down;
            }
        }
    }

    // Refresh video so the screen doesn't freeze
    if (video_cb) {
        SDL_Surface* target = world_pixels ? world_pixels : Intro_Buffer;
        if (!target) target = MainScreenSurface();
        if (target && target->pixels) {
            video_cb(target->pixels, target->w, target->h, target->pitch);
        } else {
            static uint32_t dummy[VIDEO_WIDTH * VIDEO_HEIGHT] = {0};
            video_cb(dummy, VIDEO_WIDTH, VIDEO_HEIGHT, VIDEO_WIDTH * sizeof(uint32_t));
        }
    }

    yielding = false;
}

RETRO_API void retro_run(void)
{
   static long frame_count = 0;
   frame_count++;

   // LOGGING: Heartbeat every frame (limited to avoiding disk spam after startup, but aggressive initially)
   if (log_cb && (frame_count < 600 || frame_count % 60 == 0)) {
       log_cb(RETRO_LOG_INFO, "[PRJ_WRKN] === FRAME %ld START ===\n", frame_count);
   }

   // 1. Poll Inputs
   if (input_poll_cb) input_poll_cb();
   
   // TRACK EMULATED CURSOR
   {
       int16_t mouse_dx = input_state_cb(0, RETRO_DEVICE_MOUSE, 0, RETRO_DEVICE_ID_MOUSE_X);
       int16_t mouse_dy = input_state_cb(0, RETRO_DEVICE_MOUSE, 0, RETRO_DEVICE_ID_MOUSE_Y);
       
       // JOYPAD EMULATION FOR MOUSE
       if (mouse_dx == 0 && mouse_dy == 0) {
           int16_t joy_x = 0;
           int16_t joy_y = 0;
           if (input_state_cb(0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_RIGHT)) joy_x += 5;
           if (input_state_cb(0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_LEFT))  joy_x -= 5;
           if (input_state_cb(0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_DOWN))  joy_y += 5;
           if (input_state_cb(0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_UP))    joy_y -= 5;
           mouse_dx = joy_x;
           mouse_dy = joy_y;
       }

       emu_cursor_x += (float)mouse_dx;
       emu_cursor_y += (float)mouse_dy;
       
       // Clamp to screen
       if (emu_cursor_x < 0) emu_cursor_x = 0;
       if (emu_cursor_x >= VIDEO_WIDTH) emu_cursor_x = VIDEO_WIDTH - 1;
       if (emu_cursor_y < 0) emu_cursor_y = 0;
       if (emu_cursor_y >= VIDEO_HEIGHT) emu_cursor_y = VIDEO_HEIGHT - 1;

       // If cursor is visible, feed position back to Aleph One via SDL events
       if (libretro_cursor_visible) {
           // MOVEMENT
           SDL_Event move_ev;
           memset(&move_ev, 0, sizeof(move_ev));
           move_ev.type = SDL_MOUSEMOTION;
           move_ev.motion.x = (int)emu_cursor_x;
           move_ev.motion.y = (int)emu_cursor_y;
           move_ev.motion.xrel = mouse_dx;
           move_ev.motion.yrel = mouse_dy;
           SDL_PushEvent(&move_ev);

           // CLICKS (Left Mouse or Joypad A)
           static bool last_click = false;
           bool click = input_state_cb(0, RETRO_DEVICE_MOUSE, 0, RETRO_DEVICE_ID_MOUSE_LEFT) ||
                        input_state_cb(0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_A);
           
           if (click != last_click) {
               SDL_Event click_ev;
               memset(&click_ev, 0, sizeof(click_ev));
               click_ev.type = click ? SDL_MOUSEBUTTONDOWN : SDL_MOUSEBUTTONUP;
               click_ev.button.button = SDL_BUTTON_LEFT;
               click_ev.button.state = click ? SDL_PRESSED : SDL_RELEASED;
               click_ev.button.x = (int)emu_cursor_x;
               click_ev.button.y = (int)emu_cursor_y;
               SDL_PushEvent(&click_ev);
               last_click = click;
           }
       }
   }

   // LOGGING: Check Input State directly
   if (log_cb && frame_count < 600) {
       // Check a few key buttons to see if RA is sending ANYTHING
       int16_t mouse_x = input_state_cb(0, RETRO_DEVICE_MOUSE, 0, RETRO_DEVICE_ID_MOUSE_X);
       bool start_btn = input_state_cb(0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_START);
       if (mouse_x != 0 || start_btn) {
           log_cb(RETRO_LOG_INFO, "[PRJ_WRKN] INPUT DETECTED: MouseX=%d, Start=%d\n", mouse_x, start_btn);
       }
   }

   SDL_PumpEvents();

   // 2. Handle Inputs (Keyboard + Mouse)
   if (input_state_cb) {
       // ... (Keep existing input mapping logic, but maybe add logs inside the loop if keys change)
       for (int i = 0; i < 9; i++) {
           bool down = input_state_cb(0, RETRO_DEVICE_JOYPAD, 0, input_map[i].retro_id);
           if (down != last_state[input_map[i].retro_id]) {
               if (log_cb) log_cb(RETRO_LOG_INFO, "[PRJ_WRKN] KEY STATE CHANGE: RetroID %d -> %d\n", input_map[i].retro_id, down);
               // ... existing SDL event pushing ...
               SDL_Event event;
               memset(&event, 0, sizeof(event));
               event.type = down ? SDL_KEYDOWN : SDL_KEYUP;
               event.key.keysym.scancode = input_map[i].scancode;
               event.key.keysym.sym = SDL_GetKeyFromScancode(input_map[i].scancode);
               event.key.state = down ? SDL_PRESSED : SDL_RELEASED;
               SDL_PushEvent(&event);
               last_state[input_map[i].retro_id] = down;
           }
       }
       // ... (Keep mouse logic) ...
   }

   try {
      // 3. Tick the Engine
      // LOGGING: Verify engine tick call
      // if (log_cb && frame_count < 600) log_cb(RETRO_LOG_INFO, "[PRJ_WRKN] Calling retro_loop_tick...\n");
      
      SDL_Event ev;
      memset(&ev, 0, sizeof(ev));
      ev.type = SDL_WINDOWEVENT;
      ev.window.event = SDL_WINDOWEVENT_EXPOSED;
      SDL_PushEvent(&ev);

      retro_loop_tick();
      global_idle_proc();
      increment_heartbeat_count(1);
      
      // 4. Graphics Hook (The "Works")
      if (video_cb) {
          SDL_Surface* target = NULL;
          short state = get_game_state();

          // If we are in a menu, chooser, or intro, use MainScreenSurface
          if (state < _game_in_progress || state == _displaying_network_game_dialogs) {
              target = MainScreenSurface();
          } else {
              target = world_pixels ? world_pixels : Intro_Buffer;
              if (!target) target = MainScreenSurface();
          }

          if (target && target->pixels) {
              if (libretro_cursor_visible) {
                  render_software_cursor(target, (int)emu_cursor_x, (int)emu_cursor_y);
              }
              video_cb(target->pixels, target->w, target->h, target->pitch);
          } else {
              static uint32_t dummy[VIDEO_WIDTH * VIDEO_HEIGHT] = {0};
              video_cb(dummy, VIDEO_WIDTH, VIDEO_HEIGHT, VIDEO_WIDTH * sizeof(uint32_t));
          }
      }
   } catch (...) {
       if (log_cb) log_cb(RETRO_LOG_ERROR, "PRJ_WRKN: CRITICAL EXCEPTION in retro_run\n");
   }
}

RETRO_API bool retro_load_game(const struct retro_game_info *info)
{
   enum retro_pixel_format fmt = RETRO_PIXEL_FORMAT_XRGB8888;
   if (!environ_cb(RETRO_ENVIRONMENT_SET_PIXEL_FORMAT, &fmt)) {
       if (log_cb) log_cb(RETRO_LOG_WARN, "PRJ_WRKN: XRGB8888 rejected, trying RGB565 fallback.\n");
       fmt = RETRO_PIXEL_FORMAT_RGB565;
       environ_cb(RETRO_ENVIRONMENT_SET_PIXEL_FORMAT, &fmt);
   }

   try {
      std::vector<const char*> args;
      args.push_back("alephone");
      args.push_back("--nosound"); // Keep nosound for now to avoid audio driver conflicts during init

      if (info && info->path) {
          // Content loaded via file browser
          args.push_back("--no-chooser");
          args.push_back("--skip-intro");
          args.push_back(info->path);
          if (log_cb) log_cb(RETRO_LOG_INFO, "PRJ_WRKN: Loading content: %s\n", info->path);
      } else {
          // "Start Core" clicked - Allow engine to handle discovery
          if (log_cb) log_cb(RETRO_LOG_INFO, "PRJ_WRKN: No content provided. Entering default startup flow.\n");
          // args.push_back("Marathon 2"); // Don't force a path, let it search
      }

      shell_options.parse(args.size(), (char**)args.data());
      initialize_application();
      
      if (info && info->path) handle_open_document(info->path);

   } catch (std::exception& e) {
      if (log_cb) log_cb(RETRO_LOG_ERROR, "PRJ_WRKN: Init Error: %s\n", e.what());
      return false;
   }
   return true;
}

RETRO_API void retro_unload_game(void) {}
RETRO_API unsigned retro_get_region(void) { return RETRO_REGION_NTSC; }
RETRO_API bool retro_load_game_special(unsigned type, const struct retro_game_info *info, size_t num) { return false; }
RETRO_API size_t retro_serialize_size(void) { return 0; }
RETRO_API bool retro_serialize(void *data_, size_t size) { return false; }
RETRO_API bool retro_unserialize(const void *data_, size_t size) { return false; }
RETRO_API void *retro_get_memory_data(unsigned id) { return NULL; }
RETRO_API size_t retro_get_memory_size(unsigned id) { return 0; }
RETRO_API void retro_cheat_reset(void) {}
RETRO_API void retro_cheat_set(unsigned index, bool enabled, const char *code) {}
