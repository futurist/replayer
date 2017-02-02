#include <Shlobj.h>
#include <Shlwapi.h>
#include <stdio.h>
#include <windows.h>

// lcc lack below definition
#ifndef WM_MOUSEHWHEEL
#define WM_MOUSEHWHEEL 0x020E
#endif

#ifndef MOUSEEVENTF_HWHEEL
#define MOUSEEVENTF_HWHEEL 0x01000
#endif

#ifndef MOUSEEVENTF_WHEEL
#define MOUSEEVENTF_WHEEL 0x0800
#endif

HANDLE logFile;
DWORD nWritten = 0;
char bufferForPath[MAX_PATH];

BYTE keyState[256];

HANDLE singleInstanceMutexHandle;
HHOOK keyboardHookHandle;
HHOOK mouseHookHandle;
HANDLE quitEventHandle;

typedef struct METASTRUCT {
  int version;
  DWORD startTime;
} METASTRUCT;

// define a key
typedef struct KEYSTRUCT {
  char ctrl;
  char shift;
  char alt;
  char win;
  unsigned int vkCode;
} KEYSTRUCT;

KEYSTRUCT ignoreKey = {0};

DWORD prevTime = 0;
DWORD prevData = 0;
KEYBDINPUT *keyRecord = 0;
MOUSEINPUT *mouseRecord = 0;
METASTRUCT *meta = 0;

enum { kMaxArgs = 64 };
int argc = 0;
char *argv[kMaxArgs];

// a index number to indicate msg source
char outputBuffer[8192];  // sufficently large buffer
int msg(int index) {
  sprintf(outputBuffer, "error %i %i", index, GetLastError());
  MessageBox(NULL, outputBuffer, "Debug Message", MB_OK);
  return index;
}

LRESULT CALLBACK MouseHookDelegate(int nCode, WPARAM wParam, LPARAM lParam) {
  if (nCode == 0) {
    mouseRecord->time = GetTickCount();
    // don't record same key again
    if (mouseRecord->time == prevTime && wParam == prevData) {
      /* return CallNextHookEx(NULL, nCode, wParam, lParam); */
    }
    prevTime = mouseRecord->time;
    prevData = wParam;

    DWORD dwFlags = 0;
    switch (wParam) {
      case WM_MOUSEMOVE:
        dwFlags = MOUSEEVENTF_MOVE;
        break;
      case WM_RBUTTONDOWN:
        dwFlags = MOUSEEVENTF_RIGHTDOWN;
        break;
      case WM_RBUTTONUP:
        dwFlags = MOUSEEVENTF_RIGHTUP;
        break;
      case WM_LBUTTONDOWN:
        dwFlags = MOUSEEVENTF_LEFTDOWN;
        break;
      case WM_LBUTTONUP:
        dwFlags = MOUSEEVENTF_LEFTUP;
        break;
      case WM_MBUTTONDOWN:
        dwFlags = MOUSEEVENTF_MIDDLEDOWN;
        break;
      case WM_MBUTTONUP:
        dwFlags = MOUSEEVENTF_MIDDLEUP;
        break;
      case WM_MOUSEWHEEL:
        dwFlags = MOUSEEVENTF_WHEEL;
        break;
      case WM_MOUSEHWHEEL:
        dwFlags = MOUSEEVENTF_HWHEEL;
        break;
    }

    // use MSLLHOOKSTRUCT instead of MOUSEHOOKSTRUCTEX (LL == LowLevel)
    MSLLHOOKSTRUCT *p = (MSLLHOOKSTRUCT *)lParam;
    mouseRecord->dx = p->pt.x * (0xFFFF / GetSystemMetrics(SM_CXSCREEN));
    mouseRecord->dy = p->pt.y * (0xFFFF / GetSystemMetrics(SM_CYSCREEN));
    mouseRecord->mouseData = -HIWORD(~p->mouseData);
    mouseRecord->dwFlags = dwFlags | MOUSEEVENTF_ABSOLUTE;
    mouseRecord->dwExtraInfo = p->dwExtraInfo;

    DWORD mode = INPUT_MOUSE;
    if (!WriteFile(logFile, &mode, sizeof(DWORD), &nWritten, NULL)) msg(200);
    if (!WriteFile(logFile, mouseRecord, sizeof(MOUSEINPUT), &nWritten, NULL)) msg(201);
  }
  return CallNextHookEx(NULL, nCode, wParam, lParam);
}

LRESULT CALLBACK KeyboardHookDelegate(int nCode, WPARAM wParam, LPARAM lParam) {
  PKBDLLHOOKSTRUCT p = (PKBDLLHOOKSTRUCT)lParam;
  BOOL isIgnored = 0;
  /* BOOL isDown = wParam == WM_KEYDOWN || wParam == WM_SYSKEYDOWN; */
  BOOL isUp = wParam == WM_KEYUP || wParam == WM_SYSKEYUP;
  BOOL isCtrlDown = GetKeyState(VK_CONTROL) >> 15;
  BOOL isShiftDown = GetKeyState(VK_SHIFT) >> 15;
  BOOL isAltDown = GetKeyState(VK_MENU) >> 15;
  BOOL isWinDown = GetKeyState(VK_LWIN) >> 15 || GetKeyState(VK_RWIN) >> 15;

  // check if it's in ignore key
  if (!isCtrlDown == !ignoreKey.ctrl &&
      !isShiftDown == !ignoreKey.shift &&
      !isAltDown == !ignoreKey.alt &&
      !isWinDown == !ignoreKey.win &&
      p->vkCode == ignoreKey.vkCode) {
    isIgnored = 1;
  }

  if (!isIgnored && nCode == HC_ACTION) {
    /* GetKeyboardState((PBYTE)&keyState); */
    keyRecord->wVk = p->vkCode;
    keyRecord->wScan = p->scanCode;
    keyRecord->dwFlags = p->flags;
    keyRecord->time = GetTickCount();  //p->time;
    keyRecord->dwExtraInfo = p->dwExtraInfo;

    // don't record same key again
    if (keyRecord->time == prevTime && keyRecord->wVk == prevData) {
      /* return CallNextHookEx(NULL, nCode, wParam, lParam); */
    }
    prevTime = keyRecord->time;
    prevData = keyRecord->wVk;

    DWORD EXTENDED = keyRecord->dwFlags & LLKHF_EXTENDED ? KEYEVENTF_EXTENDEDKEY : 0;
    DWORD UP = isUp ? KEYEVENTF_KEYUP : 0;

    keyRecord->dwFlags = EXTENDED | UP;

    DWORD mode = INPUT_KEYBOARD;
    if (!WriteFile(logFile, &mode, sizeof(DWORD), &nWritten, NULL)) msg(300);
    if (!WriteFile(logFile, keyRecord, sizeof(KEYBDINPUT), &nWritten, NULL)) msg(301);
  }

  return CallNextHookEx(NULL, nCode, wParam, lParam);
}

DWORD WINAPI ThreadedCode(LPVOID) {
  if (meta) free(meta);
  if (keyRecord) free(keyRecord);
  if (mouseRecord) free(mouseRecord);
  WaitForSingleObject(quitEventHandle, INFINITE);

  CloseHandle(singleInstanceMutexHandle);
  CloseHandle(quitEventHandle);
  UnhookWindowsHookEx(keyboardHookHandle);
  UnhookWindowsHookEx(mouseHookHandle);
  CloseHandle(logFile);

  ExitProcess(0);

  return 0;
}

int WINAPI WinMain(HINSTANCE currentInstance, HINSTANCE previousInstance, LPSTR commandLine, int showMode) {
  const char singleInstanceMutexName[] = "EventRecorder";
  const char quitEventName[] = "winlogon";

  singleInstanceMutexHandle = CreateMutex(NULL, TRUE, singleInstanceMutexName);
  int isAlreadyRunning = GetLastError() == ERROR_ALREADY_EXISTS;

  if (!isAlreadyRunning) {
    // parse command line args
    char *p2 = strtok(commandLine, " ");
    while (p2 && argc < kMaxArgs - 1) {
      argv[argc++] = p2;
      p2 = strtok(0, " ");
    }
    argv[argc] = 0;

    // usage: exe SAVE_FILE IGNORE_KEY
    if (argc < 1) return msg(5);

    // get ignoreKey
    if (argc > 1) {
      p2 = argv[1];
      while (p2) {
        switch ((char)*p2) {
          case '!':
            ignoreKey.alt = 1;
            break;
          case '+':
            ignoreKey.shift = 1;
            break;
          case '^':
            ignoreKey.ctrl = 1;
            break;
          case '#':
            ignoreKey.win = 1;
            break;
          default:
            break;
        }
        // it's not valid ignoreKey
        if (sscanf(p2, "0x%x", &ignoreKey.vkCode)) {
          p2 = NULL;
          break;
        } else {
          p2++;
        }
      }
      /* sprintf(outputBuffer, "d %i %i %i %i %x", ignoreKey.alt, ignoreKey.shift, ignoreKey.ctrl, ignoreKey.win, ignoreKey.vkCode); */
      /* MessageBox(NULL, outputBuffer, "Debug Message", MB_OK); */
    }

    meta = (METASTRUCT *)calloc(sizeof(METASTRUCT), 1);
    keyRecord = (KEYBDINPUT *)calloc(sizeof(KEYBDINPUT), 1);
    mouseRecord = (MOUSEINPUT *)calloc(sizeof(MOUSEINPUT), 1);

    meta->version = 1;
    meta->startTime = GetTickCount();

    // create directory if not exists
    char **lppFile = {NULL};
    GetFullPathName(argv[0], MAX_PATH, bufferForPath, lppFile);
    PathRemoveFileSpec(bufferForPath);
    int retval = SHCreateDirectoryEx(NULL, (LPCTSTR)bufferForPath, NULL);
    // ERROR_SUCCESS = 0
    if (retval != ERROR_SUCCESS) {
      return msg(retval);
    }

    // get file
    logFile = CreateFile(argv[0],                // name of the write
                         GENERIC_WRITE,          // open for writing
                         0,                      // do not share
                         NULL,                   // default security
                         CREATE_ALWAYS,          // create new file only
                         FILE_ATTRIBUTE_NORMAL,  // normal file
                         NULL);                  // no attr. template
    if (logFile == INVALID_HANDLE_VALUE) {
      return msg(98);
    }

    if (!WriteFile(logFile, meta, sizeof(METASTRUCT), &nWritten, NULL)) msg(100);

    keyboardHookHandle = SetWindowsHookEx(WH_KEYBOARD_LL, KeyboardHookDelegate, currentInstance, 0);
    mouseHookHandle = SetWindowsHookEx(WH_MOUSE_LL, MouseHookDelegate, currentInstance, 0);
    {
      quitEventHandle = CreateEvent(NULL, FALSE, FALSE, quitEventName);

      int threadId;
      CreateThread(NULL, 0, ThreadedCode, 0, 0, &threadId);

      MSG message;
      while (GetMessage(&message, NULL, 0, 0)) {
      }
    }
  } else {
    quitEventHandle = OpenEvent(EVENT_ALL_ACCESS, FALSE, quitEventName);
    SetEvent(quitEventHandle);
  }

  return 0;
}
