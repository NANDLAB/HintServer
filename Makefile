.PHONY: all clean

SHCHECK = $(if $(filter $(.SHELLSTATUS),0),,$(error Shell function returned non-zero exit code!))

CC  := gcc
CXX := g++
RM  := rm
SED := sed

CPPFLAGS := -pthread
CFLAGS   := -O3 -Wall
CXXFLAGS := -O3 -Wall $(shell pkg-config --cflags Magick++)$(SHCHECK)
LDFLAGS  := -pthread
LDLIBS   := $(shell pkg-config --libs Magick++)$(SHCHECK) -lvlc -lboost_system

OUT := HintServer
SRC := $(wildcard *.c *.cpp)
OBJ := $(addsuffix .o,$(basename $(SRC)))
DEP := $(addsuffix .d,$(basename $(SRC)))

all: $(OUT)

ifneq ($(MAKECMDGOALS),clean)
include $(DEP)
endif

$(OUT): $(OBJ)
	$(CXX) -o $@ $^ $(LDFLAGS) $(LDLIBS)

%.o: %.cpp
	$(CXX) -o $@ $< -c $(CPPFLAGS) $(CXXFLAGS)

%.o: %.c
	$(CC) -o $@ $< -c $(CPPFLAGS) $(CFLAGS)

%.d: %.cpp
	$(CXX) -M $(CPPFLAGS) $(CXXFLAGS) $< | $(SED) 's,\($*\)\.o[ :]*,\1.o $@ : ,g' > $@

%.d: %.c
	$(CC) -M $(CPPFLAGS) $(CFLAGS) $< | $(SED) 's,\($*\)\.o[ :]*,\1.o $@ : ,g' > $@

clean:
	$(RM) -f $(OUT) $(OBJ) $(DEP)
