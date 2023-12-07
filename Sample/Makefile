IGNORE := .
ROOT := ..
SOURCE := $(ROOT)/Source
INCLUDE := $(ROOT)/Source
BINARY := $(ROOT)/Binary

CXXFLAGS := -std=c++17 -I$(Include)

TARGET := $(BINARY)/test

OBJECTS :=
OBJECTS += $(SOURCE)/Thread.o
OBJECTS += $(SOURCE)/ThreadPool.o
OBJECTS += test.o

default: $(OBJECTS)
	${CXX} $^ -o $(TARGET)
%.o: %.cpp
	${CXX} $(CXXFLAGS) -c $< -o $@

clean:
	rm -rf $(OBJECTS) $(TARGET)
