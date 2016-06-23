ROOTDIR = ../..
include $(ROOTDIR)/Rules.make

INCLUDES += -I$(APP_INC_DIR)
IXML_OBJS = ixml/attr.o ixml/document.o ixml/element.o ixml/ixml.o ixml/ixmlmembuf.o ixml/ixmlparser.o ixml/namedNodeMap.o ixml/node.o ixml/nodeList.o

C_FLAGS += -Wall -g -fPIC
AR_FLAGS += -rcs


CC = $(CROSS)gcc $(INCLUDES) $(C_FLAGS)
AR = $(CROSS)ar

all: ipc zlib ssl upnp smtp readline install

install: ipc
	cp -r esmtp-plugins $(APP_OUT_DIR)/lib
	cp -d lib*.so* $(APP_OUT_DIR)/lib

ixml: $(IXML_OBJS)
	$(CC) -shared -o libixml.so $(IXML_OBJS)
	$(CROSS)strip libixml.so

ipc: ipc/mylog.o ipc/avi.o ipc/msg.o ipc/cfg.o ipc/helper.o
	$(CROSS)gcc -shared -o libipc.so ipc/mylog.o ipc/avi.o ipc/msg.o ipc/cfg.o ipc/helper.o
	$(CROSS)strip libipc.so

zlib:
	make -C zlib-1.2.8
	cp -d zlib-1.2.8/lib*.so* .
	$(CROSS)strip libz.so.1.2.8

ssl:
	make -C openssl-1.0.1s
	$(CROSS)ar x openssl-1.0.1s/libcrypto.a
	$(CROSS)gcc -shared -o libcrypto.so.1.0.1 *.o
	rm *.o
	$(CROSS)strip libcrypto.so.1.0.1
	rm libcrypto.so.1; ln -s libcrypto.so.1.0.1 libcrypto.so.1
	rm libcrypto.so; ln -s libcrypto.so.1.0.1 libcrypto.so
	$(CROSS)ar x openssl-1.0.1s/libssl.a
	$(CROSS)gcc -shared -o libssl.so.1.0.1 *.o
	rm *.o
	$(CROSS)strip libssl.so.1.0.1
	rm libssl.so.1; ln -s libssl.so.1.0.1 libssl.so.1
	rm libssl.so; ln -s libssl.so.1.0.1 libssl.so
	
upnp:
	make -C libupnp-1.6.19
	cp -d libupnp-1.6.19/ixml/.libs/lib*.so* .
	$(CROSS)strip libixml.so.2.0.8
	cp -d libupnp-1.6.19/threadutil/.libs/lib*.so* .
	$(CROSS)strip libthreadutil.so.6.0.4
	cp -d libupnp-1.6.19/upnp/.libs/lib*.so* .
	$(CROSS)strip libupnp.so.6.3.3
	
smtp:
	make -C libesmtp-1.0.6
	cp -d libesmtp-1.0.6/.libs/lib*.so* .
	cp libesmtp-1.0.6/login/.libs/*.so ./esmtp-plugins
	cp libesmtp-1.0.6/plain/.libs/*.so ./esmtp-plugins
	cp libesmtp-1.0.6/crammd5/.libs/*.so ./esmtp-plugins
	cp libesmtp-1.0.6/ntlm/.libs/*.so ./esmtp-plugins
	$(CROSS)strip libesmtp.so.6.1.6
	$(CROSS)strip ./esmtp-plugins/*.so
	
readline:
	make -C readline-6.3
	cp readline-6.3/shlib/libreadline.so.6.3 .
	$(CROSS)strip libreadline.so.6.3
	ln -s libreadline.so.6.3 libreadline.so
	

clean:
	-$(RM) -f ipc/*.o
	-$(RM) -f ipc/*.a
	-$(RM) -f ixml/*.o
	-$(RM) -f ixml/*.a

