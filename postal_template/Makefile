LIBS :=

SRC_DIR := $(CURDIR)/src
TEST_DIR := $(CURDIR)/test
OUTPUT_DIR ?= $(CURDIR)/bin

SOURCES := $(wildcard $(SRC_DIR)/*.c)
OUTPUT := $(OUTPUT_DIR)/postal_template
MAIN_FILE := $(SRC_DIR)/main.c

TEST_DEPENDENCIES := $(filter-out $(MAIN_FILE), $(SOURCES))
TEST_SOURCES := $(wildcard $(TEST_DIR)/*.c)
TEST_BIN := $(TEST_DIR)/bin
TEST_EXE := $(TEST_BIN)/test

COMMON_SRC_DIR := $(PROJECT_ROOT)/common/src
COMMON_SOURCES := $(wildcard $(COMMON_SRC_DIR)/*.c)
EXTRA_INCLUDES := -I$(COMMON_SRC_DIR)

.PHONY: all clean test

all: $(SOURCES)
	@mkdir -p $(OUTPUT_DIR)
	@$(CC) $(EXTRA_INCLUDES) $(CFLAGS) $(SOURCES) $(COMMON_SOURCES) $(LIBS) -o $(OUTPUT)
ifndef DEBUG
	@strip -s $(OUTPUT)
endif

test:
	@mkdir -p $(TEST_BIN)
	@$(CC) $(EXTRA_INCLUDES) $(CFLAGS) $(TEST_DEPENDENCIES) $(TEST_SOURCES) $(COMMON_SOURCES) $(LIBS) -o $(TEST_EXE)
	@$(TEST_EXE)
	@rm -rf $(TEST_BIN)

clean:
	@rm -rf $(OUTPUT_DIR)/*