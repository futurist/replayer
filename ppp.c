#include <errno.h>
#include <stdio.h>
#include <windows.h>

char strBuffer[100];
DWORD prevTime = 0;
char *buf = 0;

typedef struct METASTRUCT {
  int version;
  DWORD startTime;
} METASTRUCT;

// a index number to indicate msg source
char outputBuffer[8192];  // sufficently large buffer
void msg(int index) {
  sprintf(outputBuffer, "index %i lastError %i  errno %i", index, GetLastError(), errno);
  MessageBox(NULL, outputBuffer, "Debug Message", MB_OK);
}

int WINAPI WinMain(HINSTANCE currentInstance, HINSTANCE previousInstance, LPSTR commandLine,
                   int showMode) {
  METASTRUCT *meta = (METASTRUCT *)calloc(sizeof(METASTRUCT), 1);
  KEYBDINPUT *keyRecord = (KEYBDINPUT *)calloc(sizeof(KEYBDINPUT), 1);
  MOUSEINPUT *mouseRecord = (MOUSEINPUT *)calloc(sizeof(MOUSEINPUT), 1);

  FILE *logFile;
  FILE *logFile2;
  char logFilePath2[MAX_PATH] = {0};
  sprintf(logFilePath2, "log.txt");
  logFile2 = fopen(logFilePath2, "w");

  int timeSpan = 0;
  INPUT ip = {0};
  DWORD mode = 0;

  // get file size
  LARGE_INTEGER nLargeInteger = {0};
  HANDLE hFile = CreateFile("log.key", GENERIC_READ, FILE_SHARE_READ, NULL,
                            OPEN_EXISTING, FILE_ATTRIBUTE_READONLY, NULL);
  if (hFile != INVALID_HANDLE_VALUE) {
    BOOL bSuccess = GetFileSizeEx(hFile, &nLargeInteger);
  } else {
    msg(99);
    return 1;
  }

  // read all content
  long fsize = nLargeInteger.LowPart;
  buf = malloc(fsize);
  long point = 0;

  DWORD nRead;
  ReadFile(hFile, buf, fsize, &nRead, NULL);
  CloseHandle(hFile);

  /* msg(fread(buf, 1, fsize, logFile)); */
  sprintf(outputBuffer, "%i %x %x %x %x", nRead, buf[fsize - 4], buf[fsize - 3], buf[fsize - 2], buf[fsize - 1]);
  MessageBox(NULL, outputBuffer, "Debug Message", MB_OK);

  return 1;

  if (fread(meta, sizeof(METASTRUCT), 1, logFile) != 1) {
    msg(100);
    return 2;
  }
  prevTime = meta->startTime;
  /* fprintf(logFile2, "time: %d\n", meta->startTime); */

  while (!feof(logFile)) {
    if (!fread(&mode, sizeof(DWORD), 1, logFile)) msg(101);
    // it's mouse event
    if (mode == INPUT_MOUSE) {
      if (!fread(mouseRecord, sizeof(MOUSEINPUT), 1, logFile)) msg(102);
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
      // playback 3 times when failed
      int errCount = 3;
      int ret = SendInput(1, &ip, sizeof(INPUT));
      while (errCount-- && (ret == 0)) {
        sleep(1);
        ret = SendInput(1, &ip, sizeof(INPUT));
      }
      /* mouse_event(mouseRecord->dwFlags, mouseRecord->dx, mouseRecord->dy, mouseRecord->mouseData, 0); */
    }

    // it's keybd event
    if (mode == INPUT_KEYBOARD && fread(keyRecord, sizeof(KEYBDINPUT), 1, logFile) == 1) {
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

  if (buf) free(buf);
  free(meta);
  free(keyRecord);
  free(mouseRecord);
  fclose(logFile);
  fclose(logFile2);

  return 0;
}
