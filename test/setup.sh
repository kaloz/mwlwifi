iw reg set US
insmod mwlwifi.ko fw_name=88W8864.bin pwr_tbl=Mamba_FCC_v1.2_5G4TX.ini
hostapd -B ./hostapd.conf
brctl addif br-lan wlan1
