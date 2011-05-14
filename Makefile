tcproxy: tcproxy.c event.c util.c event.h util.h
	gcc -o tcproxy -g -ggdb tcproxy.c event.c util.c

opt: tcproxy.c event.c util.c event.h util.h
	gcc -O3 -o tcproxy -g tcproxy.c event.c util.c

run: tcproxy
	rm -rf core.*
	./tcproxy

callgrind: tcproxy
	valgrind --tool=callgrind --collect-systime=yes ./tcproxy

massif: tcproxy
	valgrind --tool=massif ./tcproxy

memcheck: tcproxy
	valgrind --leak-check=full --log-file=memcheck.out ./tcproxy

clean:
	rm -rf tcproxy core.*
