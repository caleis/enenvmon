# fifo2mqttpub makefile
#
# makefile verion: v0.1 (from version 0.1.00 of fifo2mqttpub)
# Template: shared library make
# Invoke with make --debug=v in case of make troubleshooting needed
#
# tool macros
CC := clang # C compiler
CFLAGS := -g -Wall -Wextra -Weverything -Wno-disabled-macro-expansion -Wno-invalid-offsetof -O2 # Flags for C compiler
CSLIBFLAGS := $(CFLAGS) -fPIC # Flags for C compiler for shared library
LDFLAGS = -g -Wall -Wextra -Weverything -Wno-disabled-macro-expansion -Wno-extended-offsetof-shared  # Linking flags
LDSLIBFLAGS = $(LDFLAGS) -shared # Linking flags for shared library
DBGFLAGS := -g
COBJFLAGS := $(CFLAGS) -c
CSLIBOBJFLAGS := $(CSLIBFLAGS) -c

# path macros
BIN_PATH := .
OBJ_PATH := .
SRC_PATH := .
DBG_PATH := .
INST_PREFIX	:= /usr/local


# Targets (executables, shared libraries)
# compile macros
TARGET_NAME := fifo2mqttpub

TARGET := $(addprefix $(BIN_PATH)/, $(notdir $(TARGET_NAME)))
TARGET_DEBUG := $(addprefix $(DBG_PATH)/, $(wildcard $(notdir $(TARGET_NAME))))

# src files & obj files
SRC := $(foreach x, $(SRC_PATH), $(wildcard $(addprefix $(x)/*,.c*)))
OBJ := $(addprefix $(OBJ_PATH)/, $(addsuffix .o, $(notdir $(basename $(SRC)))))
OBJ_DEBUG := $(addprefix $(DBG_PATH)/, $(addsuffix .o, $(notdir $(basename $(SRC)))))

# clean files list
DISTCLEAN_LIST := $(OBJ) $(OBJ_DEBUG)
CLEAN_LIST := $(TARGET) $(TARGET_DEBUG) $(SRC:.c=.d) $(DISTCLEAN_LIST)

# default rule
default: listvars makedir all

# all make rule
all: $(TARGET)

# recipe for creating dependency files
$(SRC:.c=.d):%.d:%.c
	{ echo -n "$(OBJ_PATH)/" & $(CC) $(CFLAGS) -MM $< ; } >$@

include $(SRC:.c=.d)

# Shared library binaries 

# Executable binaries
$(BIN_PATH)/fifo2mqttpub: $(SRC_PATH)/fifo2mqttpub.c
	$(CC) $(CFLAGS) -lzrpicommon -lpaho-mqtt3as -o $@ $<
	
# Object files for shared libraries

# Object files for executables (not needed, compiled in one go)

# Other recipes

# Note the use of the double dollar sign to pass a single dollar sign to the shell.
.PHONY: install
install:
	test -d $(INST_PREFIX) || mkdir $(INST_PREFIX)
	test -d $(INST_PREFIX)/lib || mkdir $(INST_PREFIX)/lib
	for libfile in $(BIN_PATH)/*.so; do install -m 0755 -T $(BIN_PATH)/$$(basename $$libfile) $(INST_PREFIX)/lib/lib$$(basename $$libfile); done
	test -d $(INST_PREFIX)/include || mkdir $(INST_PREFIX)/include
	for hdrfile in $(SRC_PATH)/*.h; do install -m 0644 $(SRC_PATH)/$$(basename $$hdrfile) $(INST_PREFIX)/include; done
	ldconfig
	ldconfig -p | grep zrpi
	test -d $(INST_PREFIX)/bin || mkdir $(INST_PREFIX)/bin
	for binfile in $$(find $(BIN_PATH) -type f ! -name "*.*"); do install -m 0755 $(BIN_PATH)/$$(basename $$binfile) $(INST_PREFIX)/bin; done

.PHONY: listvars
listvars:
	@echo ------------------------- Makefile parameters ----------------------------
	@echo Sources: $(SRC)
	@echo Objects: $(OBJ)
	@echo Target: $(TARGET)
	@echo Dependency files: $(SRC:.c=.d)
	@echo Clean list: $(CLEAN_LIST)
	@echo Dist clean list: $(DISTCLEAN_LIST)
	@echo Install target: $(INST_PREFIX)
	@echo --------------------------------------------------------------------------

.PHONY: makedir
makedir:
	@mkdir -p $(BIN_PATH) $(OBJ_PATH) $(DBG_PATH)

.PHONY: clean
clean:
	@echo CLEAN $(CLEAN_LIST)
	@rm -f $(CLEAN_LIST)

.PHONY: distclean
distclean:
	@echo CLEAN $(DISTCLEAN_LIST)
	@rm -f $(DISTCLEAN_LIST)
