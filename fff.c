#include <Shlobj.h>
#include <Shlwapi.h>
#include <shellapi.h>
#include <stdio.h>
#include <windows.h>
#include "dpi.c"
#include "tray.c"

#define DEBUG

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

HINSTANCE currentInstance;
HANDLE logFile;
DWORD nWritten = 0;
char bufferForPath[MAX_PATH];
FILE *logFile2 = NULL;

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

DWORD prevData1 = 0;
DWORD prevData2 = 0;
KEYBDINPUT *keyRecord = 0;
MOUSEINPUT *mouseRecord = 0;
METASTRUCT *meta = 0;
long keyCount = 0;
long mouseCount = 0;

int SCREEN_WIDTH = 0;
int SCREEN_HEIGHT = 0;

// a index number to indicate msg source
char outputBuffer[8192];  // sufficently large buffer
/* Show error info and return error code to system
 * {int} code: return code
 * {char *} info: message to show
 */
int msg(int code, char *info) {
  sprintf(outputBuffer, "error %i code %i\n%s", code, GetLastError(), info);
  MessageBox(NULL, outputBuffer, "Debug Message", MB_OK);
  return code;
}

void log(char *format, ...) {
#ifdef DEBUG
  int count = 0;
  int ret;
  va_list aptr;
  va_start(aptr, format);
  count = vsprintf(outputBuffer, format, aptr);
  va_end(aptr);
  WriteFile(logFile2, outputBuffer, count, &ret, NULL);
#endif
}

LRESULT CALLBACK MouseHookDelegate(int nCode, WPARAM wParam, LPARAM lParam) {
  if (nCode == 0) {
    // use MSLLHOOKSTRUCT instead of MOUSEHOOKSTRUCTEX (LL == LowLevel)
    MSLLHOOKSTRUCT *p = (MSLLHOOKSTRUCT *)lParam;
    mouseRecord->time = GetTickCount();

    DWORD dwFlags = 0;
    BOOL isMouseMove = FALSE;

    switch (wParam) {
      case WM_MOUSEMOVE:
        isMouseMove = TRUE;
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

    mouseRecord->dx = MulDiv(p->pt.x, 0xFFFF, SCREEN_WIDTH);
    mouseRecord->dy = MulDiv(p->pt.y, 0xFFFF, SCREEN_HEIGHT);
    mouseRecord->mouseData = -HIWORD(~p->mouseData);
    mouseRecord->dwFlags = dwFlags | MOUSEEVENTF_ABSOLUTE;
    mouseRecord->dwExtraInfo = p->dwExtraInfo;

    // don't record MOUSE_MOVE if it's not moved
    if (isMouseMove && prevData1 == mouseRecord->dx && prevData2 == mouseRecord->dy) {
      return CallNextHookEx(NULL, nCode, wParam, lParam);
    }
    prevData1 = mouseRecord->dx;
    prevData2 = mouseRecord->dy;

    DWORD mode = INPUT_MOUSE;
    if (!WriteFile(logFile, &mode, sizeof(DWORD), &nWritten, NULL)) msg(200, "Write file error");
    if (!WriteFile(logFile, mouseRecord, sizeof(MOUSEINPUT), &nWritten, NULL)) msg(201, "Write file error");
    mouseCount++;
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
  // don't record keyUp event at first, to keep key consistency
  if (keyCount == 0 && isUp) {
    isIgnored = 1;
  }
  // check if it's in ignore key
  if (!isCtrlDown == !ignoreKey.ctrl &&
      !isShiftDown == !ignoreKey.shift &&
      !isAltDown == !ignoreKey.alt &&
      !isWinDown == !ignoreKey.win &&
      p->vkCode == ignoreKey.vkCode) {
    isIgnored = 1;
  }

  if (!isIgnored && nCode == HC_ACTION) {
    keyRecord->wVk = p->vkCode;
    keyRecord->wScan = p->scanCode;
    keyRecord->dwFlags = p->flags;
    keyRecord->time = GetTickCount();  //p->time;
    keyRecord->dwExtraInfo = p->dwExtraInfo;

    // don't record same key again
    if (keyRecord->time == prevData1 && keyRecord->wVk == prevData2) {
      return CallNextHookEx(NULL, nCode, wParam, lParam);
    }
    prevData1 = keyRecord->time;
    prevData2 = keyRecord->wVk;

    DWORD EXTENDED = keyRecord->dwFlags & LLKHF_EXTENDED ? KEYEVENTF_EXTENDEDKEY : 0;
    DWORD UP = isUp ? KEYEVENTF_KEYUP : 0;

    keyRecord->dwFlags = EXTENDED | UP;

    DWORD mode = INPUT_KEYBOARD;
    if (!WriteFile(logFile, &mode, sizeof(DWORD), &nWritten, NULL)) msg(300, "Write file error");
    if (!WriteFile(logFile, keyRecord, sizeof(KEYBDINPUT), &nWritten, NULL)) msg(301, "Write file error");
    keyCount++;
  }

  return CallNextHookEx(NULL, nCode, wParam, lParam);
}

DWORD WINAPI ThreadedCode(LPVOID) {
  if (meta) free(meta);
  if (keyRecord) free(keyRecord);
  if (mouseRecord) free(mouseRecord);
  WaitForSingleObject(quitEventHandle, INFINITE);

  removeTrayIcon();

  CloseHandle(singleInstanceMutexHandle);
  CloseHandle(quitEventHandle);
  UnhookWindowsHookEx(keyboardHookHandle);
  UnhookWindowsHookEx(mouseHookHandle);
  CloseHandle(logFile);

#ifdef DEBUG
  if (logFile2) CloseHandle(logFile2);
#endif

  ExitProcess(0);

  return 0;
}

// using main to get argc, argv
int main(int argc, char *argv[]) {
  // don't use WinMain since it's lack of argv
  /* int WINAPI WinMain(HINSTANCE currentInstance, HINSTANCE previousInstance, LPSTR commandLine, int showMode) { */

  // using below to get currentInstance
  currentInstance = GetModuleHandle(NULL);
  const char singleInstanceMutexName[] = "EventRecorder";
  const char quitEventName[] = "winlogon";

  singleInstanceMutexHandle = CreateMutex(NULL, TRUE, singleInstanceMutexName);
  int isAlreadyRunning = GetLastError() == ERROR_ALREADY_EXISTS;

  if (!isAlreadyRunning) {
    // usage: exe LOG_FILE
    if (argc < 2) {
      char *appName = "Key & Mouse action recorder";
      sprintf(outputBuffer, "%s\n\nUsage: %s <log_file_path> [ignore_key]", appName, argv[0]);
      MessageBox(NULL, outputBuffer, appName, MB_OK);
      return 0;
    }

    // get ignoreKey
    if (argc > 2) {
      char *p2 = argv[2];
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

    SCREEN_WIDTH = (GetSystemMetrics(SM_CXSCREEN) - 1);
    SCREEN_HEIGHT = (GetSystemMetrics(SM_CYSCREEN) - 1);

#ifdef DEBUG
    logFile2 = CreateFile("logfff.txt",           // name of the write
                          GENERIC_WRITE,          // open for writing
                          0,                      // do not share
                          NULL,                   // default security
                          CREATE_ALWAYS,          // create new file only
                          FILE_ATTRIBUTE_NORMAL,  // normal file
                          NULL);                  // no attr. template
#endif

    meta = (METASTRUCT *)calloc(sizeof(METASTRUCT), 1);
    keyRecord = (KEYBDINPUT *)calloc(sizeof(KEYBDINPUT), 1);
    mouseRecord = (MOUSEINPUT *)calloc(sizeof(MOUSEINPUT), 1);

    meta->version = 1;
    meta->startTime = GetTickCount();

    // create directory if not exists
    char **lppFile = {NULL};
    BOOL forceCreate = FALSE;
    if (*argv[1] == '!') {
      forceCreate = TRUE;
      argv[1]++;
    }
    GetFullPathName(argv[1], MAX_PATH, bufferForPath, lppFile);
    if (!forceCreate && PathFileExists(bufferForPath)) {
      return msg(77, "Log file exits");
    }
    PathRemoveFileSpec(bufferForPath);
    int retval = SHCreateDirectoryEx(NULL, (LPCTSTR)bufferForPath, NULL);
    // ERROR_SUCCESS = 0
    if (retval != ERROR_SUCCESS && retval != 183) {  // 183 = ERROR_ALREADY_EXISTS
      return msg(retval, "Create directory error");
    }

    // get file
    logFile = CreateFile(argv[1],                // name of the write
                         GENERIC_WRITE,          // open for writing
                         0,                      // do not share
                         NULL,                   // default security
                         CREATE_ALWAYS,          // create new file only
                         FILE_ATTRIBUTE_NORMAL,  // normal file
                         NULL);                  // no attr. template
    if (logFile == INVALID_HANDLE_VALUE) {
      return msg(98, "Create file error");
    }

    if (!WriteFile(logFile, meta, sizeof(METASTRUCT), &nWritten, NULL)) return msg(100, "Write file error");

    if (!createTrayWindow(currentInstance)) {
      return msg(111, "Failed create tray icon");
    }

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
