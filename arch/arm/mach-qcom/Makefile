obj-$(CONFIG_USE_OF) += board-dt.o
obj-$(CONFIG_SMP)	+= platsmp.o
obj-$(CONFIG_ARCH_QCS405) += board-qcs405.o
obj-$(CONFIG_ARCH_QCS403) += board-qcs403.o
obj-$(CONFIG_ARCH_TRINKET) += board-trinket.o
obj-$(CONFIG_ARCH_SDXPRAIRIE) += board-sdxprairie.o
obj-$(CONFIG_ARCH_MDM9607) += board-mdm9607.o
obj-$(CONFIG_ARCH_SDM660) += board-660.o

# SWISTART
obj-$(CONFIG_SIERRA_SSMEM) += sierra_smem.o sierra_smem_msg.o sierra_smem_errdump.o sierra_smem_mode.o
obj-$(CONFIG_SIERRA) += sierra_serial.o
obj-y += sierra_bscommon.o
CFLAGS_sierra_smem_mode.o := -I$(src)
CFLAGS_sierra_bscommon.o := -I$(src)

obj-$(CONFIG_SIERRA) += sierra/
# SWISTOP
