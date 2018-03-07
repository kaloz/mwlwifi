# mwlwifi
mac80211 driver for the Marvell 88W8x64 802.11ac chip

## Building mwlwifi With OpenWrt/LEDE
1. Modify `package/kernel/mwlwifi/Makefile`:
    ```
    PKG_VERSION:=10.3.4.0-20180305
    PKG_SOURCE_URL:=https://github.com/kaloz/mwlwifi
    PKG_SOURCE_PROTO:=git
    PKG_SOURCE_VERSION:=b1b9a9e1c1beee30a8cce4038f4109727362ebe0
    PKG_MIRROR_HASH:=7ca5604eb601784224bf2d9e6c4facbc82fbe90b18a19286511782095f3db640
    ```

2. Rename `package/kernel/mwlwifi/patches` to `package/kernel/mwlwifi/patches.tmp`.
3. Run the following commands:
    ```sh
    make V=s package/kernel/mwlwifi/{clean,prepare,compile} # -jx - where X is your cores/threads or just leave it out
    make V=s # -jx - where X is your cores/threads or just leave it out
    ```

### Special Considerations

* After driver 10.3.0.17-20160603, [MAX-MPDU-7991] should be removed from vht_capab command of hostapd.

* Hostpad must include the following commit for 160 MHz operation:
    ```
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
    ```

* In order to let STA mode to support 160 MHz operation, mac80211 package should be 2016-10-08 or later.

* WiFi device does not use HT rates when using TKIP as the encryption cipher. If you want to have good performance, please use AES only.

* DTS parameters for mwlwifi driver (pcie@X,0):
    ```sh
    #Disable 2g band
    marvell,2ghz = <0>;

    #Disable 5g band
    marvell,5ghz = <0>;
    
    #Specify antenna number, default is 4x4. For WRT1200AC, you must set these values to 2x2.
    marvell,chainmask = <4 4>;
    
    #Specify external power table. If your device needs external power table, you must provide the power table via this parameter, otherwise the Tx power will be pretty low.
    marvell,powertable
    ```

    To see if your device needs/accepts an external power table or not, run the following:
    ```sh
    cat /sys/kernel/debug/ieee80211/phy0/mwlwifi/info
    ```
    
    You should see a line in the results which looks like the following:
    ```sh
    power table loaded from dts: no
    ```

    If it is "no", it does not allow you to load external power table (for newer devices due to FCC regulations). If it is "yes", you must provide power table in DTS file (for older devices).

* Changing interrupt to different CPU cores:
    ```sh
    #Use CPU0:
    echo 1 > /proc/irq/irq number of phy0 or phy1/smp_affinity

    #Use CPU1:
    echo 2 > /proc/irq/irq number of phy0 or phy1/smp_affinity
    ```

## Note for DFS of WRT3200ACM (88W8964)

All WRT3200ACM devices are programmed with device power table. Mwlwifi driver will base on region code to set country code for your device and it will not allow you to change country code. There are another wifi (phy2) on WRT3200ACM which is not mwlwifi. It will allow you to change country code. Under this case, country code setting will be conflicted and it will let DFS can't work.

There are two ways to resolve this problem or for the European version, right now, only the 2nd option works:
    
1. Please don't change country code and let mwlwifi set it for you.   
2. Remove phy2. Under this case, even though you change country code, mwlwifi will reject it. Because phy2 is not existed, country code setting won't be conflicted. To do this, run the following commands (for OpenWrt/LEDE):
    
```sh
opkg remove kmod-mwifiex-sdio
opkg remove mwifiex-sdio-firmware
reboot
```
        
### For the European version, also requires the following
      
* Remove the following ```radio2``` and ```default_radio2``` section from the ```/etc/config/wireless```    
* You find out you country: ```iw reg get``` (let's say it says ```FR``` - France), then
  * Either add this option to ```radio0``` and ```radio1``` section  in the  ```/etc/config/wireless``` as ```option country 'FR'``` 
  * Or via ```LUCI``` at ```/cgi-bin/luci/admin/network/wireless```, click both wireless interfaces with ```EDIT``` and the country settings is in the ```Advanced Settings``` tab, where you can set it.
    
The best way is let mwlwifi set country code for you in the US version router.

#### Note

There will be a change in the driver as is described in:  
https://github.com/kaloz/mwlwifi/issues/280#issuecomment-370997269   
Once, it is implemented, the first option will be enough.

##### The European version on the radio0 5ghz 160 mhz channel

160 mhz works only with channel 100 upwards, then ```mwlwifi``` auto set it up, ```auto``` is not working for now.

## Replacing mwlwifi on a Current OpenWrt/LEDE Build

1. Establish a symbolic link to your working mwlwifi directory with current mwlwifi package name under directory "dl":

    You should execute something like the following:
    ```sh
    cd dl
    git clone https://github.com/kaloz/mwlwifi /home/dlin/home2/projects/github/mwlwifi
    ln -s /home/dlin/home2/projects/github/mwlwifi ./mwlwifi-10.3.2.0-20170110
    ls -all
    ```

    The output is like:    
    ```text
    lrwxrwxrwx 1 dlin dlin      48  mwlwifi-10.3.2.0-20170110 -> /home/dlin/home2/projects/github/mwlwifi
    -rw-r--r-- 1 dlin dlin 4175136  mwlwifi-10.3.2.0-20170110.tar.xz
    ```

2. Back up original mwlwifi package and tar your working mwlwifi to replace original mwlwifi package:

    ```sh
    mv mwlwifi-10.3.2.0-20170110.tar.xz mwlwifi-10.3.2.0-20170110.backup.tar.xz
    tar Jcvf mwlwifi-10.3.2.0-20170110.tar.xz mwlwifi-10.3.2.0-20170110/.
    ```

3. You can use `make V=s` to build the whole image or `make V=s package/kernel/mwlwifi/{clean,prepare,compile}` to build mwlwifi package. The generated whole image or mwlwifi package can be found under directory "bin".

Due to package version being the same as previous one, you need to add option `--force-reinstall` when you use `opkg` to update mwlwifi package on your device.

## Monitor interface for debug

1. Create moinitor interface mon0:
    ```sh
    iw wlan0/wlan1 interface add mon0 type monitor
    ifconfig mon0 up
    ```

2. Use tcpdump to dump dhcp packets:
    ```sh
    tcpdump -vvvi mon0 -n port 67 and port 68
    ```

3. Use tcpdump to dump icmp packets:
    ```sh
    tcpdump -vvvi mon0 icmp
    ```
