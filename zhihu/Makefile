export THEOS_DEVICE_IP = 127.0.0.1

# 11.0, not 13.0: a 13.0 deployment target makes clang emit LC_BUILD_VERSION,
# which the old substitute-loader on iOS 13 does not understand -- it silently
# skips the tweak. 11.0 produces the legacy LC_VERSION_MIN_IPHONEOS instead.
TARGET := iphone:clang:13.7:11.0
ARCHS = arm64
INSTALL_TARGET_PROCESSES = osee2unifiedRelease

include $(THEOS)/makefiles/common.mk

TWEAK_NAME = NoUnderlineZhihu
NoUnderlineZhihu_FILES = Tweak.xm
NoUnderlineZhihu_CFLAGS = -fobjc-arc -I../common
NoUnderlineZhihu_FRAMEWORKS = UIKit WebKit

include $(THEOS_MAKE_PATH)/tweak.mk
