# LIBDDDVB
The libdddvb provides a userspace library to simplify tuning and
CI use. It detects DVB cards and their capabilities and selects
free frontends depending on a given delivery system.

Please note that the libdddvb library and the ddzap tool are
provided as examples and for testing purposes.


In order to install the libdddvb library you need the libdvben50221.

On an Ubuntu and other Debian based system you can install it like this:

`sudo apt-get install dvb-apps` 

After that you can build the libdddvb:

`git clone https://github.com/DigitalDevices/dddvb.git` 

`cd dddvb/lib/; make` 

`sudo make install` 

If your distribution does not include a dvb-apps package, you can follow the 
instructions at 

https://www.linuxtv.org/wiki/index.php/LinuxTV_dvb-apps

on how to build it yourself.


# DDZAP

You can now use the example program ddzap to see how the library
is used or to test your cards.

A typical usage example would be the tuning of a
sattelite channel with a given LNB configuration.
 
`ddzap -d S2 -p h -f 11494000  -s 22000000  -c ~/dddvb/lib/config/`

where you would use the example configuration file for a unicable LNB
given in the sources. If you leave out the `-c` option a standard universal
LNB is assumed to be connected.

Additionally you can use ddzap to open the /dev/dvb/adaptorX/dvrY device
that corresponds to the tuner that was automatically selected by the library
and pipe it to a media player, e.g.


`ddzap -d S2 -p h -f 11494000  -s 22000000  -c /home/dmocm/ddzapconf/  -o|vlc -` 

