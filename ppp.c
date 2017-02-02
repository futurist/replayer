#include <errno.h>
#include <stdio.h>
#include <windows.h>

char strBuffer[100];
DWORD prevTime = 0;

typedef struct METASTRUCT {
  int version;
  DWORD startTime;
} METASTRUCT;

typedef struct BUFFERSTRUCT {
  size_t point;
  size_t size;
  char *buf;
} BUFFERSTRUCT;

enum { kMaxArgs = 64 };
int argc = 0;
char *argv[kMaxArgs];

HANDLE singleInstanceMutexHandle;
HANDLE quitEventHandle;
int running = TRUE;

METASTRUCT *meta = NULL;
KEYBDINPUT *keyRecord = NULL;
MOUSEINPUT *mouseRecord = NULL;
char *buf = NULL;
FILE *logFile2;
BYTE keyState[256];

int timeSpan = 0;
INPUT ip = {0};
DWORD mode = 0;

// a index number to indicate msg source
char outputBuffer[8192];  // sufficently large buffer
int msg(int index) {
  sprintf(outputBuffer, "index %i lastError %i  errno %i", index, GetLastError(), errno);
  MessageBox(NULL, outputBuffer, "Debug Message", MB_OK);
  return index;
}

int readData(void *buf, size_t size, BUFFERSTRUCT *src) {
  if (src->point + size > src->size) return 0;
  memcpy(buf, src->buf + src->point, size);
  src->point += size;
  return 1;
}

DWORD WINAPI ThreadedCode(LPVOID) {
  WaitForSingleObject(quitEventHandle, INFINITE);

  CloseHandle(singleInstanceMutexHandle);
  CloseHandle(quitEventHandle);

  running = FALSE;

  /* ExitProcess(0); */

  return !running;
}

void resetAllKeys(void) {
  GetKeyboardState((PBYTE)&keyState);
  // i<8 is mouse
  for (UINT i = 8; i < 256; i++) {
    if (GetAsyncKeyState(i) >> 15) {
      // key is down
      ip.type = INPUT_KEYBOARD;
      ip.ki.wVk = i;
      ip.ki.wScan = MapVirtualKeyEx(i, 4, NULL);  // MAPVK_VK_TO_VSC_EX=4
      ip.ki.dwFlags = KEYEVENTF_KEYUP;
      ip.ki.time = 0;
      ip.ki.dwExtraInfo = 0;
      SendInput(1, &ip, sizeof(INPUT));
      // also up extended_key
      ip.ki.dwFlags |= KEYEVENTF_EXTENDEDKEY;
      SendInput(1, &ip, sizeof(INPUT));
    }
  }
}

void cleanUp(void) {
  resetAllKeys();
  if (buf) free(buf);
  if (meta) free(meta);
  if (keyRecord) free(keyRecord);
  if (mouseRecord) free(mouseRecord);
  if (logFile2) fclose(logFile2);
}

int WINAPI WinMain(HINSTANCE currentInstance, HINSTANCE previousInstance, LPSTR commandLine, int showMode) {
  const char singleInstanceMutexName[] = "EventRecorder";
  const char quitEventName[] = "winlogon";

  singleInstanceMutexHandle = CreateMutex(NULL, TRUE, singleInstanceMutexName);
  int isAlreadyRunning = GetLastError() == ERROR_ALREADY_EXISTS;

  if (!isAlreadyRunning) {
    {
      quitEventHandle = CreateEvent(NULL, FALSE, FALSE, quitEventName);

      int threadId;
      CreateThread(NULL, 0, ThreadedCode, 0, 0, &threadId);
    }

    // parse command line args
    char *p2 = strtok(commandLine, " ");
    while (p2 && argc < kMaxArgs - 1) {
      argv[argc++] = p2;
      p2 = strtok(0, " ");
    }
    argv[argc] = 0;

    // usage: exe SAVE_FILE IGNORE_KEY
    if (argc < 1) return msg(5);

    meta = (METASTRUCT *)calloc(sizeof(METASTRUCT), 1);
    keyRecord = (KEYBDINPUT *)calloc(sizeof(KEYBDINPUT), 1);
    mouseRecord = (MOUSEINPUT *)calloc(sizeof(MOUSEINPUT), 1);

    char logFilePath2[MAX_PATH] = {0};
    sprintf(logFilePath2, "log.txt");
    logFile2 = fopen(logFilePath2, "w");

    // get file size
    LARGE_INTEGER nLargeInteger = {0};
    HANDLE hFile = CreateFile(argv[0], GENERIC_READ, FILE_SHARE_READ, NULL,
                              OPEN_EXISTING, FILE_ATTRIBUTE_READONLY, NULL);
    if (hFile != INVALID_HANDLE_VALUE) {
      BOOL bSuccess = GetFileSizeEx(hFile, &nLargeInteger);
      if (!bSuccess) return msg(98);
    } else {
      return msg(99);
    }

    // read all content
    size_t fsize = nLargeInteger.LowPart;
    buf = malloc(fsize);
    DWORD nRead;
    if (!ReadFile(hFile, buf, fsize, &nRead, NULL)) {
      CloseHandle(hFile);
      return msg(98);
    }
    CloseHandle(hFile);

    BUFFERSTRUCT source = {0};
    source.point = 0;
    source.size = fsize;
    source.buf = buf;

    /* sprintf(outputBuffer, "%i %x %x %p", fsize, source.point, source.size, source.buf); */
    /* MessageBox(NULL, commandLine, "Debug Message", MB_OK); */

    if (readData(meta, sizeof(METASTRUCT), &source) != 1) {
      msg(100);
      return 2;
    }
    prevTime = meta->startTime;
    /* fprintf(logFile2, "time: %d %x\n", meta->startTime, running); */

    while (running && source.point < fsize) {
      if (!readData(&mode, sizeof(DWORD), &source)) msg(101);
      // it's mouse event
      if (mode == INPUT_MOUSE) {
        if (!readData(mouseRecord, sizeof(MOUSEINPUT), &source)) msg(102);
        timeSpan = mouseRecord->time - prevTime;
        prevTime = mouseRecord->time;
        if (timeSpan > 0) sleep(timeSpan);
        /* fprintf(logFile2, "%X %X %X %X\n", mouseRecord->time, mouseRecord->dx, mouseRecord->dy, mouseRecord->dwFlags); */
        ip.type = INPUT_MOUSE;
        ip.mi.dx = mouseRecord->dx;
        ip.mi.dy = mouseRecord->dy;
        ip.mi.mouseData = mouseRecord->mouseData;
        ip.mi.dwFlags = mouseRecord->dwFlags;
        ip.mi.dwExtraInfo = mouseRecord->dwExtraInfo;
        ip.mi.time = 0;
        int ret = SendInput(1, &ip, sizeof(INPUT));
        // playback 3 times when failed
        int errCount = 3;
        // sendInput fail, retry
        while (errCount-- && (ret == 0)) {
          msg(555);
          sleep(1);
          ret = SendInput(1, &ip, sizeof(INPUT));
        }
        /* mouse_event(mouseRecord->dwFlags, mouseRecord->dx, mouseRecord->dy, mouseRecord->mouseData, 0); */
      }

      // it's keybd event
      if (mode == INPUT_KEYBOARD && readData(keyRecord, sizeof(KEYBDINPUT), &source) == 1) {
        timeSpan = keyRecord->time - prevTime;
        prevTime = keyRecord->time;
        if (timeSpan > 0) sleep(timeSpan);

        /* DWORD isExtended = keyRecord->flags & LLKHF_EXTENDED ? KEYEVENTF_EXTENDEDKEY : 0; */
        /* DWORD isUp = keyRecord->flags & LLKHF_UP ? KEYEVENTF_KEYUP : 0; */
        /* fprintf(logFile2, " %c %X, %X %d %X %X\n", keyRecord->vkCode, keyRecord->scanCode, keyRecord->flags, */
        /*         keyRecord->time, isExtended, isUp); */

        // Set up a generic keyboard event.
        ip.type = INPUT_KEYBOARD;
        ip.ki.wScan = keyRecord->wScan;  // hardware scan code for key
        ip.ki.time = 0;
        ip.ki.dwExtraInfo = keyRecord->dwExtraInfo;

        // Press the "A" key
        ip.ki.wVk = keyRecord->wVk;          // virtual-key code for the "a" key
        ip.ki.dwFlags = keyRecord->dwFlags;  // 0 for key press
        SendInput(1, &ip, sizeof(INPUT));

        /* keybd_event(keyRecord->vkCode, keyRecord->scanCode, isExtended | isUp, */
        /*             keyRecord->dwExtraInfo); */
      }
    }

    cleanUp();

  } else {
    running = FALSE;
    quitEventHandle = OpenEvent(EVENT_ALL_ACCESS, FALSE, quitEventName);
    SetEvent(quitEventHandle);
  }

  /* printf("exit code %i", !running); */
  ExitProcess(!running);
  return !running;
}
