/**
   modules/rf.c: experimental rf module for openchronos-ng

   Copyright (C) 2019 Luca Lorello <strontiumaluminate@gmail.com>

   http://github.com/HashakGik/openchronos-ng-elf

   This file is part of openchronos-ng.

   openchronos-ng is free software: you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation, either version 3 of the License, or
   (at your option) any later version.

   openchronos-ng is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program.  If not, see <http://www.gnu.org/licenses/>.
**/

/* This module communicates with the RF access point via SimpliciTI protocol and sends dummy data viable for the original "ACC" mode.

   The module currently DOES NOT WORK! (the library won't link due to insufficient RAM).
 */

#include "menu.h"
#include "messagebus.h"
#include "drivers/display.h"
#include "drivers/radio.h"
#include "drivers/timer.h"
#include "simpliciti/simpliciti.h"

unsigned char simpliciti_flag;
unsigned char simpliciti_ed_address[4] = THIS_DEVICE_ADDRESS;
unsigned char chronos_black = 0;
unsigned char rf_frequoffset;	// = 0; // ???
unsigned char simpliciti_data[SIMPLICITI_MAX_PAYLOAD_LENGTH];
unsigned char simpliciti_reply_count;

static void rf_act(void)
{
    simpliciti_data[0] = 0x01;	// "ACC" mode
    simpliciti_data[1] = 0;
    simpliciti_data[2] = 0;
    simpliciti_data[3] = 0;

    open_radio();

    display_chars(0, LCD_SEG_L2_4_0, "SCAN", BLINK_SET | SEG_ON);

    display_symbol(0, LCD_ICON_BEEPER1, BLINK_SET | SEG_ON);
    display_symbol(0, LCD_ICON_BEEPER2, BLINK_SET | SEG_ON);
    display_symbol(0, LCD_ICON_BEEPER3, BLINK_SET | SEG_ON);

    if (simpliciti_link())	// Always fails due to insufficient RAM...
    {
	display_chars(0, LCD_SEG_L2_4_0, "SCAN", BLINK_SET | SEG_OFF);
	display_chars(0, LCD_SEG_L2_4_0, "OK", SEG_SET);

	display_symbol(0, LCD_ICON_BEEPER1, BLINK_OFF | SEG_ON);
	display_symbol(0, LCD_ICON_BEEPER2, BLINK_OFF | SEG_ON);
	display_symbol(0, LCD_ICON_BEEPER3, BLINK_OFF | SEG_ON);

	simpliciti_main_tx_only();	// use simpliciti_main_sync(); for "SYNC" mode
    } else {
	display_chars(0, LCD_SEG_L2_4_0, "SCAN", BLINK_OFF);
	display_chars(0, LCD_SEG_L2_4_0, "FAIL", SEG_SET);

	display_symbol(0, LCD_ICON_BEEPER1, BLINK_OFF | SEG_OFF);
	display_symbol(0, LCD_ICON_BEEPER2, BLINK_OFF | SEG_OFF);
	display_symbol(0, LCD_ICON_BEEPER3, BLINK_OFF | SEG_OFF);
    }

    close_radio();
}

static void rf_deac(void)
{
    display_symbol(0, LCD_ICON_BEEPER1, BLINK_OFF | SEG_OFF);
    display_symbol(0, LCD_ICON_BEEPER2, BLINK_OFF | SEG_OFF);
    display_symbol(0, LCD_ICON_BEEPER3, BLINK_OFF | SEG_OFF);

    simpliciti_flag = SIMPLICITI_TRIGGER_STOP;	// is this required???

    close_radio();

}




void mod_rf_init(void)
{
    menu_add_entry("RADIO", NULL, NULL, NULL, NULL, NULL, NULL, &rf_act,
		   &rf_deac);
}


void simpliciti_get_ed_data_callback(void)	// Called from the SimpliciTI library when the AP requests informations in tx-only mode.
{
    // simply fill simpliciti_data[...] and trigger the transmission with: simpliciti_flag |= SIMPLICITI_TRIGGER_SEND_DATA

    timer0_delay(5, LPM3_bits);


    simpliciti_data[0] = 0x01;
    simpliciti_data[1] = 0x01;
    simpliciti_data[2] = 0x01;
    simpliciti_data[3] = 0x01;

    // Trigger packet sending
    simpliciti_flag |= SIMPLICITI_TRIGGER_SEND_DATA;




#ifdef USE_WATCHDOG
    // Service watchdog
    WDTCTL = WDTPW + WDTIS__512K + WDTSSEL__ACLK + WDTCNTCL;
#endif
}

void simpliciti_sync_decode_ap_cmd_callback(void)	// Called from the SimpliciTI library in sync mode when the AP sends a command.
{
    //uint8_t i;
    //int16_t t1, offset;

    // Default behaviour is to send no reply packets
    simpliciti_reply_count = 0;

    switch (simpliciti_data[0]) {
    case SYNC_AP_CMD_NOP:
	display_chars(0, LCD_SEG_L1_3_0, "NOP", SEG_SET);
	break;

    case SYNC_AP_CMD_GET_STATUS:	// Send watch parameters
	simpliciti_data[0] = SYNC_ED_TYPE_STATUS;
	// Send single reply packet
	simpliciti_reply_count = 1;

	display_chars(0, LCD_SEG_L1_3_0, "GET", SEG_SET);
	break;

    case SYNC_AP_CMD_SET_WATCH:	// Set watch parameters
	/* sys.flag.use_metric_units = (simpliciti_data[1] >> 7) & 0x01;
	   sTime.hour = simpliciti_data[1] & 0x7F;
	   sTime.minute = simpliciti_data[2];
	   sTime.second = simpliciti_data[3];
	   sDate.year = (simpliciti_data[4] << 8) + simpliciti_data[5];
	   sDate.month = simpliciti_data[6];
	   sDate.day = simpliciti_data[7];
	   sAlarm.hour = simpliciti_data[8];
	   sAlarm.minute = simpliciti_data[9];
	   // Set temperature and temperature offset
	   t1 = (s16) ((simpliciti_data[10] << 8) + simpliciti_data[11]);
	   offset = t1 - (sTemp.degrees - sTemp.offset);
	   sTemp.offset = offset;
	   sTemp.degrees = t1;
	   // Set altitude
	   sAlt.altitude = (s16) ((simpliciti_data[12] << 8) + simpliciti_data[13]);
	   update_pressure_table(sAlt.altitude, sAlt.pressure, sAlt.temperature);

	   display_chars(LCD_SEG_L2_5_0, (u8 *) "  DONE", SEG_ON);
	   sRFsmpl.display_sync_done = 1; */

	display_chars(0, LCD_SEG_L1_3_0, "SET", SEG_SET);

	break;

    case SYNC_AP_CMD_GET_MEMORY_BLOCKS_MODE_1:
	// Send sequential packets out in a burst
	simpliciti_data[0] = SYNC_ED_TYPE_MEMORY;
	// Get burst start and end packet
	/*           burst_start = (simpliciti_data[1] << 8) + simpliciti_data[2];
	   burst_end = (simpliciti_data[3] << 8) + simpliciti_data[4];
	   // Set burst mode
	   burst_mode = 1;
	   // Number of packets to send
	   simpliciti_reply_count = burst_end - burst_start; */

	display_chars(0, LCD_SEG_L1_3_0, "MBM1", SEG_SET);
	break;

    case SYNC_AP_CMD_GET_MEMORY_BLOCKS_MODE_2:
	// Send specified packets out in a burst
	simpliciti_data[0] = SYNC_ED_TYPE_MEMORY;
	// Store the requested packets
	/*            for (i = 0; i < BM_SYNC_BURST_PACKETS_IN_DATA; i++)
	   {
	   burst_packet[i] = (simpliciti_data[i * 2 + 1] << 8) + simpliciti_data[i * 2 + 2];
	   }
	   // Set burst mode
	   burst_mode = 2;
	   // Number of packets to send
	   simpliciti_reply_count = BM_SYNC_BURST_PACKETS_IN_DATA; */

	display_chars(0, LCD_SEG_L1_3_0, "MBM2", SEG_SET);
	break;

    case SYNC_AP_CMD_ERASE_MEMORY:	// Erase data logger memory
	display_chars(0, LCD_SEG_L1_3_0, "ERAS", SEG_SET);
	break;

    case SYNC_AP_CMD_EXIT:	// Exit sync mode

	display_chars(0, LCD_SEG_L1_3_0, "EXIT", SEG_SET);

	simpliciti_flag |= SIMPLICITI_TRIGGER_STOP;
	break;
    }



}

void simpliciti_sync_get_data_callback(unsigned int index)	// Called from the SimpliciTI library when the AP requests informations in sync mode.
{



}
