#Copyright (C) 2021 by Thomas A. Early, N7TAE

include tcd.mk

GCC = g++

ifeq ($(debug), true)
CFLAGS = -ggdb3 -W -Werror -Icodec2 -MMD -MD -std=c++17
else
CFLAGS = -W -Werror -Icodec2 -MMD -MD -std=c++17
endif

LDFLAGS = -pthread

SRCS = $(wildcard *.cpp) $(wildcard codec2/*.cpp) $(wildcard imbe_vocoder/*.cpp) $(wildcard mbe/*.cpp)
OBJS = $(SRCS:.cpp=.o)
DEPS = $(SRCS:.cpp=.d)
EXE = tcd

$(EXE) : $(OBJS)
	$(GCC) $(OBJS) $(LDFLAGS) -o $@

%.o : %.cpp
	$(GCC) $(CFLAGS) -c $< -o $@

clean :
	$(RM) $(EXE) $(OBJS) $(DEPS)

-include $(DEPS)

# The install and uninstall targets need to be run by root
install : $(EXE)
	cp $(EXE) $(BINDIR)
	cp $(EXE).service /etc/systemd/system/
	systemctl enable $(EXE)
	systemctl daemon-reload
	systemctl start $(EXE)

uninstall :
	systemctl stop $(EXE)
	systemctl disable $(EXE)
	systemctl daemon-reload
	rm -f /etc/systemd/system/$(EXE).service
	rm -f $(BINDIR)/$(EXE)
	systemctl daemon-reload
