#
# tuya security device side Makefile
#
TUYA_VERIFY_OBJ = ecdsa.o sha256.o tuya_verify.o
TUYA_VERIFY_LIBNAME = libtuya_verify
TUYA_VERIFY_TEST = tuya_verify_test
TUYA_VERIFY_TEST_SRC = tuya_verify_test.c

LDLIBS =

LIBVERSION = 1.0.0

CC ?= gcc

PREFIX ?= /usr/local
INCLUDE_PATH ?= include
LIBRARY_PATH ?= lib
INSTALL_INCLUDE_PATH = $(DESTDIR)$(PREFIX)/$(INCLUDE_PATH)
INSTALL_LIBRARY_PATH = $(DESTDIR)$(PREFIX)/$(LIBRARY_PATH)
INSTALL ?= cp -a

CFLAGS += -fPIC -I.
CFLAGS += ${SHARED_CFLAGS}

#library file extensions
SHARED = so
STATIC = a

#library names
TUYA_VERIFY_SHARED = $(TUYA_VERIFY_LIBNAME).$(SHARED)
TUYA_VERIFY_SHARED_VERSION = $(TUYA_VERIFY_LIBNAME).$(SHARED).$(LIBVERSION)
TUYA_VERIFY_STATIC = $(TUYA_VERIFY_LIBNAME).$(STATIC)

.PHONY: all shared static tests clean install

all: shared static tests

shared: $(TUYA_VERIFY_SHARED)

static: $(TUYA_VERIFY_STATIC)

tests: $(TUYA_VERIFY_TEST)

.c.o:
	$(CC) -c $(CFLAGS) $<

#tests
$(TUYA_VERIFY_TEST): $(TUYA_VERIFY_TEST_SRC)
	$(CC) $(CFLAGS) $(TUYA_VERIFY_TEST_SRC) $(TUYA_VERIFY_OBJ) -o $@ $(LDLIBS) -static

#static libraries
$(TUYA_VERIFY_STATIC): $(TUYA_VERIFY_OBJ)
	$(AR) rcs $@ $<

#shared libraries .so.1.0.0
$(TUYA_VERIFY_SHARED_VERSION): $(TUYA_VERIFY_OBJ)
	$(CC) -shared -o $@ $< $(LDFLAGS)

#objects
$(TUYA_VERIFY_OBJ): tuya_verify.c

#links .so -> .so.1.0.0
$(TUYA_VERIFY_SHARED): $(TUYA_VERIFY_SHARED_VERSION)
	ln -s $(TUYA_VERIFY_SHARED_VERSION) $(TUYA_VERIFY_SHARED)

#install
install-tuya_verify:
	mkdir -p $(INSTALL_LIBRARY_PATH) $(INSTALL_INCLUDE_PATH)
	$(INSTALL) tuya_verify.h $(INSTALL_INCLUDE_PATH)
	$(INSTALL) $(TUYA_VERIFY_SHARED) $(TUYA_VERIFY_SHARED_VERSION) $(INSTALL_LIBRARY_PATH)

install: install-tuya_verify

#uninstall
uninstall-tuya_verify:
	$(RM) $(INSTALL_LIBRARY_PATH)/$(TUYA_VERIFY_SHARED)
	$(RM) $(INSTALL_LIBRARY_PATH)/$(TUYA_VERIFY_SHARED_VERSION)
	$(RM) $(INSTALL_INCLUDE_PATH)/tuya_verify.h

remove-dir:
	$(if $(wildcard $(INSTALL_LIBRARY_PATH)/*.*),,rmdir $(INSTALL_LIBRARY_PATH))
	$(if $(wildcard $(INSTALL_INCLUDE_PATH)/*.*),,rmdir $(INSTALL_INCLUDE_PATH))

uninstall: uninstall-tuya_verify remove-dir

clean:
	@$(RM) *.o
	@$(RM) $(TUYA_VERIFY_SHARED) $(TUYA_VERIFY_SHARED_VERSION)
	@$(RM) $(TUYA_VERIFY_STATIC) $(TUYA_VERIFY_TEST)
