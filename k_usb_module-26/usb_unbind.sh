echo 1-1:1.0 | sudo tee  /sys/bus/usb/drivers/usb-storage/unbind
exec $SHELL 
