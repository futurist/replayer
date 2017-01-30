#include <stdio.h>
#include <windows.h>

char strBuffer[100];
DWORD prevTime = 0;

typedef struct METASTRUCT {
  int version;
  DWORD startTime;
} METASTRUCT;

// a index number to indicate msg source
char outputBuffer[8192];  // sufficently large buffer
void msg(int index) {
  sprintf(outputBuffer, "index %i lastError %i", index, GetLastError());
  MessageBox(NULL, outputBuffer, "Debug Message", MB_OK);
}

int main() {
  INPUT ip = {0};
  int retval = 0;

  char buffer[8192];  // sufficently large buffer

  LARGE_INTEGER nLargeInteger = {0};
  HANDLE hFile = CreateFile("log.key", GENERIC_READ, FILE_SHARE_READ, NULL,
                            OPEN_EXISTING, FILE_ATTRIBUTE_READONLY, NULL);
  if (hFile != INVALID_HANDLE_VALUE) {
    BOOL bSuccess = GetFileSizeEx(hFile, &nLargeInteger);
  } else {
    msg(99);
    return 1;
  }

  /* msg(nLargeInteger.LowPart); */
  /* msg(nLargeInteger.HighPart); */

  /* HANDLE hout = CreateFile("log.key", GENERIC_READ, 0, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, 0); */
  /* if (hout == INVALID_HANDLE_VALUE) msg(333); */

  char *buf = 0;
  long fsize = nLargeInteger.LowPart;

  // read all content
  buf = malloc(fsize);
  long point = 0;
  char c = 0;

  DWORD nRead;
  ReadFile(hFile, buf, fsize, &nRead, NULL);
  msg(nRead);
  CloseHandle(hFile);

  sleep(0);
  ip.type = INPUT_MOUSE;
  ip.mi.dx = 0x72AE;
  ip.mi.dy = 0x18E7;
  /* ip.mi.mouseData = mouseRecord->mouseData; */
  ip.mi.dwFlags = 0x8001;
  ip.mi.dwExtraInfo = 0;
  ip.mi.time = 0;
  retval = SendInput(1, &ip, sizeof(INPUT));
  if (retval > 0) {
    sprintf(buffer, "SendInput sent %i", retval);
  } else {
    sprintf(buffer, "Unable to send input commands. Error is: %i", GetLastError());
  }
  MessageBox(NULL, buffer, "Debug Message", MB_OK);
  return 0;
}
