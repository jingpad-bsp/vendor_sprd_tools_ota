LOCAL_PATH:= $(call my-dir)
include $(CLEAR_VARS)
LOCAL_SRC_FILES := sprd_install.cpp
LOCAL_C_INCLUDES += \
    $(LOCAL_PATH)/../ \
    bootable/recovery \
    bootable/recovery/recovery_ui/include

LOCAL_STATIC_LIBRARIES += \
    libfs_mgr \
    libselinux \
    libapplypatch \
    libcrypto_static \
    libziparchive

ifeq ($(TARGET_USERIMAGES_USE_EXT4), true)
    LOCAL_CFLAGS += -DUSE_EXT4
    LOCAL_C_INCLUDES += system/extras/ext4_utils/include
    LOCAL_STATIC_LIBRARIES += libext4_utils libz
endif

TARGET_RECOVERY_UPDATER_EXTRA_LIBS := $(LOCAL_STATIC_LIBRARIES)

LOCAL_MODULE := libsprd_updater
include $(BUILD_STATIC_LIBRARY)
