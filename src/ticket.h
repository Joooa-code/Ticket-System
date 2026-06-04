#ifndef TICKET_SYSTEM_TICKET_H
#define TICKET_SYSTEM_TICKET_H
class SeatManager {
public:
    // 获取某车次某天某区间的余票
    int getRemainSeat(const char* trainID, int dateOffset, int segmentIdx);
    // 扣减余票（购票成功时）
    bool deductSeat(const char* trainID, int dateOffset, int fromSegment, int toSegment, int num);
    // 增加余票（退票时）
    bool addSeat(const char* trainID, int dateOffset, int fromSegment, int toSegment, int num);
    // 持久化到文件
};
#endif //TICKET_SYSTEM_TICKET_H