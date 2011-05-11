tcproxy: tcproxy.c
	gcc -Wall -o tcproxy -g -ggdb tcproxy.c event.c util.c
