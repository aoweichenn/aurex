ROOT_DIR ?= ..
BUILD_DIR ?= $(ROOT_DIR)/axon/build
M0C ?= $(ROOT_DIR)/build/m0c
CC ?= cc
RUNTIME ?= $(ROOT_DIR)/selfhost/runtime/runtime.c
M0FLAGS = -I $(ROOT_DIR) -I $(ROOT_DIR)/selfhost/src

all: $(BUILD_DIR)/nex

$(BUILD_DIR)/nex.c: $(ROOT_DIR)/axon/nex.ax $(ROOT_DIR)/axon/nex/scan.ax
	mkdir -p $(BUILD_DIR)
	$(M0C) $(M0FLAGS) $(ROOT_DIR)/axon/nex.ax -o $(BUILD_DIR)/nex.c

$(BUILD_DIR)/nex: $(BUILD_DIR)/nex.c
	$(CC) $(BUILD_DIR)/nex.c $(RUNTIME) -o $(BUILD_DIR)/nex

.PHONY: all clean

clean:
	rm -rf $(BUILD_DIR)
