-include Makefile.rules

TOP=$(shell pwd)

#SCRIPTS=$(wildcard scripts/*.sh)
SCRIPTS = scripts/video.sh

.PHONY : all pre clean build lib rtsp mosaic record upload mvr install scripts

all: clean pre lib rtsp mosaic record upload mvr install compress

build: pre lib rtsp mosaic record upload mvr

pre:
	@mkdir -p ${TOP}/${BINARES}
	@mkdir -p ${TOP}/${BINARES}/${USER}
	@mkdir -p ${TOP}/${BINARES}/${USER}/${ARCH}

scp:
#	@${MAKE} -C gst $@
	@${MAKE} -C lib $@
	@${MAKE} -C rtsp $@
	@${MAKE} -C mosaic $@
	@${MAKE} -C record $@
	@${MAKE} -C upload $@
	@${MAKE} -C mvr $@
#	@$ sudo scp ${SCRIPTS} ${TARGET}

install:
	@${MAKE} -C lib $@
	@${MAKE} -C rtsp $@
	@${MAKE} -C mosaic $@
	@${MAKE} -C record $@
	@${MAKE} -C upload $@
	@${MAKE} -C mvr $@

compress: install
	rm -rf mvr.tar
	tar -cvf mvr.tar -C ${TOP}/${BINARES}/${USER}/${ARCH}/ .
	@$ sudo scp mvr.tar ${TARGET}
	@$ sudo scp etc/untar.sh ${TARGET}

clean:
	rm -rf mvr.tar
	@rm -f ${TOP}/${BINARES}/${USER}/${ARCH}/*
	@${MAKE} -C gst $@
	@${MAKE} -C lib $@
	@${MAKE} -C rtsp $@
	@${MAKE} -C mosaic $@
	@${MAKE} -C record $@
	@${MAKE} -C upload $@
	@${MAKE} -C mvr $@
	
gst:
	@${MAKE} -C $@

lib:
	@${MAKE} -C $@
	
rtsp:
	@${MAKE} -C $@

mosaic:
	@${MAKE} -C $@

upload:
	@${MAKE} -C $@

record:
	@${MAKE} -C $@

mvr:
	@${MAKE} -C $@

scripts:
	@$ sudo scp ${SCRIPTS} ${TARGET}

