#ifndef TICKET_SYSTEM_USER_H
#define TICKET_SYSTEM_USER_H
class User {
public:
    char username[21];
    char password[31];
    char name[16];        // 最多5个汉字，UTF-8 下每个汉字3字节，安全取11
    char mailAddr[31];
    int privilege;        // 0~10
};
class UserManager {
private:
    // 用户数据文件路径：
    // // 在线用户列表：
public:
    bool addUser(const char* curUser, const char* username, const char* password,
                 const char* name, const char* mail, int privilege);
    bool login(const char* username, const char* password);
    bool logout(const char* username);
    bool queryProfile(const char* curUser, const char* targetUser, char* out);
    bool modifyProfile(const char* curUser, const char* targetUser, const char* newPwd,
                       const char* newName, const char* newMail, int newPriv, char* out);
    bool isOnline(const char* username);
    int  getPrivilege(const char* username);
};
#endif //TICKET_SYSTEM_USER_H