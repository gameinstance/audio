COMMON_PATH = ../
LIB_PATH = ${COMMON_PATH}BUILD/lib/

########################################################################################################

TARGET   = audio.so

INCLUDES = -I./deps

LIBS     = -L${LIB_PATH} -lbasics -lstream

########################################################################################################


CXXFLAGS = -pedantic-errors -Wall -Wextra -Werror -Wno-attributes  \
            -Wpointer-arith -Wmissing-declarations -D_GNU_SOURCE   \
            -pthread -fPIC -g -std=c++23
LDFLAGS  = -L/usr/lib -lstdc++ -lm -L/usr/lib/x86_64-linux-gnu/    \
            ${LIBS}                                                \
            -shared -Wl,-soname,lib${TARGET}
BUILD    = ./BUILD
INCLUDE  = -I./include ${INCLUDES}
SRC      = ${wildcard src/*.cc}


TARGET_PATH = ${BUILD}/lib${TARGET}
OBJECTS = ${SRC:%.cc=${BUILD}/%.o}
DEPENDENCIES = ${OBJECTS:.o=.d}


all: build ${TARGET_PATH}


${BUILD}/%.o: %.cc
	@mkdir -p ${@D}
	${CXX} ${CXXFLAGS} ${INCLUDE} -c $< -MMD -o $@


${TARGET_PATH}: ${OBJECTS}
	@mkdir -p ${@D}
	${CXX} ${CXXFLAGS} -o ${TARGET_PATH} $^ ${LDFLAGS}


-include ${DEPENDENCIES}

.PHONY: all build clean install details


build:
	@mkdir -p ${BUILD}


install: ${TARGET_PATH}
	-install $< ${LIB_PATH}


clean:
	-@rm -rvf ${BUILD}/*


details:
	@echo "[*] Target so dir:   ${BUILD}       "
	@echo "[*] Object dir:      ${BUILD}       "
	@echo "[*] Shared lib dir:  ${LIB_PATH}    "
	@echo "[*] Sources:         ${SRC}         "
	@echo "[*] Objects:         ${OBJECTS}     "
	@echo "[*] Dependencies:    ${DEPENDENCIES}"
