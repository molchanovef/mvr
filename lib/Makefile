TOP=../
-include ${TOP}/Makefile.rules

CFLAGS = -Wall -g -fPIC -I${INCLUDE_DIR}

all: ipc

ipc: ipc/avi.o ipc/mylog.o ipc/btr.o
	$(CC) -shared -o libipc.so ipc/avi.o ipc/mylog.o ipc/btr.o
	$(STRIP) libipc.so

install: ipc
	@echo "---------- Install libraries"
	@$ cp lib*.so* ${INSTALL_DIR}

scp: ipc
ifdef CROSS
	@echo "---------- Install libraries on ${TARGET}"
	-@$ sudo scp lib*.so* ${TARGET}
endif

clean:
	-$(RM) -f ipc/*.o
	-$(RM) -f ipc/*.a
	-$(RM) -f *.so

