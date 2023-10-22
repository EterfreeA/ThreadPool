Root=..
Source=$(Root)/Source
Include=$(Root)/Source
Binary=$(Root)/Binary

CC=g++
CFLAG=-std=c++17 -I$(Include) -c

OBJS=$(Source)/Thread.o $(Source)/ThreadPool.o test.o

default:$(OBJS)
	${CC} $^ -o $(Binary)/test
%.o:%.cpp
	$(CC) $(CFLAG) $< -o $@

clean:
	rm -rf $(OBJS) $(Binary)/test
