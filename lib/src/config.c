/*  
    (C) 2012-17 Digital Devices GmbH. 

    This file is part of the libdddvb.

    Libdddvb is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    Octoserve is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with octoserve.  If not, see <http://www.gnu.org/licenses/>.
*/

#include "dddvb.h"

#include <string.h>

int parse_config(struct dddvb *dd, char *name, char *sec,
		 void (*cb)(struct dddvb *, char *, char *) )
{
	char line[256], csec[80], par[80], val[80], *p;
	FILE *f;
	char fname[90];
	size_t name_len, config_len;

	name_len = strlen(name);
	config_len = strlen(dd->config);
	
	if (name_len + config_len > sizeof(fname) - 1)
		return -1;
	memcpy(fname, dd->config, config_len);
	if (name_len)
		memcpy(fname + config_len, name, name_len);
	else
		memcpy(fname + config_len, "dddvb.conf", 11);
	
	if ((f = fopen (fname, "r")) == NULL) {
		printf("config fiile %s not found\n", fname);
		return -1;
	}
	
	while ((p = fgets(line, sizeof(line), f))) {
		if (*p == '\r' || *p == '\n' || *p == '#')
			continue;
		if (*p == '[') {
			if ((p = strtok(line + 1, "]")) == NULL)
				continue;
			strncpy(csec, p, sizeof(csec));
			//printf("current section %s\n", csec);
			if (!strcmp(sec, csec) && cb)
				cb(dd, NULL, NULL);
			continue;
		}
		if (!(p = strtok(line, "=")))
			continue;
		strncpy(par, p, sizeof(par));
		if (!(p = strtok(NULL, "=")))
			continue;
		strncpy (val, p, sizeof(val));
		//printf("%s=%s\n", par, val);
		if (!strcmp(sec, csec) && cb)
			cb(dd, par, val);
	}
	if (!strcmp(sec, csec) && cb)
		cb(dd, NULL, NULL);
	fclose(f);
	return 0;
}
