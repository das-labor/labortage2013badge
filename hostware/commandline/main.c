/* Name: set-led.c
 * Project: hid-custom-rq example
 * Author: Christian Starkjohann
 * Creation Date: 2008-04-10
 * Tabsize: 4
 * Copyright: (c) 2008 by OBJECTIVE DEVELOPMENT Software GmbH
 * License: GNU GPL v2 (see License.txt), GNU GPL v3 or proprietary (CommercialLicense.txt)
 * This Revision: $Id: set-led.c 692 2008-11-07 15:07:40Z cs $
 */

/*
General Description:
This is the host-side driver for the custom-class example device. It searches
the USB for the LEDControl device and sends the requests understood by this
device.
This program must be linked with libusb on Unix and libusb-win32 on Windows.
See http://libusb.sourceforge.net/ or http://libusb-win32.sourceforge.net/
respectively.
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <inttypes.h>
#include <getopt.h>
#include <ctype.h>
#include <usb.h>        /* this is libusb */
#include <arpa/inet.h>
#include "hexdump.h"
#include "opendevice.h" /* common code moved to separate module */

#include "../../firmware/requests.h"   /* custom request numbers */
#include "../../firmware/usbconfig.h"  /* device's VID/PID and names */

int safety_question_override=0;
int pad=-1;

char* fname;
usb_dev_handle *handle = NULL;

int8_t hex_to_int(char c) {
    int8_t r = -1;
    if (c >= '0' && c <= '9') {
        r = c - '0';
    } else {
        c |= 'a' ^ 'A';
        if (c >= 'a' && c <= 'f') {
            r = 10 + c - 'a';
        }
    }
    return r;
}

void press_button(char *param){
    usb_control_msg(handle, USB_TYPE_VENDOR | USB_RECIP_DEVICE | USB_ENDPOINT_OUT, CUSTOM_RQ_PRESS_BUTTON, 0, 0, NULL, 0, 5000);
}

void set_dbg(char *hex_string){
	uint8_t buffer[(strlen(hex_string) + 1) / 2];
	size_t i = 0, length = 0;
	int8_t t;

	memset(buffer, 0, (strlen(hex_string) + 1) / 2);

	while (hex_string[i]) {
	    t = hex_to_int(hex_string[i]);
	    if (t == -1){
	        break;
	    }
	    if (i & 1) {
	        buffer[length++] |= t;
	    } else {
	        buffer[length] |= t << 4;
	    }
	}

	usb_control_msg(handle, USB_TYPE_VENDOR | USB_RECIP_DEVICE | USB_ENDPOINT_OUT, CUSTOM_RQ_SET_DBG, 0, 0, (char*)buffer, length, 5000);
}

void get_dbg(char *param){
	uint16_t buffer[256];
	int cnt;
	cnt = usb_control_msg(handle, USB_TYPE_VENDOR | USB_RECIP_DEVICE | USB_ENDPOINT_IN, CUSTOM_RQ_GET_DBG, 0, 0, (char*)buffer, 256, 5000);
	printf("DBG-Buffer:\n");
	hexdump_block(stdout, buffer, 0, cnt, 16);
}

void clr_dbg(char *param){
    usb_control_msg(handle, USB_TYPE_VENDOR | USB_RECIP_DEVICE | USB_ENDPOINT_IN, CUSTOM_RQ_CLR_DBG, 0, 0, NULL, 0, 5000);
}

void set_secret(char *hex_string){
    uint8_t buffer[(strlen(hex_string) + 1) / 2];
    size_t i = 0, length = 0;
    int8_t t;

    memset(buffer, 0, (strlen(hex_string) + 1) / 2);

    while (hex_string[i]) {
        t = hex_to_int(hex_string[i]);
        if (t == -1){
            break;
        }
        if (i & 1) {
            buffer[length++] |= t;
        } else {
            buffer[length] |= t << 4;
        }
    }

    usb_control_msg(handle, USB_TYPE_VENDOR | USB_RECIP_DEVICE | USB_ENDPOINT_OUT, CUSTOM_RQ_SET_SECRET, length * 8, 0, (char*)buffer, length, 5000);
}

void inc_counter(char *param){
    usb_control_msg(handle, USB_TYPE_VENDOR | USB_RECIP_DEVICE | USB_ENDPOINT_IN, CUSTOM_RQ_INC_COUNTER, 0, 0, NULL, 0, 5000);
}

void get_counter(char *param){
    uint32_t counter;
    int cnt;
    cnt = usb_control_msg(handle, USB_TYPE_VENDOR | USB_RECIP_DEVICE | USB_ENDPOINT_IN, CUSTOM_RQ_GET_COUNTER, 0, 0, (char*)&counter, 4, 5000);
    if (cnt == 4) {
        printf("internal counter = %"PRId32"\n", counter);
    } else {
        fprintf(stderr, "Error: reading %d bytes for counter, expecting 4\n", cnt);
    }
}

void reset_counter(char *param){
    usb_control_msg(handle, USB_TYPE_VENDOR | USB_RECIP_DEVICE | USB_ENDPOINT_IN, CUSTOM_RQ_RESET_COUNTER, 0, 0, NULL, 0, 5000);
}

void get_reset_counter (char *param){
    uint8_t counter;
    int cnt;
    cnt = usb_control_msg(handle, USB_TYPE_VENDOR | USB_RECIP_DEVICE | USB_ENDPOINT_IN, CUSTOM_RQ_GET_RESET_COUNTER, 0, 0, (char*)&counter, 1, 5000);
    if (cnt == 1) {
        printf("internal reset counter = %"PRId8"\n", counter);
    } else {
        fprintf(stderr, "Error: reading %d bytes for reset counter, expecting 1\n", cnt);
    }
}

void get_digits(char *param){
    uint8_t digits;
    int cnt;
    cnt = usb_control_msg(handle, USB_TYPE_VENDOR | USB_RECIP_DEVICE | USB_ENDPOINT_IN, CUSTOM_RQ_GET_DIGITS, 0, 0, (char*)&digits, 1, 5000);
    if (cnt == 1) {
        printf("digits = %"PRId8"\n", digits);
    } else {
        fprintf(stderr, "Error: reading %d bytes for reset counter, expecting 1\n", cnt);
    }
}

void set_digits(char *param){
    long d;
    d = strtol(param, NULL, 10);
    if (d > 9 || d < 6) {
        fprintf(stderr, "Error: <digits> must be in range 6, 7, 8 or 9\n");
        return;
    }
    usb_control_msg(handle, USB_TYPE_VENDOR | USB_RECIP_DEVICE | USB_ENDPOINT_IN, CUSTOM_RQ_SET_DIGITS, d, 0, NULL, 0, 5000);
}

void get_token(char *param){
    char token[10];
    int cnt;
    cnt = usb_control_msg(handle, USB_TYPE_VENDOR | USB_RECIP_DEVICE | USB_ENDPOINT_IN, CUSTOM_RQ_GET_TOKEN, 0, 0, token, 9, 5000);
    if (cnt < 9 ) {
        token[cnt] = '\0';
        printf("token = %s\n", token);
    } else {
        fprintf(stderr, "Error: reading %d bytes for token, expecting max. 9\n", cnt);
    }
}

void read_mem(char* param){
	int length=0;
	uint8_t *buffer, *addr;
	int cnt;
	FILE* f=NULL;
	if(fname){
		f = fopen(fname, "wb");
		if(!f){
			fprintf(stderr, "ERROR: could not open %s for writing\n", fname);
			exit(1);
		}
	}
	sscanf(param, "%i:%i", (int*)&addr, &length);
	if(length<=0){
		return;
	}
	buffer = malloc(length);
	if(!buffer){
		if(f)
			fclose(f);
		fprintf(stderr, "ERROR: out of memory\n");
		exit(1);
	}
	cnt = usb_control_msg(handle, USB_TYPE_VENDOR | USB_RECIP_DEVICE | USB_ENDPOINT_IN, CUSTOM_RQ_READ_MEM, (intptr_t)addr, 0, (char*)buffer, length, 5000);
	if(cnt!=length){
		if(f)
			fclose(f);
		fprintf(stderr, "ERROR: received %d bytes from device while expecting %d bytes\n", cnt, length);
		exit(1);
	}
	if(f){
		cnt = fwrite(buffer, 1, length, f);
		fclose(f);
		if(cnt!=length){
			fprintf(stderr, "ERROR: could write only %d bytes out of %d bytes\n", cnt, length);
			exit(1);
		}

	}else{
		hexdump_block(stdout, buffer, addr, length, 8);
	}
}

void write_mem(char* param){
	int length;
	uint8_t *addr, *buffer, *data=NULL;
	int cnt=0;
	FILE* f=NULL;

	if(fname){
		f = fopen(fname, "rb");
		if(!f){
			fprintf(stderr, "ERROR: could not open %s for writing\n", fname);
			exit(1);
		}
	}
	sscanf(param, "%i:%i:%n", (int*)&addr, &length, &cnt);
	data += cnt;
	if(length<=0){
		return;
	}
	buffer = malloc(length);
	if(!buffer){
		if(f)
			fclose(f);
		fprintf(stderr, "ERROR: out of memory\n");
		exit(1);
	}
	memset(buffer, (uint8_t)pad, length);
	if(!data && !f && length==0){
		fprintf(stderr, "ERROR: no data to write\n");
		exit(1);
	}
	if(f){
		cnt = fread(buffer, 1, length, f);
		fclose(f);
		if(cnt!=length && pad==-1){
			fprintf(stderr, "Warning: could ony read %d bytes from file; will only write these bytes", cnt);
		}
	}else if(data){
		char xbuffer[3]= {0, 0, 0};
		uint8_t fill=0;
		unsigned idx=0;
		while(*data && idx<length){
			while(*data && !isxdigit(*data)){
				++data;
			}
			xbuffer[fill++] = *data;
			if(fill==2){
				uint8_t t;
				t = strtoul(xbuffer, NULL, 16);
				buffer[idx++] = t;
				fill = 0;
			}
		}

	}
	cnt = usb_control_msg(handle, USB_TYPE_VENDOR | USB_RECIP_DEVICE | USB_ENDPOINT_OUT, CUSTOM_RQ_WRITE_MEM, (intptr_t)addr, 0, (char*)buffer, length, 5000);
	if(cnt!=length){
		fprintf(stderr, "ERROR: device accepted ony %d bytes out of %d\n", cnt, length);
		exit(1);
	}

}

void read_flash(char* param) {
	int length=0;
	uint8_t *buffer, *addr;
	int cnt;
	FILE* f=NULL;
	if (fname) {
		f = fopen(fname, "wb");
		if (!f) {
			fprintf(stderr, "ERROR: could not open %s for writing\n", fname);
			exit(1);
		}
	}
	sscanf(param, "%i:%i", (int*)&addr, &length);
	if (length <= 0){
		return;
	}
	buffer = malloc(length);
	if (!buffer) {
		if (f)
			fclose(f);
		fprintf(stderr, "ERROR: out of memory\n");
		exit(1);
	}
	cnt = usb_control_msg(handle, USB_TYPE_VENDOR | USB_RECIP_DEVICE | USB_ENDPOINT_IN, CUSTOM_RQ_READ_FLASH, (intptr_t)addr, 0, (char*)buffer, length, 5000);
	if (cnt != length){
		if (f)
			fclose(f);
		fprintf(stderr, "ERROR: received %d bytes from device while expecting %d bytes\n", cnt, length);
		exit(1);
	}
	if (f) {
		cnt = fwrite(buffer, 1, length, f);
		fclose(f);
		if (cnt != length) {
			fprintf(stderr, "ERROR: could write only %d bytes out of %d bytes\n", cnt, length);
			exit(1);
		}

	} else {
		hexdump_block(stdout, buffer, addr, length, 8);
	}
}

void soft_reset(char* param){
	unsigned delay = 0;
	if (param) {
		sscanf(param, "%i", &delay);
	}
	delay &= 0xf;
	usb_control_msg(handle, USB_TYPE_VENDOR | USB_RECIP_DEVICE | USB_ENDPOINT_IN, CUSTOM_RQ_RESET, (int)delay, 0, NULL, 0, 5000);
}

void read_button(char* param){
	uint8_t v;
	usb_control_msg(handle, USB_TYPE_VENDOR | USB_RECIP_DEVICE | USB_ENDPOINT_IN, CUSTOM_RQ_READ_BUTTON, 0, 0, (char*)&v, 1, 5000);
	printf("button is %s\n",v?"on":"off");
}

void wait_for_button(char* param){
	volatile uint8_t v = 0, x = 1;
	if(param){
		printf("DBG: having param: %s\n", param);
		if (!(strcmp(param,"off") && strcmp(param,"0"))) {
			x = 0;
		}
	}
	do{
		usb_control_msg(handle, USB_TYPE_VENDOR | USB_RECIP_DEVICE | USB_ENDPOINT_IN, CUSTOM_RQ_READ_BUTTON, 0, 0, (char*)&v, 1, 5000);
	}while(x!=v);
	printf("button is %s\n",v?"on":"off");
}

void read_temperature(char* param){
	uint16_t v;
	int cnt;
	cnt = usb_control_msg(handle, USB_TYPE_VENDOR | USB_RECIP_DEVICE | USB_ENDPOINT_IN, CUSTOM_RQ_READ_TMPSENS, 0, 0, (char*)&v, 2, 5000);
	if (cnt == 2) {
	    printf("temperature raw value: %hd 0x%hx\n", v, v);
	} else {
        fprintf(stderr, "Error: reading %d bytes for temperature, expecting 2\n", cnt);
	}
}


static struct option long_options[] =
             {
               /* These options don't set a flag.
                  We distinguish them by their indices. */
               {"set-secret",        required_argument, NULL, 's'},
               {"inc-counter",       no_argument,       NULL, 'i'},
               {"get-counter",       no_argument,       NULL, 'c'},
               {"reset-counter",     no_argument,       NULL, 'r'},
               {"get-reset-counter", no_argument,       NULL, 'q'},
               {"get-digits",        no_argument,       NULL, 'D'},
               {"set-digits",        required_argument, NULL, 'd'},
               {"reset",             optional_argument, NULL, 'R'},
               {"get-token",         no_argument,       NULL, 't'},
               {"read-button",       no_argument,       NULL, 'b'},
               {"press-button",      no_argument,       NULL, 'B'},
               {"get-dbg",           no_argument,       NULL, 'x'},
               {"set-dbg",           required_argument, NULL, 'y'},
               {"clr-dbg",           no_argument,       NULL, 'z'},
               {0, 0, 0, 0}
             };

static void usage(char *name)
{
	char *usage_str =
	"usage:\n"
    "    %s <command> <parameter string>\n"
	"  <command> is one of the following\n"
	"    -s --set-secret <secret> .... set secret (<secret> is a byte sequence in hex)\n"
	"    -i --inc-counter ............ increment internal counter\n"
	"    -c --get-counter ............ get current counter value\n"
	"    -r --reset-counter .......... reset internal counter to zero\n"
	"    -q --get-reset-counter....... get the times the counter was reseted\n"
	"    -D --get-digits ............. get the amount of digits per token\n"
	"    -d --set-digits <digits>..... set the amount of digits per token (in range 6..9)\n"
    "    -R --reset[=<delay>] ........ reset the controller with delay (in range 0..9)\n"
    "    -t --get-token .............. get a token\n\n"
	"    -b --read-button ............ read status of button\n"
    "    -B --press-button ........... simulate a button press\n"
	"    -x --get-dbg ................ get content of the debug register\n"
    "    -y --set-dbg <data>.......... set content of the debug register (<data> is a byte squence in hex of max. 8 bytes)\n"
    "    -z --clr-dbg ................ clear the content of the debug register\n"

	" Please note:\n"
	"   If you use optional parameters you have to use two different way to specify the parameter,\n"
	"   depending on if you use short or long options.\n"
	"   Short options: You have to put the parameter directly behind the option letter. Exp: -R6\n"
	"   Long options: You have to seperate the option from the parameter with '='. Exp: --reset=6\n"
	;
	fprintf(stderr, usage_str, name);
}


int main(int argc, char **argv)
{
  const unsigned char rawVid[2] = {USB_CFG_VENDOR_ID}; 
  const unsigned char rawPid[2] = {USB_CFG_DEVICE_ID};
  char vendor[] = {USB_CFG_VENDOR_NAME, 0};
  char product[] = {USB_CFG_DEVICE_NAME, 0};
  int  vid, pid;
  int  c, option_index;
  void(*action_fn)(char*) = NULL;
  char* main_arg = NULL;
  usb_init();
  if(argc < 2){   /* we need at least one argument */
    usage(argv[0]);
    exit(1);
  }
  /* compute VID/PID from usbconfig.h so that there is a central source of information */
    vid = rawVid[1] * 256 + rawVid[0];
    pid = rawPid[1] * 256 + rawPid[0];
    /* The following function is in opendevice.c: */
    if(usbOpenDevice(&handle, vid, vendor, pid, NULL, NULL, NULL, NULL) != 0){
        fprintf(stderr, "Could not find USB device \"%s\" with vid=0x%x pid=0x%x\n", product, vid, pid);
        exit(1);
    }

    for (;;) {
    	c = getopt_long(argc, argv, "s:icrqDd:R::tbBxy:z", long_options, &option_index);
    	if (c == -1) {
    		break;
    	}

    	if (action_fn && strchr("sicrqDdRtbBxyz", c)) {
    		/* action given while already having an action */
    		usage(argv[0]);
    		exit(1);
    	}

    	if(strchr("sdRy", c)){
    		main_arg = optarg;
    	}
/*
    "    -s --set-secret <secret> .... set secret (<secret> is a byte sequence in hex)\n"
    "    -i --inc-counter ............ increment internal counter\n"
    "    -c --get-counter ............ get current counter value\n"
    "    -r --reset-counter .......... reset internal counter to zero\n"
    "    -q --get-reset-counter....... get the times the counter was reseted\n"
    "    -D --get-digits ............. get the amount of digits per token\n"
    "    -d --set-digits <digits>..... set the amount of digits per token (in range 6..9)\n"
    "    -R --reset[=<delay>] ........ reset the controller with delay (in range 0..9)\n"
    "    -t --get-token .............. get a token\n\n"
    "    -b --read-button ............ read status of button\n"
    "    -B --press-button ........... simulate a button press\n"
    "    -x --get-dbg ................ get content of the debug register\n"
    "    -y --set-dbg <data>.......... set content of the debug register (<data> is a byte squence in hex of max. 8 bytes)\n"
    "    -z --clr-dbg ................ clear the content of the debug register\n"
*/
    	switch (c) {
        case 's': action_fn = set_secret; break;
        case 'i': action_fn = inc_counter; break;
        case 'c': action_fn = get_counter; break;
        case 'r': action_fn = reset_counter; break;
        case 'q': action_fn = get_reset_counter; break;
        case 'D': action_fn = get_digits; break;
        case 'd': action_fn = set_digits; break;
        case 'R': action_fn = soft_reset; break;
        case 't': action_fn = get_token; break;
        case 'b': action_fn = read_button; break;
        case 'B': action_fn = press_button; break;
    	case 'y': action_fn = set_dbg; break;
    	case 'x': action_fn = get_dbg; break;
    	case 'z': action_fn = clr_dbg; break;
        default:
    		break;
    	}
    }

    if (!action_fn) {
    	usage(argv[0]);
    	fprintf(stderr, "Error: no action specified\n");
    	return 1;
    } else {
        action_fn(main_arg);
    	usb_close(handle);
    	return 0;
    }

}
