CC=gcc
CFLAGS=-Wall -pthread
SDL_CFLAGS=$(shell sdl2-config --cflags)
SDL_LIBS=$(shell sdl2-config --libs)

all: main timer_test pthread_test model_check

main: main.c fish_system.c queue.c logger.c
	$(CC) $(CFLAGS) main.c fish_system.c queue.c logger.c -o main

timer_test: timer_test.c
	$(CC) $(CFLAGS) timer_test.c -o timer_test

pthread_test: pthread_test.c
	$(CC) $(CFLAGS) pthread_test.c -o pthread_test

model_check: model_check.c
	$(CC) $(CFLAGS) model_check.c -o model_check

visual_sim: visual_sim.c fish_system.c
	$(CC) $(CFLAGS) $(SDL_CFLAGS) visual_sim.c fish_system.c -o visual_sim $(SDL_LIBS)

clean:
	rm -f main timer_test pthread_test model_check visual_sim
