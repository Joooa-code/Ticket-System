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
    bool operator>(const User& other) const {
        return strcmp(username, other.username) > 0;
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
    Username(const Username& other){
        strncpy(username, other.username, 20);
        username[20] = '\0';
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
    bool operator!=(const Username& other) const {
        return strcmp(username, other.username) != 0;
    }
    bool operator==(const Username& other) const {
        return strcmp(username, other.username) == 0;
    }
};
class UserManager {
private:
    BPlusTree<Username, User> user_map;
    sjtu::map<Username, int> login_list;  // 用户名->权限
public:
    UserManager():user_map("user_leaf", "user_tree") {}
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
        // 补充empty()实现
        if (user_map.empty()) {
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
            strncpy(target.name, newName, 15);
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
    void clean() {
        // 清空在线列表
        login_list.clear();
        // 析构 B+ 树
        user_map.~BPlusTree();
        // 删除文件
        std::remove("user_tree");
        std::remove("user_leaf");
        // 重建 B+ 树
        new (&user_map) BPlusTree<Username, User>("user_leaf", "user_tree");
    }

};

#endif //TICKET_SYSTEM_USER_H