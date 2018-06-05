all: app_copy2 app_paste2 app_wait2 serverclip

app_copy2: app_copy2.o mylib.o clipboard.o
	gcc app_copy2.c mylib.c clipboard.c -o app_copy2

app_paste2: app_paste2.o mylib.o clipboard.o
	gcc app_paste2.c mylib.c clipboard.c -o app_paste2

app_wait2: app_wait2.o mylib.o clipboard.o
	gcc app_wait2.c mylib.c clipboard.c -o app_wait2

serverclip: serverclip.o mylib.o clipboard.o
	gcc -pthread serverclip.c mylib.c clipboard.c -o clipboard