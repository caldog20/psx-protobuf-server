# Include the nanopb provided Makefile rules
include ./nanopb.mk

# Compiler flags to enable all warnings & debug info
CFLAGS = -ansi -Werror  -g -O0
CFLAGS += -I$(NANOPB_DIR)

all: server

.SUFFIXES:

clean:
	rm -f server client siopayload.pb.c siopayload.pb.h

%: %.c common.c siopayload.pb.c
	$(CC) $(CFLAGS) -o $@ $^ $(NANOPB_CORE)
