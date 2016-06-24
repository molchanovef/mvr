TOP=../
-include ${TOP}/Makefile.rules

CFLAGS = -Wall -g -fPIC -I${INCLUDE_DIR}

all: ipc

ipc: ipc/avi.o ipc/mylog.o
	$(CC) -shared -o libipc.so ipc/avi.o ipc/mylog.o
	$(STRIP) libipc.so

install:
	@echo "---------- Install libraries"
	@$ cp lib*.so* ${INSTALL_DIR}
ifdef CROSS
	@echo "---------- Install libraries on ${TARGET}"
	-@$ sudo scp lib*.so* ${TARGET_LIB}
endif

clean:
	-$(RM) -f ipc/*.o
	-$(RM) -f ipc/*.a

