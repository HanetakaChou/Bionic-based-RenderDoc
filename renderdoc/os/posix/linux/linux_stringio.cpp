/******************************************************************************
 * The MIT License (MIT)
 *
 * Copyright (c) 2019-2020 Baldur Karlsson
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 ******************************************************************************/

#include <ctype.h>
#include <dlfcn.h>
#include <errno.h>
#include <pwd.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <time.h>
#include <unistd.h>
#include <set>
#include <byteswap.h>
#include "api/app/renderdoc_app.h"
#include "api/replay/replay_enums.h"
#include "common/common.h"
#include "common/threading.h"
#include "os/os_specific.h"
#include "strings/string_utils.h"

#if ENABLED(RDOC_XLIB)
#include <X11/Xlib.h>
#include <X11/keysym.h>
#else
typedef struct _XDisplay Display;
#endif

#if ENABLED(RDOC_XCB)
#include <X11/keysym.h>
#include <xcb/xcb_keysyms.h>
#else
struct xcb_connection_t;
#endif

#if ENABLED(RDOC_WAYLAND)
#include <linux/input.h>
#include <wayland-client.h>
#include <map>
#else
struct wl_display;
struct wl_surface;
#endif

namespace Keyboard
{
  void Init()
  {
  }

  bool PlatformHasKeyInput()
  {
#if ENABLED(RDOC_XCB) || ENABLED(RDOC_XLIB) || ENABLED(RDOC_WAYLAND)
    return true;
#else
    return false;
#endif
  }

#if ENABLED(RDOC_XLIB)

  Display *CurrentXDisplay = NULL;

  void UseXlibDisplay(Display *dpy)
  {
    if (CurrentXDisplay || dpy == NULL)
      return;

    CurrentXDisplay = XOpenDisplay(XDisplayString(dpy));
  }

  bool GetXlibKeyState(int key)
  {
    if (CurrentXDisplay == NULL)
      return false;

    KeySym ks = 0;

    if (key >= eRENDERDOC_Key_A && key <= eRENDERDOC_Key_Z)
      ks = key;
    if (key >= eRENDERDOC_Key_0 && key <= eRENDERDOC_Key_9)
      ks = key;

    switch (key)
    {
    case eRENDERDOC_Key_Divide:
      ks = XK_KP_Divide;
      break;
    case eRENDERDOC_Key_Multiply:
      ks = XK_KP_Multiply;
      break;
    case eRENDERDOC_Key_Subtract:
      ks = XK_KP_Subtract;
      break;
    case eRENDERDOC_Key_Plus:
      ks = XK_KP_Add;
      break;
    case eRENDERDOC_Key_F1:
      ks = XK_F1;
      break;
    case eRENDERDOC_Key_F2:
      ks = XK_F2;
      break;
    case eRENDERDOC_Key_F3:
      ks = XK_F3;
      break;
    case eRENDERDOC_Key_F4:
      ks = XK_F4;
      break;
    case eRENDERDOC_Key_F5:
      ks = XK_F5;
      break;
    case eRENDERDOC_Key_F6:
      ks = XK_F6;
      break;
    case eRENDERDOC_Key_F7:
      ks = XK_F7;
      break;
    case eRENDERDOC_Key_F8:
      ks = XK_F8;
      break;
    case eRENDERDOC_Key_F9:
      ks = XK_F9;
      break;
    case eRENDERDOC_Key_F10:
      ks = XK_F10;
      break;
    case eRENDERDOC_Key_F11:
      ks = XK_F11;
      break;
    case eRENDERDOC_Key_F12:
      ks = XK_F12;
      break;
    case eRENDERDOC_Key_Home:
      ks = XK_Home;
      break;
    case eRENDERDOC_Key_End:
      ks = XK_End;
      break;
    case eRENDERDOC_Key_Insert:
      ks = XK_Insert;
      break;
    case eRENDERDOC_Key_Delete:
      ks = XK_Delete;
      break;
    case eRENDERDOC_Key_PageUp:
      ks = XK_Prior;
      break;
    case eRENDERDOC_Key_PageDn:
      ks = XK_Next;
      break;
    case eRENDERDOC_Key_Backspace:
      ks = XK_BackSpace;
      break;
    case eRENDERDOC_Key_Tab:
      ks = XK_Tab;
      break;
    case eRENDERDOC_Key_PrtScrn:
      ks = XK_Print;
      break;
    case eRENDERDOC_Key_Pause:
      ks = XK_Pause;
      break;
    default:
      break;
    }

    if (ks == 0)
      return false;

    KeyCode kc = XKeysymToKeycode(CurrentXDisplay, ks);

    char keyState[32];
    XQueryKeymap(CurrentXDisplay, keyState);

    int byteIdx = (kc / 8);
    int bitMask = 1 << (kc % 8);

    uint8_t keyByte = (uint8_t)keyState[byteIdx];

    return (keyByte & bitMask) != 0;
  }

#else

  // if RENDERDOC_WINDOWING_XLIB is not enabled

  void UseXlibDisplay(Display *dpy)
  {
  }

  bool GetXlibKeyState(int key)
  {
    return false;
  }

#endif

#if ENABLED(RDOC_XCB)

  xcb_connection_t *connection;
  xcb_key_symbols_t *symbols;

  void UseXcbConnection(xcb_connection_t *conn)
  {
    connection = conn;
    symbols = xcb_key_symbols_alloc(conn);
  }

  bool GetXCBKeyState(int key)
  {
    if (symbols == NULL)
      return false;

    xcb_keysym_t ks = 0;

    if (key >= eRENDERDOC_Key_A && key <= eRENDERDOC_Key_Z)
      ks = key;
    if (key >= eRENDERDOC_Key_0 && key <= eRENDERDOC_Key_9)
      ks = key;

    switch (key)
    {
    case eRENDERDOC_Key_Divide:
      ks = XK_KP_Divide;
      break;
    case eRENDERDOC_Key_Multiply:
      ks = XK_KP_Multiply;
      break;
    case eRENDERDOC_Key_Subtract:
      ks = XK_KP_Subtract;
      break;
    case eRENDERDOC_Key_Plus:
      ks = XK_KP_Add;
      break;
    case eRENDERDOC_Key_F1:
      ks = XK_F1;
      break;
    case eRENDERDOC_Key_F2:
      ks = XK_F2;
      break;
    case eRENDERDOC_Key_F3:
      ks = XK_F3;
      break;
    case eRENDERDOC_Key_F4:
      ks = XK_F4;
      break;
    case eRENDERDOC_Key_F5:
      ks = XK_F5;
      break;
    case eRENDERDOC_Key_F6:
      ks = XK_F6;
      break;
    case eRENDERDOC_Key_F7:
      ks = XK_F7;
      break;
    case eRENDERDOC_Key_F8:
      ks = XK_F8;
      break;
    case eRENDERDOC_Key_F9:
      ks = XK_F9;
      break;
    case eRENDERDOC_Key_F10:
      ks = XK_F10;
      break;
    case eRENDERDOC_Key_F11:
      ks = XK_F11;
      break;
    case eRENDERDOC_Key_F12:
      ks = XK_F12;
      break;
    case eRENDERDOC_Key_Home:
      ks = XK_Home;
      break;
    case eRENDERDOC_Key_End:
      ks = XK_End;
      break;
    case eRENDERDOC_Key_Insert:
      ks = XK_Insert;
      break;
    case eRENDERDOC_Key_Delete:
      ks = XK_Delete;
      break;
    case eRENDERDOC_Key_PageUp:
      ks = XK_Prior;
      break;
    case eRENDERDOC_Key_PageDn:
      ks = XK_Next;
      break;
    case eRENDERDOC_Key_Backspace:
      ks = XK_BackSpace;
      break;
    case eRENDERDOC_Key_Tab:
      ks = XK_Tab;
      break;
    case eRENDERDOC_Key_PrtScrn:
      ks = XK_Print;
      break;
    case eRENDERDOC_Key_Pause:
      ks = XK_Pause;
      break;
    default:
      break;
    }

    if (ks == 0)
      return false;

    xcb_keycode_t *keyCodes = xcb_key_symbols_get_keycode(symbols, ks);

    if (!keyCodes)
      return false;

    xcb_query_keymap_cookie_t keymapcookie = xcb_query_keymap(connection);
    xcb_query_keymap_reply_t *keys = xcb_query_keymap_reply(connection, keymapcookie, NULL);

    bool ret = false;

    if (keys && keyCodes[0] != XCB_NO_SYMBOL)
    {
      int byteIdx = (keyCodes[0] / 8);
      int bitMask = 1 << (keyCodes[0] % 8);

      ret = (keys->keys[byteIdx] & bitMask) != 0;
    }

    free(keyCodes);
    free(keys);

    return ret;
  }

#else

  // if RENDERDOC_WINDOWING_XCB is not enabled

  void UseXcbConnection(xcb_connection_t *conn)
  {
  }

  bool GetXCBKeyState(int key)
  {
    return false;
  }

#endif

#if ENABLED(RDOC_WAYLAND)

#include <wayland-client.h>

  std::set<wl_display *> displays;
  std::set<wl_surface *> surfaces;
  std::map<rdcpair<wl_registry *, uint32_t>, wl_seat *> seatNames;
  std::map<wl_seat *, wl_keyboard *> seatKeyboard;
  bool inFocus = false;
  Threading::CriticalSection waylandLock;

  bool keyState[eRENDERDOC_Key_Max] = {};

  void WaylandKeymapDummy(void *data, wl_keyboard *keyboard, uint32_t format, int fd, uint32_t size)
  {
  }

  void WaylandModifiersDummy(void *data, wl_keyboard *keyboard, uint32_t serial,
                             uint32_t mods_depressed, uint32_t mods_latched, uint32_t mods_locked,
                             uint32_t group)
  {
  }

  void WaylandRepeatInfoDummy(void *data, wl_keyboard *keyboard, int32_t rate, int32_t delay)
  {
  }

  void WaylandEnter(void *data, wl_keyboard *keyboard, uint32_t serial, wl_surface *surf, wl_array *keys)
  {
    SCOPED_LOCK(waylandLock);
    inFocus = surfaces.find(surf) != surfaces.end();
    RDCEraseEl(keyState);
  }

  void WaylandLeave(void *data, wl_keyboard *keyboard, uint32_t serial, wl_surface *surf)
  {
    SCOPED_LOCK(waylandLock);
    inFocus = false;
    RDCEraseEl(keyState);
  }

  void WaylandKeypress(void *data, wl_keyboard *keyboard, uint32_t serial, uint32_t time,
                       uint32_t key, uint32_t state)
  {
    int keyIdx = -1;
    switch (key)
    {
    case KEY_0:
      keyIdx = eRENDERDOC_Key_0;
      break;
    case KEY_1:
      keyIdx = eRENDERDOC_Key_1;
      break;
    case KEY_2:
      keyIdx = eRENDERDOC_Key_2;
      break;
    case KEY_3:
      keyIdx = eRENDERDOC_Key_3;
      break;
    case KEY_4:
      keyIdx = eRENDERDOC_Key_4;
      break;
    case KEY_5:
      keyIdx = eRENDERDOC_Key_5;
      break;
    case KEY_6:
      keyIdx = eRENDERDOC_Key_6;
      break;
    case KEY_7:
      keyIdx = eRENDERDOC_Key_7;
      break;
    case KEY_8:
      keyIdx = eRENDERDOC_Key_8;
      break;
    case KEY_9:
      keyIdx = eRENDERDOC_Key_9;
      break;
    case KEY_A:
      keyIdx = eRENDERDOC_Key_A;
      break;
    case KEY_B:
      keyIdx = eRENDERDOC_Key_B;
      break;
    case KEY_C:
      keyIdx = eRENDERDOC_Key_C;
      break;
    case KEY_D:
      keyIdx = eRENDERDOC_Key_D;
      break;
    case KEY_E:
      keyIdx = eRENDERDOC_Key_E;
      break;
    case KEY_F:
      keyIdx = eRENDERDOC_Key_F;
      break;
    case KEY_G:
      keyIdx = eRENDERDOC_Key_G;
      break;
    case KEY_H:
      keyIdx = eRENDERDOC_Key_H;
      break;
    case KEY_I:
      keyIdx = eRENDERDOC_Key_I;
      break;
    case KEY_J:
      keyIdx = eRENDERDOC_Key_J;
      break;
    case KEY_K:
      keyIdx = eRENDERDOC_Key_K;
      break;
    case KEY_L:
      keyIdx = eRENDERDOC_Key_L;
      break;
    case KEY_M:
      keyIdx = eRENDERDOC_Key_M;
      break;
    case KEY_N:
      keyIdx = eRENDERDOC_Key_N;
      break;
    case KEY_O:
      keyIdx = eRENDERDOC_Key_O;
      break;
    case KEY_P:
      keyIdx = eRENDERDOC_Key_P;
      break;
    case KEY_Q:
      keyIdx = eRENDERDOC_Key_Q;
      break;
    case KEY_R:
      keyIdx = eRENDERDOC_Key_R;
      break;
    case KEY_S:
      keyIdx = eRENDERDOC_Key_S;
      break;
    case KEY_T:
      keyIdx = eRENDERDOC_Key_T;
      break;
    case KEY_U:
      keyIdx = eRENDERDOC_Key_U;
      break;
    case KEY_V:
      keyIdx = eRENDERDOC_Key_V;
      break;
    case KEY_W:
      keyIdx = eRENDERDOC_Key_W;
      break;
    case KEY_X:
      keyIdx = eRENDERDOC_Key_X;
      break;
    case KEY_Y:
      keyIdx = eRENDERDOC_Key_Y;
      break;
    case KEY_Z:
      keyIdx = eRENDERDOC_Key_Z;
      break;

    case KEY_KPSLASH:
      keyIdx = eRENDERDOC_Key_Divide;
      break;
    case KEY_KPASTERISK:
      keyIdx = eRENDERDOC_Key_Multiply;
      break;
    case KEY_KPMINUS:
      keyIdx = eRENDERDOC_Key_Subtract;
      break;
    case KEY_KPPLUS:
      keyIdx = eRENDERDOC_Key_Plus;
      break;

    case KEY_F1:
      keyIdx = eRENDERDOC_Key_F1;
      break;
    case KEY_F2:
      keyIdx = eRENDERDOC_Key_F2;
      break;
    case KEY_F3:
      keyIdx = eRENDERDOC_Key_F3;
      break;
    case KEY_F4:
      keyIdx = eRENDERDOC_Key_F4;
      break;
    case KEY_F5:
      keyIdx = eRENDERDOC_Key_F5;
      break;
    case KEY_F6:
      keyIdx = eRENDERDOC_Key_F6;
      break;
    case KEY_F7:
      keyIdx = eRENDERDOC_Key_F7;
      break;
    case KEY_F8:
      keyIdx = eRENDERDOC_Key_F8;
      break;
    case KEY_F9:
      keyIdx = eRENDERDOC_Key_F9;
      break;
    case KEY_F10:
      keyIdx = eRENDERDOC_Key_F10;
      break;
    case KEY_F11:
      keyIdx = eRENDERDOC_Key_F11;
      break;
    case KEY_F12:
      keyIdx = eRENDERDOC_Key_F12;
      break;

    case KEY_HOME:
      keyIdx = eRENDERDOC_Key_Home;
      break;
    case KEY_END:
      keyIdx = eRENDERDOC_Key_End;
      break;
    case KEY_INSERT:
      keyIdx = eRENDERDOC_Key_Insert;
      break;
    case KEY_DELETE:
      keyIdx = eRENDERDOC_Key_Delete;
      break;
    case KEY_PAGEUP:
      keyIdx = eRENDERDOC_Key_PageUp;
      break;
    case KEY_PAGEDOWN:
      keyIdx = eRENDERDOC_Key_PageDn;
      break;

    case KEY_BACKSPACE:
      keyIdx = eRENDERDOC_Key_Backspace;
      break;
    case KEY_TAB:
      keyIdx = eRENDERDOC_Key_Tab;
      break;
    case KEY_SYSRQ:
      keyIdx = eRENDERDOC_Key_PrtScrn;
      break;
    case KEY_PAUSE:
      keyIdx = eRENDERDOC_Key_Pause;
      break;
    }

    if (keyIdx < 0)
      return;

    {
      SCOPED_LOCK(waylandLock);
      keyState[keyIdx] = (state == WL_KEYBOARD_KEY_STATE_PRESSED);
    }
  }

  void WaylandSeatCaps(void *data, wl_seat *seat, uint32_t capabilities)
  {
    if (capabilities & WL_SEAT_CAPABILITY_KEYBOARD)
    {
      {
        SCOPED_LOCK(waylandLock);
        if (seatKeyboard[seat])
          return;
      }

      wl_keyboard *keyboard = wl_seat_get_keyboard(seat);
      static const wl_keyboard_listener listener = {
          WaylandKeymapDummy,
          WaylandEnter,
          WaylandLeave,
          WaylandKeypress,
          WaylandModifiersDummy,
          WaylandRepeatInfoDummy,
      };
      wl_keyboard_add_listener(keyboard, &listener, NULL);

      {
        SCOPED_LOCK(waylandLock);
        seatKeyboard[seat] = keyboard;
      }
    }
    else
    {
      wl_keyboard *keyboard = NULL;

      {
        SCOPED_LOCK(waylandLock);
        keyboard = seatKeyboard[seat];

        if (!keyboard)
          return;

        seatKeyboard[seat] = NULL;
      }

      wl_keyboard_destroy(keyboard);
    }
  }

  void WaylandRegistryAdd(void *data, wl_registry *reg, uint32_t name, const char *iface,
                          uint32_t version)
  {
    if (!strcmp(iface, "wl_seat"))
    {
      wl_seat *seat = (wl_seat *)wl_registry_bind(reg, name, &wl_seat_interface, 1);
      static const wl_seat_listener listener = {&WaylandSeatCaps};
      wl_seat_add_listener(seat, &listener, NULL);

      {
        SCOPED_LOCK(waylandLock);
        seatNames[{reg, name}] = seat;
      }
    }
  }

  void WaylandRegistryRemove(void *data, wl_registry *reg, uint32_t name)
  {
    SCOPED_LOCK(waylandLock);
    auto it = seatNames.find({reg, name});
    if (it != seatNames.end())
    {
      wl_seat_destroy(it->second);
      seatNames.erase(it);
    }
  }

  void UseWaylandDisplay(wl_display *disp)
  {
    // only listen to each display once at most
    {
      SCOPED_LOCK(waylandLock);
      if (displays.find(disp) != displays.end())
        return;
      displays.insert(disp);
    }

    static const wl_registry_listener listener = {&WaylandRegistryAdd, &WaylandRegistryRemove};

    // get the registry and listen to it. This will then let us find seats
    wl_registry_add_listener(wl_display_get_registry(disp), &listener, NULL);
  }

  void AddWaylandInputWindow(wl_surface *wnd)
  {
    SCOPED_LOCK(waylandLock);
    surfaces.insert(wnd);
  }

  void RemoveWaylandInputWindow(wl_surface *wnd)
  {
    SCOPED_LOCK(waylandLock);
    surfaces.erase(wnd);
  }

  bool GetWaylandKeyState(int key)
  {
    SCOPED_LOCK(waylandLock);
    return keyState[key];
  }

#else

  void UseWaylandDisplay(wl_display *disp)
  {
  }

  void AddWaylandInputWindow(wl_surface *wnd)
  {
  }

  void RemoveWaylandInputWindow(wl_surface *wnd)
  {
  }

  bool GetWaylandKeyState(int key)
  {
    return false;
  }

#endif

  WindowingSystem UseUnknownDisplay(void *disp)
  {
    // could be wayland or xlib, try to detect.
    // both Display* and wl_display* are valid pointers, so dereference and read the first pointer
    // sized bytes
    void *firstPointer = NULL;
    memcpy(&firstPointer, disp, sizeof(void *));

    // in a Display* we don't know what this contains, but in a wl_display it should point to the
    // wl_display_interface exported symbol. Check with dladdr
    Dl_info info;
    if (dladdr(firstPointer, &info) && !strcmp(info.dli_sname, "wl_display_interface"))
    {
      UseWaylandDisplay((wl_display *)disp);
      return WindowingSystem::Wayland;
    }
    else
    {
      UseXlibDisplay((Display *)disp);
      return WindowingSystem::Xlib;
    }
  }

  void AddInputWindow(WindowingSystem windowSystem, void *wnd)
  {
    if (windowSystem == WindowingSystem::Wayland)
    {
      AddWaylandInputWindow((wl_surface *)wnd);
    }
    else
    {
      // TODO check against this drawable & parent window being focused in GetKeyState
    }
  }

  void RemoveInputWindow(WindowingSystem windowSystem, void *wnd)
  {
    if (windowSystem == WindowingSystem::Wayland)
    {
      RemoveWaylandInputWindow((wl_surface *)wnd);
    }
  }

  bool GetKeyState(int key)
  {
    return GetXCBKeyState(key) || GetXlibKeyState(key) || GetWaylandKeyState(key);
  }
} // namespace Keyboard

namespace FileIO
{
  rdcstr GetTempRootPath()
  {
    return "/tmp";
  }

  rdcstr GetAppFolderFilename(const rdcstr &filename)
  {
    passwd *pw = getpwuid(getuid());
    const char *homedir = pw->pw_dir;

    rdcstr ret = rdcstr(homedir) + "/.renderdoc/";

    mkdir(ret.c_str(), S_IRWXU | S_IRWXG | S_IROTH | S_IXOTH);

    return ret + filename;
  }

  void GetExecutableFilename(rdcstr &selfName)
  {
    char path[512] = {0};
    readlink("/proc/self/exe", path, 511);

    selfName = rdcstr(path);
  }

  int LibraryLocator = 42;

  void GetLibraryFilename(rdcstr &selfName)
  {
    // this is a hack, but the only reliable way to find the absolute path to the library.
    // dladdr would be fine but it returns the wrong result for symbols in the library

    rdcstr librenderdoc_path;

    FILE *f = fopen("/proc/self/maps", "r");

    if (f)
    {
      // read the whole thing in one go. There's no need to try and be tight with
      // this allocation, so just make sure we can read everything.
      char *map_string = new char[1024 * 1024];
      memset(map_string, 0, 1024 * 1024);

      ::fread(map_string, 1, 1024 * 1024, f);

      ::fclose(f);

      char *c = strstr(map_string, "/librenderdoc.so");

      if (c)
      {
        // walk backwards until we hit the start of the line
        while (c > map_string)
        {
          c--;

          if (c[0] == '\n')
          {
            c++;
            break;
          }
        }

        // walk forwards across the address range (00400000-0040c000)
        while (isalnum(c[0]) || c[0] == '-')
          c++;

        // whitespace
        while (c[0] == ' ')
          c++;

        // permissions (r-xp)
        while (isalpha(c[0]) || c[0] == '-')
          c++;

        // whitespace
        while (c[0] == ' ')
          c++;

        // offset (0000b000)
        while (isalnum(c[0]) || c[0] == '-')
          c++;

        // whitespace
        while (c[0] == ' ')
          c++;

        // dev
        while (isalnum(c[0]) || c[0] == ':')
          c++;

        // whitespace
        while (c[0] == ' ')
          c++;

        // inode
        while (isdigit(c[0]))
          c++;

        // whitespace
        while (c[0] == ' ')
          c++;

        // FINALLY we are at the start of the actual path
        char *end = strchr(c, '\n');

        if (end)
          librenderdoc_path = rdcstr(c, end - c);
      }

      delete[] map_string;
    }

    if (librenderdoc_path.empty())
    {
      RDCWARN("Couldn't get librenderdoc.so path from /proc/self/maps, falling back to dladdr");

      Dl_info info;
      if (dladdr(&LibraryLocator, &info))
        librenderdoc_path = info.dli_fname;
    }

    selfName = librenderdoc_path;
  }
}; // namespace FileIO

namespace StringFormat
{

  void Shutdown() {}

  bool iConv_UTF16ToUTF8(uint16_t const *pInBuf, size_t *pInCharsLeft, uint8_t *pOutBuf, size_t *pOutCharsLeft);

  bool iConv_UTF8ToUTF16(uint8_t const *pInBuf, size_t *pInCharsLeft, uint16_t *pOutBuf, size_t *pOutCharsLeft);

  rdcstr Wide2UTF8(const rdcwstr &s)
  {
    // include room for null terminator, assuming unicode input (not ucs)
    // utf-8 characters can be max 4 bytes.
    size_t len = (s.length() + 1) * 4;

    rdcarray<char> charBuffer(len);

    uint16_t const *inbuf;
    // Handle BOM
    {
      uint16_t *inbuf_bom = const_cast<uint16_t *>(reinterpret_cast<uint16_t const *>(s.c_str()));
      if (inbuf_bom[0] == 0XFEFF)
      {
        inbuf = inbuf_bom + 1;
      }
      else if (inbuf_bom[0] == 0XFFFE)
      {
        for (size_t i = 1; i <= s.length(); ++i)
        {
          inbuf_bom[i] = (bswap_16(inbuf_bom[i]));
        }
        inbuf = inbuf_bom + 1;
      }
      else
      {
        inbuf = inbuf_bom;
      }
    }
    size_t insize = s.length() + 1; // include null terminator
    uint8_t *outbuf = reinterpret_cast<uint8_t *>(&charBuffer[0]);
    size_t outsize = len;

    bool ret = iConv_UTF16ToUTF8(inbuf, &insize, outbuf, &outsize);
    if (!ret)
    {
#if ENABLED(RDOC_DEVEL)
      RDCWARN("Failed to convert wstring");
#endif
      return "";
    }

    // convert to string from null-terminated string - utf-8 never contains
    // 0 bytes before the null terminator, and this way we don't care if
    // charBuffer is larger than the string
    return rdcstr(&charBuffer[0]);
  }

  rdcwstr UTF82Wide(const rdcstr &s)
  {
    // include room for null terminator, for ascii input we need at least as many
    // output chars as input.
    size_t len = s.length() + 1;

    rdcarray<wchar_t> wcharBuffer(len);

    uint8_t const *inbuf;
    // Handle BOM
    {
      uint8_t const *inbuf_bom = reinterpret_cast<uint8_t const *>(s.c_str());
      if (inbuf_bom[0] == 0xEF && inbuf_bom[1] == 0xBB && inbuf_bom[2] == 0xBF)
      {
        inbuf = inbuf_bom + 3;
      }
      else
      {
        inbuf = inbuf_bom;
      }
    }
    size_t insize = s.length() + 1; // include null terminator
    uint16_t *outbuf = reinterpret_cast<uint16_t *>(&wcharBuffer[0]);
    size_t outsize = len;

    bool ret = iConv_UTF8ToUTF16(inbuf, &insize, outbuf, &outsize);
    if (!ret)
    {
#if ENABLED(RDOC_DEVEL)
      RDCWARN("Failed to convert wstring");
#endif
      return L"";
    }

    // convert to string from null-terminated string
    return rdcwstr(&wcharBuffer[0]);
  }

  bool iConv_UTF16ToUTF8(uint16_t const *pInBuf, size_t *pInCharsLeft, uint8_t *pOutBuf, size_t *pOutCharsLeft)
  {
    while ((*pInCharsLeft) >= 1)
    {
      uint32_t ucs4code = 0; //Accumulator

      //UTF-16 To UCS-4
      if ((*pInBuf) >= 0XD800U && (*pInBuf) <= 0XDBFFU) //110110xxxxxxxxxx
      {
        if ((*pInCharsLeft) >= 2)
        {
          ucs4code += (((*pInBuf) - 0XD800U) << 10U); //Accumulate

          ++pInBuf;
          --(*pInCharsLeft);

          if ((*pInBuf) >= 0XDC00U && (*pInBuf) <= 0XDFFF) //110111xxxxxxxxxx
          {
            ucs4code += ((*pInBuf) - 0XDC00U); //Accumulate

            ++pInBuf;
            --(*pInCharsLeft);
          }
          else
          {
            return false;
          }

          ucs4code += 0X10000U;
        }
        else
        {
          return false;
        }
      }
      else
      {
        ucs4code += (*pInBuf); //Accumulate

        ++pInBuf;
        --(*pInCharsLeft);
      }

      //UCS-4 To UTF-16
      if (ucs4code < 128U) //0XXX XXXX
      {
        if ((*pOutCharsLeft) >= 1)
        {
          (*pOutBuf) = static_cast<uint8_t>(ucs4code);

          ++pOutBuf;
          --(*pOutCharsLeft);
        }
        else
        {
          return false;
        }
      }
      else if (ucs4code < 2048U) //110X XXXX 10XX XXXX
      {
        if ((*pOutCharsLeft) >= 2)
        {
          (*pOutBuf) = static_cast<uint8_t>(((ucs4code & 0X7C0U) >> 6U) + 192U);

          ++pOutBuf;
          --(*pOutCharsLeft);

          (*pOutBuf) = static_cast<uint8_t>((ucs4code & 0X3FU) + 128U);

          ++pOutBuf;
          --(*pOutCharsLeft);
        }
        else
        {
          return false;
        }
      }
      else if (ucs4code < 0X10000U) //1110 XXXX 10XX XXXX 10XX XXXX
      {
        if ((*pOutCharsLeft) >= 3)
        {
          (*pOutBuf) = static_cast<uint8_t>(((ucs4code & 0XF000U) >> 12U) + 224U);

          ++pOutBuf;
          --(*pOutCharsLeft);

          (*pOutBuf) = static_cast<uint8_t>(((ucs4code & 0XFC0U) >> 6U) + 128U);

          ++pOutBuf;
          --(*pOutCharsLeft);

          (*pOutBuf) = static_cast<uint8_t>((ucs4code & 0X3FU) + 128U);

          ++pOutBuf;
          --(*pOutCharsLeft);
        }
        else
        {
          return false;
        }
      }
      else if (ucs4code < 0X200000U) //1111 0XXX 10XX XXXX 10XX XXXX 10XX XXXX
      {
        if ((*pOutCharsLeft) >= 4)
        {
          (*pOutBuf) = static_cast<uint8_t>(((ucs4code & 0X1C0000U) >> 18U) + 240U);

          ++pOutBuf;
          --(*pOutCharsLeft);

          (*pOutBuf) = static_cast<uint8_t>(((ucs4code & 0X3F000U) >> 12U) + 128U);

          ++pOutBuf;
          --(*pOutCharsLeft);

          (*pOutBuf) = static_cast<uint8_t>(((ucs4code & 0XFC0U) >> 6U) + 128U);

          ++pOutBuf;
          --(*pOutCharsLeft);

          (*pOutBuf) = static_cast<uint8_t>((ucs4code & 0X3FU) + 128U);

          ++pOutBuf;
          --(*pOutCharsLeft);
        }
        else //ucs4code >= 0X200000U
        {
          return false;
        }
      }
    }

    return true;
  }

  bool iConv_UTF8ToUTF16(uint8_t const *pInBuf, size_t *pInCharsLeft, uint16_t *pOutBuf, size_t *pOutCharsLeft)
  {
    while ((*pInCharsLeft) >= 1)
    {
      uint32_t ucs4code = 0; //Accumulator

      //UTF-8 To UCS-4
      if ((*pInBuf) < 128U) //0XXX XXXX
      {
        ucs4code += (*pInBuf); //Accumulate

        ++pInBuf;
        --(*pInCharsLeft);
      }
      else if ((*pInBuf) < 192U) //10XX XXXX
      {
        return false;
      }
      else if ((*pInBuf) < 224U) //110X XXXX 10XX XXXX
      {

        if ((*pInCharsLeft) >= 2)
        {
          ucs4code += (((*pInBuf) - 192U) << 6U); //Accumulate

          ++pInBuf;
          --(*pInCharsLeft);

          if ((*pInBuf) >= 128U && (*pInBuf) < 192U) //10XX XXXX
          {
            ucs4code += ((*pInBuf) - 128U); //Accumulate

            ++pInBuf;
            --(*pInCharsLeft);
          }
          else
          {
            return false;
          }
        }
        else
        {
          return false;
        }
      }
      else if ((*pInBuf) < 240U) //1110 XXXX 10XX XXXX 10XX XXXX
      {
        if ((*pInCharsLeft) >= 3)
        {
          ucs4code += (((*pInBuf) - 224U) << 12U); //Accumulate

          ++pInBuf;
          --(*pInCharsLeft);

          if ((*pInBuf) >= 128U && (*pInBuf) < 192U) //10XX XXXX
          {
            ucs4code += (((*pInBuf) - 128U) << 6U); //Accumulate

            ++pInBuf;
            --(*pInCharsLeft);
          }
          else
          {
            return false;
          }

          if ((*pInBuf) >= 128U && (*pInBuf) < 192U) //10XX XXXX
          {
            ucs4code += ((*pInBuf) - 128U); //Accumulate

            ++pInBuf;
            --(*pInCharsLeft);
          }
          else
          {
            return false;
          }
        }
        else
        {
          return false;
        }
      }
      else // ((*pInBuf) >= 240U) 1111 0XXX 10XX XXXX 10XX XXXX 10XX XXXX
      {
        if ((*pInCharsLeft) >= 4)
        {
          ucs4code += (((*pInBuf) - 240U) << 18U); //Accumulate

          ++pInBuf;
          --(*pInCharsLeft);

          if ((*pInBuf) >= 128U && (*pInBuf) < 192U) //10XX XXXX
          {
            ucs4code += (((*pInBuf) - 128U) << 12U); //Accumulate

            ++pInBuf;
            --(*pInCharsLeft);
          }
          else
          {
            return false;
          }

          if ((*pInBuf) >= 128U && (*pInBuf) < 192U) //10XX XXXX
          {
            ucs4code += (((*pInBuf) - 128U) << 6U); //Accumulate

            ++pInBuf;
            --(*pInCharsLeft);
          }
          else
          {
            return false;
          }

          if ((*pInBuf) >= 128U && (*pInBuf) < 192U) //10XX XXXX
          {
            ucs4code += ((*pInBuf) - 128U); //Accumulate

            ++pInBuf;
            --(*pInCharsLeft);
          }
          else
          {
            return false;
          }
        }
        else
        {
          return false;
        }
      }

      //UCS-4 To UTF-16
      if (ucs4code < 0X10000U)
      {
        if ((*pOutCharsLeft) >= 1)
        {
          (*pOutBuf) = static_cast<uint16_t>(ucs4code);

          ++pOutBuf;
          --(*pOutCharsLeft);
        }
        else
        {
          return false;
        }
      }
      else //ucs4code >= 0X10000U
      {
        if ((*pOutCharsLeft) >= 2)
        {
          (*pOutBuf) = static_cast<uint16_t>((((ucs4code - 65536U) & (0XFFC00U)) >> 10U) + 0XD800U); //110110xxxxxxxxxx

          ++pOutBuf;
          --(*pOutCharsLeft);

          (*pOutBuf) = static_cast<uint16_t>(((ucs4code - 65536U) & (0X3FFU)) + 0XDC00U); //110111xxxxxxxxxx

          ++pOutBuf;
          --(*pOutCharsLeft);
        }
        else
        {
          return false;
        }
      }
    }

    return true;
  }

}; // namespace StringFormat

namespace OSUtility
{
  void WriteOutput(int channel, const char *str)
  {
    if (channel == OSUtility::Output_StdOut)
    {
      fprintf(stdout, "%s", str);
      fflush(stdout);
    }
    else if (channel == OSUtility::Output_StdErr)
    {
      fprintf(stderr, "%s", str);
      fflush(stderr);
    }
  }

  uint64_t GetMachineIdent()
  {
    uint64_t ret = MachineIdent_Linux;

#if defined(_M_ARM) || defined(__arm__)
    ret |= MachineIdent_Arch_ARM;
#else
    ret |= MachineIdent_Arch_x86;
#endif

#if ENABLED(RDOC_X64)
    ret |= MachineIdent_64bit;
#else
    ret |= MachineIdent_32bit;
#endif

    return ret;
  }
}; // namespace OSUtility
