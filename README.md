# mwlwifi
mac80211 driver for the Marvell 88W8864 802.11ac chip

* How to build mwlwifi with OpenWrt:

10.3.0.17-20160601 had been modified to be built with latest backports package "compat-wireless-2016-01-10" without patches.

a. Modify package/kernel/mwlwifi/Makefile:

PKG_VERSION:=10.3.0.17-20160601
&
PKG_SOURCE_VERSION:=4bb95ba1aeccce506a95499b49b9b844ecfae8a1

b. Rename package/kernel/mwlwifi/patches to package/kernel/mwlwifi/patches.tmp

c. make package/kernel/mwlwifi/clean

d. make V=s (-jx)

* For driver 10.3.0.17-20160603, [MAX-MPDU-7991] should be removed from vht_capab command of hostapd.
