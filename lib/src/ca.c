#include "libdddvb.h"
#include "dddvb.h"
#include "tools.h"
#include "debug.h"

#include <linux/dvb/dmx.h>
#include <linux/dvb/frontend.h>
#include <linux/dvb/video.h>
#include <linux/dvb/net.h>
#include <linux/dvb/ca.h>

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <stddef.h>

#include <sys/types.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <netdb.h>
#include <net/if.h>
#include <net/ethernet.h>
#include <net/if_arp.h>
#include <netinet/tcp.h>

#define MMI_STATE_CLOSED 0
#define MMI_STATE_OPEN 1
#define MMI_STATE_ENQ 2
#define MMI_STATE_MENU 3


int set_nonblock(int fd)
{
	int fl = fcntl(fd, F_GETFL);
	
	if (fl < 0)
		return fl;
	return fcntl(fd, F_SETFL, fl | O_NONBLOCK);
}

int streamsock(const char *port, int family, struct sockaddr *sadr)
{
	int one=1, sock;
	struct addrinfo *ais, *ai, hints = {
		.ai_flags = AI_PASSIVE, 
		.ai_family = family,
		.ai_socktype = SOCK_STREAM,
		.ai_protocol = 0, .ai_addrlen = 0,
		.ai_addr = NULL, .ai_canonname = NULL, .ai_next = NULL,
	};
	if (getaddrinfo(NULL, port, &hints, &ais) < 0)
		return -1;
	for (ai = ais; ai; ai = ai->ai_next)  {
		sock = socket(ai->ai_family, ai->ai_socktype, ai->ai_protocol);
		if (sock == -1)
			continue;
		if (!setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, &one, sizeof(one)) &&
		    !bind(sock, ai->ai_addr, ai->ai_addrlen)) {
			*sadr = *ai->ai_addr;
			break;
		}
		close(sock);
		sock = -1;
	}
	freeaddrinfo(ais);
	return sock;
}

int unixsock(const char *path)
{
	unlink(path);

	struct sockaddr_un sa;
	size_t hlen = offsetof(struct sockaddr_un, sun_path);
	size_t pathlen = strlen(path);
	if (pathlen > sizeof(sa.sun_path))
		return(-1);
	memset(&sa, 0, hlen);
	sa.sun_family = AF_UNIX;
	if (pathlen > 0)
		memcpy(sa.sun_path, path, pathlen);

	int sock = socket(AF_UNIX, SOCK_STREAM, 0);
	if (sock == -1)
		return(-1);

	if (bind(sock, (struct sockaddr *) &sa, (socklen_t) (hlen + pathlen)) == -1) {
		close(sock);
		return(-1);
	}

	return(sock);
}

static int ai_callback(void *arg, uint8_t slot_id, uint16_t session_number,
		       uint8_t application_type, uint16_t application_manufacturer,
		       uint16_t manufacturer_code, uint8_t menu_string_length,
		       uint8_t *menu_string)
{
	struct dddvb_ca *ca = arg;

	dbgprintf(DEBUG_CA, "Application type: %02x\n", application_type);
	dbgprintf(DEBUG_CA, "Application manufacturer: %04x\n", application_manufacturer);
	dbgprintf(DEBUG_CA, "Manufacturer code: %04x\n", manufacturer_code);
	dbgprintf(DEBUG_CA, "Menu string: %.*s\n", menu_string_length, menu_string);

	return 0;
}

static int ca_info_callback(void *arg, uint8_t slot_id, uint16_t snum, 
			    uint32_t id_count, uint16_t *ids)
{
	struct dddvb_ca *ca = arg;
	uint32_t i;
	
	dbgprintf(DEBUG_CA, "CAM supports the following ca system ids:\n");
	for (i = 0; i < id_count; i++) {
		dbgprintf(DEBUG_CA, "  0x%04x\n", ids[i]);
	}
	ca->resource_ready = 1;
	return 0;
}

#if 0
static int handle_pmt(struct dvbca *ca, uint8_t *buf, int size)
{
	int listmgmt = CA_LIST_MANAGEMENT_ONLY;
        uint8_t capmt[4096];
	struct section *section = section_codec(buf, size);
	struct section_ext *section_ext = section_ext_decode(section, 0);
	struct mpeg_pmt_section *pmt = mpeg_pmt_section_codec(section_ext);

	dbgprintf(DEBUG_CA, "handle pmt\n");
	if (section_ext->version_number == ca->ca_pmt_version &&
		ca->pmt == ca->pmt_old)
		return;
	if (ca->pmt != ca->pmt_old) {
		ca->pmt_old = ca->pmt;
		ca->sentpmt = 0;
	}
	if (ca->resource_ready) {
		ca->data_pmt_version = pmt->head.version_number;
		
		if (ca->sentpmt) {
			listmgmt = CA_LIST_MANAGEMENT_UPDATE;
		//return;
		}
		ca->sentpmt = 1;
		dbgprintf(DEBUG_CA, "set ca_pmt\n");
	
		if ((size = en50221_ca_format_pmt(pmt, capmt, sizeof(capmt), ca->moveca, listmgmt,
						  CA_PMT_CMD_ID_OK_DESCRAMBLING)) < 0) {
			dbgprintf(DEBUG_CA, "Failed to format PMT\n");
			return -1;
		}
		if (en50221_app_ca_pmt(ca->stdcam->ca_resource, ca->stdcam->ca_session_number, capmt, size)) {
			dbgprintf(DEBUG_CA, "Failed to send PMT\n");
			return -1;
		}
	}
	
}
#endif


#if 0
static void handle_tdt(struct dddvb_ca *ca)
{
	struct section *section;
	struct dvb_tdt_section *tdt;
        uint8_t sec[4096];
	time_t dvb_time;
	int len;
	
	if (ca->stdcam == NULL)
		return;
	if (ca->stdcam->dvbtime == NULL)
		return;
	len = getsec(ca->input, 0x14, 0, 0x70, sec); 
	if (len < 0)
		return;
	dbgprintf(DEBUG_CA, "got tdt\n");

	section = section_codec(sec, len);
	if (section == NULL)
		return;
	tdt = dvb_tdt_section_codec(section);
	if (tdt == NULL)
		return;
	dvb_time = dvbdate_to_unixtime(tdt->utc_time);

	dbgprintf(DEBUG_CA, "set dvbtime\n");
	if (ca->stdcam->dvbtime)
		ca->stdcam->dvbtime(ca->stdcam, dvb_time);
}
#endif

static int handle_pmts(struct dddvb_ca *ca)
{
	int listmgmt = CA_LIST_MANAGEMENT_ONLY;
        uint8_t sec[4096], capmt[4096];
	struct section *section;
	struct section_ext *section_ext;
	struct mpeg_pmt_section *pmt;
	int i, size, num, len;

	if (!ca->resource_ready)
		return 0;
	dbgprintf(DEBUG_CA, "handle pmts\n");
	for (i = num = 0; i < MAX_PMT; i++) 
		if (ca->pmt[i])
			num++;
	for (i = 0; i < num; i++) {
		len = -1;//getsec(ca->input, ca->pmt[i] & 0xffff, ca->pmt[i] >> 16, 2, sec); 
		if (len < 0)
			continue;
		section = section_codec(sec, len);
		section_ext = section_ext_decode(section, 0);
		pmt = mpeg_pmt_section_codec(section_ext);

		ca->ca_pmt_version[i] = section_ext->version_number;
		if (ca->sentpmt) {
			//return 0;
			listmgmt = CA_LIST_MANAGEMENT_UPDATE;
		} else {
			listmgmt = CA_LIST_MANAGEMENT_ONLY;
			if (num > 1) {
				listmgmt = CA_LIST_MANAGEMENT_MORE;
				if (i == 0)
					listmgmt = CA_LIST_MANAGEMENT_FIRST;
				if (i == num - 1)
					listmgmt = CA_LIST_MANAGEMENT_LAST;
			}
		}
		dbgprintf(DEBUG_CA, "set ca_pmt\n");
	
		if ((size = en50221_ca_format_pmt(pmt, capmt, sizeof(capmt), ca->moveca, listmgmt,
						  CA_PMT_CMD_ID_OK_DESCRAMBLING)) < 0) {
			dbgprintf(DEBUG_CA, "Failed to format PMT\n");
			return -1;
		}
		//dump(capmt, size);
		if (en50221_app_ca_pmt(ca->stdcam->ca_resource, ca->stdcam->ca_session_number, capmt, size)) {
			dbgprintf(DEBUG_CA, "Failed to send PMT\n");
			return -1;
		}
	}
	if (num)
		ca->sentpmt = 1;
	return 0;
}

void dump(FILE *fp, uint8_t *b, int l)
{
	int i, j;
	
	for (j = 0; j < l; j += 16, b += 16) { 
		for (i = 0; i < 16; i++)
			if (i + j < l)
				fprintf(fp, "%02x ", b[i]);
			else
				fprintf(fp, "   ");
		fprintf(fp, " | ");
		for (i = 0; i < 16; i++)
			if (i + j < l)
				fprintf(fp, "%c", (b[i] > 31 && b[i] < 127) ? b[i] : '.');
		fprintf(fp, "\n");
	}
}

static int set_pmts(struct dddvb_ca *ca, uint8_t **pmts)
{
	int listmgmt = CA_LIST_MANAGEMENT_ONLY;
        uint8_t sec[4096], capmt[4096];
	struct section *section;
	struct section_ext *section_ext;
	struct mpeg_pmt_section *pmt;
	int i, size, num, len;

	if (!ca->resource_ready)
		return -EBUSY;
	for (i = 0; i < MAX_PMT; i++) 
		if (!pmts[i])
		    break;
	num = i;
	dbgprintf(DEBUG_CA, "handle %d pmts\n", num);
	for (i = 0; i < num; i++) {
		memcpy(sec, pmts[i], 3);
		len = ((sec[1] & 0x0f) << 8) | sec[2];
		len += 3;
		memcpy(sec, pmts[i], len);
		//dump(stderr, sec, len);
		section = section_codec(sec, len);
		if (!section) {
			dbgprintf(DEBUG_CA, "section_codec failed\n");;
			continue;
		}
		section_ext = section_ext_decode(section, 0);
		if (!section_ext) {
			dbgprintf(DEBUG_CA, "section_ext_decode failed\n");;
			continue;
		}
		pmt = mpeg_pmt_section_codec(section_ext);
		//pmt = (struct mpeg_pmt_section *) section_ext;
		if (!pmt) {
			dbgprintf(DEBUG_CA, "mpeg_pmt_section_codec failed\n");;
			continue;
		}
		
		dbgprintf(DEBUG_CA, "PMT = %p  section=%p\n", pmt, section);
		
		ca->ca_pmt_version[i] = section_ext->version_number;
		if (ca->sentpmt) {
			//return 0;
			listmgmt = CA_LIST_MANAGEMENT_UPDATE;
		} else {
			listmgmt = CA_LIST_MANAGEMENT_ONLY;
			if (num > 1) {
				listmgmt = CA_LIST_MANAGEMENT_MORE;
				if (i == 0)
					listmgmt = CA_LIST_MANAGEMENT_FIRST;
				if (i == num - 1)
					listmgmt = CA_LIST_MANAGEMENT_LAST;
			}
		}
		//dump(stderr, (uint8_t *) pmt, len);
		dbgprintf(DEBUG_CA, "format ca_pmt %p length %u\n", pmt, len);

		if ((size = en50221_ca_format_pmt(pmt, capmt, sizeof(capmt), ca->moveca, listmgmt,
						  CA_PMT_CMD_ID_OK_DESCRAMBLING)) < 0) {
			dbgprintf(DEBUG_CA, "Failed to format PMT\n");
			return -1;
		}
		//dump(capmt, size);
		dbgprintf(DEBUG_CA, "set ca_pmt\n");
		if (en50221_app_ca_pmt(ca->stdcam->ca_resource, ca->stdcam->ca_session_number, capmt, size)) {
			dbgprintf(DEBUG_CA, "Failed to send PMT\n");
			return -1;
		}
	}
	if (num)
		ca->sentpmt = 1;
	return 0;
}

static void proc_csock_msg(struct dddvb_ca *ca, uint8_t *buf, int len)
{
	if (*buf == '\r') {
		return;
	} else if (*buf == '\n') {
		switch(ca->mmi_state) {
		case MMI_STATE_CLOSED:
		case MMI_STATE_OPEN:
			if ((ca->mmi_bufp == 0) && (ca->resource_ready)) {
				en50221_app_ai_entermenu(ca->stdcam->ai_resource, 
							 ca->stdcam->ai_session_number);
			}
			break;
			
		case MMI_STATE_ENQ:
			if (ca->mmi_bufp == 0) {
				en50221_app_mmi_answ(ca->stdcam->mmi_resource, 
						     ca->stdcam->mmi_session_number,
						     MMI_ANSW_ID_CANCEL, NULL, 0);
			} else {
				en50221_app_mmi_answ(ca->stdcam->mmi_resource, 
						     ca->stdcam->mmi_session_number,
						     MMI_ANSW_ID_ANSWER, 
						     ca->mmi_buf, ca->mmi_bufp);
			}
			ca->mmi_state = MMI_STATE_OPEN;
			break;
			
		case MMI_STATE_MENU:
			ca->mmi_buf[ca->mmi_bufp] = 0;
			en50221_app_mmi_menu_answ(ca->stdcam->mmi_resource, 
						  ca->stdcam->mmi_session_number,
						  atoi(ca->mmi_buf));
			ca->mmi_state = MMI_STATE_OPEN;
			break;
		}
		ca->mmi_bufp = 0;
	} else {
		if (ca->mmi_bufp < (sizeof(ca->mmi_buf) - 1)) {
			ca->mmi_buf[ca->mmi_bufp++] = *buf;
		}
	}
}

static int proc_csock(struct dddvb_ca *ca)
{
	uint8_t buf[1024];
	int len, i, res;
	
	if (ca->stdcam == NULL)
		return -1;
	while ((len = recv(ca->sock, buf, 1, 0)) >= 0) {
		if (len == 0) 
			goto release;
		if (len < 0) {
			if (errno != EAGAIN) 
				goto release;
			return 0;
		}
		proc_csock_msg(ca, buf, len);
	}
	return 0;
release:
	close(ca->sock);
	ca->sock = -1;
	return -1;
}


static void handle_ci(struct dddvb_ca *ca)
{
	uint8_t sec[4096];
	uint32_t pmt_count, tdt_count;
	int len;
	int sock, i;
	struct sockaddr sadr;
	char port[6], path[32];

	if (ca->dd->cam_family == 1) {
		snprintf(port, sizeof(port), "%u", (uint16_t) (8888 + ca->nr));
		sock = streamsock(port, AF_INET, &sadr);
	} else if (ca->dd->cam_family == 2) {
		snprintf(path, sizeof(path), "/var/run/resiplayer/cam%u", ca->nr);
		sock = unixsock(path);
	} else {
		sock = -1;
	}
	if (listen(sock, 4) < 0) {
		dbgprintf(DEBUG_CA, "listen error");
		return;
	}
	ca->sock = -1;
	
	while (1) {//!ca->exit) {
		struct timeval timeout;
		uint32_t count = 0;
		int num;
		int mfd;
		fd_set fds;
		
		timeout.tv_sec = 0;
		timeout.tv_usec = 200000;
		FD_ZERO(&fds);
		if (ca->sock < 0) { 
			FD_SET(sock, &fds);
			num = select(sock + 1, &fds, NULL, NULL, &timeout);
		} else {
			FD_SET(ca->sock, &fds);
			num = select(ca->sock + 1, &fds, NULL, NULL, &timeout);
		}
		if (num > 0) {
			if (ca->sock < 0) {
				if (FD_ISSET(sock, &fds)) {
					socklen_t len;
					struct sockaddr cadr;
					
					ca->sock = accept(sock, &cadr, &len);
					if (ca->sock >= 0) {
						set_nonblock(ca->sock);
					}
				}
			} else {
				if (FD_ISSET(ca->sock, &fds)) {
					proc_csock(ca);
				}
			}
		}
		
		pthread_mutex_lock(&ca->mutex);
		if (!ca->state) {
			pthread_mutex_unlock(&ca->mutex);
			continue;
		}
		if (ca->setpmt) {
			dbgprintf(DEBUG_CA, "got new PMT %08x\n", ca->pmt_new); 
			memcpy(ca->pmt, ca->pmt_new, sizeof(ca->pmt));
			memset(ca->pmt_old, 0, sizeof(ca->pmt_old));
			for (i = 0; i < MAX_PMT; i++)
				ca->ca_pmt_version[i] = -1;
			ca->sentpmt = 0;
			ca->setpmt = 0;
			pmt_count = 0;
			tdt_count = 0;
		}
		pthread_mutex_unlock(&ca->mutex);

		/*
		  if (!ca->sentpmt)
		    handle_pmts(ca);
		else {
			pmt_count++;
			if (pmt_count == 10) {
				//handle_pmts(ca);
				pmt_count = 0;
			}
		}
		*/
		tdt_count++;
		if (tdt_count == 10) {
			//handle_tdt(ca);
			tdt_count = 0;
		}
	}
}


int set_pmt(struct dddvb_ca *ca, uint32_t *pmt)
{
	dbgprintf(DEBUG_CA, "set_pmt %08x %08x %08x\n", pmt[0], pmt[1], pmt[2]);
	pthread_mutex_lock(&ca->mutex);
	ca->setpmt = 1;
	memcpy(ca->pmt_new, pmt, sizeof(ca->pmt_new));
	pthread_mutex_unlock(&ca->mutex);
	return 0;
}


static void ci_poll(struct dddvb_ca *ca)
{
	while (!ca->dd->exit) {
		ca->stdcam->poll(ca->stdcam);
		
	}
}


static int mmi_close_callback(void *arg, uint8_t slot_id, uint16_t snum,
			      uint8_t cmd_id, uint8_t delay)
{
	struct dddvb_ca *ca = arg;
	
	ca->mmi_state = MMI_STATE_CLOSED;
	if (ca->dd->cam_proto == 2)
		sendstringx(ca->sock, "CLOSE", 0, NULL);
	return 0;
}

static int mmi_display_control_callback(void *arg, uint8_t slot_id, uint16_t snum,
					uint8_t cmd_id, uint8_t mmi_mode)
{
	struct dddvb_ca *ca = arg;
	struct en50221_app_mmi_display_reply_details reply;

	if (cmd_id != MMI_DISPLAY_CONTROL_CMD_ID_SET_MMI_MODE) {
		en50221_app_mmi_display_reply(ca->stdcam->mmi_resource, snum,
					      MMI_DISPLAY_REPLY_ID_UNKNOWN_CMD_ID, &reply);
		return 0;
	}

	// we only support high level mode
	if (mmi_mode != MMI_MODE_HIGH_LEVEL) {
		en50221_app_mmi_display_reply(ca->stdcam->mmi_resource, snum,
					      MMI_DISPLAY_REPLY_ID_UNKNOWN_MMI_MODE, &reply);
		return 0;
	}

	reply.u.mode_ack.mmi_mode = mmi_mode;
	en50221_app_mmi_display_reply(ca->stdcam->mmi_resource, snum,
				      MMI_DISPLAY_REPLY_ID_MMI_MODE_ACK, &reply);
	ca->mmi_state = MMI_STATE_OPEN;
	if (ca->dd->cam_proto == 2)
		sendstringx(ca->sock, "OPEN", 0, NULL);
	return 0;
}

static int mmi_enq_callback(void *arg, uint8_t slot_id, uint16_t snum,
			    uint8_t blind_answer, uint8_t expected_answer_length,
			    uint8_t *text, uint32_t text_size)
{
	struct dddvb_ca *ca = arg;
	
	if (ca->sock >= 0) {
		if (ca->dd->cam_proto == 1)
			sendstring(ca->sock, "%.*s: ", text_size, text);
		if (ca->dd->cam_proto == 2)
			sendstringx(ca->sock, "ENQ", text_size, text);
	}
	//mmi_enq_blind = blind_answer;
	//mmi_enq_length = expected_answer_length;
	ca->mmi_state = MMI_STATE_ENQ;
	return 0;
}

static int mmi_menu_callback(void *arg, uint8_t slot_id, uint16_t snum,
			     struct en50221_app_mmi_text *title,
			     struct en50221_app_mmi_text *sub_title,
			     struct en50221_app_mmi_text *bottom,
			     uint32_t item_count, struct en50221_app_mmi_text *items,
			     uint32_t item_raw_length, uint8_t *items_raw)
{
	uint32_t i;
	struct dddvb_ca *ca = arg;

	if ((ca->sock >= 0) && (ca->dd->cam_proto == 1)) {
		if (title->text_length) 
			sendstring(ca->sock, "%.*s\n", title->text_length, title->text);
		if (sub_title->text_length) 
			sendstring(ca->sock, "%.*s\n", sub_title->text_length, sub_title->text);
		for (i = 0; i < item_count; i++) 
			sendstring(ca->sock, "%i. %.*s\n", i + 1, items[i].text_length, items[i].text);
		if (bottom->text_length) 
			sendstring(ca->sock, "%.*s\n", bottom->text_length, bottom->text);
	}
	if (ca->dd->cam_proto == 2) {
		sendstringx(ca->sock, "MENU", title->text_length, title->text);
		sendstringx(ca->sock, "MSUB", sub_title->text_length, sub_title->text);
		for (i = 0; i < item_count; i++) 
			sendstringx(ca->sock, "ITEM", items[i].text_length, items[i].text);
		sendstringx(ca->sock, "MEND", bottom->text_length, bottom->text);
	}
	ca->mmi_state = MMI_STATE_MENU;
	return 0;
}


static int init_ca_stack(struct dddvb_ca *ca)
{	
	ca->tl = en50221_tl_create(1, 16);
	if (ca->tl == NULL) {
		dbgprintf(DEBUG_CA, "Failed to create transport layer\n");
		return -1;
	}
	ca->sl = en50221_sl_create(ca->tl, 16);
	if (ca->sl == NULL) {
		dbgprintf(DEBUG_CA, "Failed to create session layer\n");
		en50221_tl_destroy(ca->tl);
		return -1;
	}

	ca->stdcam = en50221_stdcam_llci_create(ca->fd, 0, ca->tl, ca->sl);
	if (!ca->stdcam) {
		dbgprintf(DEBUG_CA, "Failed to create stdcam\n");
		en50221_sl_destroy(ca->sl);
		en50221_tl_destroy(ca->tl);
		return -1;
	}
	if (ca->stdcam->ai_resource) {
		en50221_app_ai_register_callback(ca->stdcam->ai_resource, ai_callback, ca);
	}
	if (ca->stdcam->ca_resource) {
		en50221_app_ca_register_info_callback(ca->stdcam->ca_resource, ca_info_callback, ca);
	}
	if (ca->stdcam->mmi_resource) {
		en50221_app_mmi_register_close_callback(ca->stdcam->mmi_resource, mmi_close_callback, ca);
		en50221_app_mmi_register_display_control_callback(ca->stdcam->mmi_resource, 
								  mmi_display_control_callback, ca);
		en50221_app_mmi_register_enq_callback(ca->stdcam->mmi_resource, mmi_enq_callback, ca);
		en50221_app_mmi_register_menu_callback(ca->stdcam->mmi_resource, mmi_menu_callback, ca);
		en50221_app_mmi_register_list_callback(ca->stdcam->mmi_resource, mmi_menu_callback, ca);
	} else {
		dbgprintf(DEBUG_CA,
			  "CAM Menus are not supported by this interface hardware\n");
	}
	return 0;
}

void cam_reset(int fd)
{
        ca_slot_info_t info;

        info.num = 0;
        if (ioctl(fd, CA_GET_SLOT_INFO, &info))
                return;
        if (info.flags & CA_CI_MODULE_READY)
		return;
        if (!(info.flags & CA_CI_MODULE_PRESENT))
		return;
	ioctl(fd, CA_RESET);
}

static int init_ca(struct dddvb *dd, int a, int f, int fd)
{
	struct dddvb_ca *ca;
	char fname[80];

	if (dd->dvbca_num >= DDDVB_MAX_DVB_CA)
		return -1;
	ca = &dd->dvbca[dd->dvbca_num];
	ca->dd = dd;
	ca->anum = a;
	ca->fnum = f;
	ca->nr = dd->dvbca_num + 1;
	ca->fd = fd;
	pthread_mutex_init(&ca->mutex, 0);

	//cam_reset(fd);
	
	init_ca_stack(ca);

	pthread_create(&ca->poll_pt, NULL, (void *) ci_poll, ca); 
	pthread_create(&ca->pt, NULL, (void *) handle_ci, ca); 

	sprintf(fname, "/dev/dvb/adapter%d/ci%d", a, f); 
	ca->ci_wfd = open(fname, O_WRONLY);
	ca->ci_rfd = open(fname, O_RDONLY | O_NONBLOCK);
	dd->dvbca_num++;
	return 0;
}

int dddvb_ca_write(struct dddvb *dd, uint32_t nr, uint8_t *buf, uint32_t len)
{
	struct dddvb_ca *ca = &dd->dvbca[nr];

	return write(ca->ci_wfd, buf, len);
}

int dddvb_ca_read(struct dddvb *dd, uint32_t nr, uint8_t *buf, uint32_t len)
{
	struct dddvb_ca *ca = &dd->dvbca[nr];

	return read(ca->ci_rfd, buf, len);
}

int dddvb_ca_set_pmts(struct dddvb *dd, uint32_t nr, uint8_t **pmts)
{
	struct dddvb_ca *ca = &dd->dvbca[nr];

	dbgprintf(DEBUG_CA, "ca%u.%u.%u:ca_set_pmt\n", ca->nr,ca->anum,ca->fnum);
	return set_pmts(ca, pmts);
}


int scan_dvbca(struct dddvb *dd)
{
	int a, f, fd;
	char fname[80];

	for (a = 0; a < 16; a++) {
		for (f = 0; f < 16; f++) {
			sprintf(fname, "/dev/dvb/adapter%d/ca%d", a, f); 
			fd = open(fname, O_RDWR);
			if (fd >= 0) {
				init_ca(dd, a, f, fd);
				//close(fd);
			}
		}
	}
	dbgprintf(DEBUG_CA, "Found %d CA interfaces\n", dd->dvbca_num);
}

