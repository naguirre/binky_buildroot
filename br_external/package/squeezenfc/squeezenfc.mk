################################################################################
#
# Squeezenfc
#
################################################################################
SQUEEZENFC_SITE_METHOD = local
SQUEEZENFC_VERSION = local
SQUEEZENFC_SITE = $(SQUEEZENFC_PKGDIR)/src
SQUEEZENFC_LICENSE = GPLv3
SQUEEZENFC_LICENSE_FILES = COPYING
SQUEEZENFC_DEPENDENCIES = host-meson libnfc

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
