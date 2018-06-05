all: app_copy1 app_paste1 app_wait1 serverclip

app_copy1: app_copy1.o mylib.o clipboard.o
	gcc app_copy1.c mylib.c clipboard.c -o app_copy1

app_paste1: app_paste1.o mylib.o clipboard.o
	gcc app_paste1.c mylib.c clipboard.c -o app_paste1

app_wait1: app_wait1.o mylib.o clipboard.o
	gcc app_wait1.c mylib.c clipboard.c -o app_wait1

serverclip: serverclip.o mylib.o clipboard.o
	gcc -pthread serverclip.c mylib.c clipboard.c -o clipboard