
CC = gcc
C++ = g++
CFLAGS = -g -Wall -Wvla -Werror -Wno-error=unused-variable

LIBS = -lqtvis -lQt5OpenGL -lQt5Widgets -lQt5Gui -lQt5Core -lGLX -lOpenGL -lpthread
INCLUDEDIR = -I/usr/local/include/qtvis
DEFINES = -DQT_CORE_LIB -DQT_GUI_LIB -DQT_OPENGL_LIB -DQT_WIDGETS_LIB

all: gol

gol: gol.o
	$(C++) $(CFLAGS) $(INCLUDEDIR) -o gol gol.o  $(LIBS)

gol.o: gol.c colors.h
	$(CC) $(CFLAGS) $(INCLUDEDIR) -c gol.c

clean:
	$(RM) gol *.o
