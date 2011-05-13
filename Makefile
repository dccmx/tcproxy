tcproxy: tcproxy.c event.c util.c event.h util.h
	gcc -Wall -o tcproxy -g -ggdb tcproxy.c event.c util.c

run: tcproxy
	rm -rf core.*
	./tcproxy

clean:
	rm -rf tcproxy core.*
