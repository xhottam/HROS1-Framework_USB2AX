###############################################################
#
# Purpose: Makefile for "action_editor"
# Author.: robotis
# Version: 0.1
# License: GPL
#
###############################################################

TARGET = bin/rme

CXX = g++
INCLUDE_DIRS = -I../../include -I../../../Framework/include
CXXFLAGS += -fPIC -O2 -DLINUX -g -Wall -fmessage-length=0 $(INCLUDE_DIRS)
LIBS += -lpthread -lncurses -lrt -lbluetooth

OBJS =	./cmd_process.o	\
		./main.o


all: darwin.a $(TARGET)

darwin.a:
	make -C ../../build

$(TARGET): $(OBJS) ../../lib/darwin.a
	$(CXX) -o $(TARGET) $(OBJS) ../../lib/darwin.a $(LIBS)
	
clean:
	rm -f $(OBJS) $(TARGET)





