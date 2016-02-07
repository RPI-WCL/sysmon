INCLUDE_DIR = ./include
CC = gcc
CFLAGS = -DxDEBUG -I$(INCLUDE_DIR)

OBJ_DIR = ./obj
LIB_DIR = 
LIBS=

_DEPS = common.h  ext_stats.h  ioconf.h  print_flags.h  rd_stats.h  sysconfig.h  utils.h  version.h
DEPS = $(patsubst %,$(INCLUDE_DIR)/%,$(_DEPS))

_OBJ = common.o  ext_stats.o  rd_stats.o  sysmon.o  utils.o
OBJ = $(patsubst %,$(OBJ_DIR)/%,$(_OBJ))

$(shell [ -d "$(OBJ_DIR)" ] || mkdir -p $(OBJ_DIR))

$(OBJ_DIR)/%.o: %.c $(DEPS)
	$(CC) -c -o $@ $< $(CFLAGS)

sysmon: $(OBJ)
	gcc -o $@ $^ $(CFLAGS) $(LIBS)

.PHONY: clean

clean:
	rm -rf $(OBJ_DIR); rm -f *~ sysmon core $(INCLUDE_DIR)/*~ 
