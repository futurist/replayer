# Wedit Makefile for project ppp
SRCDIR=f:\crx\replayer
CFLAGS=-I"d:\lcc\include" -I"D:\lcc\include"  -g2 
CC=$(LCCROOT)\bin\lcc.exe
LINKER=$(LCCROOT)\bin\lcclnk.exe
OBJS=\
	ppp.obj \
	app.res

LIBS=shell32.lib
EXE=ppp.exe

$(EXE):	$(OBJS) Makefile
	$(LINKER)  -subsystem windows -o $(SRCDIR)\lcc\ppp.exe $(OBJS) $(LIBS)

# Build ppp.c
PPP_C=\
	$(SRCDIR)\tray.c\

ppp.obj: $(PPP_C) $(SRCDIR)\ppp.c
	$(CC) -c $(CFLAGS) $(SRCDIR)\ppp.c

# Build app.res
APP_RC=\

app.res:	$(APP_RC) $(SRCDIR)\app.rc
	$(LCCROOT)\bin\lrc.exe -I$(SRCDIR) -I"d:\lcc\include" -I"D:\lcc\include"  $(SRCDIR)\app.rc

link:
	$(LINKER)  -subsystem windows -o $(SRCDIR)\lcc\ppp.exe $(OBJS) $(LIBS)

clean:
	del $(OBJS) ppp.exe
