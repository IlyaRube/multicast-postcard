SKIP_CHECK_TARGETS := install_dependencies clean
EXPLICIT_GOALS := $(MAKECMDGOALS)
EXPLICIT_GOALS := $(if $(EXPLICIT_GOALS),$(EXPLICIT_GOALS),all)

ifneq ($(filter-out $(SKIP_CHECK_TARGETS),$(EXPLICIT_GOALS)),)

ifndef TEMPLATE
$(error Error: path to template must be specified with variable TEMPLATE)
endif

ifndef INJECTOR
$(error Error: path to injector must be specified with variable INJECTOR)
endif

ifeq ($(wildcard $(TEMPLATE)),)
$(error Error: template file does not exists)
endif

ifeq ($(wildcard $(INJECTOR)),)
$(error Error: injector file does not exists)
endif

endif

LIBS := $(shell pkg-config --static --libs libavfilter libswscale libavformat libavcodec libavutil)

SRC_DIR := $(CURDIR)/src
TEST_DIR := $(CURDIR)/test
OUTPUT_DIR ?= $(CURDIR)/bin

COMMON_SRC_DIR := $(PROJECT_ROOT)/common/src
COMMON_SOURCES := $(wildcard $(COMMON_SRC_DIR)/*.c)
EXTRA_INCLUDES := -I$(COMMON_SRC_DIR)


SOURCES := $(wildcard $(SRC_DIR)/*.c)
OUTPUT := $(OUTPUT_DIR)/create_postal
MAIN_FILE := $(SRC_DIR)/main.c

TEST_DEPENDENCIES := $(filter-out $(MAIN_FILE), $(SOURCES))
TEST_SOURCES := $(wildcard $(TEST_DIR)/*.c)
TEST_BIN := $(TEST_DIR)/bin
TEST_EXE := $(TEST_BIN)/test

FFMPEG_KEYRING := /etc/apt/keyrings/liquidsoap-ffmpeg.gpg
FFMPEG_LIST := /etc/apt/sources.list.d/liquidsoap-ffmpeg.list

.PHONY: all clean install_dependencies test

all: $(SOURCES)
	@mkdir -p $(OUTPUT_DIR)
	@$(CC) $(EXTRA_INCLUDES) $(CFLAGS) $(SOURCES) $(COMMON_SOURCES) $(LIBS) -o $(OUTPUT)
ifndef DEBUG
	@strip -s $(OUTPUT)
endif
	@$(INJECTOR) $(TEMPLATE) $(OUTPUT)

clean:
	@rm -rf $(OUTPUT_DIR)/*

install_dependencies:
	@echo "Detecting package manager..."
	@if [ -x "$$(command -v apt-get)" ]; then \
		sudo mkdir -p /etc/apt/keyrings; \
		echo "Adding ffmpeg static libraries APT repo"; \
		curl -fsSL https://liquidsoap.info/ffmpeg-static-build/key.asc | sudo gpg --yes --dearmor -o $(FFMPEG_KEYRING); \
        	echo "deb [signed-by=$(FFMPEG_KEYRING)] https://liquidsoap.info/ffmpeg-static-build stable main" | sudo tee $(FFMPEG_LIST) > /dev/null; \
		echo "Updating APT repos and installing packages" \
        	sudo apt-get update; \
        	sudo apt-get install ffmpeg-liquidsoap pkgconf; \
	else \
	  	echo "Unknown package manager. Install ffmpeg and pkg-config manually (static libraries are required)"; \
	fi

test:
	@mkdir -p $(TEST_BIN)
	@$(CC) $(EXTRA_INCLUDES) $(CFLAGS) $(TEST_DEPENDENCIES) $(TEST_SOURCES) $(COMMON_SOURCES) $(LIBS) -o $(TEST_EXE)
	@$(TEST_EXE)
	@rm -rf $(TEST_BIN)
