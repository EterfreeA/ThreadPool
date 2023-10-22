Root=..
Source=$(Root)/Source
Include=$(Root)/Source
Binary=$(Root)/Binary

CC=g++
CFLAG=-std=c++20 -I$(Include) -c

OBJS=$(Source)/Concurrency/TaskManager.o $(Source)/Concurrency/TaskQueue.o $(Source)/Concurrency/Thread.o $(Source)/Concurrency/ThreadPool.o $(Source)/Core/Logger.o $(Source)/Core/Timer.o $(Source)/Core/SpinAdapter.o $(Source)/Platform/Core/Linux/Common.o test.o

default:$(OBJS)
	${CC} $^ -o $(Binary)/test
%.o:%.cpp
	$(CC) $(CFLAG) $< -o $@

clean:
	rm -rf $(OBJS) $(Binary)/test
