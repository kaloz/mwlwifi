obj-m += mwlwifi.o

mwlwifi-objs		+= main.o
mwlwifi-objs		+= mac80211.o
mwlwifi-objs		+= fwdl.o
mwlwifi-objs		+= fwcmd.o
mwlwifi-objs		+= tx.o
mwlwifi-objs		+= rx.o
mwlwifi-objs		+= isr.o
mwlwifi-$(CONFIG_THERMAL)	+= thermal.o
mwlwifi-$(CONFIG_DEBUG_FS)	+= debugfs.o
ifeq (1, $(BUILD_MFG))
mwlwifi-objs += mfg.o
endif

AS		= $(CROSS_COMPILE)as
LD		= $(CROSS_COMPILE)ld
CC		= $(CROSS_COMPILE)gcc

EXTRA_CFLAGS+= -I${KDIR}
EXTRA_CFLAGS+= -O2 -funroll-loops -D__CHECK_ENDIAN__

ifeq (1, $(BUILD_MFG))
EXTRA_CFLAGS+= -DSUPPORT_MFG
endif

ifeq (1, $(BUILD_BG4CT_A0))
EXTRA_CFLAGS+= -DBG4CT_A0_WORKAROUND
endif

EXTRA_CFLAGS+= -I${PWD}

all:
	$(MAKE) -C $(KDIR) SUBDIRS=$(PWD) modules

clean:
	rm -f *.o *.a *.s *.ko *.ko.cmd *.o.cmd *.mod.* .mwlwifi.*
	rm -rf modules.order Module.symvers .tmp_versions
	find . -name ".*.o.cmd" -exec rm -f {} \;
