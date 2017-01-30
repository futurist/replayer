#include <stdio.h>
#include <windows.h>

char strBuffer[100];
DWORD prevTime = 0;

typedef struct METASTRUCT {
  int version;
  DWORD startTime;
} METASTRUCT;

int main() {
  INPUT ip = {0};
  int retval = 0;

  char buffer[8192];  // sufficently large buffer

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
