# mwlwifi
mac80211 driver for the Marvell 88W8x64 802.11ac chip

* How to build mwlwifi with OpenWrt:

    10.3.0.17-20160601 had been modified to be built with latest backports package "compat-wireless-2016-01-10" without patches.

    a. Modify package/kernel/mwlwifi/Makefile:

    PKG_VERSION:=10.3.4.0-20170727 & PKG_SOURCE_VERSION:=3ca0ea1c201bab59c9d4c3941584e34a7acf0325

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

* DTS parameters for mwlwifi driver (pcie@X,0):

    marvell,2ghz = <0>; => Disable 2g band.

    marvell,5ghz = <0>; => Disable 5g band.

    marvell,chainmask = <4 4>; => Specify antenna number, default is 4x4. For WRT1200AC, you must set these values to 2x2.

    marvell,powertable => Specify external power table. If your device needs external power table, you must provide the power table via this parameter, otherwise the Tx power will be pretty low.

    root@lede:/# cat /sys/kernel/debug/ieee80211/phy0/mwlwifi/info

    power table loaded from dts: no

    => If it is "no", it does not allow you to load external power table (for new device).

    => If it is "yes", you must provide power table in DTS file (for old device).

* The way to change interrupt to different CPU cores:

    root@lede:/# echo 1 > /proc/irq/irq number of phy0 or phy1/smp_affinity => use CPU0

    root@lede:/# echo 2 > /proc/irq/irq number of phy0 or phy1/smp_affinity => use CPU1

* How to replace mwlwifi on your current LEDE/OpenWrt build:

    a. Establish a symbolic link to your working mwlwifi directory with current mwlwifi package name under directory "dl":

    #ls -l mwlwifi*

    lrwxrwxrwx 1 dlin dlin      48  mwlwifi-10.3.2.0-20170110 -> /home/dlin/home2/projects/github/mwlwifi

    -rw-r--r-- 1 dlin dlin 4175136  mwlwifi-10.3.2.0-20170110.tar.xz

    b. Back up original mwlwifi package and tar your working mwlwifi to replace oringial mwlwifi package:

    #tar Jcvf mwlwifi-10.3.2.0-20170110.tar.xz mwlwifi-10.3.2.0-20170110/.

    c. You can use "make V=s" to build the whole image or "make V=s package/kernel/mwlwifi/compile" to build mwlwifi package. The generated whole image or mwlwifi package can be found under directory "bin".

    d. Due to package version is the same as previous one, you need to add option "--force-reinstall" when you use "opkg" to update mwlwifi package on your device.

* Note for DFS of WRT3200ACM (88W8964):

    All WRT3200ACM devices are programmed with device power table. Mwlwifi driver will base on region code to set country code for your device and it will not allow you to change country code. There are another wifi (phy2) on WRT3200ACM which is not mwlwifi. It will allow you to change country code. Under this case, country code setting will be conflicted and it will let DFS can't work.

    There are two ways to resolve this problem:

    a. Please don't change country code and let mwlwifi set it for you.

    b. Remove phy2. Under this case, even though you change country code, mwlwifi will reject it. Because phy2 is not existed, country code setting won't be conflicted.

    opkg remove kmod-mwifiex-sdio

    opkg remove mwifiex-sdio-firmware

    reboot

    The better way is let mwlwifi set country code for you.
