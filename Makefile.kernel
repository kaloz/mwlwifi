obj-$(CONFIG_MWLWIFI)	+= mwlwifi.o

mwlwifi-objs		+= main.o
mwlwifi-objs		+= mac80211.o
mwlwifi-objs		+= fwdl.o
mwlwifi-objs		+= fwcmd.o
mwlwifi-objs		+= tx.o
mwlwifi-objs		+= rx.o
mwlwifi-objs		+= isr.o
mwlwifi-$(CONFIG_THERMAL)	+= thermal.o
mwlwifi-$(CONFIG_DEBUG_FS)	+= debugfs.o

ccflags-y += -D__CHECK_ENDIAN__
