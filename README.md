# mwlwifi
mac80211 driver for the Marvell 88W8864 802.11ac chip

How to build mwlwifi with OpenWrt:

10.3.0.17-20160601 had been modified to be built with latest backports package "compat-wireless-2016-01-10" without patches.

1. Modify package/kernel/mwlwifi/Makefile:

PKG_VERSION:=10.3.0.17-20160601
&
PKG_SOURCE_VERSION:=4bb95ba1aeccce506a95499b49b9b844ecfae8a1

2. Rename package/kernel/mwlwifi/patches to package/kernel/mwlwifi/patches.tmp

3. make package/kernel/mwlwifi/clean

4. make V=s (-jx)

