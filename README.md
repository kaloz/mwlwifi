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

* After driver 10.3.0.17-20160603, [MAX-MPDU-7991] should be removed from vht_capab command of hostapd.

* Hostpad must include following commit for 160 MHz operation:

    commit 03a72eacda5d9a1837a74387081596a0d5466ec1
    Author: Jouni Malinen <jouni@qca.qualcomm.com>
    Date:   Thu Dec 17 18:39:19 2015 +0200
    
    VHT: Add an interoperability workaround for 80+80 and 160 MHz channels
 
    Number of deployed 80 MHz capable VHT stations that do not support 80+80
    and 160 MHz bandwidths seem to misbehave when trying to connect to an AP
    that advertises 80+80 or 160 MHz channel bandwidth in the VHT Operation
    element. To avoid such issues with deployed devices, modify the design
    based on newly proposed IEEE 802.11 standard changes.
 
    This allows poorly implemented VHT 80 MHz stations to connect with the
    AP in 80 MHz mode. 80+80 and 160 MHz capable stations need to support
    the new workaround mechanism to allow full bandwidth to be used.
    However, there are more or less no impacted station with 80+80/160
    capability deployed.
 
    Signed-off-by: Jouni Malinen jouni@qca.qualcomm.com

    Note: After hostapd package 2016-06-15, this commit is already included.

* In order to let STA mode to support 160 MHz operation, mac80211 package should be 2016-10-08 or later.
* WiFi device does not use HT rates when using TKIP as the encryption cipher.
  If you want to have good performance, please use AES only. 
