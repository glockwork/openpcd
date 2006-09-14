/* OpenPCD USB handler - handle incoming USB requests on OUT pipe
 * (C) 2006 by Harald Welte <hwelte@hmw-consulting.de>
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by 
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 */

#include <sys/types.h>
#include <errno.h>
#include <string.h>

#include <openpcd.h>

#include <os/pcd_enumerate.h>
#include <os/usb_handler.h>
#include <os/req_ctx.h>
#include <os/led.h>
#include <os/dbgu.h>

#include "../openpcd.h"

static usb_cmd_fn *cmd_hdlrs[16];

int usb_hdlr_register(usb_cmd_fn *hdlr, u_int8_t class)
{
	cmd_hdlrs[class] = hdlr;
	return 0;
}

void usb_hdlr_unregister(u_int8_t class)
{
	cmd_hdlrs[class] = NULL;
}

static int usb_in(struct req_ctx *rctx)
{
	struct openpcd_hdr *poh = (struct openpcd_hdr *) &rctx->rx.data[0];
	struct openpcd_hdr *pih = (struct openpcd_hdr *) &rctx->tx.data[0];
	usb_cmd_fn *hdlr;

	DEBUGP("usb_in(cls=%d) ", OPENPCD_CMD_CLS(poh->cmd));

	if (rctx->rx.tot_len < sizeof(*poh))
		return -EINVAL;
	
	memcpy(pih, poh, sizeof(*poh));
	rctx->tx.tot_len = sizeof(*poh);

	hdlr = cmd_hdlrs[OPENPCD_CMD_CLS(poh->cmd)];
	if (hdlr) 
		return (hdlr)(rctx);
	else
		DEBUGPCR("no handler for this class\n");
}

/* Process all pending request contexts that want to Tx on either
 * IN or INTERRUPT endpoint */
void usb_out_process(void)
{
	struct req_ctx *rctx;

	while (rctx = req_ctx_find_get(RCTX_STATE_UDP_EP3_PENDING,
				       RCTX_STATE_UDP_EP3_BUSY)) {
		DEBUGPCRF("EP3_BUSY for ctx %u", req_ctx_num(rctx));
		if (udp_refill_ep(3, rctx) < 0)
			req_ctx_set_state(rctx, RCTX_STATE_UDP_EP3_PENDING);
	}

	while (rctx = req_ctx_find_get(RCTX_STATE_UDP_EP2_PENDING,
				       RCTX_STATE_UDP_EP2_BUSY)) {
		DEBUGPCRF("EP2_BUSY for ctx %u", req_ctx_num(rctx));
		if (udp_refill_ep(2, rctx) < 0)
			req_ctx_set_state(rctx, RCTX_STATE_UDP_EP2_PENDING);
	}
}

/* process incoming USB packets (OUT pipe) that have already been 
 * put into request contexts by the UDP IRQ handler */
void usb_in_process(void)
{
	struct req_ctx *rctx;

	while (rctx = req_ctx_find_get(RCTX_STATE_UDP_RCV_DONE,
				       RCTX_STATE_MAIN_PROCESSING)) {
	     	DEBUGPCRF("found used ctx %u: len=%u", 
			req_ctx_num(rctx), rctx->rx.tot_len);
		usb_in(rctx);
	}
	udp_unthrottle();
}
