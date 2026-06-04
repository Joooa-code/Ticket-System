#ifndef TICKET_SYSTEM_SYSTEM_H
#define TICKET_SYSTEM_SYSTEM_H
class System {
private:
    UserManager userMgr;
    TrainManager trainMgr;
    SeatManager seatMgr;
    OrderManager orderMgr;

    void handleAddUser(const ArgParser& args);
    void handleLogin(const ArgParser& args);
    void handleLogout(const ArgParser& args);
    void handleQueryProfile(const ArgParser& args);
    void handleModifyProfile(const ArgParser& args);
    void handleAddTrain(const ArgParser& args);
    void handleDeleteTrain(const ArgParser& args);
    void handleReleaseTrain(const ArgParser& args);
    void handleQueryTrain(const ArgParser& args);
    void handleQueryTicket(const ArgParser& args);
    void handleQueryTransfer(const ArgParser& args);  // 关键
    void handleBuyTicket(const ArgParser& args);
    void handleQueryOrder(const ArgParser& args);
    void handleRefundTicket(const ArgParser& args);
    void handleClean(const ArgParser& args);
    void handleExit(const ArgParser& args);
public:
    void run();
};
#endif //TICKET_SYSTEM_SYSTEM_H