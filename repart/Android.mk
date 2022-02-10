# Copyright (C) 2007 The Android Open Source Project
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#      http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.

LOCAL_PATH := $(call my-dir)

include $(CLEAR_VARS)

# SPRD: Add block device file system check function {@
blkfs_c_includes := \
    external/e2fsprogs/lib

blkfs_cflags := -O2 \
    -DHAVE_RECOVERY_BLKFS
# @}

tune2fs_static_libraries := \
    libext2_com_err \
    libext2_blkid \
    libext2_quota \
    libext2_uuid \
    libext2_e2p \
    libext2fs

reparter_common_static_libraries := \
    libapplypatch \
    libbspatch \
    libedify \
    libziparchive \
    libotautil \
    libbootloader_message \
    libutils \
    libext4_utils \
    libfec \
    libfec_rs \
    libfs_mgr \
    liblog \
    libselinux \
    libsparse \
    libsquashfs_utils \
    libbz \
    libz \
    libbase \
    libcrypto \
    libcrypto_utils \
    libcutils \
    libtune2fs \
    librecovery \
    liblp \
    $(tune2fs_static_libraries)

# repater (static executable)
# ===============================

LOCAL_MODULE := repart

LOCAL_SRC_FILES := \
	crc32.cpp \
	gptdata.cpp \
	main.cpp

LOCAL_C_INCLUDES += \
	$(LOCAL_PATH)/$(BOARD_PRODUCT_NAME) \
	$(TARGET_OTA_EXTENSIONS_DIR) \
    system/vold \
    system/core/adb \
    bootable/recovery \
    $(blkfs_c_includes)

LOCAL_CFLAGS += $(blkfs_cflags)

LOCAL_STATIC_LIBRARIES := \
    $(reparter_common_static_libraries)

LOCAL_MODULE_CLASS := EXECUTABLES

LOCAL_FORCE_STATIC_EXECUTABLE := true

include $(BUILD_EXECUTABLE)
