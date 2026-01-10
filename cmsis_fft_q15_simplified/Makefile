# Makefile for CMSIS FFT Q15 Simplified Library
# Target: PC (x86_64) for validation

CC = gcc
CFLAGS = -Wall -Wextra -O2 -g -Iinclude
LDFLAGS = -lm

# Source files
SRC_DIR = src
TEST_DIR = test
BUILD_DIR = build

SOURCES = $(SRC_DIR)/rfft_init_q15.c \
          $(SRC_DIR)/rfft_q15.c \
          $(SRC_DIR)/cfft_q15.c \
          $(SRC_DIR)/cfft_radix4_q15.c \
          $(SRC_DIR)/bit_reversal.c \
          $(SRC_DIR)/twiddle_tables.c

OBJECTS = $(SOURCES:$(SRC_DIR)/%.c=$(BUILD_DIR)/%.o)

# Test programs
TEST_API = $(BUILD_DIR)/test_api
TEST_EXAMPLES = $(BUILD_DIR)/test_examples
TEST_PROPERTIES = $(BUILD_DIR)/test_properties
TEST_FFT_MAIN = $(BUILD_DIR)/test_fft_main

.PHONY: all clean test test-examples test-properties

all: $(BUILD_DIR) $(TEST_API) $(TEST_EXAMPLES) $(TEST_PROPERTIES) $(TEST_FFT_MAIN)

$(BUILD_DIR):
	mkdir -p $(BUILD_DIR)

$(BUILD_DIR)/%.o: $(SRC_DIR)/%.c
	$(CC) $(CFLAGS) -c $< -o $@

$(TEST_API): $(OBJECTS) $(TEST_DIR)/test_api.c
	$(CC) $(CFLAGS) $(OBJECTS) $(TEST_DIR)/test_api.c -o $@ $(LDFLAGS)

$(TEST_EXAMPLES): $(OBJECTS) $(TEST_DIR)/test_examples.c
	$(CC) $(CFLAGS) $(OBJECTS) $(TEST_DIR)/test_examples.c -o $@ $(LDFLAGS)

$(TEST_PROPERTIES): $(OBJECTS) $(TEST_DIR)/test_properties.c
	$(CC) $(CFLAGS) $(OBJECTS) $(TEST_DIR)/test_properties.c -o $@ $(LDFLAGS)

$(TEST_FFT_MAIN): $(OBJECTS) $(TEST_DIR)/test_fft_main.c
	$(CC) $(CFLAGS) $(OBJECTS) $(TEST_DIR)/test_fft_main.c -o $@ $(LDFLAGS)

test: $(TEST_API)
	@echo "Running API tests..."
	@./$(TEST_API)

test-examples: $(TEST_EXAMPLES)
	@echo "Running unit tests..."
	@./$(TEST_EXAMPLES)

test-properties: $(TEST_PROPERTIES)
	@echo "Running property-based tests..."
	@./$(TEST_PROPERTIES)

clean:
	rm -rf $(BUILD_DIR)

help:
	@echo "Available targets:"
	@echo "  all              - Build all test programs"
	@echo "  test             - Run API tests"
	@echo "  test-examples    - Run unit tests"
	@echo "  test-properties  - Run property-based tests"
	@echo "  clean            - Remove build artifacts"
	@echo "  help             - Show this help message"
