-include ../Makefile.rules

# -print-search-dirs
SOURCES=$(wildcard *.c)
OBJECTS=$(SOURCES:.c=.o)
EXECUTABLE=xmlparse
SCRIPTS=xml.xml

ifdef CROSS
SYSROOTFS=/home/mef/iMX6/fsl-release-bsp/build-dfb/tmp/sysroots/imx6qsabresd
CFLAGS += -DCROSS
CFLAGS += -I${SYSROOTFS}/usr/include/libxml2
LDFLAGS = -lxml2
else
#run pkg-config --cflags gstreamer-0.10 to get CFLAGS
CFLAGS += -I/usr/include/libxml2
#run pkg-config --libs gstreamer-0.10 to get LDFLAGS
LDFLAGS = -lxml2
endif

.PHONY : all build install clean install

all: $(SOURCES) $(EXECUTABLE) install
	
$(EXECUTABLE): $(OBJECTS) 
	@echo "---------- Build ${EXECUTABLE} application"
	$(CC) $(LDFLAGS) $(OBJECTS) -o $@

.c.o:
	@echo "---------- Build objects: ${OBJECTS}"
	$(CC) $(CFLAGS) $< -o $@

install:
	@echo "---------- Install ${EXECUTABLE} application on ${INSTALL_DIR}"
	@$ cp ${EXECUTABLE} ${INSTALL_DIR}
	-@$ cp ${SCRIPTS} ${INSTALL_DIR}
ifdef CROSS
	@echo "---------- Install ${EXECUTABLE} application on ${TARGET}"
	-@$ sudo scp ${EXECUTABLE} ${TARGET}
	-@$ sudo scp ${SCRIPTS} ${TARGET}
endif

clean:
	@echo "---------- Remove ${EXECUTABLE} application"
	@$ if [ -e ${EXECUTABLE} ]; then rm ${EXECUTABLE}; fi;
	@echo "---------- Remove object files"
	@$ rm -f *.o

