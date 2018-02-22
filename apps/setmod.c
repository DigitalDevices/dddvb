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
  printf("Usage: %s [-d device] [-f base_frequency[Hz]]"
    " [-m modulation[qam_16, qam32, qam_64, qam_128, qam256]] [-p pcr_correction[1/0]]"
    " [-b bitrate[bps]] [-g gain[dB]]\n", argv[0]);
}

static int set_mod_property (int fd, uint32_t cmd, uint32_t data)
{
  struct dtv_property p;
  struct dtv_properties c;
  int ret;

  if( fd < 0 ) return -1;
  p.cmd = cmd;
  c.num = 1;
  c.props = &p;
  p.u.data = data;
  ret = ioctl(fd, FE_SET_PROPERTY, &c);

  if(cmd == MODULATOR_GAIN && ret == -EINVAL) //Expected for modc Gen 1 cards
    return 0;
  if (ret < 0)
  {
    fprintf(stderr, "FE_SET_PROPERTY  %d returned %d\n", cmd, errno);
  }
}

int modulator_set_output_power (int fd, int power)
{
  unsigned int atten = 0;
  unsigned int gain = 0;
  if ( power > POWER_REF )
    gain = (power - POWER_REF) * 8;
  else
    atten = POWER_REF - power;

  if (gain > RF_VGA_GAIN_MAX)
    gain = RF_VGA_GAIN_MAX;
  if (atten > 31)
    atten = 31;

  set_mod_property(fd, MODULATOR_ATTENUATOR,atten);
  usleep(1000); // driver bug

  set_mod_property(fd, MODULATOR_GAIN,gain);
  return 0;
}


int main(int argc, char* argv[])
{
  int fd;
  struct dvb_mod_params mp;
  struct dvb_mod_channel_params mc;
  char *device = NULL;
  uint32_t base_freq = 0;
  uint8_t gain = POWER_REF;
  char *modulation_str = NULL;
  int pcr_correction = -1;
  uint64_t bitrate = 0;
  int opt;

  if (argc != 7)
  {
  }

  while ((opt = getopt(argc, argv, "d:f:m:p:b:g:")) != -1)
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
      case 'g':
        gain = strtoul(optarg, NULL, 10);
        break;
    }
  }

  if (device == NULL)
  {
    usage(argv);
    exit(EXIT_FAILURE);
  }

  char *device = argv[1];
  uint32_t base_freq = strtoul(argv[2], NULL, 10);
  if (base_freq < 114000000 || base_freq > 794000000)
  {
    printf("Invalid frequency \'%i\'\n", base_freq);
    exit(EXIT_FAILURE);
  }
  uint32_t attenuation = strtoul(argv[3], NULL, 10);

  enum fe_modulation modulation;
  char *modulation_str = argv[4];
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
  int pcr_correction = strtoul(argv[5], NULL, 10);
  uint64_t bitrate = strtoul(argv[6], NULL, 10);

  fd = open(device, O_RDONLY);
  if (fd == -1)
  {
    printf("Invalid device \'%s\': %s\n", device, strerror(errno));
    exit(EXIT_FAILURE);
  }

  printf("Setting base_freq=%u modulation=%s, pcr_correction=%d"
    "input_bitrate=%u gain=%udB on %s\n",
    base_freq, modulation_str, pcr_correction, bitrate, gain, device);

  mp.base_frequency = base_freq;
  ioctl(fd, DVB_MOD_SET, &mp);

  modulator_set_output_power(fd, gain);

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
