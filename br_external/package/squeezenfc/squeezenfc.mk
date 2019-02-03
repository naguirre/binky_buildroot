################################################################################
#
# Squeezenfc
#
################################################################################
SQUEEZENFC_VERSION = 8f7925cec87f693e5a670785af152d6dd9f8ae58
SQUEEZENFC_SITE = $(call github,naguirre,squeezenfc,$(SQUEEZENFC_VERSION))
SQUEEZENFC_LICENSE = GPLv3
SQUEEZENFC_LICENSE_FILES = COPYING
SQUEEZENFC_DEPENDENCIES = host-meson libnfc libcurl cjson

SQUEEZENFC_CONF_OPTS += --prefix=/usr --libdir=/usr/lib --cross-file $(HOST_DIR)/etc/meson/cross-compilation.conf --buildtype=release

SQUEEZENFC_NINJA_OPTS = $(if $(VERBOSE),-v) -j$(PARALLEL_JOBS)

define SQUEEZENFC_CONFIGURE_CMDS
rm -rf $(@D)/build
mkdir -p $(@D)/build
$(TARGET_MAKE_ENV) meson $(SQUEEZENFC_CONF_OPTS) $(@D) $(@D)/build
endef

define SQUEEZENFC_BUILD_CMDS
$(TARGET_MAKE_ENV) ninja $(SQUEEZENFC_NINJA_OPTS) -C $(@D)/build
endef

define SQUEEZENFC_INSTALL_TARGET_CMDS
$(TARGET_MAKE_ENV) DESTDIR=$(TARGET_DIR) ninja $(SQUEEZENFC_NINJA_OPTS) -C $(@D)/build install
rm -rf $(TARGET_DIR)/usr/include
endef

define SQUEEZENFC_INSTALL_STAGING_CMDS
$(TARGET_MAKE_ENV) DESTDIR=$(STAGING_DIR) ninja $(SQUEEZENFC_NINJA_OPTS) -C $(@D)/build install
endef

$(eval $(generic-package))
