-include ../Makefile.rules

CFLAGS = -Wall -g -fPIC

all: ipc install

install: ipc
	@echo "---------- Install libraries on ${INSTALL_DIR}"
	@$ cp lib*.so* ${INSTALL_DIR}
ifdef CROSS
	@echo "---------- Install libraries on ${TARGET}"
	-@$ sudo scp lib*.so* ${TARGET}
endif

ipc: ipc/avi.o ipc/mylog.o
	$(CC) -shared -o libipc.so ipc/avi.o ipc/mylog.o
	$(STRIP) libipc.so

clean:
	-$(RM) -f ipc/*.o
	-$(RM) -f ipc/*.a

