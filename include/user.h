#ifndef TICKET_SYSTEM_USER_H
#define TICKET_SYSTEM_USER_H
#include <cstring>
#include "bpt.h"
#include "map.h"
class User {
public:
    char username[21];
    char password[31];
    char name[16];        // 最多5个汉字，UTF-8 下每个汉字3字节
    char mailAddr[31];
    int privilege;        // 0~10
    User() :privilege(0){
        username[0] = password[0] = name[0] = mailAddr[0] = '\0';
    }
    bool operator<(const User& other) const {
        return strcmp(username, other.username) < 0;
    }
    bool operator==(const User& other) const {
        return strcmp(username, other.username) == 0;
    }
};
struct Username {
    char username[21];
    Username() {
        username[0] = '\0';
    }
    Username(const char* name) {
        strncpy(username, name, 20);
        username[20] = '\0';
    }
    bool operator<(const Username& other) const {
        return strcmp(username, other.username) < 0;
    }
    bool operator>(const Username& other) const {
        return strcmp(username, other.username) > 0;
    }
    bool operator==(const Username& other) const {
        return strcmp(username, other.username) == 0;
    }
};
class UserManager {
private:
    BPlusTree<Username, User> user_map;
    sjtu::map<Username, int> login_list;  // 用户名->权限
    int user_count = 0;
public:
    UserManager() {
        user_map("user_tree", "user_leaf");
    }
    bool isOnline(const char* username) {
        return login_list.find(Username(username)) != login_list.end();
    }
    int  getPrivilege(const char* username) {
        auto it = login_list.find(Username(username));
        if (it != login_list.end()) return it->second;
        return -1;   // 该用户未登录
    }
    int addUser(const char* curUser, const char* username, const char* password,
                 const char* name, const char* mail, int privilege) {
        if (!user_map.find(Username(username)).empty()) return -1;

        // 创建第一个用户
        if (user_count == 0) {
            User newUser;
            strncpy(newUser.username, username, 20);
            newUser.username[20] = '\0';
            strncpy(newUser.password, password, 30);
            newUser.password[30] = '\0';
            strncpy(newUser.name, name, 15);
            newUser.name[15] = '\0';
            strncpy(newUser.mailAddr, mail, 30);
            newUser.mailAddr[30] = '\0';
            newUser.privilege = 10;
            user_map.insert(Username(username), newUser);
            ++user_count;
            return 0;
        }

        if (!isOnline(curUser)) return -1;
        int cur_priv = getPrivilege(curUser);
        if (cur_priv <= privilege) return -1;

        User newUser;
        strncpy(newUser.username, username, 20);
        newUser.username[20] = '\0';
        strncpy(newUser.password, password, 30);
        newUser.password[30] = '\0';
        strncpy(newUser.name, name, 15);
        newUser.name[15] = '\0';
        strncpy(newUser.mailAddr, mail, 30);
        newUser.mailAddr[30] = '\0';
        newUser.privilege = privilege;
        user_map.insert(Username(username), newUser);
        ++user_count;
        return 0;
    }
    int login(const char* username, const char* password) {
        if (isOnline(username)) return -1;
        auto res = user_map.find(Username(username));
        if (res.empty()) return -1;
        const User& u = res[0];
        if (strcmp(u.password, password) != 0) return -1;
        login_list[Username(username)] = u.privilege;
        return 0;
    }
    int logout(const char* username) {
        auto it = login_list.find(Username(username));
        if (it == login_list.end()) return -1;
        login_list.erase(it);
        return 0;
    }
    int queryProfile(const char* curUser, const char* targetUser, char* out) {
        if (!isOnline(curUser)) return -1;
        int cur_priv = getPrivilege(curUser);
        auto res = user_map.find(Username(targetUser));
        if (res.empty()) return -1;
        const User& target = res[0];
        if (cur_priv < target.privilege && strcmp(curUser, targetUser) != 0) {
            return -1;
        }
        sprintf(out, "%s %s %s %d", target.username, target.name, target.mailAddr, target.privilege);
        return 0;
    }
    // 不修改的参数用nullptr
    int modifyProfile(const char* curUser, const char* targetUser, const char* newPwd,
                       const char* newName, const char* newMail, int newPriv, char* out) {
        if (!isOnline(curUser)) return -1;
        int cur_priv = getPrivilege(curUser);
        auto res = user_map.find(Username(targetUser));
        if (res.empty()) return -1;
        User target = res[0];
        if (cur_priv < target.privilege && strcmp(curUser, targetUser) != 0) {
            return -1;
        }
        if (newPriv != -1 && newPriv >= cur_priv) return -1;
        if (newPwd) {
            strncpy(target.password, newPwd, 30);
            target.password[30] = '\0';
        }
        if (newName) {
            strncpy(target.name, newName, 50);
            target.name[15] = '\0';
        }
        if (newMail) {
            strncpy(target.mailAddr, newMail, 30);
            target.mailAddr[30] = '\0';
        }
        if (newPriv != -1) {
            target.privilege = newPriv;
        }
        user_map.remove(Username(targetUser), res[0]);
        user_map.insert(Username(targetUser), target);
        auto it = login_list.find(Username(targetUser));
        if (it != login_list.end()) {
            it->second = target.privilege;
        }
        sprintf(out, "%s %s %s %d", target.username, target.name, target.mailAddr, target.privilege);
        return 0;
    }


};

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
#endif //TICKET_SYSTEM_USER_H