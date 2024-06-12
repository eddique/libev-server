CC = gcc
CFLAGS = -g -Wall -Iinclude -I/opt/homebrew/opt/libev/include # -Os -ffunction-sections -fdata-sections
LDFLAGS = -L/opt/homebrew/opt/libev/lib -lev # -Wl,--gc-sections -static
SRC_DIR = src
OBJ_DIR = obj
BIN_DIR = bin
SOURCES = $(wildcard $(SRC_DIR)/*.c)
OBJECTS = $(patsubst $(SRC_DIR)/%.c, $(OBJ_DIR)/%.o, $(SOURCES))
EXECUTABLE = $(BIN_DIR)/libev-server

all: dirs $(EXECUTABLE)


dirs:
	mkdir -p $(OBJ_DIR) $(BIN_DIR)
$(EXECUTABLE): $(OBJECTS)
	$(CC) $(CFLAGS) $^ $(LDFLAGS) -o $@

$(OBJ_DIR)/%.o: $(SRC_DIR)/%.c
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -f $(OBJ_DIR)/*.o
	rm -f $(BIN_DIR)/*

.PHONY: all clean
