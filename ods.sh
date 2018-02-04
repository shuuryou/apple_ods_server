#!/usr/bin/env bash
export PATH='/usr/local/sbin:/usr/local/bin:/usr/sbin:/usr/bin:/sbin:/bin'
exec 1> >(logger -t UDEV-ODS) 2>&1

SERVICE=/etc/avahi/services/ods.service

if [ "x${ID_CDROM_MEDIA}" != "x1" ]
then
	echo "CD-ROM drive is empty. Removing MDNS service."

	rm -f "$SERVICE"
	exit 0
fi

if [ "x${ID_CDROM_MEDIA_TRACK_COUNT_AUDIO}" != "x" ]
then
	echo "CD-ROM drive contains a disk, but it is an Audio CD. Removing MDNS service."
	rm -f "$SERVICE"
	exit 0
fi

echo "CD-ROM drive contains an acceptable disk. Adding MDNS service."

cat << EOF > "$SERVICE"
<?xml version="1.0" standalone='no'?>
<!DOCTYPE service-group SYSTEM "avahi-service.dtd">
<service-group>
 <name replace-wildcards="yes">%h</name>
 <service>
   <type>_odisk._tcp</type>
   <port>65432</port>
   <txt-record>sys=waMa=0,adVF=0x4,adDT=0x3,adCC=1</txt-record>
   <txt-record>ods=adVN=CDROM,adVT=public.cd-media</txt-record>
 </service>
</service-group>
EOF
