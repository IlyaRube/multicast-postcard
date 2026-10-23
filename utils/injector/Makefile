LIBS :=

SRC_DIR := $(CURDIR)/src
OUTPUT_DIR ?= $(CURDIR)/bin

SOURCES := $(wildcard $(SRC_DIR)/*.c)
OUTPUT := $(OUTPUT_DIR)/injector

COMMON_SRC_DIR := $(PROJECT_ROOT)/common/src
COMMON_SOURCES := $(wildcard $(COMMON_SRC_DIR)/*.c)
EXTRA_INCLUDES := -I$(COMMON_SRC_DIR)

.PHONY: all clean

all: $(SOURCES)
	@mkdir -p $(OUTPUT_DIR)
	@$(CC) $(EXTRA_INCLUDES) $(CFLAGS) $(SOURCES) $(COMMON_SOURCES) $(LIBS) -o $(OUTPUT)

clean:
	@rm -rf $(OUTPUT_DIR)/*