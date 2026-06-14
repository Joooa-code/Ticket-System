#ifndef TICKET_SYSTEM_TRAIN_H
#define TICKET_SYSTEM_TRAIN_H
#include "bpt.h"
#include "vector.h"
#include "memory_river.h"
#include "utils.h"
#include <cstdio>
#include <cstring>
#include <iostream>
struct TrainKey {
    char trainID[21];
    TrainKey() {
        trainID[0] = '\0';
    }
    TrainKey(const char* id) {
        strncpy(trainID, id, 20);
        trainID[20] = '\0';
    }
    bool operator<(const TrainKey& other) const {
        return strcmp(trainID, other.trainID) < 0;
    }
    bool operator>(const TrainKey& other) const {
        return strcmp(trainID, other.trainID) > 0;
    }
    bool operator==(const TrainKey& other) const {
        return strcmp(trainID, other.trainID) == 0;
    }
};
struct StationKey {
    char stationID[31];
    StationKey() {
        stationID[0]='\0';
    }
    StationKey(const char* id1) {
        strncpy(stationID, id1, 30);
        stationID[30] = '\0';
    }
    bool operator<(const StationKey& other) const {
        int op = strcmp(stationID, other.stationID);
        return op < 0;
    }
    bool operator>(const StationKey& other) const {
        int op = strcmp(stationID, other.stationID);
        return op > 0;
    }
    bool operator==(const StationKey& other) const {
        return strcmp(stationID, other.stationID) == 0;
    }
};
struct StationValue {
    char trainID[21];
    int stationIdx;           // 该车次中的站序（0-based）
    int departTimeMin;        // 离开该站的绝对分钟（发车后偏移）
    int cumTime;              // 从始发站到该站的累计分钟
    int cumPrice;             // 从始发站到该站的累计票价
    StationValue():stationIdx(0), departTimeMin(0), cumTime(0), cumPrice(0) {
        trainID[0] = '\0';
    }
    bool operator<(const StationValue& other) const{
        int cmp = strcmp(trainID, other.trainID);
        return cmp < 0;
    }
    bool operator==(const StationValue& other) const{
        return strcmp(trainID, other.trainID) == 0;
    }
};
struct SeatKey {
    char trainID[21];  // 列车id
    int dateOffset;  // 列车发车时间（以偏移形式存储）
    SeatKey() : dateOffset(0){ trainID[0] = '\0'; }
    SeatKey(const char* tid, int date) {
        strncpy(trainID, tid, 20);
        trainID[20] = '\0';
        dateOffset = date;
    }
    bool operator<(const SeatKey& other) const {
        int cmp = strcmp(trainID, other.trainID);
        if (cmp != 0) return cmp < 0;
        return dateOffset < other.dateOffset;
    }
    bool operator>(const SeatKey& other) const {
        int cmp = strcmp(trainID, other.trainID);
        if (cmp != 0) return cmp > 0;
        return dateOffset > other.dateOffset;
    }
    bool operator==(const SeatKey& other) const {
        return strcmp(trainID, other.trainID) == 0 && dateOffset == other.dateOffset;
    }
};
struct SeatValue {
    int seat[99];  //从i到i + 1的余票
    int stationNum;  // 站数
    SeatValue() : stationNum(0) {
        for (int i = 0; i < 99; i++) {
            seat[i] = 0;
        }
    }
    bool operator<(const SeatValue& other) const{
        return false;
    }
    bool operator==(const SeatValue& other) const {
        return false;
    }
};
class Train {
public:
    char trainID[21];
    int stationNum;                // 站数
    char stations[100][31];        // 车站经过的所有站名
    int seatNum;                   // 总座位数
    int prefixPrices[100];        // 累加票价，即从始发站到当前站的总票价
    int startTimeMinutes;          // 发车时间的分钟表示
    int arriveTimes[100];          // 累加时间，即到达每一站所需时间（分钟）
    int leaveTimes[100];           // 累加时间，即离开每一站所需时间（分钟）
    int saleStart;                 // 开售日期，从6月1日算起的第几天
    int saleEnd;                   // 售卖时间区间：2026.6.1~2026.8.31
    char type;                     // 火车类型
    bool released;                 // 是否已发布
    Train() : stationNum(0), seatNum(0), startTimeMinutes(0), saleStart(0), saleEnd(0), type('\0'), released(false) {
        trainID[0] = '\0';
        for (int i = 0; i < 100; i++) {
            stations[i][0] = '\0';
            prefixPrices[i] = arriveTimes[i] = leaveTimes[i] = 0;
        }
    }
    bool operator<(const Train& other) const {
        return strcmp(trainID, other.trainID) < 0;
    }
    bool operator==(const Train& other) const {
        return strcmp(trainID, other.trainID) == 0;
    }
};
class TrainManager {
private:
    MemoryRiver<Train> train_info;
    BPlusTree<TrainKey, int> train_tree;
    BPlusTree<StationKey, StationValue> station_tree;
    MemoryRiver<SeatValue> seat_info;
    BPlusTree<SeatKey, int> seat_tree;
public:
    TrainManager():train_info("train.data"),
    train_tree("train_leaf", "train_tree"),
    station_tree("station_leaf", "station_tree"),
    seat_info("seat_data"),
    seat_tree("seat_leaf", "seat_tree"){}
    int addTrain(const char *trainID, int stationNum, int seatNum, const char stations[][31], const int* prices,
                 const char *startTime, const int* travelTimes, const int* stopoverTimes, const char *saleStart,
                 const char *saleEnd, char type) {
        if (!train_tree.find(TrainKey(trainID)).empty()) {
            return -1;
        }
        Train t;
        strncpy(t.trainID, trainID, 20);
        t.trainID[20] = '\0';
        t.stationNum = stationNum;
        t.seatNum = seatNum;
        for (int i = 0; i < stationNum; ++i) {
            strncpy(t.stations[i], stations[i], 30);
            t.stations[i][30] = '\0';
        }
        int hour, minute;
        sscanf(startTime, "%d:%d", &hour, &minute);
        t.startTimeMinutes = hour * 60 + minute;
        t.saleStart = dateToOffset(saleStart);
        t.saleEnd = dateToOffset(saleEnd);
        t.type = type;
        t.released = false;
        t.prefixPrices[0] = 0;
        for (int i = 1; i < stationNum; ++i) {
            t.prefixPrices[i] = t.prefixPrices[i - 1] + prices[i - 1];
        }
        t.arriveTimes[0] = 0;
        t.leaveTimes[0] = 0;
        for (int i = 1; i < stationNum; ++i) {
            // 到达第 i 站的时间 = 离开第 i-1 站的时间 + 第 i-1 段行车时间
            t.arriveTimes[i] = t.leaveTimes[i - 1] + travelTimes[i - 1];
            if (i == stationNum - 1) {
                // 终点站没有停留，离开时间等于到达时间
                t.leaveTimes[i] = t.arriveTimes[i];
            } else {
                // 中间站：离开时间 = 到达时间 + 停留时间
                t.leaveTimes[i] = t.arriveTimes[i] + stopoverTimes[i - 1];
            }
        }
        int offset = train_info.write(t);
        train_tree.insert(TrainKey(trainID), offset);
        return 0;
    }
    int deleteTrain(const char* trainID) {
        auto res = train_tree.find(TrainKey(trainID));
        if (res.empty()) return -1;
        int offset = res[0];
        Train t;
        train_info.read(t, offset);
        if (t.released) return -1;
        train_tree.remove(TrainKey(trainID), offset);
        return 0;
    }
    int releaseTrain(const char* trainID) {
        auto res = train_tree.find(TrainKey(trainID));
        if (res.empty()) return -1;
        int offset = res[0];
        Train t;
        train_info.read(t, offset);
        if (t.released) return -1;
        t.released = true;
        train_info.update(t, offset);
        // 更新车站索引
        for (int i = 0; i < t.stationNum; ++i) {
            StationKey key(t.stations[i]);
            StationValue val;
            strncpy(val.trainID, trainID, 20);
            val.trainID[20] = '\0';
            val.stationIdx = i;
            val.departTimeMin = t.leaveTimes[i];
            val.cumTime = t.arriveTimes[i];
            val.cumPrice = t.prefixPrices[i];
            station_tree.insert(key, val);
        }
        // 初始化余票：售卖区间内的每一天
        for (int date = t.saleStart; date <= t.saleEnd; ++date) {
            SeatKey sk(t.trainID, date);
            SeatValue sv;
            sv.stationNum = t.stationNum;
            for (int seg = 0; seg < t.stationNum-1; ++seg)
                sv.seat[seg] = t.seatNum;
            int seatOffset = seat_info.write(sv);
            seat_tree.insert(sk, seatOffset);
        }
        return 0;
    }
    void queryTrain(const char* trainID, const char* date) {
    int base_day = dateToOffset(date);
    auto res = train_tree.find(TrainKey(trainID));
    if (res.empty()) {
        std::cout << "-1\n";
        return;
    }
    Train t;
    train_info.read(t, res[0]);
    if (base_day < t.saleStart || base_day > t.saleEnd) {
        std::cout << "-1\n";
        return;
    }
    // 输出第一行
    std::cout << t.trainID << ' ' << t.type << '\n';
    int baseAbs = base_day * 1440 + t.startTimeMinutes;
    for (int i = 0; i < t.stationNum; ++i) {
        char arriveDate[12] = "xx-xx", arriveTime[9] = "xx:xx";
        char leaveDate[12] = "xx-xx", leaveTime[9] = "xx:xx";
        int price = t.prefixPrices[i];
        int seat = t.seatNum;
        if (i > 0) {
            int arriveAbs = baseAbs + t.arriveTimes[i];
            int day = arriveAbs / 1440;
            int minute = arriveAbs % 1440;
            offsetToDate(day, arriveDate);
            sprintf(arriveTime, "%02d:%02d", minute/60, minute%60);
        }
        if (i < t.stationNum-1) {
            int leaveAbs = baseAbs + t.leaveTimes[i];
            int day = leaveAbs / 1440;
            int minute = leaveAbs % 1440;
            offsetToDate(day, leaveDate);
            sprintf(leaveTime, "%02d:%02d", minute/60, minute%60);
            if (t.released) {
                SeatKey sk(t.trainID, base_day);
                auto seatRes = seat_tree.find(sk);
                if (!seatRes.empty()) {
                    SeatValue sv;
                    seat_info.read(sv, seatRes[0]);
                    seat = sv.seat[i];
                }
            }
        } else {
            strcpy(leaveDate, "xx-xx");
            strcpy(leaveTime, "xx:xx");
            seat = -1;
        }
        // 输出每一站的信息
        if (i == t.stationNum-1) {
            std::cout << t.stations[i] << ' ' << arriveDate << ' ' << arriveTime
                      << " -> " << leaveDate << ' ' << leaveTime
                      << ' ' << price << " x\n";
        } else {
            std::cout << t.stations[i] << ' ' << arriveDate << ' ' << arriveTime
                      << " -> " << leaveDate << ' ' << leaveTime
                      << ' ' << price << ' ' << seat << '\n';
        }
    }
}
    // 余票查询
    int getRemainSeat(const char* trainID, int dateOffset, int segmentIdx) {
        SeatKey key(trainID, dateOffset);
        auto res = seat_tree.find(key);
        if (res.empty()) return 0;
        SeatValue sv;
        seat_info.read(sv, res[0]);
        if (segmentIdx < 0 || segmentIdx >= sv.stationNum-1) return 0;
        return sv.seat[segmentIdx];
    }
    // 减少某车次某天某区间的座位（返回是否成功）
    bool deductSeat(const char* trainID, int dateOffset, int segmentIdx, int num) {
        SeatKey key(trainID, dateOffset);
        auto res = seat_tree.find(key);
        if (res.empty()) return false;
        SeatValue sv;
        seat_info.read(sv, res[0]);
        if (sv.seat[segmentIdx] < num) return false;
        sv.seat[segmentIdx] -= num;
        seat_info.update(sv, res[0]);
        return true;
    }
    // 增加座位（退票时使用）
    void addSeat(const char* trainID, int dateOffset, int segmentIdx, int num) {
        SeatKey key(trainID, dateOffset);
        auto res = seat_tree.find(key);
        if (res.empty()) return;
        SeatValue sv;
        seat_info.read(sv, res[0]);
        sv.seat[segmentIdx] += num;
        seat_info.update(sv, res[0]);
    }
    // 获取车次信息（返回指针，需手动delete）
    Train* getTrain(const char* trainID) {
        auto res = train_tree.find(TrainKey(trainID));
        if (res.empty()) return nullptr;
        Train* t = new Train();
        train_info.read(*t, res[0]);
        return t;
    }
    bool isReleased(const char* trainID) {
        Train* t = getTrain(trainID);
        if (!t) return false;
        bool ret = t->released;
        delete t;
        return ret;
    }
    sjtu::vector<StationValue> getStationTrains(const char* station) {
        return station_tree.find(StationKey(station));
    }
    void clean() {
        // 清空 MemoryRiver 文件
        train_info.initialise();
        seat_info.initialise();
        // 析构 B+ 树
        train_tree.~BPlusTree();
        station_tree.~BPlusTree();
        seat_tree.~BPlusTree();
        // 删除文件
        std::remove("train_leaf");
        std::remove("train_tree");
        std::remove("station_leaf");
        std::remove("station_tree");
        std::remove("seat_leaf");
        std::remove("seat_tree");
        // 重建 B+ 树
        new (&train_tree) BPlusTree<TrainKey, int>("train_leaf", "train_tree");
        new (&station_tree) BPlusTree<StationKey, StationValue>("station_leaf", "station_tree");
        new (&seat_tree) BPlusTree<SeatKey, int>("seat_leaf", "seat_tree");
    }
};

#endif //TICKET_SYSTEM_TRAIN_H