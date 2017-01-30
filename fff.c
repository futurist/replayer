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
char bufferForKeys[100];

BYTE keyState[256];
WCHAR buffer[16];
SYSTEMTIME sysTime;

HANDLE singleInstanceMutexHandle;
HHOOK keyboardHookHandle;
HHOOK mouseHookHandle;
HANDLE quitEventHandle;

typedef struct METASTRUCT {
  int version;
  DWORD startTime;
} METASTRUCT;

DWORD prevTime = 0;
DWORD prevData = 0;
KEYBDINPUT *keyRecord = 0;
MOUSEINPUT *mouseRecord = 0;
METASTRUCT *meta = 0;

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
    // http://stackoverflow.com/questions/19462161/get-wheel-delta-wparamwparam-in-mouse-hook-returning-0
    // https://msdn.microsoft.com/en-us/library/windows/desktop/ms644970(v=vs.85).aspx
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
  if (nCode == HC_ACTION) {
    /* GetKeyboardState((PBYTE)&keyState); */
    PKBDLLHOOKSTRUCT p = (PKBDLLHOOKSTRUCT)lParam;
    keyRecord->wVk = p->vkCode;
    keyRecord->wScan = p->scanCode;
    keyRecord->dwFlags = p->flags;
    keyRecord->time = GetTickCount();  //p->time;
    keyRecord->dwExtraInfo = 0;

    /* BOOL isDown = wParam == WM_KEYDOWN || wParam == WM_SYSKEYDOWN; */
    BOOL isUp = wParam == WM_KEYUP || wParam == WM_SYSKEYUP;

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
    meta = (METASTRUCT *)calloc(sizeof(METASTRUCT), 1);
    keyRecord = (KEYBDINPUT *)calloc(sizeof(KEYBDINPUT), 1);
    mouseRecord = (MOUSEINPUT *)calloc(sizeof(MOUSEINPUT), 1);

    meta->version = 1;
    meta->startTime = GetTickCount();

    // get file
    logFile = CreateFile("log.key",              // name of the write
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
