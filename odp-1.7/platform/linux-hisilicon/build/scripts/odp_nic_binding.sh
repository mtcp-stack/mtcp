#!/bin/bash

eth_name=$3
PLATFORM_DEV_DIR="/sys/bus/platform/devices"
PLATFORM_DRV_DIR="/sys/bus/platform/drivers"
PCI_DEV_DIR="/sys/bus/pci/devices"
PCI_DRV_DIR="/sys/bus/pci/drivers"
PCI_KDRIVER="ixgbe"
PCI_UDRIVER="igb_uio"
SOC_KDRIVER="hns-nic"
SOC_UDRIVER="pv660_hns"
DRIVER_USED=("ixgbe" "ixgbe" "ixgbe")
DRIVER_UNUSED=("igb_uio" "igb_uio" "igb_uio")
NODE_INFO=("soc@0:ethernet@0" "soc@0:ethernet@0" "soc@0:ethernet@0")
IF_DEV_NAME=("eth0" "eth0" "eth0")

function fnhelp()
{
	echo -e "To display current device status:"
	echo -e "	./odp_nic_binding.sh --status"
	echo -e "To bind eth1 or soc@000000000:ethernet@0 from the current driver and move to use igb_uio"
	echo -e "PCI:"
	echo -e "	./odp_nic_binding.sh --bind igb_uio eth1"
	echo -e "SOC:"
	echo -e "	./odp_nic_binding.sh --bind hns-nic soc@000000000:ethernet@0"
	echo -e "	or ./odp_nic_binding.sh --bind pv660_hns soc@000000000:ethernet@0"	
	echo -e "To unbind 0000:01:00.0 or soc@0:ethernet@0 from using any driver"
	echo -e "PCI:"
	echo -e "	./odp_nic_binding.sh --unbind ixgbe 0000:01:00.0 "
	echo -e "	or ./odp_nic_binding.sh --unbind ixgbe eth1 "
	echo -e "	or ./odp_nic_binding.sh --unbind igb_uio 0000:01:00.0 "	
	echo -e "SOC:"
	echo -e "	./odp_nic_binding.sh --unbind hns-nic soc@000000000:ethernet@0"		
	echo -e "	or ./odp_nic_binding.sh --unbind pv660_hns soc@000000000:ethernet@0"		
}

if [ -z "$1" ];then
	fnhelp
elif [ $1 == "--status" ];then
	dev_num=0
	for file in `ls $PLATFORM_DEV_DIR |grep -E "ethernet@"`;
	do
		DRIVER_USED[$dev_num]=`cat $PLATFORM_DEV_DIR/$file/uevent |grep -E "DRIVER" |awk -F'=' '{print $2}'`
		if [ "$SOC_KDRIVER" = "${DRIVER_USED[dev_num]}" ];then
			DRIVER_UNUSED[$dev_num]=$SOC_UDRIVER
		elif [ "$SOC_UDRIVER" = "${DRIVER_USED[dev_num]}" ];then
			DRIVER_UNUSED[dev_num]=$SOC_KDRIVER
		else
			((dev_num+=1))
			continue
		fi
		NODE_INFO[$dev_num]=$file
		IF_DEV_NAME[$dev_num]=`ls $PLATFORM_DEV_DIR/$file/net`
		((dev_num+=1))	
	done
	for file in `ls $PCI_DEV_DIR`;
	do	
		DRIVER_USED[$dev_num]=`cat $PCI_DEV_DIR/$file/uevent |grep -E "DRIVER" |awk -F'=' '{print $2}'`
		if [[ $PCI_KDRIVER == ${DRIVER_USED[$dev_num]} ]];then
			DRIVER_UNUSED[$dev_num]=$PCI_UDRIVER
		elif [[ $PCI_UDRIVER == ${DRIVER_USED[$dev_num]} ]];then
			DRIVER_UNUSED[$dev_num]=$PCI_KDRIVER
		else
			((dev_num+=1))
			continue
		fi	
		NODE_INFO[$dev_num]=$file
		IF_DEV_NAME[$dev_num]=`ls $PCI_DEV_DIR/$file/net`
		((dev_num+=1))			
	done
	echo -e "Network devices using ODP-compatible driver:\n"
	for((i=0;i<dev_num;i++))
	do
		if [[ ${DRIVER_USED[$i]} == $SOC_UDRIVER ]] || [[ ${DRIVER_USED[$i]} == $PCI_UDRIVER ]];then
			echo -n ${NODE_INFO[$i]}
			echo -n "  "
			echo -n ${IF_DEV_NAME[$i]}
			echo -n "  "
			echo -n "used driver="
			echo -n ${DRIVER_USED[$i]} 
			echo -n "  "
			echo -n "unused driver="
			echo -n ${DRIVER_UNUSED[$i]}
			echo -e "\n"
		fi
	done
	echo -e "Network devices using kernel driver:\n"	
	for((i=0;i<dev_num;i++))
	do
		if [[ ${DRIVER_USED[$i]} == $SOC_KDRIVER ]] || [[ ${DRIVER_USED[$i]} == $PCI_KDRIVER ]];then
			echo -n ${NODE_INFO[$i]}
			echo -n "  "
			echo -n ${IF_DEV_NAME[$i]}
			echo -n "  "
			echo -n "used driver="
			echo -n ${DRIVER_USED[$i]} 
			echo -n "  "
			echo -n "unused driver="
			echo -n ${DRIVER_UNUSED[$i]}
			echo -e "\n"
		fi
	done	
		
elif [ $1 == "--bind" ];then
	if [ $2 == "igb_uio" ];then
		if [[ $eth_name == o* ]]||[[ $eth_name == e*  ]];then
		pci_num=`ethtool -i $eth_name |grep -E "bus-info:" |awk -F': ' '{ print $2 }'`
		else
		pci_num=$eth_name
		fi
		if [ -z "$pci_num" ];then
			echo "$eth_name doesn't exit!\n"
			exit 1
		fi
		if [ -d "$PCI_DRV_DIR/$PCI_KDRIVER/$pci_num" ];then
			echo $pci_num >$PCI_DEV_DIR/$pci_num/driver/unbind
		fi
		echo $pci_num >$PCI_DRV_DIR/$PCI_UDRIVER/bind
	elif [ $2 == "ixgbe" ];then
		if [[ $eth_name == o* ]]||[[ $eth_name == e* ]];then
		pci_num=`ethtool -i $eth_name |grep -E "bus-info:" |awk -F': ' '{ print $2 }'`
		else
		pci_num=$eth_name
		fi
		if [ -z "$pci_num" ];then
			echo "$eth_name doesn't exit!\n"
			exit 1
		fi
		if [ -d "$PCI_DRV_DIR/$PCI_UDRIVER/$pci_num" ];then
			echo $pci_num >$PCI_DEV_DIR/$pci_num/driver/unbind
		fi		
		echo $pci_num >$PCI_DRV_DIR/$PCI_KDRIVER/bind
	elif [ $2 == $SOC_UDRIVER ];then
		if [ -d "$PLATFORM_DRV_DIR/$SOC_KDRIVER/$eth_name" ];then
			echo $eth_name >$PLATFORM_DEV_DIR/$eth_name/driver/unbind
		fi		
		echo $eth_name >$PLATFORM_DRV_DIR/$SOC_UDRIVER/bind	
	elif [ $2 == "hns-nic" ];then
		if [ -d "$PLATFORM_DRV_DIR/$SOC_UDRIVER/$eth_name" ];then
			echo $eth_name >$PLATFORM_DEV_DIR/$eth_name/driver/unbind
		fi		
		echo $eth_name >$PLATFORM_DRV_DIR/$SOC_KDRIVER/bind	
	else
		echo "input param error!\n"	
		fnhelp
		exit 1
	fi
elif [ $1 == "--unbind" ];then
	if [ $2 == "igb_uio" ];then
		if [[ $eth_name == o* ]]||[[ $eth_name == e*  ]];then
		pci_num=`ethtool -i $eth_name |grep -E "bus-info:" |awk -F': ' '{ print $2 }'`
		else
		pci_num=$eth_name
		fi
		echo $pci_num >$PCI_DEV_DIR/$pci_num/driver/unbind
	elif [ $2 == "ixgbe" ];then
		if [[ $eth_name == o* ]]||[[ $eth_name == e*  ]];then
		pci_num=`ethtool -i $eth_name |grep -E "bus-info:" |awk -F': ' '{ print $2 }'`
		else
		pci_num=$eth_name
		fi
		echo $pci_num >$PCI_DEV_DIR/$pci_num/driver/unbind
	elif [ $2 == $SOC_UDRIVER ];then
		echo $eth_name >$PLATFORM_DEV_DIR/$eth_name/driver/unbind
	elif [ $2 == "hns-nic" ];then
		echo $eth_name >$PLATFORM_DEV_DIR/$eth_name/driver/unbind
	else
		echo "input param error!\n"	
		fnhelp
		exit i
	fi
else
		echo "input param error!\n"	
		fnhelp
		exit 1	
fi	