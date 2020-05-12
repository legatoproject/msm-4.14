# SWISTART
# Sierra make file for LK

LOCAL_DIR := $(GET_LOCAL_DIR)

INCLUDES += -I$(LOCAL_DIR)/api

CFLAGS += -DSWI_IMAGE_LK

OBJS += \
	$(LOCAL_DIR)/src/ssmem_lk.o \
	$(LOCAL_DIR)/src/ssmem_core.o \
	$(LOCAL_DIR)/src/ssmem_user.o \
	$(LOCAL_DIR)/src/ssmem_utils.o \
	$(LOCAL_DIR)/src/ssmem_bscommon.o

# SWISTOP