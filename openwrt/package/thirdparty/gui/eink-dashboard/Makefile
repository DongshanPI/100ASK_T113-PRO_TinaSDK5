include $(TOPDIR)/rules.mk

PKG_NAME:=eink-dashboard
PKG_VERSION:=0.1.0
PKG_RELEASE:=1
PKG_LICENSE:=GPL-3.0-only
PKG_LICENSE_FILES:=LICENSE
PKG_SOURCE_VERSION:=087cbaf80cebcbc5d101d37ab3520b3c6fe461c5
PKG_SOURCE:=$(PKG_SOURCE_VERSION).tar.gz
PKG_SOURCE_URL:=https://github.com/dshanpi/EinkOS/archive/
PKG_HASH:=654edf790410e2b4008b87389639e92c9185fc5aff8c1b6b6105b01cac374ef7
PKG_BUILD_DIR:=$(BUILD_DIR)/EinkOS-$(PKG_SOURCE_VERSION)
EINK_SOURCE_EPOCH:=1791620123
TEST_CODE_DIR:=$(LICHEE_TOP_DIR)/eink-3.52/epd_test
PKG_FILE_DEPENDS+=$(TEST_CODE_DIR) $(LICHEE_TOP_DIR)/kernel/linux-5.4/include/uapi/linux/epd_100ask.h

include $(INCLUDE_DIR)/package.mk

define Package/eink-dashboard
  SECTION:=gui
  CATEGORY:=Gui
  SUBMENU:=Littlevgl
  TITLE:=EinkOS Linux e-paper desktop and applications
  URL:=https://github.com/dshanpi/EinkOS
  DEPENDS:=+libpthread +iw +btmanager-v4.0 +bluez-daemon +bluez-utils +dnsmasq
endef

define Package/eink-dashboard/description
  EinkOS desktop and connection service, fetched from a pinned upstream commit.
  Board drivers and epd-test remain in this hardware repository.
endef

define Package/eink-dashboard/conffiles
/etc/eink-dashboard.conf
endef

define Build/Configure
endef

EINK_MAKE_ARGS = CC="$(TARGET_CC)" CFLAGS="$(TARGET_CFLAGS)" \
  LDFLAGS="$(TARGET_LDFLAGS)" BT_INCLUDE_DIR="$(STAGING_DIR)/usr/include" \
  PLATFORM=tina STATIC=0 COMMIT="$(PKG_SOURCE_VERSION)" \
  SOURCE_DATE_EPOCH="$(EINK_SOURCE_EPOCH)"

define Build/Compile
	$(MAKE) -C $(PKG_BUILD_DIR) $(EINK_MAKE_ARGS) all
	$(TARGET_CC) $(TARGET_CFLAGS) \
		-iquote $(LICHEE_TOP_DIR)/kernel/linux-5.4/include/uapi/linux \
		$(TEST_CODE_DIR)/epd_test.c -o $(PKG_BUILD_DIR)/epd-test $(TARGET_LDFLAGS)
endef

define Package/eink-dashboard/install
	$(MAKE) -C $(PKG_BUILD_DIR) $(EINK_MAKE_ARGS) DESTDIR="$(1)" install
	$(INSTALL_BIN) $(PKG_BUILD_DIR)/epd-test $(1)/usr/bin/epd-test
endef

$(eval $(call BuildPackage,eink-dashboard))
