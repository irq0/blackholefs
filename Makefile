CFLAGS = -c -Wall -std=gnu99 -Wpedantic

CFLAGS += `pkg-config --cflags fuse`
LDFLAGS += `pkg-config --libs fuse`

CFLAGS += -ggdb -fno-omit-frame-pointer -O1

SRCS = bhfs.c
OBJS = $(SRCS:.c=.o)
EXE = bhfs

all: $(EXE)

$(EXE): $(OBJS)
	$(CC) $(OBJS) -o $@ $(LDFLAGS)

*.o: *.c
	$(CC) $(CFLAGS) $< -o $@

clean:
	rm -rf $(OBJS) $(EXE)
