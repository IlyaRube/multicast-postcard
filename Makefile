# Определяет флаги на основе выбранного режима сборки (определена ли переменная DEBUG), если они не переданы явно
ifndef CFLAGS
CFLAGS := -static-libgcc -Wall -Wpedantic -Wextra -Werror -std=gnu23

ifdef DEBUG
CFLAGS += -O0 -g
else
CFLAGS += -O2 -flto
endif

endif

PROJECT_ROOT := $(CURDIR)

OUTPUT_DIR ?= $(CURDIR)/bin
OUTPUT_TMP := $(OUTPUT_DIR)/tmp

SOURCES := $(wildcard $(SRC_DIR)/*.c)
OUTPUT := $(OUTPUT_DIR)/create_postal

.PHONY: all injector template create_postal clean prepare install_dependencies

INJECTOR_SRC_DIR := $(CURDIR)/utils/injector
TEMPLATE_SRC_DIR := $(CURDIR)/postal_template
CREATE_POSTAL_SRC_DIR := $(CURDIR)/create_postal

INJECTOR_DIR := $(OUTPUT_TMP)
TEMPLATE_DIR := $(OUTPUT_TMP)

INJECTOR_ORIGIN := $(origin INJECTOR)
TEMPLATE_ORIGIN := $(origin TEMPLATE)

# Определяет директории, в которые будут собраны зависимости (injector и template собираются в tmp и очищаются, если явно не указаны соответствующие цели)
ifneq ($(filter injector,$(MAKECMDGOALS)),)
	INJECTOR_DIR := $(OUTPUT_DIR)
endif
ifneq ($(filter template,$(MAKECMDGOALS)),)
	TEMPLATE_DIR := $(OUTPUT_DIR)
endif

# Определяет зависимости цели create_postal (если указан INJECTOR, то цель injector игнорируется, аналогично для TEMPLATE)
CREATE_POSTAL_DEPS := prepare
ifeq ($(INJECTOR_ORIGIN),undefined)
	CREATE_POSTAL_DEPS += injector
	INJECTOR := $(INJECTOR_DIR)/injector
endif
ifeq ($(TEMPLATE_ORIGIN),undefined)
	CREATE_POSTAL_DEPS += template
	TEMPLATE := $(TEMPLATE_DIR)/postal_template
endif

export CFLAGS
export PROJECT_ROOT

# Цель по умолчанию
all: injector template create_postal

# Сборка injector. Игнорируется если INJECTOR явно передан
injector: prepare
ifeq ($(INJECTOR_ORIGIN),undefined)
	@echo "Building injector"
	@$(MAKE) -C $(INJECTOR_SRC_DIR) OUTPUT_DIR=$(INJECTOR_DIR)
else
	@echo "INJECTOR variable specified, build of injector will be skipped"
endif

# Сборка template. Игнорируется если TEMPLATE явно передан
template: prepare
ifeq ($(TEMPLATE_ORIGIN),undefined)
	@echo "Building template"
	@$(MAKE) -C $(TEMPLATE_SRC_DIR) OUTPUT_DIR=$(TEMPLATE_DIR)
else
	@echo "TEMPLATE variable specified, build of template will be skipped"
endif

# Сборка create_postal
create_postal: $(CREATE_POSTAL_DEPS)
	@echo "Building create_postal"
	@$(MAKE) -C $(CREATE_POSTAL_SRC_DIR) OUTPUT_DIR=$(OUTPUT_DIR) INJECTOR=$(INJECTOR) TEMPLATE=$(TEMPLATE)
	@rm -rf $(OUTPUT_TMP)

# Подготовка выходной директории
prepare:
	@echo "Creating output directory"
	@mkdir -p $(OUTPUT_TMP)

# Очистка выходной директории
clean:
	@echo "Cleaning output directory"
	@rm -rf $(OUTPUT_DIR)

# Установка всех нужных зависимостей
install_dependencies:
	@echo "Installing dependencies"
	@$(MAKE) -C $(CREATE_POSTAL_SRC_DIR) install_dependencies

# Запуск тестов
test: $(CREATE_POSTAL_DEPS)
	@echo "Testing postal_template"
	@$(MAKE) -C $(TEMPLATE_SRC_DIR) test
	@echo "Testing create_postal"
	@$(MAKE) -C $(CREATE_POSTAL_SRC_DIR) INJECTOR=$(INJECTOR) TEMPLATE=$(TEMPLATE) test
