#ifndef TICKET_SYSTEM_UTILS_H
#define TICKET_SYSTEM_UTILS_H
#include <cstdio>
// 时间转换 "hh:mm" -> 分钟
int timeToMinutes(const char* date);
inline int daysInMonth(int month) {
    static const int days[] = {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
    return days[month-1];
}
// 日期转换 "mm-dd" -> 距6月1日的天数
inline int dateToOffset(const char* date) {
    int month, day;
    sscanf(date, "%d-%d", &month, &day);
    int offset = 0;
    for (int m = 6; m < month; ++m) {
        offset += daysInMonth(m);
    }
    offset += (day - 1);
    return offset;
}
inline void offsetToDate(int offset, char* out) {
    int day = offset + 1;
    int month = 6;
    while (day > daysInMonth(month)) {
        day -= daysInMonth(month);
        ++month;
    }
    sprintf(out, "%02d-%02d", month, day);
}
inline void formatTime(int absMin, char* dateStr, char* timeStr) {
    int day = absMin / 1440;
    int minute = absMin % 1440;
    int month = 6, dayOfMonth = day + 1;
    while (dayOfMonth > daysInMonth(month)) {
        dayOfMonth -= daysInMonth(month);
        ++month;
    }
    sprintf(dateStr, "%02d-%02d", month, dayOfMonth);
    sprintf(timeStr, "%02d:%02d", minute / 60, minute % 60);
}
// 分钟转 "mm-dd hh:mm"
void minutesToDateTime(int baseDateOffset, int minutes, char* out);

// 快速排序
template<typename T, typename Compare>
void quickSort(sjtu::vector<T>& arr, int left, int right, Compare cmp) {
    if (left >= right) return;
    int i = left, j = right;
    T pivot = arr[(left + right) / 2];
    while (i <= j) {
        while (cmp(arr[i], pivot)) ++i;
        while (cmp(pivot, arr[j])) --j;
        if (i <= j) {
            T tmp = arr[i];
            arr[i] = arr[j];
            arr[j] = tmp;
            ++i; --j;
        }
    }
    quickSort(arr, left, j, cmp);
    quickSort(arr, i, right, cmp);
}
#endif //TICKET_SYSTEM_UTILS_H