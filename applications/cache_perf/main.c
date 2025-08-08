
#include "msg.h"
#include "shell.h"
#include "net/gnrc/netif.h"
#include "net/gnrc/pktdump.h"

#include "thread.h"
#include "xtimer.h"
#include "ztimer.h"

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stdbool.h>

#include "demo_button.h"
#include "demo_neopixels.h"
#include "onboard_leds.h"
#include "demo_throttlers.h"

#include "iperf.h"

/* main thread's message queue */
#define MAIN_QUEUE_SIZE (32)

static msg_t _main_msg_queue[MAIN_QUEUE_SIZE];

int main(void)
{
	msg_init_queue(_main_msg_queue, MAIN_QUEUE_SIZE);
	shell_run(NULL, line_buf, SHELL_DEFAULT_BUFSIZE);
	return 0;
}
