
include ../version.mk
include ../config.mk

srcdir = .
prefix = /home/faygarcia/Desktop/os_folder
exec_prefix =   ${prefix}

COBJS = libuser.o
CFLAGS += -Wall -g -I. -I../src -I$(INC_DIR) -Wno-pointer-to-int-cast -Wno-int-to-pointer-cast
CFLAGS += -DPHASE_3
TARGET = libuser$(VERSION).a

ifeq ($(shell uname),Darwin)
	CFLAGS += -D_XOPEN_SOURCE
endif

all: $(TARGET)

$(TARGET):	$(COBJS)
		$(AR) -r $@ $(COBJS) 

install:	$(TARGET)
		mkdir -p $(INC_DIR) $(LIB_DIR)
		$(INSTALL_DATA) libuser.h $(INC_DIR)
		$(INSTALL_DATA) $(TARGET) $(LIB_DIR)

libuser.o:	libuser.h libuser.c

clean:
	rm -f $(COBJS) $(TARGET) *.a 

distclean: clean
	rm -rf Makefile config.h config.log config.status config.mk autom4te.cache
