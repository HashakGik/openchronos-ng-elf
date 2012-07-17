/*
    battery.c: battery voltage display module

    Copyright (C) 2012 Matthew Excell <matt@excellclan.com>

    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <openchronos.h>

#include "drivers/display.h"
#include "drivers/battery.h"

static void display_battery(void)
{
	/* display battery percentage on line one */
	display_chars(0, LCD_SEG_L1_2_0, _itopct(BATTERY_EMPTY_THRESHOLD,
		   BATTERY_FULL_THRESHOLD, battery_info.voltage), SEG_SET);

#ifdef CONFIG_BATTERY_SHOW_VOLTAGE
	/* display battery voltage in line two (xx.x format) */
	_printf(0, LCD_SEG_L2_3_0, "%4u", battery_info.voltage);
#endif
}

static void battery_event(enum sys_message event)
{
	display_battery();
}

static void battery_activate(void)
{
#ifdef CONFIG_BATTERY_MONITOR
	sys_messagebus_register(&battery_event, SYS_MSG_BATT);
#else
	/* don't need this if the background task is compiled in */
	battery_measurement();
#endif

	/* display static symbols */
#ifdef CONFIG_BATTERY_SHOW_VOLTAGE
	display_symbol(0, LCD_SEG_L2_DP,    SEG_ON);
#endif
	display_symbol(0, LCD_SYMB_BATTERY, SEG_ON);
	display_symbol(0, LCD_SYMB_PERCENT, SEG_ON);

	/* refresh display */
	display_battery();
}

static void battery_deactivate(void)
{
#ifdef CONFIG_BATTERY_MONITOR
	sys_messagebus_unregister(&battery_event);
#endif

	/* cleanup screen */
	display_clear(0, 1);
#ifdef CONFIG_BATTERY_SHOW_VOLTAGE
	display_clear(0, 2);
#endif

	/* clear static symbols */
#ifdef CONFIG_BATTERY_SHOW_VOLTAGE
	display_symbol(0, LCD_SEG_L2_DP, SEG_OFF);
#endif
	display_symbol(0, LCD_SYMB_PERCENT, SEG_OFF);
	display_symbol(0, LCD_SYMB_BATTERY, SEG_OFF);
}

void mod_battery_init(void)
{
	menu_add_entry(" BATT", NULL, NULL, NULL, NULL, NULL, NULL,
		&battery_activate, &battery_deactivate);
}
