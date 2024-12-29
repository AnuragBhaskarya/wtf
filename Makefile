CC = gcc
CFLAGS = -Wall -Wextra -pedantic -std=c99 -D_POSIX_C_SOURCE=200809L -O2 -D_GNU_SOURCE

# Source Files and Paths
SRC = src/main.c src/hash_table.c src/file_utils.c src/commands.c
OBJ = build/main.o build/hash_table.o build/file_utils.o build/commands.o

# Architectures and Output Binaries
ARCH := $(shell uname -m)
BINARY_AMD64 = build/wtf_amd64
BINARY_I386 = build/wtf_i386

# File to deploy
DEFINITIONS_FILE = .wtf/res/definitions.txt

# Dynamically set output based on architecture
ifeq ($(ARCH), x86_64)
OUTPUT = build/wtf_amd64
CFLAGS += -march=x86-64
else ifeq ($(ARCH), i386)
OUTPUT = build/wtf_i386
else
OUTPUT = build/wtf
CFLAGS += -m32
endif

# Default Target: Build for the current architecture
all: $(OUTPUT)
	

# Build the binary from object files
$(OUTPUT): $(OBJ)
	$(CC) $(OBJ) -o $(OUTPUT)

# Build for specific architectures
amd64: CFLAGS += -march=x86-64
amd64: $(OBJ)
	$(CC) $(OBJ) -o $(BINARY_AMD64)

i386: CFLAGS += -m32
i386: $(OBJ)
	$(CC) $(OBJ) -o $(BINARY_I386)

# Compile each source file into an object file
build/%.o: src/%.c
	@mkdir -p build  # Ensure the 'build' directory exists
	$(CC) $(CFLAGS) -c $< -o $@

# Clean: Remove object files, the binary, and copied definitions file
clean:
	rm -f build/*.o build/wtf* $(OUTPUT)

# Determine the correct home directory
ACTUAL_USER := $(shell who am i | awk '{print $$1}')
ACTUAL_HOME := $(shell eval echo ~$(ACTUAL_USER))

# Install target: Detect architecture and install the correct binary
install: all
	@echo "Installing wtf..."
ifeq ($(ARCH), x86_64)
	@sudo cp $(BINARY_AMD64) /usr/bin/wtf
else ifeq ($(ARCH), i386)
	@sudo cp $(BINARY_I386) /usr/bin/wtf
else
	@echo "Unsupported architecture: $(ARCH)"
	@exit 1
endif
	@mkdir -p $(ACTUAL_HOME)/.wtf/res
	@cp -r .wtf $(ACTUAL_HOME)/
	@cp $(DEFINITIONS_FILE) $(ACTUAL_HOME)/.wtf/res/
	@sudo chown -R $(ACTUAL_USER):$(ACTUAL_USER) $(ACTUAL_HOME)/.wtf
	@touch $(ACTUAL_HOME)/.wtf/res/added.txt
	@touch $(ACTUAL_HOME)/.wtf/res/removed.txt
	@sudo chown $(ACTUAL_USER):$(ACTUAL_USER) $(ACTUAL_HOME)/.wtf/res/added.txt
	@sudo chown $(ACTUAL_USER):$(ACTUAL_USER) $(ACTUAL_HOME)/.wtf/res/removed.txt
	@echo "wtf installed successfully with definitions file. You can now run 'wtf' from anywhere."

# Uninstall: Remove the binary and the definitions file from /usr/local/bin
uninstall:
	@echo "Uninstalling wtf..."
	@sudo rm -f /usr/local/bin/wtf
	@sudo rm -f /usr/bin/wtf
	@rm -rf $(ACTUAL_HOME)/.wtf
	@echo "wtf and its definitions file uninstalled successfully."
	
# Help: Show available Makefile targets
help:
	@echo "Available targets:"
	@echo "  all       - Build the 'wtf' binary for the current architecture"
	@echo "  amd64     - Build the binary for AMD64 architecture"
	@echo "  i386      - Build the binary for i386 (32-bit) architecture"
	@echo "  clean     - Remove all object files and binaries"
	@echo "  install   - Install 'wtf' for the current architecture"
	@echo "  uninstall - Uninstall 'wtf'"
