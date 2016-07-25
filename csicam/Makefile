TOP=../
-include ${TOP}/Makefile.rules

# -print-search-dirs
SOURCES=$(wildcard *.c)
OBJECTS=$(SOURCES:.c=.o)
EXECUTABLE=csicam
SCRIPTS=
#USE_BTRLIB=1
ifdef CROSS
SYSROOTFS=/home/mef/iMX6/fsl-release-bsp/build-dfb/tmp/sysroots/imx6qsabresd
CFLAGS += -DCROSS
CFLAGS += -I${SYSROOTFS}/usr/include/gstreamer-0.10 \
		 -I${SYSROOTFS}/usr/include/glib-2.0 \
		 -I${SYSROOTFS}/usr/lib/glib-2.0/include \
		 -I${SYSROOTFS}/usr/include/libxml2 \
		 -I${INCLUDE_DIR}
LDFLAGS = -lgstreamer-0.10 -lgobject-2.0 -lgmodule-2.0 -lgthread-2.0 -lrt -lxml2 -lglib-2.0
ifdef USE_BTRLIB
CFLAGS += -DBTRLIB
LDFLAGS += -lpthread -L${LIB_DIR} -lipc
endif
else
#run pkg-config --cflags gstreamer-0.10 to get CFLAGS
CFLAGS +=	-I/usr/include/gstreamer-0.10 \
			-I/usr/include/glib-2.0 \
			-I/usr/lib/x86_64-linux-gnu/glib-2.0/include \
			-I/usr/include/libxml2 \
			-I${INCLUDE_DIR}
#run pkg-config --libs gstreamer-0.10 to get LDFLAGS
LDFLAGS = -lgstreamer-0.10 -lgobject-2.0 -lgmodule-2.0 -lgthread-2.0 -lrt -lxml2 -lglib-2.0
ifdef USE_BTRLIB
CFLAGS += -DBTRLIB
LDFLAGS += -L${LIB_DIR} -lipc
endif
endif

.PHONY : all install clean

all: $(EXECUTABLE)
	
$(EXECUTABLE): $(OBJECTS) 
	@echo "---------- Build ${EXECUTABLE} application"
ifdef CROSS
	-@$(CC) $(OBJECTS) -o $@ $(LDFLAGS)
else
	libtool --mode=link ${CC} -Wall `pkg-config --cflags --libs gstreamer-0.10` ${OBJECTS} -o $@
endif
	@echo "---------- Install ${EXECUTABLE} application on ${INSTALL_DIR}"
	@$ mv ${EXECUTABLE} ${INSTALL_DIR}
#	@$ cp ${SCRIPTS} ${INSTALL_DIR}

.c.o:
	@echo "---------- Build $<"
	-@$(CC) $(CFLAGS) $< -o $@

scp: $(EXECUTABLE)
ifdef CROSS
	@echo "---------- Install ${EXECUTABLE} application on ${TARGET}"
	-@$ sudo scp ${INSTALL_DIR}/${EXECUTABLE} ${TARGET}
#	-@$ sudo scp ${INSTALL_DIR}/${SCRIPTS} ${TARGET}
endif

clean:
	@echo "---------- Remove ${EXECUTABLE} application"
	@$ if [ -e ${EXECUTABLE} ]; then rm ${EXECUTABLE}; fi;
	@echo "---------- Remove object files"
	@$ rm -f *.o

