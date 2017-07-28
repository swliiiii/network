
CC=gcc620
CXX=g++620
INC_PATH = network
CFLAGS += -g -MMD
CFLAGS += -I$(INC_PATH) -I$(INC_PATH)/linux

LDFLAGS += -lpthread -lrt -static-libstdc++

TARGET_PATH=./bin
TARGET= ./bin/network
OUTPUT_PATH = ./obj

SUBDIR =Tool linux .


#…Ë÷√VPATH
EMPTY = 
SPACE = $(EMPTY)$(EMPTY) 
VPATH = $(subst $(SPACE), : ,$(strip $(foreach n,$(SUBDIR), $(INC_PATH)/$(n)))) : $(OUTPUT_PATH)

STD_OBJECT = /usr/local/gcc-6.2.0/lib64/libstdc++.a
CXX_SOURCES = $(notdir $(foreach n, $(SUBDIR), $(wildcard $(INC_PATH)/$(n)/*.cpp)))
CXX_OBJECTS = $(patsubst  %.cpp,  %.o, $(CXX_SOURCES))
DEP_FILES = $(patsubst  %.cpp,  $(OUTPUT_PATH)/%.d, $(CXX_SOURCES))

$(TARGET):  $(CXX_OBJECTS)
	@if [ ! -d $(dir $(TARGET)) ] ; then  \
	 echo mkdir $(dir $(TARGET)); \
   	 mkdir $(dir $(TARGET)) ;  \
	fi
	$(CXX) $(LDFLAGS) -o $@ $(foreach n, $(CXX_OBJECTS), $(OUTPUT_PATH)/$(n))
	#******************************************************************************#
	#                          Bulid successful !                                  #
	#******************************************************************************#

	
%.o:%.cpp
	@if [ ! -d $(OUTPUT_PATH) ] ; then  \
	 echo mkdir $(OUTPUT_PATH); \
   	 mkdir $(OUTPUT_PATH) ;  \
	fi
	$(CXX) -c $(CFLAGS) -MT $@ -MF $(OUTPUT_PATH)/$(notdir $(patsubst  %.cpp, %.d,  $<)) -o $(OUTPUT_PATH)/$@ $< 

	
-include $(DEP_FILES)

test:
	@echo $(CXX_OBJECTS)
	
mkdir:
	mkdir -p $(dir $(TARGET))
	mkdir -p $(OUTPUT_PATH)
	
rmdir:
	rm -rf $(dir $(TARGET))
	rm -rf $(OUTPUT_PATH)

clean:
	rm -f $(OUTPUT_PATH)/*
all:
	@make clean
	@make
release:
	./build.sh
