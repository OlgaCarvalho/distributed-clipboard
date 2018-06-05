all: application serverclip

application: application.o mylib.o clipboard.o
	gcc application.c mylib.c clipboard.c -o application

serverclip: serverclip.o mylib.o clipboard.o
	gcc -pthread serverclip.c mylib.c clipboard.c -o clipboard