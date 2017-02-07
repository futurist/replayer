// from winvnc/dpi.h

#define MOUSEEVENTF_VIRTUALDESK 0x4000

BOOL _fInitialized = FALSE;
int _dpiX;
int _dpiY;

void _InitDPI(void) {
  if (!_fInitialized) {
    HDC hdc = GetDC(NULL);
    if (hdc) {
      RECT rect;
      GetClipBox(hdc, &rect);
      int dpiwidth = GetSystemMetrics(SM_CXVIRTUALSCREEN);
      int dpiheight = GetSystemMetrics(SM_CYVIRTUALSCREEN);
      int fullwidth = rect.right - rect.left;
      int fullheight = rect.bottom - rect.top;
      // Seeme GetDC can return NULL, in that case fullwidth=0 and devide by 0
      if (fullwidth == 0) fullwidth = dpiwidth;
      if (fullheight == 0) fullheight = dpiheight;

      _dpiX = dpiwidth * 96 / fullwidth;    //GetDeviceCaps(hdc, LOGPIXELSX);
      _dpiY = dpiheight * 96 / fullheight;  //GetDeviceCaps(hdc, LOGPIXELSY);
      ReleaseDC(NULL, hdc);
    }
    _fInitialized = TRUE;
  }
}

// Convert between raw pixels and relative pixels.
int ScaleX(int x) {
  _InitDPI();
  return MulDiv(x, _dpiX, 96);
}
int ScaleY(int y) {
  _InitDPI();
  return MulDiv(y, _dpiY, 96);
}

int _ScaledSystemMetricX(int nIndex) {
  _InitDPI();
  return MulDiv(GetSystemMetrics(nIndex), 96, _dpiX);
}

int _ScaledSystemMetricY(int nIndex) {
  _InitDPI();
  return MulDiv(GetSystemMetrics(nIndex), 96, _dpiY);
}

// Determine the screen dimensions in relative pixels.
int ScaledScreenWidth(void) { return _ScaledSystemMetricX(SM_CXSCREEN); }
int ScaledScreenHeight(void) { return _ScaledSystemMetricY(SM_CYSCREEN); }
int ScaledScreenVirtualWidth(void) { return _ScaledSystemMetricX(SM_CXVIRTUALSCREEN); }
int ScaledScreenVirtualHeight(void) { return _ScaledSystemMetricY(SM_CYVIRTUALSCREEN); }
int ScaledScreenVirtualX(void) { return _ScaledSystemMetricX(SM_XVIRTUALSCREEN); }
int ScaledScreenVirtualY(void) { return _ScaledSystemMetricY(SM_YVIRTUALSCREEN); }
