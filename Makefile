CC ?= cc

ifeq ($(OS),Windows_NT)
	TARGET ?= bfc.exe
else
	TARGET ?= bfc
endif

.PHONY: debug clean

$(TARGET): bfc.c
	$(CC) bfc.c -O2 -Wall -Wextra -pedantic -o $(TARGET)

debug: bfc.c
	$(CC) bfc.c -O0 -Wall -Wextra -pedantic -g3 -fsanitize=address,undefined,leak -o $(TARGET)

clean:
	rm -f $(TARGET)
