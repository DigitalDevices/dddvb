#ifndef _DDDVB_TOOLS_H_
#define _DDDVB_TOOLS_H_

time_t mtime(time_t *t);
int sendlen(int sock, char *buf, int len);
int sendstring(int sock, char *fmt, ...);

#endif /* _DDDVB_TOOLS_H_ */
