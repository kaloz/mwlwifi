obj-m += mwlwifi.o

mwlwifi-objs		+= mwl_main.o
mwlwifi-objs		+= mwl_mac80211.o
mwlwifi-objs		+= mwl_fwdl.o
mwlwifi-objs		+= mwl_fwcmd.o
mwlwifi-objs		+= mwl_tx.o
mwlwifi-objs		+= mwl_rx.o
mwlwifi-objs		+= mwl_debug.o

AS		= $(CROSS_COMPILE)as
LD		= $(CROSS_COMPILE)ld
CC		= $(CROSS_COMPILE)gcc

ifeq (1, $(MWLDBG))
EXTRA_CFLAGS+= -DMWL_DEBUG
endif

EXTRA_CFLAGS+= -I${KDIR}
EXTRA_CFLAGS+= -O2 -funroll-loops

EXTRA_CFLAGS+= -I${PWD}

all:
	$(MAKE) -C $(KDIR) SUBDIRS=$(PWD) modules

clean:
	rm -f *.o *.a *.s *.ko *.ko.cmd *.o.cmd *.mod.* .mwlwifi.*
	rm -rf modules.order Module.symvers .tmp_versions
	find . -name ".*.o.cmd" -exec rm -f {} \;
