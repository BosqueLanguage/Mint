MAKE_PATH=$(realpath $(dir $(lastword $(MAKEFILE_LIST))))
BUILD_DIR=$(MAKE_PATH)/
BIN_DIR=$(MAKE_PATH)/../bin/
SRC_DIR=$(MAKE_PATH)/../src/

OUT_EXE=$(BUILD_DIR)output/
OUT_OBJ=$(BUILD_DIR)output/obj/

APPLICATION_DIR=$(SRC_DIR)application/
COMMON_DIR=$(SRC_DIR)
SERVER_DIR=$(SRC_DIR)server/

RSHOOK_TEST_DIR=$(MAKE_PATH)/../test/

#dev is default, for another flavor : make BUILD=release or debug
BUILD := debug

CPP=g++ 
CPP_STDFLAGS=-Wall -Wextra -Wno-unused-parameter -Wuninitialized -Werror -std=gnu++20

CPPFLAGS_OPT.debug=-O0 -g -ggdb -fno-omit-frame-pointer -fsanitize=address
CPPFLAGS_OPT.dev=-O0 -g -ggdb -fno-omit-frame-pointer
CPPFLAGS_OPT.release=-O3 -march=x86-64-v3
CPPFLAGS=${CPPFLAGS_OPT.${BUILD}} ${CPP_STDFLAGS}
CPPFLAGS_TEST=${CPPFLAGS_OPT.dev} ${CPP_STDFLAGS}

APPLICATION_FLAGS=-DDEFAULT_ROUTES
APPLICATION_HEADERS=$(APPLICATION_DIR)routes.h
APPLICATION_SOURCES=$(APPLICATION_DIR)routes.cpp
APPLICATION_OBJS=$(OUT_OBJ)routes.o

COMMON_HEADERS=$(COMMON_DIR)common.h
COMMON_SOURCES=$(COMMON_DIR)common.cpp
COMMON_OBJS=$(OUT_OBJ)common.o

SERVER_HEADERS=$(SERVER_DIR)server.h $(SERVER_DIR)alloc.h
SERVER_SOURCES=$(SERVER_DIR)server.cpp $(SERVER_DIR)alloc.cpp
SERVER_OBJS=$(OUT_OBJ)server.o $(OUT_OBJ)alloc.o

JSON_INCLUDES=-I $(BUILD_DIR)include/json/

# MAKEFLAGS += -j4

all: $(OUT_EXE)rshook

$(OUT_EXE)rshook: $(APPLICATION_OBJS) $(COMMON_OBJS) $(SERVER_OBJS) $(APPLICATION_HEADERS) $(COMMON_HEADERS) $(SERVER_HEADERS) $(SRC_DIR)rshook.cpp
	@mkdir -p $(OUT_EXE)
	$(CPP) $(CPPFLAGS) $(JSON_INCLUDES) -o $(OUT_EXE)rshook $(APPLICATION_OBJS) $(COMMON_OBJS) $(SERVER_OBJS) $(SRC_DIR)rshook.cpp -luring

$(OUT_OBJ)server.o: $(APPLICATION_HEADERS) $(COMMON_HEADERS) $(SERVER_HEADERS) $(SERVER_SOURCES)
	@mkdir -p $(OUT_OBJ)
	$(CPP) $(CPPFLAGS) -o $(OUT_OBJ)server.o -c $(SERVER_DIR)server.cpp

$(OUT_OBJ)alloc.o: $(COMMON_HEADERS) $(SERVER_HEADERS) $(SERVER_SOURCES)
	@mkdir -p $(OUT_OBJ)
	$(CPP) $(CPPFLAGS) -o $(OUT_OBJ)alloc.o -c $(SERVER_DIR)alloc.cpp

$(OUT_OBJ)common.o: $(COMMON_HEADERS) $(COMMON_SOURCES)
	@mkdir -p $(OUT_OBJ)
	$(CPP) $(CPPFLAGS) -o $(OUT_OBJ)common.o -c $(COMMON_DIR)common.cpp

$(OUT_OBJ)routes.o: $(APPLICATION_HEADERS) $(APPLICATION_SOURCES)
	@mkdir -p $(OUT_OBJ)
	$(CPP) $(CPPFLAGS) -o $(OUT_OBJ)routes.o -c $(APPLICATION_DIR)routes.cpp

clean:
	rm -rf $(OUT_EXE)* $(OUT_OBJ)*.o $(BIN_DIR)*
