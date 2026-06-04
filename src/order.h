#ifndef TICKET_SYSTEM_ORDER_H
#define TICKET_SYSTEM_ORDER_H
enum OrderStatus { SUCCESS, PENDING, REFUSED };

class Order {
public:
    char trainID[21];
    char from[11];
    char to[11];
    int departureDate;          // 从6月1日起的天数
    int leaveTimeMinutes;       // 出发时间（分钟）
    int arriveTimeMinutes;      // 到达时间
    int price;                  // 总价
    int num;                    // 票数
    int timestamp;              // 下单时刻（用于排序）
    OrderStatus status;
};

class OrderManager {
public:
    // 为某用户添加订单（返回订单编号）
    int addOrder(const char* username, const Order& order);
    // 修改订单状态
    bool setOrderStatus(const char* username, int orderIdx, OrderStatus status);
    // 查询用户所有订单，按时间倒序输出
    bool queryOrders(const char* username, char* out);
    // 退订第 n 个订单（从新到旧）
    bool refundTicket(const char* username, int n);
};

// 候补项
struct WaitingItem {
    char username[21];
    char from[11];
    char to[11];
    int num;
    int orderIdx;               // 在原用户订单列表中的索引
    int timestamp;              // 加入队列时间（用于先进先出）
};

class WaitingQueue {
public:
    // 将用户加入候补
    void push(const char* trainID, int dateOffset, const WaitingItem& item);
    // 当有余票释放时，尝试满足队列中的订单
    void tryFulfill(const char* trainID, int dateOffset, SeatManager& seatMgr, OrderManager& orderMgr);
};
#endif //TICKET_SYSTEM_ORDER_H