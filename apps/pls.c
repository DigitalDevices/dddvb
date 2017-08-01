#include <stdio.h>
#include <ctype.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <stdint.h>
#include <stdlib.h>
#include <getopt.h>
#include <stdio.h>
#include <stdint.h>

static uint32_t gold2root(uint32_t gold)
{
	uint32_t x, g;
	
	for (g = 0, x = 1; g < gold; g++)
		x = (((x ^ (x >> 7)) & 1) << 17) | (x >> 1);
	return x;
}

static uint32_t root2gold(uint32_t root)
{
	uint32_t x, g;
	
	for (g = 0, x = 1; g < 0x3ffff; g++)  {
		if (root == x)
			return g;
		x = (((x ^ (x >> 7)) & 1) << 17) | (x >> 1);
	}
	return 0xffffffff;
}

int main(int argc, char **argv)
{
	uint32_t gold = 0xffffffff, root = 0xffffffff;
	
	while (1) {
                int option_index = 0;
		int c;
                static struct option long_options[] = {
			{"gold", required_argument, 0, 'g'},
			{"root", required_argument, 0, 'r'},
			{"help", no_argument , 0, 'h'},
			{0, 0, 0, 0}
		};
                c = getopt_long(argc, argv, 
				"r:g:h",
				long_options, &option_index);
		if (c==-1)
 			break;
		
		switch (c) {
		case 'g':
			gold = strtoul(optarg, NULL, 0);
			break;
		case 'r':
			root = strtoul(optarg, NULL, 0);
			break;
		case 'h':
			printf("cit -a<adapter> -d<device>\n");
			exit(-1);
		default:
			break;
			
		}
	}
	if (optind < argc) {
		printf("Warning: unused arguments\n");
	}
	if (gold != 0xffffffff && root != 0xffffffff) {
		printf("Only specify root or gold code\n");
		exit(-1);
	};
	if (gold != 0xffffffff) {
		if (gold < 0x3ffff) {
			root = gold2root(gold);
			printf("gold = %llu (0x%05x)   root = %llu (0x%05x)\n",
			       gold, gold, root, root);
		} else
			printf("Invalid gold code specified.\n");
		exit(0);
	} 
	if (root != 0xffffffff) {
		if (root > 0 && root < 0x40000)
			gold = root2gold(root);
		if (gold != 0xffffffff)
			printf("gold = %llu (0x%05x)   root = %llu (0x%05x)\n",
			       gold, gold, root, root);
		else
			printf("Invalid root code specified.\n");
		exit(0);
	}
	printf("Specify either root or gold code with -r or -g.\n");
}
