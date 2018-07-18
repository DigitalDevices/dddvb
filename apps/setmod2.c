#include <errno.h>
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

#include <linux/dvb/mod.h>

static int set_property(int fd, uint32_t cmd, uint32_t data)
{
  struct dtv_property p;
  struct dtv_properties c;
  int ret;

  p.cmd = cmd;
  c.num = 1;
  c.props = &p;
  p.u.data = data;
  ret = ioctl(fd, FE_SET_PROPERTY, &c);
  if (ret < 0) {
    fprintf(stderr, "FE_SET_PROPERTY returned %d\n", errno);
    return -1;
  }
  return 0;
}

static void set_output_power (int fd, int power)
{
  unsigned int attenuation = 0;
  unsigned int gain = 0;

  if (power > POWER_REF) {
    gain = (power - POWER_REF) * 8;
  }
  else {
    attenuation = POWER_REF - power;
  }

  if (gain > RF_VGA_GAIN_MAX) {
    gain = RF_VGA_GAIN_MAX;
  }
  if (attenuation > 31) {
    attenuation = 31;
  }

  set_property(fd, MODULATOR_ATTENUATOR, attenuation);
  usleep(1000); // driver bug

  set_property(fd, MODULATOR_GAIN, gain);
}

static void usage(char* argv[])
{
  printf("Usage: %s [-d device] [-f frequency[Hz]] [-P power[dB]]"
    " [-m modulation[qam_16, qam32, qam_64, qam_128, qam256]] [-p pcr_correction[1/0]]"
    " [-b bitrate[bps]]\n", argv[0]);
}

int main(int argc, char* argv[])
{
  int fd;

  char *device = NULL;
  uint32_t freq = 0;
  uint8_t power = POWER_REF;
  char *modulation_str = NULL;
  int pcr_correction = -1;
  uint64_t bitrate = 0;

  int opt;

  while ((opt = getopt(argc, argv, "d:f:m:p:b:P:")) != -1)
  {
    switch (opt)
    {
      case 'd':
        device = optarg;
        break;
      case 'f':
        freq = strtoul(optarg, NULL, 10);
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
      case 'P':
        power = strtoul(optarg, NULL, 10);
        break;
    }
  }

  if (device == NULL)
  {
    usage(argv);
    exit(EXIT_FAILURE);
  }

  if (freq < 114000000 || freq > 858000000)
  {
    printf("Invalid frequency \'%i\'\n", freq);
    exit(EXIT_FAILURE);
  }

  fd = open(device, O_RDONLY);
  if (fd == -1)
  {
    printf("Invalid device \'%s\': %s\n", device, strerror(errno));
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

  printf("Setting freq=%u power=%u modulation=%s, pcr_correction=%d "
    "input_bitrate=%u on %s\n",
    freq, power, modulation_str, pcr_correction, bitrate, device);

  set_property(fd, MODULATOR_FREQUENCY, freq);
  set_property(fd, MODULATOR_MODULATION, modulation);
  set_property(fd, MODULATOR_SYMBOL_RATE, 6900000);
  set_property(fd, MODULATOR_PCR_MODE, pcr_correction);
  set_output_power(fd, power);
  set_property(fd, MODULATOR_INPUT_BITRATE, bitrate << 32);

  close(fd);
}
