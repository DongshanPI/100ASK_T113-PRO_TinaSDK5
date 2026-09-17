include $(TOPDIR)/rules.mk

PKG_NAME:=eink-dashboard
PKG_VERSION:=1.2.0
PKG_RELEASE:=2
PKG_LICENSE:=MIT
PKG_BUILD_DIR:=$(BUILD_DIR)/$(PKG_NAME)
SRC_CODE_DIR:=$(LICHEE_TOP_DIR)/eink-3.52/lvgl_epd_demo
TEST_CODE_DIR:=$(LICHEE_TOP_DIR)/eink-3.52/epd_test
PKG_FILE_DEPENDS+=$(SRC_CODE_DIR) $(TEST_CODE_DIR) $(LICHEE_TOP_DIR)/kernel/linux-5.4/include/uapi/linux/epd_100ask.h

include $(INCLUDE_DIR)/package.mk

define Package/eink-dashboard
  SECTION:=gui
  CATEGORY:=Gui
  SUBMENU:=Littlevgl
  TITLE:=240x360 E-Ink desktop OS shell
  DEPENDS:=+libpthread +iw
endef

define Package/eink-dashboard/description
  Bilingual LVGL desktop shell and system applications for the 100ASK 240x360 E-Ink panel.
endef

define Build/Prepare
	$(INSTALL_DIR) $(PKG_BUILD_DIR)
	$(CP) $(SRC_CODE_DIR)/* $(PKG_BUILD_DIR)/
	$(CP) $(TEST_CODE_DIR)/epd_test.c $(PKG_BUILD_DIR)/
endef

define Build/Configure
endef

define Build/Compile
	$(MAKE) -C $(PKG_BUILD_DIR) clean
	$(MAKE) -C $(PKG_BUILD_DIR) \
		CC="$(TARGET_CC)" \
		SDK_BASE="$(LICHEE_TOP_DIR)" \
		CFLAGS="$(TARGET_CFLAGS)" \
		LDFLAGS="$(TARGET_LDFLAGS)" \
		STATIC=0 \
		all
	$(TARGET_CC) $(TARGET_CFLAGS) \
		-iquote $(LICHEE_TOP_DIR)/kernel/linux-5.4/include/uapi/linux \
		$(PKG_BUILD_DIR)/epd_test.c -o $(PKG_BUILD_DIR)/epd-test \
		$(TARGET_LDFLAGS)
endef

define Package/eink-dashboard/conffiles
/etc/eink-dashboard.conf
endef

define Package/eink-dashboard/install
	$(INSTALL_DIR) $(1)/usr/bin
	$(INSTALL_BIN) $(PKG_BUILD_DIR)/eink-dashboard $(1)/usr/bin/eink-dashboard
	$(INSTALL_BIN) $(PKG_BUILD_DIR)/epd-test $(1)/usr/bin/epd-test
	$(INSTALL_DIR) $(1)/etc/init.d
	$(INSTALL_BIN) ./files/eink-dashboard.init $(1)/etc/init.d/eink-dashboard
	$(INSTALL_DIR) $(1)/etc
	$(INSTALL_CONF) ./files/eink-dashboard.conf $(1)/etc/eink-dashboard.conf
	$(INSTALL_DIR) $(1)/usr/share/licenses/eink-dashboard
	$(INSTALL_DATA) ./files/THIRD_PARTY_NOTICES $(1)/usr/share/licenses/eink-dashboard/
	$(INSTALL_DATA) ./files/OFL-1.1.txt $(1)/usr/share/licenses/eink-dashboard/
	$(INSTALL_DIR) $(1)/usr/share/eink-os/books
	$(INSTALL_DATA) ./files/welcome.txt $(1)/usr/share/eink-os/books/
endef

$(eval $(call BuildPackage,eink-dashboard))
