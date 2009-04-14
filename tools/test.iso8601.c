/* 
 * Copyright (C) 2005 Andrew Beekhof <andrew@beekhof.net>
 * 
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 * 
 * This software is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 * 
 * You should have received a copy of the GNU General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

#include <crm_internal.h>
#include <crm/common/iso8601.h>
#include <unistd.h>

char command = 0;

static struct crm_option long_options[] = {
    /* Top-level Options */
    {"help",    0, 0, '?', "\tThis text"},
    {"version", 0, 0, '$', "\tVersion information"  },
    {"verbose", 0, 0, 'V', "\tIncrease debug output"},

    {"-spacer-",    0, 0, '-', "\nCommands:"},
    {"now",      0, 0, 'n', "\tDisplay the current date/time"},
    {"date",     1, 0, 'd', "Parse an ISO8601 date/time.  Eg. '2005-01-20 00:30:00 +01:00' or '2005-040'"},
    {"period",   1, 0, 'p', "Parse an ISO8601 date/time with interval/period (wth start time).  Eg. '2005-040/2005-043'"},
    {"duration", 1, 0, 'D', "Parse an ISO8601 date/time with duration (wth start time). Eg. '2005-040/P1M'"},

    {"-spacer-",0, 0, '-', "\nOutput Modifiers:"},
    {"local",   0, 0, 'L', "\tShow result as a 'local' date/time"},
    {"ordinal", 0, 0, 'O', "\tShow result as an 'ordinal' date/time"},
    {"week",    0, 0, 'W', "\tShow result as an 'calendar week' date/time"},
    {"-spacer-",0, 0, '-', "\nFor more information on the ISO8601 standard, see: http://en.wikipedia.org/wiki/ISO_8601"},
    
    {0, 0, 0, 0}
};

int
main(int argc, char **argv)
{
	int argerr = 0;
	int flag;
	int index = 0;
	int print_options = 0;
	char *input_s = NULL;
	char *mutable_s = NULL;
	
	crm_log_init("iso8601", LOG_INFO, FALSE, TRUE, 0, NULL);
	crm_set_options("V?d:p:D:WOLn", "command [output modifier] ", long_options, "Display and parse ISO8601 dates and times");
	
	if(argc < 2) {
		argerr++;
	}

	while (1) {
		flag = crm_get_option(argc, argv, &index);
		if (flag == -1)
			break;

		switch(flag) {
			case 'V':
				cl_log_enable_stderr(TRUE);
				alter_debug(DEBUG_INC);
				break;
			case '?':
			case '$':
				crm_help(flag, 0);
				break;
			case 'n':
				command = flag;
				break;
			case 'd':
			case 'p':
			case 'D':
				command = flag;
				input_s = crm_strdup(optarg);
				break;
			case 'W':
				print_options |= ha_date_weeks;
				break;
			case 'O':
				print_options |= ha_date_ordinal;
				break;
			case 'L':
				print_options |= ha_log_local;
				break;
		}
	}

	if(input_s == NULL && command != 'n') {
		crm_help('?', 1);
	}
	
	mutable_s = input_s;

	if(command == 'd') {
		ha_time_t *date_time = parse_date(&mutable_s);
		if(date_time == NULL) {
			fprintf(stderr, "Invalid date/time specified: %s\n", input_s);
			crm_help('?',1);
		}
		log_date(LOG_INFO, "parsed", date_time,
			 print_options|ha_log_date|ha_log_time);
		
	} else if(command == 'p') {
		ha_time_period_t *interval = parse_time_period(&mutable_s);
		if(interval == NULL) {
			fprintf(stderr, "Invalid interval specified: %s\n", input_s);
			crm_help('?',1);
		}
		log_time_period(LOG_INFO, interval,
				print_options|ha_log_date|ha_log_time);
		
	} else if(command == 'D') {
		ha_time_t *duration = parse_time_duration(&mutable_s);
		if(duration == NULL) {
			fprintf(stderr, "Invalid duration specified: %s\n", input_s);
			crm_help('?',1);
		}
		log_date(LOG_INFO, "Duration", duration,
			 print_options|ha_log_date|ha_log_time|ha_log_local);

	} else if(command == 'n') {
		ha_time_t *now = new_ha_date(TRUE);
		if(now == NULL) {
			fprintf(stderr, "Internal error: couldnt determin 'now' !\n");
			crm_help('?',1);
		}
		log_date(LOG_INFO, "Current date/time", now,
			 print_options|ha_log_date|ha_log_time);
	}
	
	return 0;
}
