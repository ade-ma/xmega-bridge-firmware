import usb
import iox32a4u as x
import time

dev = usb.core.find(idVendor=0x59e3, idProduct=0xf000)

def bigWrite(addr, value):
	dev.ctrl_transfer(0x40|0x80, 0x16, value, addr, 0)

def smallWrite(addr, value):
	dev.ctrl_transfer(0x40|0x80, 0x08, value, addr, 0)

def bigRead(addr):
	res = dev.ctrl_transfer(0x40|0x80, 0x17, 0, addr, 2)
	return (res[1] << 8) | res[0]

def smallRead(addr):
	return dev.ctrl_transfer(0x40|0x80, 0x09, 0, addr, 1)[0]

#dev.set_configuration()
testPattern = (chr(0xf0)+chr(0xff))*32

for i in range(10):
	dev.write(0x02, testPattern, 0, 100)
	dev.read(0x81, 64, 0, 100)
