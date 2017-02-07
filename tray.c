// tray icon message
#define TRAY_ID 1508
#define HWND_MESSAGE ((HWND)-3)
#define WM_MYMESSAGE ((WM_USER + TRAY_ID))

NOTIFYICONDATA nid;
HWND hTrayWnd;
static const char *CLASS_NAME = "DUMMY_CLASS";
WNDCLASSEX wx = {0};

void removeTrayIcon(void) {
  nid.uFlags = 0;
  Shell_NotifyIcon(NIM_DELETE, &nid);
}

LRESULT CALLBACK TrayWndProc(HWND hWnd, UINT Msg, WPARAM wParam, LPARAM lParam) {
  switch (Msg) {
    case WM_CREATE:
      break;
    case WM_MYMESSAGE:
      if (lParam == WM_LBUTTONDOWN) {
        // should pause/resume
      }
      break;
    case WM_DESTROY:
      PostQuitMessage(WM_QUIT);
      break;
    default:
      return DefWindowProc(hWnd, Msg, wParam, lParam);
      break;
  }
  return 0;
}

BOOL createTrayIcon(void) {
  ZeroMemory(&nid, sizeof(nid));

  nid.cbSize = sizeof(NOTIFYICONDATA);
  nid.hWnd = hTrayWnd;
  nid.uID = TRAY_ID;
  nid.uVersion = NOTIFYICON_VERSION;
  nid.uCallbackMessage = WM_MYMESSAGE;
  nid.hIcon = LoadIcon(NULL, IDI_APPLICATION);
  strcpy(nid.szTip, "input player");
  nid.uFlags = NIF_MESSAGE | NIF_ICON | NIF_TIP;

  return Shell_NotifyIcon(NIM_ADD, &nid);
}

BOOL createTrayWindow(HINSTANCE hInstance) {
  wx.cbSize = sizeof(WNDCLASSEX);
  wx.lpfnWndProc = (WNDPROC)TrayWndProc;  // function which will handle messages
  wx.hInstance = hInstance;
  wx.hIcon = (HICON)LoadIcon(NULL, (LPCTSTR)MAKEINTRESOURCE((WORD)IDI_ASTERISK));
  wx.lpszClassName = CLASS_NAME;
  if (RegisterClassEx(&wx)) {
    hTrayWnd = CreateWindowEx(0, CLASS_NAME, "dummy_name", 0, 0, 0, 0, 0, HWND_MESSAGE, NULL, NULL, NULL);
    return createTrayIcon();
  }
  return FALSE;
}
