#define DDB_MAGIC 'd'


struct ddb_flashio {
	__u8 *write_buf;
	__u32 write_len;
	__u8 *read_buf;
	__u32 read_len;
	__u32 link;
};

struct ddb_gpio {
	__u32 mask;
	__u32 data;
};

struct ddb_id {
	__u16 vendor;
	__u16 device;
	__u16 subvendor;
	__u16 subdevice;
	__u32 hw;
	__u32 regmap;
};

struct ddb_reg {
	__u32 reg;
	__u32 val;
};

struct ddb_mem {
	__u32  off;
	__u8  *buf;
	__u32  len;
};

struct ddb_mdio {
	__u8   adr;
	__u8   reg;
	__u16  val;
};

struct ddb_i2c_msg {
	__u8   bus;
	__u8   adr;
	__u8  *hdr;
	__u32  hlen;
	__u8  *msg;
	__u32  mlen;
};

#define IOCTL_DDB_FLASHIO    _IOWR(DDB_MAGIC, 0x00, struct ddb_flashio)
#define IOCTL_DDB_GPIO_IN    _IOWR(DDB_MAGIC, 0x01, struct ddb_gpio)
#define IOCTL_DDB_GPIO_OUT   _IOWR(DDB_MAGIC, 0x02, struct ddb_gpio)
#define IOCTL_DDB_ID         _IOR(DDB_MAGIC, 0x03, struct ddb_id)
#define IOCTL_DDB_READ_REG   _IOWR(DDB_MAGIC, 0x04, struct ddb_reg)
#define IOCTL_DDB_WRITE_REG  _IOW(DDB_MAGIC, 0x05, struct ddb_reg)
#define IOCTL_DDB_READ_MEM   _IOWR(DDB_MAGIC, 0x06, struct ddb_mem)
#define IOCTL_DDB_WRITE_MEM  _IOR(DDB_MAGIC, 0x07, struct ddb_mem)
#define IOCTL_DDB_READ_MDIO  _IOWR(DDB_MAGIC, 0x08, struct ddb_mdio)
#define IOCTL_DDB_WRITE_MDIO _IOR(DDB_MAGIC, 0x09, struct ddb_mdio)
#define IOCTL_DDB_READ_I2C   _IOWR(DDB_MAGIC, 0x0a, struct ddb_i2c_msg)
#define IOCTL_DDB_WRITE_I2C  _IOR(DDB_MAGIC, 0x0b, struct ddb_i2c_msg)

enum {
	UNKNOWN_FLASH = 0,
	ATMEL_AT45DB642D = 1,
	SSTI_SST25VF016B = 2,
	SSTI_SST25VF032B = 3,
	SSTI_SST25VF064C = 4,
	SPANSION_S25FL116K = 5,
	SPANSION_S25FL132K = 6,
	SPANSION_S25FL164K = 7,
	WINBOND_W25Q16JV = 8,
	WINBOND_W25Q32JV = 9,
	WINBOND_W25Q64JV = 10,
	WINBOND_W25Q128JV = 11,
};

struct flash_info {
	uint8_t id[3];
	uint32_t type;
	uint32_t ssize;
	uint32_t fsize;
	char *name;
};

struct ddflash {
	int fd;
	uint32_t link;
	char *fname;
	
	struct ddb_id id;
	uint32_t version;

	char    *flash_name;
	uint32_t flash_type;
	uint32_t sector_size;
	uint32_t size;

	uint32_t bufsize;
	uint32_t block_erase;

	uint8_t *buffer;
};

