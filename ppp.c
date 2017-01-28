#include <stdio.h>
#include <windows.h>

char strBuffer[100];
DWORD prevTime = 0;

typedef struct METASTRUCT {
  int version;
  DWORD startTime;
} METASTRUCT;

int WINAPI WinMain(HINSTANCE currentInstance, HINSTANCE previousInstance,
                   LPSTR commandLine, int showMode) {

  FILE *logFile;
  char logFilePath[MAX_PATH] = {0};
  sprintf(logFilePath, "log.key");
  logFile = fopen(logFilePath, "r");

  FILE *logFile2;
  char logFilePath2[MAX_PATH] = {0};
  sprintf(logFilePath2, "log.txt");
  logFile2 = fopen(logFilePath2, "w");

  if (logFile == NULL) {
    perror("Error read input file");
    return 1;
  } else {

    int timeSpan = 0;
    METASTRUCT *meta = (METASTRUCT *)malloc(sizeof(METASTRUCT));
    KBDLLHOOKSTRUCT *keyRecord =
        (KBDLLHOOKSTRUCT *)malloc(sizeof(KBDLLHOOKSTRUCT));

    if (fread(meta, sizeof(METASTRUCT), 1, logFile) != 1) {
      perror("Error read header");
      return 2;
    }
    prevTime = meta->startTime;
    fprintf(logFile2, "time: %d\n", meta->startTime);

    while (!feof(logFile) &&
           fread(keyRecord, sizeof(KBDLLHOOKSTRUCT), 1, logFile) == 1) {

      if (prevTime) {

        timeSpan = keyRecord->time - prevTime;
        if (timeSpan < 0) {
          perror("Error time record");
          return 3;
        }
        if (timeSpan > 0)
          sleep(timeSpan);

        BOOL isExtended = keyRecord->flags & LLKHF_EXTENDED ? KEYEVENTF_EXTENDEDKEY : 0;
        BOOL isUp = keyRecord->flags & LLKHF_UP ? KEYEVENTF_KEYUP : 0;
        fprintf(logFile2, " %c %X %d %X %X\n", keyRecord->vkCode, keyRecord->flags, keyRecord->time, isExtended, isUp);
        keybd_event(keyRecord->vkCode, keyRecord->scanCode, isExtended | isUp,
                    keyRecord->dwExtraInfo);
      }
      prevTime = keyRecord->time;
    }
  }

  fclose(logFile);
  fclose(logFile2);

  return 0;
}
