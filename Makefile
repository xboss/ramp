CC = gcc
CFLAGS = -c -Wall
# CDEBUG = -g 
CDEBUG = -g -DDEBUG
OUTPUT_DIR = ./build/
SRC = $(wildcard src/*.c)
OBJ = $(patsubst src/%.c, %.o, $(SRC))
OBJ_OUT = $(patsubst src/%.c, $(OUTPUT_DIR)%.o, $(SRC))
INCLUDE = -I./src -I./src/3rd/EasyTCP/src
# LIB = -lev -lssl -lcrypto -lpthread
LIB = -lev -lssl -lcrypto

EASYTCP_SRC = $(wildcard src/3rd/EasyTCP/src/*.c)
EASYTCP_OBJ = $(patsubst src/3rd/EasyTCP/src/%.c, %.o, $(EASYTCP_SRC))
EASYTCP_OBJ_OUT = $(patsubst src/3rd/EasyTCP/src/%.c, $(OUTPUT_DIR)%.o, $(EASYTCP_SRC))

# TEST_SRC = $(wildcard test/*.c)
# TEST_OBJ = $(patsubst test/%.c, %.o, $(TEST_SRC))

.PHONY:all clean test


all: $(OBJ) $(EASYTCP_OBJ)
	$(CC) $(OBJ_OUT) $(EASYTCP_OBJ_OUT) -o $(OUTPUT_DIR)ramp $(LIB)
# $(CC) $(OBJ_OUT) $(OUTPUT_DIR)skcp_client.o -o $(OUTPUT_DIR)skcp_client $(LIB)
# $(CC) $(OBJ_OUT) $(OUTPUT_DIR)skcp_server.o -o $(OUTPUT_DIR)skcp_server $(LIB)

%.o: src/%.c
	@echo $< $@
	$(CC) $(INCLUDE) $(CFLAGS) $(CDEBUG) $< -o $(OUTPUT_DIR)$@

%.o: src/3rd/EasyTCP/src/%.c
	@echo $< $@
	$(CC) $(INCLUDE) $(CFLAGS) $(CDEBUG) $< -o $(OUTPUT_DIR)$@

clean:
	rm -rf $(OUTPUT_DIR)*