
#include <windows.h>
#include <stdio.h>

char strBuffer[100];


int WINAPI WinMain( HINSTANCE currentInstance, HINSTANCE previousInstance, LPSTR commandLine, int showMode )
{
   
        FILE* logFile;
        char logFilePath[MAX_PATH] = { 0 };
        sprintf( logFilePath, "log.key" );
        logFile = fopen( logFilePath, "r" );


        FILE* logFile2;
        char logFilePath2[MAX_PATH] = { 0 };
        sprintf( logFilePath2, "log.txt" );
        logFile2 = fopen( logFilePath2, "w" );


    if (logFile==NULL) {
        perror("Error read input file");
        return 1;
    }
    else {

        KBDLLHOOKSTRUCT *p= (KBDLLHOOKSTRUCT*)malloc(sizeof(KBDLLHOOKSTRUCT));
        size_t size = sizeof(KBDLLHOOKSTRUCT);

        while( !feof(logFile) && fread(p, size, 1, logFile) == 1 ) {
                         
            sprintf( strBuffer, " %d\n", 1);
            fputs( strBuffer, logFile2 );

            keybd_event(p->vkCode, p->scanCode, p->flags, p->dwExtraInfo);

        }
    }

fclose( logFile );
fclose( logFile2 );

    return 0;
}
