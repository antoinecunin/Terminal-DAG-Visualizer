CC      = gcc
CFLAGS  = -Wall -Wextra -O2
LDFLAGS = -lncursesw -lm
TARGET  = drawdag

$(TARGET): drawdag.c
	$(CC) $(CFLAGS) -o $@ $< $(LDFLAGS)

debug: drawdag.c
	$(CC) -Wall -Wextra -g -O0 -o $(TARGET) $< $(LDFLAGS)

valgrind: debug
	valgrind --leak-check=full --show-leak-kinds=all --error-exitcode=1 \
		./$(TARGET) --print
	@echo "=== valgrind: OK ==="

valgrind-file: debug edges.txt
	valgrind --leak-check=full --show-leak-kinds=all --error-exitcode=1 \
		./$(TARGET) --print edges.txt
	@echo "=== valgrind (file): OK ==="

clean:
	rm -f $(TARGET)

.PHONY: clean debug valgrind valgrind-file
