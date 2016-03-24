EXECUTABLE:=amdgpuinfo
CFLAGS:=-Wall -Wextra -std=c99 -D_FORTIFY_SOURCE=2
ADL_SDK:=./ADL_SDK/include

CFLAGS+=-DLINUX -I $(ADL_SDK)
LDLIBS=-ldl

# Enable gdb over dlopen(). Use ./gdbinit instead
# LDLIBS+=-lpthread

OBJS=amdinfo.o

all: default
default: release

release: CFLAGS+=-g0 -O3 -s
release: $(EXECUTABLE)

debug: CFLAGS+=-g3 -O0
debug: $(EXECUTABLE)

$(EXECUTABLE): $(OBJS)
	$(CC) $(OBJS) $(LDLIBS) -o $(EXECUTABLE)

clean:
	rm -f -- $(EXECUTABLE) $(OBJS)

.PHONY: all default release debug clean
