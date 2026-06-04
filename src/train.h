#ifndef TICKET_SYSTEM_TRAIN_H
#define TICKET_SYSTEM_TRAIN_H
class Train {
public:
    char trainID[21];
    int stationNum;                // 站数
    char stations[100][31];        // 每个站名最多10汉字
    int seatNum;                   // 总座位数
    int prices[100];               // 区间票价，长度 stationNum-1
    int startTimeMinutes;          // 发车时间，从00:00转换为分钟
    int travelTimes[100];          // 行车时间（分钟），长度 stationNum-1
    int stopoverTimes[100];        // 停留时间，长度 stationNum-2
    int saleStart;                 // 开售日期，从6月8日算起的第几天（0~92）
    int saleEnd;
    char type;                     // 火车类型，如 'G'
    bool released;                 // 是否已发布
    // 辅助缓存：每个站的累计时间和累计票价（用于快速查询）
    int cumulativeTime[100];       // 从始发站到第i站的累计分钟
    int cumulativePrice[100];
};
class TrainManager {
private:
    // 车次索引文件
    // 车次数据文件
public:
    bool addTrain(const Train& t);
    bool deleteTrain(const char* trainID);
    bool releaseTrain(const char* trainID);
    bool queryTrain(const char* trainID, int dateOffset, char* out); // dateOffset 为距离6月1日的天数
    Train* getTrain(const char* trainID);
    bool isReleased(const char* trainID);
};

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
#endif //TICKET_SYSTEM_TRAIN_H