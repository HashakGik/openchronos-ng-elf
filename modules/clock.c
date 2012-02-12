// *************************************************************************************************
//
//	Copyright (C) 2009 Texas Instruments Incorporated - http://www.ti.com/
//
//
//	  Redistribution and use in source and binary forms, with or without
//	  modification, are permitted provided that the following conditions
//	  are met:
//
//	    Redistributions of source code must retain the above copyright
//	    notice, this list of conditions and the following disclaimer.
//
//	    Redistributions in binary form must reproduce the above copyright
//	    notice, this list of conditions and the following disclaimer in the
//	    documentation and/or other materials provided with the
//	    distribution.
//
//	    Neither the name of Texas Instruments Incorporated nor the names of
//	    its contributors may be used to endorse or promote products derived
//	    from this software without specific prior written permission.
//
//	  THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
//	  "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
//	  LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
//	  A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
//	  OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
//	  SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
//	  LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
//	  DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
//	  THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
//	  (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
//	  OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
//
// *************************************************************************************************

// system
#include <ezchronos.h>

// driver
#include "ports.h"
#include "display.h"
#include "timer.h"

// logic
#include "clock.h"
#include "user.h"

#ifdef CONFIG_SIDEREAL
#include "sidereal.h"
#endif

#include "date.h"

#ifdef CONFIG_CLOCK_ONLY_SYNC
#include "rfsimpliciti.h"
#endif


// *************************************************************************************************
// Global Variable section
struct time sTime;

// Display values for time format selection
#if (OPTION_TIME_DISPLAY == CLOCK_DISPLAY_SELECT)
const uint8_t selection_Timeformat[][4] = {
	"24H", "12H"
};
#endif


void clock_event(rtca_tevent_ev_t ev)
{
	// Use sTime.drawFlag to minimize display updates
	// sTime.drawFlag = 2: minute, second
	// sTime.drawFlag = 3: hour, minute

	sTime.drawFlag = (ev >= RTCA_EV_HOUR ? 3 : ev + 2);
	sTime.update_display = 1;
}


// *************************************************************************************************
// @fn          convert_hour_to_12H_format
// @brief       Convert internal 24H time to 12H time.
// @param       uint8_t hour		Hour in 24H format
// @return      uint8_t				Hour in 12H format
// *************************************************************************************************
#if (OPTION_TIME_DISPLAY > CLOCK_24HR)
static uint8_t convert_hour_to_12H_format(uint8_t hour)
{
	// 00:00 .. 11:59 --> AM 12:00 .. 11:59
	if (hour == 0)
		return (hour + 12);
	else if (hour <= 12)
		return (hour);
	// 13:00 .. 23:59 --> PM 01:00 .. 11:59
	else
		return (hour - 12);
}


// *************************************************************************************************
// @fn          is_hour_am
// @brief       Checks if internal 24H time is AM or PM
// @param       uint8_t hour		Hour in 24H format
// @return      uint8_t				1 = AM, 0 = PM
// *************************************************************************************************
static uint8_t is_hour_am(uint8_t hour)
{
	// 00:00 .. 11:59 --> AM 12:00 .. 11:59
	if (hour < 12) return (1);
	// 12:00 .. 23:59 --> PM 12:00 .. 11:59
	else return (0);
}

#if (OPTION_TIME_DISPLAY == CLOCK_DISPLAY_SELECT)
// *************************************************************************************************
// @fn          display_selection_Timeformat
// @brief       Display time format 12H / 24H.
// @param       uint8_t segments			Target segments where to display information
//				uint32_t index			0 or 1, index for value string
//				uint8_t digits			Not used
//				uint8_t blanks			Not used
// @return      none
// *************************************************************************************************
static void display_selection_Timeformat1(uint8_t segments, uint32_t index, uint8_t digits, uint8_t blanks, uint8_t dummy)
{
	if (index < 2) display_chars(segments, (uint8_t *)selection_Timeformat[index], SEG_ON_BLINK_ON);
}
#endif // CLOCK_DISPLAY_SELECT

#endif //OPTION_TIME_DISPLAY


// *************************************************************************************************
// @fn          mx_time
// @brief       Clock set routine.
// @param       uint8_t line		LINE1, LINE2
// @return      none
// *************************************************************************************************
static void mx_time(uint8_t line)
{
	uint8_t select;
	int32_t timeformat;
	int16_t timeformat1;
	uint8_t hours;
	uint8_t minutes;
	uint8_t seconds;
	uint8_t *str;

	// Clear display
	clear_display_all();

#ifdef CONFIG_CLOCK_ONLY_SYNC

	if (sys.flag.low_battery) return;

	display_sync(LINE2, DISPLAY_LINE_UPDATE_FULL);
	start_simpliciti_sync();

#else

	// Convert global time to local variables
	// Global time keeps on ticking in background until it is overwritten
	if (sys.flag.am_pm_time) {
		timeformat 	= TIMEFORMAT_12H;
	} else {
		timeformat 	= TIMEFORMAT_24H;
	}

	timeformat1	= timeformat;
	rtca_get_time(&hours, &minutes, &seconds);

	// Init value index
	select = 0;

	// Loop values until all are set or user breaks	set
	while (1) {
		// Idle timeout: exit without saving
		if (sys.flag.idle_timeout) {
			// Roll back time format
			if (timeformat1 == TIMEFORMAT_12H)
				sys.flag.am_pm_time = 1;
			else
				sys.flag.am_pm_time = 0;

			display_symbol(LCD_SYMB_AM, SEG_OFF);
			break;
		}

		// Button STAR (short): save, then exit
		if (button.flag.star) {
			// Store time in RTC
			rtca_set_time(hours, minutes, seconds);

			// Full display update is done when returning from function
			display_symbol(LCD_SYMB_AM, SEG_OFF);

#ifdef CONFIG_SIDEREAL

			if (sSidereal_time.sync > 0)
				sync_sidereal();

#endif
			break;
		}

		int32_t val;

		switch (select) {
#if (OPTION_TIME_DISPLAY == CLOCK_DISPLAY_SELECT)

		case 0:		// Clear LINE1 and LINE2 and AM icon - required when coming back from set_value(seconds)
			clear_display();
			display_symbol(LCD_SYMB_AM, SEG_OFF);

			// Set 24H / 12H time format
			set_value(&timeformat, 1, 0, 0, 1, SETVALUE_ROLLOVER_VALUE + SETVALUE_DISPLAY_SELECTION + SETVALUE_NEXT_VALUE, LCD_SEG_L1_3_1, display_selection_Timeformat1);

			// Modify global time format variable immediately to update AM/PM icon correctly
			if (timeformat == TIMEFORMAT_12H)
				sys.flag.am_pm_time = 1;
			else
				sys.flag.am_pm_time = 0;

			select = 1;
			break;
#else

		case 0:
#endif
		case 1:		// Display HH:MM (LINE1) and .SS (LINE2)
			str = _itoa(hours, 2, 0);
			display_chars(LCD_SEG_L1_3_2, str, SEG_ON);
			display_symbol(LCD_SEG_L1_COL, SEG_ON);

			str = _itoa(minutes, 2, 0);
			display_chars(LCD_SEG_L1_1_0, str, SEG_ON);

			str = _itoa(seconds, 2, 0);
			display_chars(LCD_SEG_L2_1_0, str, SEG_ON);
			display_symbol(LCD_SEG_L2_DP, SEG_ON);

			// Set hours
			val = hours;
			set_value(&val, 2, 0, 0, 23, SETVALUE_ROLLOVER_VALUE + SETVALUE_DISPLAY_VALUE + SETVALUE_NEXT_VALUE, LCD_SEG_L1_3_2, display_hours_12_or_24);
			hours = val;
			select = 2;
			break;

		case 2:		// Set minutes
			val = minutes;
			set_value(&val, 2, 0, 0, 59, SETVALUE_ROLLOVER_VALUE + SETVALUE_DISPLAY_VALUE + SETVALUE_NEXT_VALUE, LCD_SEG_L1_1_0, display_value1);
			minutes = val;
			select = 3;
			break;

		case 3:		// Set seconds
			val = seconds;
			set_value(&val, 2, 0, 0, 59, SETVALUE_ROLLOVER_VALUE + SETVALUE_DISPLAY_VALUE + SETVALUE_NEXT_VALUE, LCD_SEG_L2_1_0, display_value1);
			seconds = val;
			select = 0;
			break;
		}
	}

	// Clear button flags
	button.all_flags = 0;

#endif
}


// *************************************************************************************************
// @fn          sx_time
// @brief       Time user routine. Toggles view style between HH:MM and SS.
// @param       line		LINE1
// @return      none
// *************************************************************************************************
static void sx_time(uint8_t line)
{
	// Toggle display view style
	if (sTime.line1ViewStyle == DISPLAY_DEFAULT_VIEW)
		sTime.line1ViewStyle = DISPLAY_ALTERNATIVE_VIEW;
	else
		sTime.line1ViewStyle = DISPLAY_DEFAULT_VIEW;
}

// *************************************************************************************************
// @fn          display_time
// @brief       Clock display routine. Supports 24H and 12H time format,
//              through the helper display_hours_with_12_24.
// @param       uint8_t line			LINE1
//		uint8_t update		DISPLAY_LINE_UPDATE_FULL, DISPLAY_LINE_UPDATE_PARTIAL
// @return      none
// *************************************************************************************************
void display_time(uint8_t line, uint8_t update)
{
	uint8_t hour, min, sec;

	rtca_get_time(&hour, &min, &sec);

	// Partial update
	if (update == DISPLAY_LINE_UPDATE_PARTIAL && sTime.update_display) {
		sTime.update_display = 0;
		if (sTime.line1ViewStyle == DISPLAY_DEFAULT_VIEW) {
			switch (sTime.drawFlag) {
			case 3:
				display_hours_12_or_24(switch_seg(line, LCD_SEG_L1_3_2, LCD_SEG_L2_3_2), hour, 2, 1, SEG_ON);

			case 2:
				display_chars(switch_seg(line, LCD_SEG_L1_1_0, LCD_SEG_L2_1_0), _itoa(min, 2, 0), SEG_ON);
			}
		} else {
			// Seconds are always updated
			display_chars(switch_seg(line, LCD_SEG_L1_1_0, LCD_SEG_L2_1_0), _itoa(sec, 2, 0), SEG_ON);
		}
	} else if (update == DISPLAY_LINE_UPDATE_FULL) {
		// Full update
		if ((line == LINE1 && sTime.line1ViewStyle == DISPLAY_DEFAULT_VIEW) || (line == LINE2 && sTime.line2ViewStyle == DISPLAY_DEFAULT_VIEW)) {
			// Display hours
			display_hours_12_or_24(switch_seg(line, LCD_SEG_L1_3_2, LCD_SEG_L2_3_2), hour, 2, 1, SEG_ON);
			// Display minute
			display_chars(switch_seg(line, LCD_SEG_L1_1_0, LCD_SEG_L2_1_0), _itoa(min, 2, 0), SEG_ON);
			display_symbol(switch_seg(line, LCD_SEG_L1_COL, LCD_SEG_L2_COL0), SEG_ON_BLINK_ON);
		} else {
			// Display seconds
			display_chars(switch_seg(line, LCD_SEG_L1_1_0, LCD_SEG_L2_1_0), _itoa(sec, 2, 0), SEG_ON);
			display_symbol(switch_seg(line, LCD_SEG_L1_DP1, LCD_SEG_L2_DP), SEG_ON);
		}
	} else if (update == DISPLAY_LINE_CLEAR) {
		display_symbol(switch_seg(line, LCD_SEG_L1_COL, LCD_SEG_L2_COL0), SEG_OFF_BLINK_OFF);
		// Change display style to default (HH:MM)
		sTime.line1ViewStyle = DISPLAY_DEFAULT_VIEW;
		// Clean up AM/PM icon
		display_symbol(LCD_SYMB_AM, SEG_OFF);
	}
}


void clock_init()
{
	// Display style of both lines is default (HH:MM)
	sTime.line1ViewStyle = DISPLAY_DEFAULT_VIEW;
	sTime.line2ViewStyle = DISPLAY_DEFAULT_VIEW;

	// Reset timeout detection
	sTime.last_activity = 0;

#ifdef CONFIG_SIDEREAL
	sTime.UTCoffset  = 0;
#endif

	// The flag will be later updated by clock_event()
	sTime.drawFlag = 0;

	//TODO: fix to use new display function
	//menu_add_entry(LINE1, &sx_time, &mx_time, &display_time);

	rtca_tevent_fn_register(&clock_event);
}