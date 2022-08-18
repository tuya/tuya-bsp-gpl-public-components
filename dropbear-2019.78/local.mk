ifdef CONFIG_TUYA_PACKAGE_DROPBEAR

LOCAL_PATH := $(call my-dir)

include $(CLEAR_VARS)
LOCAL_PKG_NAME := dropbear
LOCAL_PKG_SRC := $(call qstrip,$(CONFIG_TUYA_PACKAGE_DROPBEAR_VERSION)).tar.bz2
LOCAL_PKG_OPTS = --disable-harden --disable-zlib --enable-bundled-libtom
LOCAL_PKG_PATCHES = 0001-tuya_comment_chmod_and_chown.patch

ifeq ($(CONFIG_TUYA_GATEWAY_SECURITY_BASELINE),y)
LOCAL_PKG_PATCHES += 0002-tuya_add_ssh_login_restriction.patch
endif

DROPBEAR_TARGETS_$(CONFIG_TUYA_PACKAGE_DROPBEAR) += dropbear
DROPBEAR_TARGETS_$(CONFIG_TUYA_PACKAGE_DROPBEAR_CLIENT) += dbclient

define compile/dropbear
	@$(MAKE) -C $(dropbear_build) SCPPROGRESS=1 PROGRAMS="$(DROPBEAR_TARGETS_y)"
endef

define install/dropbear
	@for i in $(DROPBEAR_TARGETS_y); do \
		install -m 755 $(dropbear_build)/$$i $(TARGET_ROOTFS)/bin/$$i; \
	done
endef

include $(BUILD_PACKAGE)

endif
