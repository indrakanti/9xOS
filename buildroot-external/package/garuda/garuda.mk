################################################################################
#
# garuda
#
################################################################################

GARUDA_VERSION = 0.1.0
GARUDA_SITE = $(BR2_EXTERNAL_NINEXOS_PATH)/../components/garuda
GARUDA_SITE_METHOD = local
# Set the real license before the first public release.
GARUDA_LICENSE = TBD
GARUDA_INSTALL_STAGING = YES

GARUDA_CONF_OPTS = -DGARUDA_BUILD_TESTS=OFF

ifeq ($(BR2_PACKAGE_GARUDA_DEMO),y)
GARUDA_CONF_OPTS += -DGARUDA_BUILD_EXAMPLES=ON
else
GARUDA_CONF_OPTS += -DGARUDA_BUILD_EXAMPLES=OFF
endif

define GARUDA_INSTALL_INIT_SYSV
	$(INSTALL) -D -m 0755 $(GARUDA_PKGDIR)/S05garudad \
		$(TARGET_DIR)/etc/init.d/S05garudad
	mkdir -p $(TARGET_DIR)/etc/default
	echo 'GARUDA_WATCHDOG="$(call qstrip,$(BR2_PACKAGE_GARUDA_WATCHDOG_DEVICE))"' \
		> $(TARGET_DIR)/etc/default/garudad
endef

$(eval $(cmake-package))
