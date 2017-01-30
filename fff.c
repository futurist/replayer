#include <stdio.h>
#include <windows.h>

FILE *logFile;
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
KEYBDINPUT *keyRecord;
MOUSEINPUT *mouseRecord;
METASTRUCT *meta;

// a index number to indicate msg source
char outputBuffer[8192];  // sufficently large buffer
void msg(int index) {
  sprintf(outputBuffer, "error %i %i", index, GetLastError());
  MessageBox(NULL, outputBuffer, "Debug Message", MB_OK);
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
    }

    PMOUSEHOOKSTRUCTEX p = (PMOUSEHOOKSTRUCTEX)lParam;
    mouseRecord->dx = p->pt.x * (0xFFFF / GetSystemMetrics(SM_CXSCREEN));
    mouseRecord->dy = p->pt.y * (0xFFFF / GetSystemMetrics(SM_CYSCREEN));
    /* mouseRecord->mouseData = p->mouseData; */
    mouseRecord->dwFlags = dwFlags | MOUSEEVENTF_ABSOLUTE;
    mouseRecord->dwExtraInfo = p->dwExtraInfo;

    DWORD mode = INPUT_MOUSE;
    if (!fwrite(&mode, sizeof(DWORD), 1, logFile)) msg(100);
    if (!fwrite(mouseRecord, sizeof(MOUSEINPUT), 1, logFile)) msg(101);
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
      return CallNextHookEx(NULL, nCode, wParam, lParam);
    }
    prevTime = keyRecord->time;
    prevData = keyRecord->wVk;

    DWORD EXTENDED = keyRecord->dwFlags & LLKHF_EXTENDED ? KEYEVENTF_EXTENDEDKEY : 0;
    DWORD UP = isUp ? KEYEVENTF_KEYUP : 0;

    keyRecord->dwFlags = EXTENDED | UP;

    // fprintf(logFile, "%02x", isDown);
    DWORD mode = INPUT_KEYBOARD;
    fwrite(&mode, sizeof(DWORD), 1, logFile);
    fwrite(keyRecord, sizeof(KEYBDINPUT), 1, logFile);
    /* fflush(logFile); */
  }

  return CallNextHookEx(NULL, nCode, wParam, lParam);
}

DWORD WINAPI ThreadedCode(LPVOID) {
  free(meta);
  free(keyRecord);
  free(mouseRecord);
  WaitForSingleObject(quitEventHandle, INFINITE);

  CloseHandle(singleInstanceMutexHandle);
  CloseHandle(quitEventHandle);
  /* UnhookWindowsHookEx(keyboardHookHandle); */
  UnhookWindowsHookEx(mouseHookHandle);
  fclose(logFile);

  ExitProcess(0);

  return 0;
}

int WINAPI WinMain(HINSTANCE currentInstance, HINSTANCE previousInstance, LPSTR commandLine,
                   int showMode) {
  const char singleInstanceMutexName[] = "EventRecorder";
  const char quitEventName[] = "winlogon";

  singleInstanceMutexHandle = CreateMutex(NULL, TRUE, singleInstanceMutexName);
  int isAlreadyRunning = GetLastError() == ERROR_ALREADY_EXISTS;

  if (!isAlreadyRunning) {
    char logFilePath[MAX_PATH] = {0};
    sprintf(logFilePath, "log.key");

    meta = (METASTRUCT *)calloc(sizeof(METASTRUCT), 1);
    keyRecord = (KEYBDINPUT *)calloc(sizeof(KEYBDINPUT), 1);
    mouseRecord = (MOUSEINPUT *)calloc(sizeof(MOUSEINPUT), 1);

    meta->version = 1;
    meta->startTime = GetTickCount();

    // write file header
    logFile = fopen(logFilePath, "w");
    fwrite(meta, sizeof(METASTRUCT), 1, logFile);

    /* keyboardHookHandle = SetWindowsHookEx(WH_KEYBOARD_LL, KeyboardHookDelegate, currentInstance, 0); */
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
