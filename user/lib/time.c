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
        return 1; //是闰年
    return 0;   //是平年
}

int get_all_time(uint32_t Timestamp, uint32_t *yr, uint32_t *mon, uint32_t *dt, uint32_t *hr, uint32_t *mn, uint32_t *sec)
{

    uint16_t year = 1970;
    uint32_t Counter = 0, CounterTemp; //随着年份迭加，Counter记录从1970 年 1 月 1 日（00:00:00 GMT）到累加到的年份的最后一天的秒数
    uint8_t Month[12] = {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
    uint8_t i;

    while (Counter <= Timestamp)    //假设今天为2018年某一天，则时间戳应小于等于1970-1-1 0:0:0 到 2018-12-31 23:59:59的总秒数
    {
        CounterTemp = Counter;			 //CounterTemp记录完全1970-1-1 0:0:0 到 2017-12-31 23:59:59的总秒数后退出循环
        Counter += 31536000; //加上今年（平年）的秒数
        if (is_leap_year(year))
        {
            Counter += 86400; //闰年多加一天
        }
        year++;
    }
    *yr = year - 1; //跳出循环即表示到达计数值当前年
    Month[1] = (is_leap_year(*yr) ? 29 : 28);
    Counter = Timestamp - CounterTemp; //Counter = Timestamp - CounterTemp  记录2018年已走的总秒数
    CounterTemp = Counter / 86400;        //CounterTemp = Counter/(24*3600)  记录2018年已【过去】天数
    Counter %= 86400;      //记录今天已走的总秒数
    *hr = Counter / 3600; //时
    *mn = Counter % 3600 / 60; //分
    *sec = Counter % 60; //秒
    for (i = 0; i < 12; i++)
    {
        if (CounterTemp < Month[i])    									//不能包含相等的情况，相等会导致最后一天切换到下一个月第一天时
        {
            //（即CounterTemp已走天数刚好为n个月完整天数相加时（31+28+31...）），
            *mon = i + 1;			  									// 月份不加1，日期溢出（如：出现32号）
            *dt = CounterTemp + 1; 								//应不作处理，CounterTemp = Month[i] = 31时，会继续循环，月份加一，
            break;																				//日期变为1，刚好符合实际日期
        }
        CounterTemp -= Month[i];
    }
    return 0;
}
