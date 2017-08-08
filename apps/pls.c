/*
 * pls.c: Convert between Gold and Root Codes for DVB-S2 PLS
 *
 * Copyright (C) 2017 Marcus Metzler <mocm@metzlerbros.de>
 *                    Ralph Metzler <rjkm@metzlerbros.de>
 *
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * version 2 only, as published by the Free Software Foundation.
 *
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA
 * 02110-1301, USA
 * Or, point your browser to http://www.gnu.org/copyleft/gpl.html
 */

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

/* According to ETSI EN 302 307 5.5.4 the PLS (Physical Layer
   Scrambling) for DVB-S2 consists of a complex randomization
   sequence which is ultimately derived from two recursively
   defined m-sequences  (=MLS or maximum length sequences)
   x(i) and y(i) of polynomials over GF(2) with m=18
   (thus their length is 2^18 - 1).
   These m-sequences with sequence y starting from y(0) and
   sequence x starting from x(n) are combined to form a set
   of 2^18 - 1 different Gold code sequences.

   This starting number n of sequence x selects which
   of those 2^18 - 1 Gold code sequences to use.
   As a DVB-S2 tuning parameter n is called the scrambling sequence index 
   (cf. ETSI EN 300 468 table 41) or Gold sequence index,
   commonly also just called "Gold code".
   The 18 values of the sequence x starting from x(n)
   (x(n) ... x(n+17)) are also called the "Root code".
   So, Gold and Root codes are not different ways of PLS, they are
   just different ways to select the same sequence start point.
     
   The initial values for x(i), i=0..18 are x(0)=1, x(1)=0, .., x(17)=0 .  
   The polynomial used for the x sequence recursion is 1+x^7+x^18.
   If the lower 18 bits of a variable "uint32_t X" contain x(n) ... x(n+17),
   then we can simply calculate x(n+1) ... x(n+18) by doing:
   X = (((X ^ (X >> 7)) & 1) << 17) | (X >> 1);
  
   So, if X contained the "Root code" corresponding to "Gold code" n,
   it will now contain the "Root code" corresponding to "Gold code" (n+1).
   Note that X=0 and n=2^18 - 1 do not exist (or rather the lattter is the same
   as n = 0) and for n=0 to 2^18 - 2 and X=1 to 2^18 - 1 there is a
   one-to-one correspondence (bijection).

   Note that PLS has nothing to do with encryption for DRM purposes. It is used
   to minimize interference between transponders.


   "Combo code":
   There is no such thing as a combo code. It is the result of a bug in older
   STV090x drivers which resulted in a crazy race condition between a Gold->Root
   conversion in the STV and an ongoing I2C write. 
   Better forget about it and determine the proper Root or Gold code.
      
 */


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
			printf("pls -g gold_code\n");
			printf("or\n");
			printf("pls -r root_code\n");
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
