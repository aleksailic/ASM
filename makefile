TARGET ?= assembler
SRC_DIRS ?= .
CC = g++

SRCS := $(shell find $(SRC_DIRS) -name *.cpp)
OBJS := $(addsuffix .o, $(basename $(SRCS)))
DEPS := $(addsuffix .d, $(basename $(SRCS)))

INC_DIRS := libs includes
INC_FLAGS := $(addprefix -I, $(INC_DIRS))

CPPFLAGS ?= $(INC_FLAGS) -MMD -MP -Wall -std=c++17

$(TARGET) : $(OBJS)
	$(CC) $(LDFLAGS) $(OBJS) -o $@ $(LOADLIBES) $(LDLIBS) -lstdc++fs 

.PHONY: clean
clean :
	$(RM) $(TARGET) $(OBJS) $(DEPS)

-include $(DEPS)