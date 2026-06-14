//
// Created by joooa on 2026/6/10.
//

#ifndef TICKET_SYSTEM_ORDER_H
#define TICKET_SYSTEM_ORDER_H
#include "bpt.h"
#include "train.h"
#include "user.h"
#include "memory_river.h"
#include "utils.h"
#include <cstring>
#include <climits>
#include <iostream>
enum OrderStatus { EMPTY, SUCCESS, PENDING, REFUSED };
struct OrderKey {
    char username[21];  // 订单发起者id
    int time;  // 下单时间的负值
    OrderKey():time(0) {
        username[0] = '\0';
    }
    OrderKey(const char* o, int t) {
        time = -t;
        strncpy(username, o, 20);
        username[20] = '\0';
    }
    bool operator<(const OrderKey &other) const {
        int cmp = strcmp(username, other.username);
        if (cmp != 0) return cmp < 0;
        return time < other.time;
    }
    bool operator==(const OrderKey &other) const {
        return strcmp(username, other.username) == 0 && time == other.time;
    }
};
struct Ticket {
    char trainID[21];
    char from[31];
    char to[31];
    int leave_date;  // 出发日期偏移
    int leaveTimeAbs;  // 出发时间（分钟）,从6.1开始的绝对偏移
    int arriveTimeAbs;  // 到达时间
    int price;  // 总价
    int duration;  // 总用时
    int seat_num;  // 票数
    Ticket() : leave_date(0), leaveTimeAbs(0), arriveTimeAbs(0),
               price(0), duration(0), seat_num(0) {
        trainID[0] = from[0] = to[0] = '\0';
    }
};
class Order {
public:
    OrderKey order_info;
    Ticket ticket_info;
    OrderStatus status = EMPTY;
};
struct WaitingKey {
    int timestamp;  // 下单时间正值
    char trainID[21];
    int leave_date;  // 出发时间偏移
    WaitingKey() : leave_date(0), timestamp(0) { trainID[0] = '\0'; }
    WaitingKey(const char* tid, int date, int ts) {
        strncpy(trainID, tid, 20);
        trainID[20] = '\0';
        leave_date = date;
        timestamp = ts;
    }
    bool operator<(const WaitingKey& other) const {
        int cmp = strcmp(trainID, other.trainID);
        if (cmp != 0) return cmp < 0;
        if (leave_date != other.leave_date) return leave_date < other.leave_date;
        return timestamp < other.timestamp;   // 时间戳小的优先
    }
    bool operator==(const WaitingKey& other) const {
        return strcmp(trainID, other.trainID) == 0 &&
               leave_date == other.leave_date &&
               timestamp == other.timestamp;
    }
};

class OrderManager {
private:
    MemoryRiver<Order> order_storage;
    BPlusTree<OrderKey, int> order_tree;  // OrderKey->order offset
    BPlusTree<WaitingKey, int> waiting_tree;  // WaitingKey -> waiting order offset
    int global_time;  //  全局时间戳,持久化在order_storage的头信息里
    void fulfillWaiting(TrainManager& trainMgr, const char* trainID, int dateOffset) {
        WaitingKey start(trainID, dateOffset, 0);
        WaitingKey end(trainID, dateOffset, INT_MAX);
        sjtu::vector<KeyValue<WaitingKey, int>> range;
        waiting_tree.rangeQuery(start, end, range);
        for (const auto& kv : range) {
            int offset = kv.value;
            Order ord;
            order_storage.read(ord, offset);
            if (ord.status != PENDING) {
                waiting_tree.remove(kv.key, offset);
                continue;
            }
            const Ticket& tic = ord.ticket_info;
            Train* train = trainMgr.getTrain(tic.trainID);
            if (!train) continue;
            int fromIdx = -1, toIdx = -1;
            for (int i = 0; i < train->stationNum; ++i) {
                if (strcmp(train->stations[i], tic.from) == 0) fromIdx = i;
                if (strcmp(train->stations[i], tic.to) == 0) toIdx = i;
            }
            if (fromIdx == -1 || toIdx == -1 || fromIdx >= toIdx) {
                delete train;
                continue;
            }
            bool ok = true;
            for (int seg = fromIdx; seg < toIdx; ++seg) {
                if (!trainMgr.deductSeat(tic.trainID, dateOffset, seg, tic.seat_num)) {
                    ok = false;
                    // 若候补失败，恢复已被扣除的票数
                    for (int s = fromIdx; s < seg; ++s) {
                        trainMgr.addSeat(tic.trainID, dateOffset, s, tic.seat_num);
                    }
                    break;
                }
            }
            if (ok) {
                ord.status = SUCCESS;
                order_storage.update(ord, offset);
                waiting_tree.remove(kv.key, offset);
            }
            delete train;
        }
    }
public:
    OrderManager() : order_storage("order_data"),
                    order_tree("order_leaf", "order_tree"),
                    waiting_tree("waiting_leaf", "waiting_tree"),
                    global_time(0) {
        std::ifstream test("order_data", std::ios::binary);
        if (!test.is_open()) {
            order_storage.initialise();
            order_storage.write_info(0, 1);   // 初始时间戳为 0
        } else {
            test.close();
            order_storage.get_info(global_time, 1);
        }
    }
    // 添加订单（返回订单偏移）
    int addOrder(const Order& order) {
        int offset = order_storage.write(const_cast<Order&>(order));
        order_tree.insert(order.order_info, offset);
        return offset;
    }
    // 修改订单状态
    bool setOrderStatus(const OrderKey& key, OrderStatus status) {
        auto res = order_tree.find(key);
        if (res.empty()) return false;
        int offset = res[0];
        Order ord;
        order_storage.read(ord, offset);
        ord.status = status;
        order_storage.update(ord, offset);
        return true;
    }
    // 查询用户所有订单，按时间倒序输出
    void query_order(const char* username) {
        OrderKey start(username, INT_MIN);
        OrderKey end(username, 0);
        sjtu::vector<KeyValue<OrderKey, int>> range;
        order_tree.rangeQuery(start, end, range);
        std::cout << range.size() << '\n';
        // 遍历输出每个订单
        for (size_t i = 0; i < range.size(); ++i) {
            Order ord;
            order_storage.read(ord, range[i].value);
            const char* statusStr = (ord.status == SUCCESS) ? "success" :
                                    (ord.status == PENDING) ? "pending" : "refunded";
            char leaveDate[12], leaveTime[9], arriveDate[12], arriveTime[9];
            formatTime(ord.ticket_info.leaveTimeAbs, leaveDate, leaveTime);
            formatTime(ord.ticket_info.arriveTimeAbs, arriveDate, arriveTime);
            std::cout << '[' << statusStr << "] " << ord.ticket_info.trainID << ' '
                      << ord.ticket_info.from << ' ' << leaveDate << ' ' << leaveTime
                      << " -> " << ord.ticket_info.to << ' ' << arriveDate << ' ' << arriveTime
                      << ' ' << ord.ticket_info.price << ' ' << ord.ticket_info.seat_num << '\n';
        }
    }
    // 退订第 n 个订单（从新到旧）
    // 要求user已登录，稍后在外部实现
    void refund_ticket(TrainManager& trainMgr, const char* username, int n) {
        OrderKey start(username, INT_MIN);
        OrderKey end(username, 0);
        sjtu::vector<KeyValue<OrderKey, int>> range;
        order_tree.rangeQuery(start, end, range);
        if (n < 1 || n > (int)range.size()) {
            std::cout << -1 << '\n';
            return;
        }
        const auto& kv = range[n - 1];
        int offset = kv.value;
        Order ord;
        order_storage.read(ord, offset);
        // 订单无效
        if (ord.status == REFUSED) {
            std::cout << -1 << '\n';
            return;
        }
        if (ord.status == SUCCESS) {
            // 释放座位
            const Ticket& tic = ord.ticket_info;
            Train* train = trainMgr.getTrain(tic.trainID);
            if (!train) {
                std::cout << -1 << '\n';
                return;
            }
            int fromIdx = -1, toIdx = -1;
            for (int i = 0; i < train->stationNum; ++i) {
                if (strcmp(train->stations[i], tic.from) == 0) fromIdx = i;
                if (strcmp(train->stations[i], tic.to) == 0) toIdx = i;
            }
            if (fromIdx != -1 && toIdx != -1 && fromIdx < toIdx) {
                for (int seg = fromIdx; seg < toIdx; ++seg)
                    trainMgr.addSeat(tic.trainID, tic.leave_date, seg, tic.seat_num);
            }
            delete train;
            // 尝试满足候补队列
            fulfillWaiting(trainMgr, tic.trainID, tic.leave_date);
        }
        if (ord.status == PENDING) {
            // 从候补队列中删除
            WaitingKey wk(ord.ticket_info.trainID, ord.ticket_info.leave_date, -ord.order_info.time);
            auto res = waiting_tree.find(wk);
            if (!res.empty()) waiting_tree.remove(wk, res[0]);
        }
        ord.status = REFUSED;
        order_storage.update(ord, offset);
        std::cout << 0 << '\n';
        return;
    }
    // 购买车票
    // 要求user已登录，稍后在外部实现
    void buy_ticket(TrainManager& trainMgr, const char* username, const char* trainID,
                  const char* date, int num, const char* from, const char* to,bool allow_waiting) {
        if (!trainMgr.isReleased(trainID)) {
            std::cout << -1 << '\n';
            return;
        }
        int dateOffset = dateToOffset(date);
        Train* train = trainMgr.getTrain(trainID);
        if (!train) {
            std::cout << -1 << '\n';
            return;
        }
        int fromIdx = -1, toIdx = -1;
        for (int i = 0; i < train->stationNum; ++i) {
            if (strcmp(train->stations[i], from) == 0) fromIdx = i;
            if (strcmp(train->stations[i], to) == 0) toIdx = i;
        }
        if (fromIdx == -1 || toIdx == -1 || fromIdx >= toIdx) {
            delete train;
            std::cout << -1 << '\n';
            return;
        }
        if (dateOffset < train->saleStart || dateOffset > train->saleEnd) {
            delete train;
            std::cout << -1 << '\n';
            return;
        }
        // 检查余票
        int minSeat = 1e9;
        for (int seg = fromIdx; seg < toIdx; ++seg) {
            int s = trainMgr.getRemainSeat(trainID, dateOffset, seg);
            if (s < minSeat) minSeat = s;
        }
        int totalPrice = (train->prefixPrices[toIdx] - train->prefixPrices[fromIdx]) * num;
        // 更新全局时间戳并持久化
        int ts = ++global_time;
        order_storage.write_info(global_time, 1);
        if (minSeat >= num) {
            // 扣票
            for (int seg = fromIdx; seg < toIdx; ++seg)
                trainMgr.deductSeat(trainID, dateOffset, seg, num);
            // 创建订单
            Order ord;
            ord.status = SUCCESS;
            ord.ticket_info = Ticket();
            strcpy(ord.ticket_info.trainID, trainID);
            strcpy(ord.ticket_info.from, from);
            strcpy(ord.ticket_info.to, to);
            ord.ticket_info.leave_date = dateOffset;
            ord.ticket_info.leaveTimeAbs = dateOffset * 1440 + train->startTimeMinutes + train->leaveTimes[fromIdx];
            ord.ticket_info.arriveTimeAbs = dateOffset * 1440 + train->startTimeMinutes + train->arriveTimes[toIdx];
            ord.ticket_info.price = totalPrice;
            ord.ticket_info.seat_num = num;
            ord.ticket_info.duration = ord.ticket_info.arriveTimeAbs - ord.ticket_info.leaveTimeAbs;
            ord.order_info = OrderKey(username, ts);
            addOrder(ord);
            delete train;
            std::cout << totalPrice << '\n';
            return;
        }
        else if (allow_waiting) {
            // 候补
            Order ord;
            ord.status = PENDING;
            ord.ticket_info = Ticket();
            strcpy(ord.ticket_info.trainID, trainID);
            strcpy(ord.ticket_info.from, from);
            strcpy(ord.ticket_info.to, to);
            ord.ticket_info.leave_date = dateOffset;
            ord.ticket_info.leaveTimeAbs = dateOffset * 1440 + train->startTimeMinutes + train->leaveTimes[fromIdx];
            ord.ticket_info.arriveTimeAbs = dateOffset * 1440 + train->startTimeMinutes + train->arriveTimes[toIdx];
            ord.ticket_info.price = totalPrice;
            ord.ticket_info.seat_num = num;
            ord.ticket_info.duration = ord.ticket_info.arriveTimeAbs - ord.ticket_info.leaveTimeAbs;
            ord.order_info = OrderKey(username, ts);
            int offset = addOrder(ord);
            // 加入候补树
            WaitingKey wk(trainID, dateOffset, ts);
            waiting_tree.insert(wk, offset);
            delete train;
            std::cout << "queue" << '\n';
            return;
        }
        delete train;
        std::cout << -1 << '\n';
        return;
    }

   void query_ticket(TrainManager& trainMgr, const char* from, const char* to,
                  const char* date, const char* order_by) {
    int leaveDate = dateToOffset(date);
    bool byTime = (strcmp(order_by, "time") == 0);
    sjtu::vector<Ticket> tickets;
    sjtu::vector<StationValue> stationTrains = trainMgr.getStationTrains(from);
    for (size_t i = 0; i < stationTrains.size(); ++i) {
        const StationValue& sv = stationTrains[i];
        Train* train = trainMgr.getTrain(sv.trainID);
        if (!train || !train->released) { delete train; continue; }
        int toIdx = -1;
        for (int j = 0; j < train->stationNum; ++j) {
            if (strcmp(train->stations[j], to) == 0) { toIdx = j; break; }
        }
        if (toIdx == -1 || sv.stationIdx >= toIdx) { delete train; continue; }
        int baseDay = leaveDate;
        int baseAbs = baseDay * 1440 + train->startTimeMinutes;
        int leaveAbs = baseAbs + sv.departTimeMin;
        int arriveAbs = baseAbs + train->arriveTimes[toIdx];
        if (baseDay < train->saleStart || baseDay > train->saleEnd) { delete train; continue; }
        int price = train->prefixPrices[toIdx] - train->prefixPrices[sv.stationIdx];
        int minSeat = 1e9;
        for (int seg = sv.stationIdx; seg < toIdx; ++seg) {
            int s = trainMgr.getRemainSeat(sv.trainID, leaveDate, seg);
            if (s < minSeat) minSeat = s;
        }
        Ticket tic;
        strcpy(tic.trainID, sv.trainID);
        strcpy(tic.from, from);
        strcpy(tic.to, to);
        tic.leave_date = leaveDate;
        tic.leaveTimeAbs = leaveAbs;
        tic.arriveTimeAbs = arriveAbs;
        tic.price = price;
        tic.duration = arriveAbs - leaveAbs;
        tic.seat_num = minSeat;
        tickets.push_back(tic);
        delete train;
    }
    auto cmp = [byTime](const Ticket& a, const Ticket& b) -> bool {
        if (byTime) {
            if (a.duration != b.duration) return a.duration < b.duration;
            if (a.price != b.price) return a.price < b.price;
            return strcmp(a.trainID, b.trainID) < 0;
        } else {
            if (a.price != b.price) return a.price < b.price;
            if (a.duration != b.duration) return a.duration < b.duration;
            return strcmp(a.trainID, b.trainID) < 0;
        }
    };
    if (tickets.size() > 0) {
        quickSort(tickets, 0, tickets.size() - 1, cmp);
    }
    std::cout << tickets.size() << '\n';
    for (size_t i = 0; i < tickets.size(); ++i) {
        const Ticket& tic = tickets[i];
        char leaveDateStr[12], leaveTimeStr[9], arriveDateStr[12], arriveTimeStr[9];
        formatTime(tic.leaveTimeAbs, leaveDateStr, leaveTimeStr);
        formatTime(tic.arriveTimeAbs, arriveDateStr, arriveTimeStr);
        std::cout << tic.trainID << ' ' << from << ' ' << leaveDateStr << ' ' << leaveTimeStr
                  << " -> " << to << ' ' << arriveDateStr << ' ' << arriveTimeStr
                  << ' ' << tic.price << ' ' << tic.seat_num << '\n';
    }
    }
    void query_transfer(TrainManager& trainMgr, const char* from, const char* to,
                   const char* date, const char* orderBy) {
    int startDate = dateToOffset(date);
    bool byTime = (strcmp(orderBy, "time") == 0);
    struct TransferResult {
        Ticket t1;
        Ticket t2;
    };
    bool found = false;
    TransferResult best;
    sjtu::vector<StationValue> fromTrains = trainMgr.getStationTrains(from);
    for (size_t i = 0; i < fromTrains.size(); ++i) {
        const StationValue& sv1 = fromTrains[i];
        Train* trainA = trainMgr.getTrain(sv1.trainID);
        if (!trainA || !trainA->released) { delete trainA; continue; }
        int fromIdx = sv1.stationIdx;
        int baseDay = startDate;
        int baseAbs1 = baseDay * 1440 + trainA->startTimeMinutes;
        int leaveAbs1 = baseAbs1 + sv1.departTimeMin;
        for (int midIdx = fromIdx + 1; midIdx < trainA->stationNum; ++midIdx) {
            const char* midStation = trainA->stations[midIdx];
            if (strcmp(midStation, to) == 0) continue;
            int arriveAbs1 = baseAbs1 + trainA->arriveTimes[midIdx];
            int arriveDay = arriveAbs1 / 1440;
            sjtu::vector<StationValue> midTrains = trainMgr.getStationTrains(midStation);
            for (size_t j = 0; j < midTrains.size(); ++j) {
                const StationValue& sv2 = midTrains[j];
                if (strcmp(sv2.trainID, sv1.trainID) == 0) continue;
                Train* trainB = trainMgr.getTrain(sv2.trainID);
                if (!trainB || !trainB->released) { delete trainB; continue; }
                int toIdx = -1;
                for (int k = 0; k < trainB->stationNum; ++k) {
                    if (strcmp(trainB->stations[k], to) == 0) { toIdx = k; break; }
                }
                if (toIdx == -1 || sv2.stationIdx >= toIdx) { delete trainB; continue; }
                int depDay = arriveDay;
                int baseAbs2 = depDay * 1440 + trainB->startTimeMinutes;
                int leaveAbs2 = baseAbs2 + sv2.departTimeMin;
                if (leaveAbs2 < arriveAbs1) {
                    depDay++;
                    baseAbs2 = depDay * 1440 + trainB->startTimeMinutes;
                    leaveAbs2 = baseAbs2 + sv2.departTimeMin;
                }
                if (depDay < trainB->saleStart || depDay > trainB->saleEnd) { delete trainB; continue; }
                int arriveAbs2 = baseAbs2 + trainB->arriveTimes[toIdx];
                // 计算第一程最小余票
                int minSeat1 = 1e9;
                for (int seg = fromIdx; seg < midIdx; ++seg) {
                    int s = trainMgr.getRemainSeat(sv1.trainID, startDate, seg);
                    if (s < minSeat1) minSeat1 = s;
                }
                // 计算第二程最小余票
                int minSeat2 = 1e9;
                for (int seg = sv2.stationIdx; seg < toIdx; ++seg) {
                    int s = trainMgr.getRemainSeat(sv2.trainID, depDay, seg);
                    if (s < minSeat2) minSeat2 = s;
                }
                if (minSeat1 == 0 || minSeat2 == 0) { delete trainB; continue; }
                int price1 = trainA->prefixPrices[midIdx] - trainA->prefixPrices[fromIdx];
                int price2 = trainB->prefixPrices[toIdx] - trainB->prefixPrices[sv2.stationIdx];
                int totalPrice = price1 + price2;
                int totalTime = arriveAbs2 - leaveAbs1;
                // 构造两个 Ticket
                Ticket t1, t2;
                strcpy(t1.trainID, sv1.trainID);
                strcpy(t1.from, from);
                strcpy(t1.to, midStation);
                t1.leave_date = startDate;
                t1.leaveTimeAbs = leaveAbs1;
                t1.arriveTimeAbs = arriveAbs1;
                t1.price = price1;
                t1.duration = arriveAbs1 - leaveAbs1;
                t1.seat_num = minSeat1;
                strcpy(t2.trainID, sv2.trainID);
                strcpy(t2.from, midStation);
                strcpy(t2.to, to);
                t2.leave_date = depDay;
                t2.leaveTimeAbs = leaveAbs2;
                t2.arriveTimeAbs = arriveAbs2;
                t2.price = price2;
                t2.duration = arriveAbs2 - leaveAbs2;
                t2.seat_num = minSeat2;
                if (!found) {
                    found = true;
                    best.t1 = t1;
                    best.t2 = t2;
                }
                else {
                    bool better = false;
                    if (byTime) {
                        if (totalTime != (best.t2.arriveTimeAbs - best.t1.leaveTimeAbs))
                            better = totalTime < (best.t2.arriveTimeAbs - best.t1.leaveTimeAbs);
                        else if (totalPrice != (best.t1.price + best.t2.price))
                            better = totalPrice < (best.t1.price + best.t2.price);
                        else if (strcmp(t1.trainID, best.t1.trainID) != 0)
                            better = strcmp(t1.trainID, best.t1.trainID) < 0;
                        else
                            better = strcmp(t2.trainID, best.t2.trainID) < 0;
                    } else {
                        if (totalPrice != (best.t1.price + best.t2.price))
                            better = totalPrice < (best.t1.price + best.t2.price);
                        else if (totalTime != (best.t2.arriveTimeAbs - best.t1.leaveTimeAbs))
                            better = totalTime < (best.t2.arriveTimeAbs - best.t1.leaveTimeAbs);
                        else if (strcmp(t1.trainID, best.t1.trainID) != 0)
                            better = strcmp(t1.trainID, best.t1.trainID) < 0;
                        else
                            better = strcmp(t2.trainID, best.t2.trainID) < 0;
                    }
                    if (better) {
                        best.t1 = t1;
                        best.t2 = t2;
                    }
                }
                delete trainB;
            }
        }
        delete trainA;
    }
    if (!found) {
        std::cout << "0\n";
        return;
    }
    // 输出第一程
    char leaveDate1[12], leaveTime1[9], arriveDate1[12], arriveTime1[9];
    formatTime(best.t1.leaveTimeAbs, leaveDate1, leaveTime1);
    formatTime(best.t1.arriveTimeAbs, arriveDate1, arriveTime1);
    std::cout << best.t1.trainID << ' ' << best.t1.from << ' ' << leaveDate1 << ' ' << leaveTime1
              << " -> " << best.t1.to << ' ' << arriveDate1 << ' ' << arriveTime1
              << ' ' << best.t1.price << ' ' << best.t1.seat_num << '\n';
    // 输出第二程
    char leaveDate2[12], leaveTime2[9], arriveDate2[12], arriveTime2[9];
    formatTime(best.t2.leaveTimeAbs, leaveDate2, leaveTime2);
    formatTime(best.t2.arriveTimeAbs, arriveDate2, arriveTime2);
    std::cout << best.t2.trainID << ' ' << best.t2.from << ' ' << leaveDate2 << ' ' << leaveTime2
              << " -> " << best.t2.to << ' ' << arriveDate2 << ' ' << arriveTime2
              << ' ' << best.t2.price << ' ' << best.t2.seat_num << '\n';
    }
};

#endif //TICKET_SYSTEM_ORDER_H