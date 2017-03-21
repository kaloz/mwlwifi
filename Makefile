obj-m += mwlwifi.o

mwlwifi-objs			+= core.o
mwlwifi-objs			+= mac80211.o
mwlwifi-$(CONFIG_THERMAL)	+= thermal.o
mwlwifi-$(CONFIG_DEBUG_FS)	+= debugfs.o
mwlwifi-objs			+= hif/fwcmd.o
mwlwifi-objs			+= hif/pcie/pcie.o
mwlwifi-objs			+= hif/pcie/fwdl.o
mwlwifi-objs			+= hif/pcie/tx.o
mwlwifi-objs			+= hif/pcie/rx.o
mwlwifi-objs			+= hif/pcie/tx_ndp.o
mwlwifi-objs			+= hif/pcie/rx_ndp.o

AS		= $(CROSS_COMPILE)as
LD		= $(CROSS_COMPILE)ld
CC		= $(CROSS_COMPILE)gcc

EXTRA_CFLAGS+= -I${KDIR}
EXTRA_CFLAGS+= -O2 -funroll-loops -D__CHECK_ENDIAN__

EXTRA_CFLAGS+= -I${PWD}

all:
	$(MAKE) -C $(KDIR) SUBDIRS=$(PWD) modules

clean:
	rm -f *.a *.s *.ko *.ko.cmd *.mod.* .mwlwifi.* modules.order Module.symvers
	rm -rf .tmp_versions
	find . -name ".*.o.cmd" -exec rm -f {} \;
	find . -name "*.o" -exec rm -f {} \;
