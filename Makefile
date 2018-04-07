all: memcpy wordcount trampoline

memcpy:
	g++ -o memcpy memcpy.cpp

wordcount:
	g++ -std=c++14 -m64 -mssse3 -O2 -Wall -o wordcount wordcount.cpp

trampoline:
	g++ -std=c++14 -o trampoline trampoline.cpp

clean:
	rm memcpy wordcount trampoline
