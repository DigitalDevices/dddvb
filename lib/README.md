# LIBDDDVB

In order to install the libdddvb library 

https://github.com/DigitalDevices/dddvb

you need dvben50221.


On an Ubuntu system this would look like this:

`sudo apt-get install dvb-apps` 

`git clone https://github.com/DigitalDevices/internal_dddvb.git` 

`cd dddvb/lib/; make` 

`sudo make install` 

# DDZAP

You can now use the example program ddzap for tuning.

A typical usage example call would be:
 

`ddzap -d S2 -p v -f 10847000  -s 23000000  -c ~/dddvb/lib/config/`

where you would use the example configuration file for a unicable LNB
given in the sources. If you leave out the `-c` option a standard universal
LNB is assumed to be connected.

Additionally you can use ddzap to open the /dev/dvb/adaptorX/dvrY device
that corresponds to the tuner that was automatically selected by the library
and pipe it to a media player, e.g.


`ddzap -d S2 -p h -f 11494000  -s 22000000  -c /home/dmocm/ddzapconf/  -o|vlc -` 


