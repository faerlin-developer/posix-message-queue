
INCLUDE = include
QUEUE_SRC = src/*.cpp

build: clean
	g++ -Wall -o bin/server $(QUEUE_SRC) test/server.cpp -I $(INCLUDE)
	g++ -Wall -o bin/client $(QUEUE_SRC) test/client.cpp -I $(INCLUDE)

clean:
	rm -rf bin
	mkdir bin
