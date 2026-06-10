#ifndef TICKET_SYSTEM_UTILS_H
#define TICKET_SYSTEM_UTILS_H
#include <stdio.h>
// 字符串分割（按 '|'）
void split(const char* src, char delim, ArrayList<char*>& out);

// 时间转换 "hh:mm" -> 分钟
int timeToMinutes(const char* date);
// 日期转换 "mm-dd" -> 距6月1日的天数
int dateToOffset(const char* date) {
    int month, day;
    sscanf(date, "%d-%d", &month, &day);
    // 每月天数
    const int daysInMonth[] = {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
    int offset = 0;
    for (int m = 6; m < month; ++m) {   // 从6月到 month-1 月
        offset += daysInMonth[m - 1];
    }
    offset += (day - 1);
    return offset;
}
int daysInMonth(int month) {
    static const int days[] = {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
    return days[month-1];
}
void offsetToDate(int offset, char* out) {
    int day = offset + 1;
    int month = 6;
    while (day > daysInMonth(month)) {
        day -= daysInMonth(month);
        ++month;
    }
    sprintf(out, "%02d-%02d", month, day);
}
// 分钟转 "mm-dd hh:mm"
void minutesToDateTime(int baseDateOffset, int minutes, char* out);

// 简单排序（快速排序），用于 query_ticket 的结果集排序
template<typename T>
void quickSort(T* arr, int left, int right, bool (*cmp)(const T&, const T&));
#endif //TICKET_SYSTEM_UTILS_H