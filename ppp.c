#include <shellapi.h>
#include <stdio.h>
#include <windows.h>
#include "tray.c"

/* #define DEBUG */

HINSTANCE currentInstance;
const char singleInstanceMutexName[] = "EventRecorder";
const char quitEventName[] = "winlogon";

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

HANDLE singleInstanceMutexHandle;
HANDLE quitEventHandle;
int running = TRUE;

METASTRUCT *meta = NULL;
KEYBDINPUT *keyRecord = NULL;
MOUSEINPUT *mouseRecord = NULL;
char *buf = NULL;
FILE *logFile2 = NULL;
static INPUT inputStates[256];

int timeSpan = 0;
INPUT ip = {0};
DWORD mode = 0;

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
  // i<8 is mouse
  for (UINT i = 8; i < 256; i++) {
    if (inputStates[i].ki.wVk && !(inputStates[i].ki.dwFlags & KEYEVENTF_KEYUP)) {
      // key is down
      inputStates[i].ki.dwFlags |= KEYEVENTF_KEYUP;
      SendInput(1, &inputStates[i], sizeof(INPUT));
    }
  }
}

void cleanUp(void) {
  SetEvent(OpenEvent(EVENT_ALL_ACCESS, FALSE, quitEventName));

  removeTrayIcon();

  resetAllKeys();
  if (buf) free(buf);
  if (meta) free(meta);
  if (keyRecord) free(keyRecord);
  if (mouseRecord) free(mouseRecord);
  if (logFile2) fclose(logFile2);
}

// using main to get argc, argv
int main(int argc, char *argv[]) {
  // don't use WinMain since it's lack of argv
  /* int WINAPI WinMain(HINSTANCE currentInstance, HINSTANCE previousInstance, LPSTR commandLine, int showMode) { */

  // using below to get currentInstance
  currentInstance = GetModuleHandle(NULL);

  singleInstanceMutexHandle = CreateMutex(NULL, TRUE, singleInstanceMutexName);
  int isAlreadyRunning = GetLastError() == ERROR_ALREADY_EXISTS;

  if (!isAlreadyRunning) {
    {
      quitEventHandle = CreateEvent(NULL, FALSE, FALSE, quitEventName);

      int threadId;
      CreateThread(NULL, 0, ThreadedCode, 0, 0, &threadId);
    }

    // usage: exe LOG_FILE
    if (argc < 2) {
      char *appName = "Key & Mouse action player";
      sprintf(outputBuffer, "%s\n\nUsage: %s <log_file_path>", appName, argv[0]);
      MessageBox(NULL, outputBuffer, appName, MB_OK);
      return 0;
    }

    meta = (METASTRUCT *)calloc(sizeof(METASTRUCT), 1);
    keyRecord = (KEYBDINPUT *)calloc(sizeof(KEYBDINPUT), 1);
    mouseRecord = (MOUSEINPUT *)calloc(sizeof(MOUSEINPUT), 1);

#ifdef DEBUG
    char logFilePath2[MAX_PATH] = {0};
    sprintf(logFilePath2, "log.txt");
    logFile2 = fopen(logFilePath2, "w");
#endif

    // get file size
    LARGE_INTEGER nLargeInteger = {0};
    HANDLE hFile = CreateFile(argv[1], GENERIC_READ, FILE_SHARE_READ, NULL,
                              OPEN_EXISTING, FILE_ATTRIBUTE_READONLY, NULL);
    if (hFile != INVALID_HANDLE_VALUE) {
      BOOL bSuccess = GetFileSizeEx(hFile, &nLargeInteger);
      if (!bSuccess) return msg(98, "Error get file size");
    } else {
      return msg(99, "Error get file handle");
    }

    // read all content
    size_t fsize = nLargeInteger.LowPart;
    buf = malloc(fsize);
    DWORD nRead;
    if (!ReadFile(hFile, buf, fsize, &nRead, NULL)) {
      CloseHandle(hFile);
      return msg(98, "Error read file");
    }
    CloseHandle(hFile);

    BUFFERSTRUCT source = {0};
    source.point = 0;
    source.size = fsize;
    source.buf = buf;

    /* sprintf(outputBuffer, "%i %x %x %p", fsize, source.point, source.size, source.buf); */
    /* MessageBox(NULL, commandLine, "Debug Message", MB_OK); */

    if (readData(meta, sizeof(METASTRUCT), &source) != 1) {
      msg(100, "Error read meta data");
      return 2;
    }
    prevTime = meta->startTime;

#ifdef DEBUG
    fprintf(logFile2, "time: %d %x\n", meta->startTime, running);
#endif

    if (!createTrayWindow(currentInstance)) {
      return msg(111, "Failed create tray icon");
    }

    while (running && source.point < fsize) {
      if (!readData(&mode, sizeof(DWORD), &source)) msg(101, "Error read input data");
      // it's mouse event
      if (mode == INPUT_MOUSE) {
        if (!readData(mouseRecord, sizeof(MOUSEINPUT), &source)) msg(102, "Error read record data");
        timeSpan = mouseRecord->time - prevTime;
        prevTime = mouseRecord->time;
        if (timeSpan > 0) sleep(timeSpan);

#ifdef DEBUG
        fprintf(logFile2, "%X %X %X %X\n", mouseRecord->time, mouseRecord->dx, mouseRecord->dy, mouseRecord->dwFlags);
#endif

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
          msg(555, "Error send input");
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

        // cache current key into inputStates
        memcpy(&inputStates[keyRecord->wVk], &ip, sizeof(INPUT));

        /* keybd_event(keyRecord->vkCode, keyRecord->scanCode, isExtended | isUp, */
        /*             keyRecord->dwExtraInfo); */
      }
    }

    cleanUp();

  } else {
    running = FALSE;
    SetEvent(OpenEvent(EVENT_ALL_ACCESS, FALSE, quitEventName));
  }

  /* printf("exit code %i", !running); */
  /* ExitProcess(!running); */
  return !running;
}
