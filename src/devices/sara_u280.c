/*
 * Race Capture Pro Firmware
 *
 * Copyright (C) 2015 Autosport Labs
 *
 * This file is part of the Race Capture Pro fimrware suite
 *
 * This is free software: you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This software is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 *
 * See the GNU General Public License for more details. You should
 * have received a copy of the GNU General Public License along with
 * this code. If not, see <http://www.gnu.org/licenses/>.
 */

#include "array_utils.h"
#include "cellular.h"
#include "cell_pwr_btn.h"
#include "gsm.h"
#include "printk.h"
#include "serial_buffer.h"
#include "sara_u280.h"
#include "taskUtil.h"

#include <string.h>
#include <stdlib.h>

static bool sara_u280_get_subscriber_number(struct serial_buffer *sb,
                                            struct cellular_info *ci)
{
        return gsm_get_subscriber_number(sb, ci);
}

static bool sara_u280_get_signal_strength(struct serial_buffer *sb,
                                          struct cellular_info *ci)
{
        return gsm_get_subscriber_number(sb, ci);
}

static bool sara_u280_get_imei(struct serial_buffer *sb,
                               struct cellular_info *ci)
{
        return gsm_get_imei(sb, ci);
}

static enum cellular_net_status sara_u280_get_net_reg_status(
        struct serial_buffer *sb,struct cellular_info *ci)
{
        return gsm_get_network_reg_status(sb, ci);
}

static bool sara_u280_get_network_reg_info(struct serial_buffer *sb,
                                           struct cellular_info *ci)
{
        return gsm_get_network_reg_info(sb, ci);
}

static bool sara_u280_is_gprs_attached(struct serial_buffer *sb)
{
        return gsm_is_gprs_attached(sb);
}

static bool sara_u280_get_ip_address(struct serial_buffer *sb)
{
        const char *cmd = "AT+UPSND=0,0";
        const char *msgs[2];
        const size_t msgs_len = ARRAY_LEN(msgs);

        serial_buffer_reset(sb);
        serial_buffer_append(sb, cmd);
        const size_t count = cellular_exec_cmd(sb, READ_TIMEOUT, msgs,
                                               msgs_len);
        const bool status = is_rsp_ok(msgs, count);
        if (!status) {
                pr_warning("[cell] Failed to read IP\r\n");
                return false;
        }

        /* +UPSND: 0,0,"93.68.225.175" */
        const char* ipaddr = msgs[0];
        for (size_t i = 2; i && ipaddr; --i)
                ipaddr = strchr(ipaddr, ',');

        if (ipaddr) {
                pr_info_str_msg("[cell] IP address: ", ipaddr);
        } else {
                pr_warning_str_msg("[cell] Unable to parse IP address: ",
                                   msgs[0]);
        }

        return ipaddr != NULL;
}

static bool sara_u280_put_pdp_config(struct serial_buffer *sb,
                                     const int pdp_id,
                                     const char *host,
                                     const char* user,
                                     const char* password)
{
        const char *msgs[1];
        const size_t msgs_len = ARRAY_LEN(msgs);

        serial_buffer_reset(sb);

        /* APN settings */
        serial_buffer_printf_append(sb, "AT+UPSD=0,1,\"%s\"", host);
        serial_buffer_printf_append(sb, ";+UPSD=0,2,\"%s\"", user);
        serial_buffer_printf_append(sb, ";+UPSD=0,3,\"%s\"", password);

        /* Dynamic IP */
        serial_buffer_append(sb, ";+UPSD=0,7,\"0.0.0.0\"");


        const size_t count = cellular_exec_cmd(sb, READ_TIMEOUT, msgs,
                                               msgs_len);
        return is_rsp_ok(msgs, count);
}

static bool sara_u280_put_dns_config(struct serial_buffer *sb,
                                     const char* dns1,
                                     const char *dns2)
{
        const char *msgs[1];
        const size_t msgs_len = ARRAY_LEN(msgs);

        serial_buffer_reset(sb);
        serial_buffer_printf_append(sb, "AT+UPSD=0,4,\"%s\"", dns1);
        if (dns2) {
                serial_buffer_append(sb, ";");
                serial_buffer_printf_append(sb, "+UPSD=0,5,\"%s\"", dns2);
        }

        const size_t count = cellular_exec_cmd(sb, READ_TIMEOUT, msgs,
                                               msgs_len);
        return is_rsp_ok(msgs, count);
}

static bool sara_u280_gprs_psd_action(struct serial_buffer *sb,
                                      const int pid,
                                      const int aid)
{
        const char *msgs[1];
        const size_t msgs_len = ARRAY_LEN(msgs);

        serial_buffer_reset(sb);
        serial_buffer_printf_append(sb, "AT+UPSDA=%d,%d", pid, aid);
        const size_t count = cellular_exec_cmd(sb, MEDIUM_TIMEOUT, msgs,
                                               msgs_len);
        const bool status = is_rsp_ok(msgs, count);

        return status;
}

static bool sara_u280_activate_pdp(struct serial_buffer *sb,
                                   const int pdp_id)
{
        return sara_u280_gprs_psd_action(sb, pdp_id, 3);
}

#if 0
static bool sara_u280_deactivate_pdp(struct serial_buffer *sb,
                                     const int pdp_id)
{
        return sara_u280_gprs_psd_action(sb, pdp_id, 4);
}
#endif

static int sara_u280_create_tcp_socket(struct serial_buffer *sb)
{
        const char *cmd = "AT+USOCR=6";
        const char *msgs[2];
        const size_t msgs_len = ARRAY_LEN(msgs);

        serial_buffer_reset(sb);
        serial_buffer_append(sb, cmd);
        const size_t count = cellular_exec_cmd(sb, READ_TIMEOUT, msgs,
                                               msgs_len);
        const bool status = is_rsp_ok(msgs, count);
        if (!status) {
                pr_warning("[sara_u280] Failed to create a socket\r\n");
                return -1;
        }

        /*
         * Parse out the socket ID.  Needed for Muxing.
         * +USOCR: <socket_id>
         */
        const int socket = msgs[0][8] - '0';
        if (socket < 0 || socket > 6) {
                pr_warning("[sara_u280] Failed to parse socket ID.\r\n");
                return -2;
        }

        pr_debug_int_msg("[sara_u280] Socket ID: ", socket);
        return socket;
}

static bool sara_u280_connect_tcp_socket(struct serial_buffer *sb,
                                         const int socket_id,
                                         const char* host,
                                         const int port)
{
        const char *msgs[1];
        const size_t msgs_len = ARRAY_LEN(msgs);

        serial_buffer_reset(sb);
        serial_buffer_printf_append(sb, "AT+USOCO=%d,\"%s\",%d", socket_id,
                                    host, port);
        const size_t count = cellular_exec_cmd(sb, CONNECT_TIMEOUT, msgs,
                                               msgs_len);
        return is_rsp_ok(msgs, count);
}

static bool sara_u280_close_tcp_socket(struct serial_buffer *sb,
                                       const int socket_id)
{
        const char *msgs[1];
        const size_t msgs_len = ARRAY_LEN(msgs);

        serial_buffer_reset(sb);
        serial_buffer_printf_append(sb, "AT+USOCL=%d", socket_id);
        const size_t count = cellular_exec_cmd(sb, MEDIUM_TIMEOUT, msgs,
                                               msgs_len);
        return is_rsp_ok(msgs, count);
}

static bool sara_u280_start_direct_mode(struct serial_buffer *sb,
                                        const int socket_id)
{
        const char *msgs[1];
        const size_t msgs_len = ARRAY_LEN(msgs);

        serial_buffer_reset(sb);
        serial_buffer_printf_append(sb, "AT+USODL=%d", socket_id);
        const size_t count = cellular_exec_cmd(sb, MEDIUM_TIMEOUT, msgs,
                                               msgs_len);
        return is_rsp(msgs, count, "CONNECT");
}

static bool sara_u280_stop_direct_mode(struct serial_buffer *sb)
{
        /* Must delay min 2000ms before stopping direct mode */
	delayMs(2100);

        serial_buffer_reset(sb);
        serial_buffer_append(sb, "+++");
        cellular_exec_cmd(sb, 0, NULL, 0);

	delayMs(500);

        size_t tries = 10;
        for (; tries; --tries) {
                if (gsm_ping_modem(sb))
                        break;
        }
        return tries > 0;
}

static bool sara_u280_init(struct serial_buffer *sb,
                           struct cellular_info *ci)
{
        /* NO-OP.  This hardware is better than sim900. */
        return true;
}

static bool sara_u280_get_sim_info(struct serial_buffer *sb,
                                   struct cellular_info *ci)
{
        return sara_u280_get_imei(sb, ci) &&
                sara_u280_get_signal_strength(sb, ci) &&
                sara_u280_get_subscriber_number(sb, ci);
}

static bool sara_u280_register_on_network(struct serial_buffer *sb,
                                          struct cellular_info *ci)
{
        /* Check our status on the network */
        for (size_t tries = 30; tries; --tries) {
                sara_u280_get_net_reg_status(sb, ci);

                switch(ci->net_status) {
                case CELLULAR_NETWORK_DENIED:
                case CELLULAR_NETWORK_REGISTERED:
                        goto out;
                default:
                        break;
                }

                /* Wait before trying again */
                delayMs(1000);
        }

out:
        return CELLULAR_NETWORK_REGISTERED == ci->net_status;
}

static bool sara_u280_setup_pdp(struct serial_buffer *sb,
                                struct cellular_info *ci,
                                const CellularConfig *cc)
{
        /* Check GPRS attached */
        bool gprs_attached;
        for (size_t tries = 10; tries; --tries) {
                gprs_attached = sara_u280_is_gprs_attached(sb);

                if (gprs_attached)
                        break;

                /* Wait before trying again.  Arbitrary backoff */
                delayMs(3000);
        }

        pr_info_str_msg("[sara_u280] GPRS Attached: ",
                        gprs_attached ? "yes" : "no");
        if (!gprs_attached)
                return false;

        /* Setup APN */
        const bool status =
                sara_u280_put_pdp_config(sb, 0, cc->apnHost, cc->apnUser,
                                         cc->apnPass) &&
                sara_u280_put_dns_config(sb, cc->dns1, cc->dns2);
        if (!status) {
                pr_warning("[sara_u280] APN/DNS config failed\r\n");
                return false;
        }

        bool gprs_active;
        for (size_t tries = 3; tries; --tries) {
                gprs_active = sara_u280_activate_pdp(sb, 0);

                if (gprs_active)
                        break;

                /* Wait before trying again.  Arbitrary backoff */
                delayMs(3000);
        }
        if (!gprs_active) {
                pr_warning("[sara_u280] Failed connect GPRS\r\n");
                return false;
        }
        pr_debug("[sara_u280] GPRS connected\r\n");

        /* Wait to get the IP */
        bool has_ip = false;
        for (size_t tries = 5; tries && !has_ip; --tries) {
                has_ip = sara_u280_get_ip_address(sb);
                delayMs(1000);
        }
        if (!has_ip)
                return false;
        pr_debug("[sara_u280] IP acquired\r\n");

        return true;
}

static bool sara_u280_connect_rcl_telem(struct serial_buffer *sb,
                                        struct cellular_info *ci,
                                        struct telemetry_info *ti,
                                        const TelemetryConfig *tc)
{
        ti->socket = sara_u280_create_tcp_socket(sb);
        if (ti->socket < 0) {
                pr_warning("[sara_u280] Failed to create a socket\r\n");
                return false;
        }

        if (!sara_u280_connect_tcp_socket(sb, ti->socket,
                                          tc->telemetryServerHost,
                                          tc->telemetry_port)) {
                pr_warning("[sara_u280] Failed to connect to ");
                pr_warning(tc->telemetryServerHost);
                pr_warning_int_msg(":", tc->telemetry_port);

                return false;
        }

        return sara_u280_start_direct_mode(sb, ti->socket);
}

static bool sara_u280_disconnect(struct serial_buffer *sb,
                                 struct cellular_info *ci,
                                 struct telemetry_info *ti)
{
        if (!sara_u280_stop_direct_mode(sb)) {
                /* Then we don't know if can issue commands */
                pr_warning("[sara_u280] Failed to escape Direct Mode\r\n");
                return -1;
        }

        if (!sara_u280_close_tcp_socket(sb, ti->socket)) {
                pr_warning_int_msg("[sara_u280] Failed to close socket ",
                                   ti->socket);
                return -2;
        }

        ti->socket = -1;
        return 0;
}

static const struct at_config* sara_u280_get_at_config()
{
        /* Optimized AT config values for U-blox sara u280 */
        static const struct at_config cfg = {
                .urc_delay_ms = 50,
        };

        return &cfg;
}


static const struct cell_modem_methods sara_u280_methods = {
        .get_at_config = sara_u280_get_at_config,
        .init_modem = sara_u280_init,
        .get_sim_info = sara_u280_get_sim_info,
        .register_on_network = sara_u280_register_on_network,
        .get_network_info = sara_u280_get_network_reg_info,
        .setup_pdp = sara_u280_setup_pdp,
        .open_telem_connection = sara_u280_connect_rcl_telem,
        .close_telem_connection = sara_u280_disconnect,
};

const struct cell_modem_methods* get_sara_u280_methods()
{
        return &sara_u280_methods;
}
