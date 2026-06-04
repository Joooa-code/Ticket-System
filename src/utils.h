#ifndef TICKET_SYSTEM_UTILS_H
#define TICKET_SYSTEM_UTILS_H
// 字符串分割（按 '|'）
void split(const char* src, char delim, ArrayList<char*>& out);

// 时间转换 "hh:mm" -> 分钟
int timeToMinutes(const char* timeStr);
// 日期转换 "mm-dd" -> 距6月8日的天数
int dateToOffset(const char* dateStr);
// 分钟转 "mm-dd hh:mm"
void minutesToDateTime(int baseDateOffset, int minutes, char* out);

// 简单排序（快速排序），用于 query_ticket 的结果集排序
template<typename T>
void quickSort(T* arr, int left, int right, bool (*cmp)(const T&, const T&));
#endif //TICKET_SYSTEM_UTILS_H