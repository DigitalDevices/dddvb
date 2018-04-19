#include <stdio.h>
#include <ctype.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <stdint.h>
#include <stdlib.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <pthread.h>
#include <errno.h>

#include <linux/dvb/mod.h>

void usage(char* argv[])
{
  printf("Usage: %s [-d device] [-f base_frequency[Hz]] [-a attenuation[dB]]"
    " [-m modulation[qam_16, qam32, qam_64, qam_128, qam256]] [-p pcr_correction[1/0]]"
    " [-b bitrate[bps]]\n", argv[0]);
}


int main(int argc, char* argv[])
{
  int fd;
  struct dvb_mod_params mp;
  struct dvb_mod_channel_params mc;
  char *device = NULL;
  uint32_t base_freq = 0;
  uint32_t attenuation = 0;
  char *modulation_str = NULL;
  int pcr_correction = -1;
  uint64_t bitrate = 0;
  int opt;

  while ((opt = getopt(argc, argv, "d:f:m:p:b:a:")) != -1)
  {
    switch (opt)
    {
      case 'd':
        device = optarg;
        break;
      case 'f':
        base_freq = strtoul(optarg, NULL, 10);
        break;
      case 'm':
        modulation_str = optarg;
        break;
      case 'p':
        pcr_correction = strtoul(optarg, NULL, 10);
        break;
      case 'b':
        bitrate = strtoul(optarg, NULL, 10);
        break;
      case 'a':
        attenuation = strtoul(optarg, NULL, 10);
        break;
    }
  }

  if (device == NULL)
  {
    usage(argv);
    exit(EXIT_FAILURE);
  }

  if (base_freq < 114000000 || base_freq > 786000000)
  {
    printf("Invalid frequency \'%i\'\n", base_freq);
    exit(EXIT_FAILURE);
  }
  enum fe_modulation modulation;
  if (strcmp(modulation_str, "qam_16") == 0)
  {
    modulation = QAM_16;
  }
  else if (strcmp(modulation_str, "qam_32") == 0)
  {
    modulation = QAM_32;
  }
  else if (strcmp(modulation_str, "qam_64") == 0)
  {
    modulation = QAM_64;
  }
  else if (strcmp(modulation_str, "qam_128") == 0)
  {
    modulation = QAM_128;
  }
  else if (strcmp(modulation_str, "qam_256") == 0)
  {
    modulation = QAM_256;
  }
  else
  {
    printf("Invalid modulation \'%s\'\n", modulation_str);
    exit(EXIT_FAILURE);
  }

  fd = open(device, O_RDONLY);
  if (fd == -1)
  {
    printf("Invalid device \'%s\': %s\n", device, strerror(errno));
    exit(EXIT_FAILURE);
  }

  printf("Setting base_freq=%u attenuation=%u modulation=%s, pcr_correction=%d"
    "input_bitrate=%u on %s\n",
    base_freq, attenuation, modulation_str, pcr_correction, bitrate, device);

  mp.base_frequency = base_freq;
  mp.attenuator = attenuation;
  ioctl(fd, DVB_MOD_SET, &mp);

  mc.modulation = modulation;
  mc.input_bitrate = bitrate << 32;
  mc.pcr_correction = pcr_correction;
  if (ioctl(fd, DVB_MOD_CHANNEL_SET, &mc) != 0)
  {
    printf("Invalid options: %s\n", strerror(errno));
    exit(EXIT_FAILURE);
  }
  close(fd);
}
