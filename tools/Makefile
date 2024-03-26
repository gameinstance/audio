COMMON_PATH = ../../
LIB_PATH = ${COMMON_PATH}BUILD/lib/

########################################################################################################

TARGET   = flac-decoder

SOURCES  = main.cc

INCLUDES = -I../include -I../deps

LIBS     = -L${LIB_PATH} -lbasics -laudio

########################################################################################################


CXXFLAGS = -pedantic-errors -Wall -Wextra -Werror -Wno-attributes \
            -Wpointer-arith -Wmissing-declarations -D_GNU_SOURCE   \
            -pthread -std=c++23
LDFLAGS  = -L/usr/lib -lstdc++ -lm -L/usr/lib/x86_64-linux-gnu/ ${LIBS}
BUILD    = ./BUILD
OBJ_DIR  = ${BUILD}
APP_DIR  = ${BUILD}
INCLUDE  = -I./ ${INCLUDES}
SRC      = ${SOURCES}


OBJECTS  = $(SRC:%.cc=$(OBJ_DIR)/%.o)
DEPENDENCIES := $(OBJECTS:.o=.d)

all: build $(APP_DIR)/$(TARGET)

$(OBJ_DIR)/%.o: %.cc
	@mkdir -p $(@D)
	$(CXX) $(CXXFLAGS) $(INCLUDE) -c $< -MMD -o $@

$(APP_DIR)/$(TARGET): $(OBJECTS)
	@mkdir -p $(@D)
	$(CXX) $(CXXFLAGS) -o $(APP_DIR)/$(TARGET) $^ $(LDFLAGS)

-include $(DEPENDENCIES)

.PHONY: all build clean debug release info

build:
	@mkdir -p $(APP_DIR)
	@mkdir -p $(OBJ_DIR)

debug: CXXFLAGS += -DDEBUG -g
debug: all

release: CXXFLAGS += -O2
release: all

run: ${APP_DIR}/${TARGET}
	LD_LIBRARY_PATH=${LIB_PATH} ${APP_DIR}/${TARGET}

clean:
	-@rm -rvf $(OBJ_DIR)/*
	-@rm -rvf $(APP_DIR)/*

info:
	@echo "[*] Application dir: ${APP_DIR}     "
	@echo "[*] Object dir:      ${OBJ_DIR}     "
	@echo "[*] Shared lib dir:  ${LIB_PATH}    "
	@echo "[*] Sources:         ${SRC}         "
	@echo "[*] Objects:         ${OBJECTS}     "
	@echo "[*] Dependencies:    ${DEPENDENCIES}"
