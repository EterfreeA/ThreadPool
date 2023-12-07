IGNORE := .
ROOT := ..
SOURCE := $(ROOT)/Source
INCLUDE := $(ROOT)/Source
BINARY := $(ROOT)/Binary

CXXFLAGS := -std=c++20 -I$(INCLUDE)

TARGET := $(BINARY)/test

OBJECTS :=
OBJECTS += $(SOURCE)/Eterfree/Concurrency/TaskManager.o
OBJECTS += $(SOURCE)/Eterfree/Concurrency/TaskQueue.o
OBJECTS += $(SOURCE)/Eterfree/Concurrency/Thread.o
OBJECTS += $(SOURCE)/Eterfree/Concurrency/ThreadPool.o
OBJECTS += $(SOURCE)/Eterfree/Core/Logger.o
OBJECTS += $(SOURCE)/Eterfree/Core/SpinAdapter.o
OBJECTS += $(SOURCE)/Eterfree/Core/Timer.o
OBJECTS += $(SOURCE)/Eterfree/Platform/Core/Linux/Common.o
OBJECTS += test.o

default: $(OBJECTS)
	${CXX} $^ -o $(TARGET)
%.o: %.cpp
	${CXX} $(CXXFLAGS) -c $< -o $@

clean:
	rm -rf $(OBJECTS) $(TARGET)
