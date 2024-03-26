# DDBridge Driver

Device driver for all Digital Devices DVB demodulator and modulator cards.

### Patches
We can only accept patches which don't break compilation for older kernels (as far back as 2.6.37).

Due to this and other changes not approved by us the kernel version of the ddbridge driver might contain
incompatiblities to this driver package.

For installation instructions see:

http://support.digital-devices.eu/index.php?article=152


To compile against the dvb-core of a current kernel compile with KERNEL_DVB_CORE=y:

make KERNEL_DVB_CORE=y install

This will only work with current mainline kernels.

Some features will also not work correctly with the mainline kernel dvb-core:

- some devices will have fewer delivery systems enumerated
  if one you need is missing you will have to fix it yourself

- the DTV_INPUT property will not work

- local bugfixes in dvb-core will be missing

- Some device names will be different because they do not exist in the kernel

Also, do not forget to delete old dvb-core modules from e.g. /lib/modules/x.y.z-amd64/updates/ ! 
