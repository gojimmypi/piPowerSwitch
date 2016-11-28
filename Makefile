SHELL = /bin/sh
.SUFFIXES:
.SUFFIXES: .c .o

piPowerSwitch : piPowerSwitch.c
	gcc -o piPowerSwitch piPowerSwitch.c

