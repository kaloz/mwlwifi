obj-m += mwlwifi.o

mwlwifi-objs			+= core.o
mwlwifi-objs			+= mac80211.o
mwlwifi-objs			+= mu_mimo.o
mwlwifi-objs			+= vendor_cmd.o
mwlwifi-objs			+= utils.o
mwlwifi-$(CONFIG_THERMAL)	+= thermal.o
mwlwifi-$(CONFIG_DEBUG_FS)	+= debugfs.o
mwlwifi-objs			+= hif/fwcmd.o
mwlwifi-objs			+= hif/pcie/pcie.o
mwlwifi-objs			+= hif/pcie/fwdl.o
mwlwifi-objs			+= hif/pcie/tx.o
mwlwifi-objs			+= hif/pcie/rx.o
mwlwifi-objs			+= hif/pcie/tx_ndp.o
mwlwifi-objs			+= hif/pcie/rx_ndp.o

ccflags-y += -I$(src)
ccflags-y += -O2 -funroll-loops -D__CHECK_ENDIAN__

all:
	$(MAKE) -C $(KDIR) M=$(PWD)

clean:
	rm -f *.a *.s *.ko *.ko.cmd *.mod.* .mwlwifi.* modules.order Module.symvers
	rm -rf .tmp_versions
	find . -name ".*.o.cmd" -exec rm -f {} \;
	find . -name "*.o" -exec rm -f {} \;
