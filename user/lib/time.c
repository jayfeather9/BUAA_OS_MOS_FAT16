#include <env.h>
#include <lib.h>
#include <mmu.h>
#include <drivers/dev_rtc.h>

u_int get_time(u_int *us) {
	u_int temp = 1;
	syscall_write_dev(&temp, DEV_RTC_ADDRESS | DEV_RTC_TRIGGER_READ,
				sizeof(u_int));
	syscall_read_dev(&temp, DEV_RTC_ADDRESS | DEV_RTC_USEC,
				sizeof(u_int));
	*us = temp;
	syscall_read_dev(&temp, DEV_RTC_ADDRESS | DEV_RTC_SEC,
				sizeof(u_int));
	return temp;
}

uint8_t is_leap_year(uint16_t year)
{
    if (((year) % 4 == 0 && (year) % 100 != 0) || (year) % 400 == 0)
        return 1;
    return 0;
}

int get_all_time(uint32_t Timestamp, uint32_t *yr, uint32_t *mon, uint32_t *dt, uint32_t *hr, uint32_t *mn, uint32_t *sec)
{

    uint16_t year = 1970;
    uint32_t Counter = 0, CounterTemp;
    uint8_t Month[12] = {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
    uint8_t i;

    while (Counter <= Timestamp)
    {
        CounterTemp = Counter;
        Counter += 31536000;
        if (is_leap_year(year))
        {
            Counter += 86400;
        }
        year++;
    }
    *yr = year - 1;
    Month[1] = (is_leap_year(*yr) ? 29 : 28);
    Counter = Timestamp - CounterTemp;
    CounterTemp = Counter / 86400;
    Counter %= 86400;
    *hr = Counter / 3600;
    *mn = Counter % 3600 / 60;
    *sec = Counter % 60;
    for (i = 0; i < 12; i++)
    {
        if (CounterTemp < Month[i])
        {
            *mon = i + 1;
            *dt = CounterTemp + 1;
            break;
        }
        CounterTemp -= Month[i];
    }
    return 0;
}

u_int get_fat_time(uint32_t year, uint32_t month, uint32_t day, uint32_t hour, uint32_t minute, uint32_t second, uint32_t us, uint8_t *CrtTimeTenth, uint16_t *CrtTime, uint16_t *CrtDate) {
    *CrtTimeTenth = us / 10000 + (second % 2) * 100;
    *CrtTime = (hour << 11) + (minute << 5) + (second >> 1);
    *CrtDate = ((year - 1980) << 9) + (month << 5) + day;
    return 0;
}

void debug_print_date(uint16_t date) {
	debugf("%04u-%02u-%02u", ((date & 0xFE00) >> 9) + 1980, (date & 0x1E0) >> 5, (date & 0x1F));
}

void debug_print_time(uint16_t time) {
	debugf("%02u:%02u:%02u", (time & 0xF800) >> 11, (time & 0x7E0) >> 5, (time & 0x1F) * 2);
}