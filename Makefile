CC      = gcc
CFLAGS  = -Wall -Wextra -O2
LDFLAGS = -lncursesw -lm
TARGET  = drawdag

SRCS    = src/main.c src/graph.c src/sugiyama.c src/canvas.c src/render.c src/parse.c
OBJS    = $(SRCS:.c=.o)

$(TARGET): $(OBJS)
	$(CC) $(CFLAGS) -o $@ $(OBJS) $(LDFLAGS)

src/%.o: src/%.c src/drawdag.h
	$(CC) $(CFLAGS) -c -o $@ $<

debug: CFLAGS = -Wall -Wextra -g -O0
debug: $(TARGET)

valgrind: debug
	valgrind --leak-check=full --show-leak-kinds=all --error-exitcode=1 \
		./$(TARGET) --print
	@echo "=== valgrind: OK ==="

valgrind-file: debug edges.txt
	valgrind --leak-check=full --show-leak-kinds=all --error-exitcode=1 \
		./$(TARGET) --print edges.txt
	@echo "=== valgrind (file): OK ==="

clean:
	rm -f $(TARGET) src/*.o

.PHONY: clean debug valgrind valgrind-file
