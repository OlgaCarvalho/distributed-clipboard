all: app_copy3 app_paste3 app_wait3 serverclip

app_copy3: app_copy3.o mylib.o clipboard.o
	gcc app_copy3.c mylib.c clipboard.c -o app_copy3

app_paste3: app_paste3.o mylib.o clipboard.o
	gcc app_paste3.c mylib.c clipboard.c -o app_paste3

app_wait3: app_wait3.o mylib.o clipboard.o
	gcc app_wait3.c mylib.c clipboard.c -o app_wait3

serverclip: serverclip.o mylib.o clipboard.o
	gcc -pthread serverclip.c mylib.c clipboard.c -o clipboard