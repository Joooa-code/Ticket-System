//
// Created by joooa on 2026/6/4.
//
#include <iostream>
#include <string>
#include <sstream>
#include <cstring>
#include "include/user.h"
#include "include/train.h"
#include "include/order.h"
#include "include/utils.h"
#include "include/vector.h"
#include "include/map.h"
using namespace std;

UserManager userMgr;
TrainManager trainMgr;
OrderManager orderMgr;

// 按 '|' 分割字符串
sjtu::vector<string> split(const string& s, char delim) {
    sjtu::vector<string> res;
    size_t start = 0, end;
    while ((end = s.find(delim, start)) != string::npos) {
        if (end > start) res.push_back(s.substr(start, end - start));
        start = end + 1;
    }
    if (start < s.size()) res.push_back(s.substr(start));
    return res;
}

int main() {
    ios::sync_with_stdio(false);
    cin.tie(nullptr);

    string line;
    while (getline(cin, line)) {
        // 解析时间戳
        size_t pos1 = line.find('[');
        size_t pos2 = line.find(']');
        int timestamp = stoi(line.substr(pos1 + 1, pos2 - pos1 - 1));
        string rest = line.substr(pos2 + 1);
        // 去除前导空格
        size_t start = rest.find_first_not_of(" \t");
        if (start == string::npos) continue;
        rest = rest.substr(start);
        // 分割命令和参数
        sjtu::vector<string> tokens;
        stringstream ss(rest);
        string token;
        while (ss >> token) tokens.push_back(token);
        if (tokens.empty()) continue;
        string cmd = tokens[0];
        // 解析参数 -x value
        sjtu::map<string, string> args;
        for (size_t i = 1; i < tokens.size(); ++i) {
            if (tokens[i][0] == '-' && tokens[i].size() == 2) {
                string key = tokens[i].substr(1);
                if (i + 1 < tokens.size()) {
                    args[key] = tokens[i + 1];
                    ++i;
                }
            }
        }
        if (cmd == "add_user") {
            const char* cur = args["c"].c_str();
            const char* u = args["u"].c_str();
            const char* p = args["p"].c_str();
            const char* n = args["n"].c_str();
            const char* m = args["m"].c_str();
            int g = stoi(args["g"]);
            int ok = userMgr.addUser(cur, u, p, n, m, g);
            cout << "[" << timestamp << "] " << (ok == 0 ? "0" : "-1") << "\n";
        }
        else if (cmd == "login") {
            const char* u = args["u"].c_str();
            const char* p = args["p"].c_str();
            int ok = userMgr.login(u, p);
            cout << "[" << timestamp << "] " << (ok == 0 ? "0" : "-1") << "\n";
        }
        else if (cmd == "logout") {
            const char* u = args["u"].c_str();
            int ok = userMgr.logout(u);
            cout << "[" << timestamp << "] " << (ok == 0 ? "0" : "-1") << "\n";
        }
        else if (cmd == "query_profile") {
            const char* cur = args["c"].c_str();
            const char* u = args["u"].c_str();
            char out[100];
            int ok = userMgr.queryProfile(cur, u, out);
            if (ok == 0) {
                cout << "[" << timestamp << "] " << out << "\n";
            }
            else {
                cout << "[" << timestamp << "] -1\n";
            }
        }
        else if (cmd == "modify_profile") {
            const char* cur = args["c"].c_str();
            const char* u = args["u"].c_str();
            const char* p = args.count("p") ? args["p"].c_str() : nullptr;
            const char* n = args.count("n") ? args["n"].c_str() : nullptr;
            const char* m = args.count("m") ? args["m"].c_str() : nullptr;
            int g = args.count("g") ? stoi(args["g"]) : -1;
            char out[100];
            int ok = userMgr.modifyProfile(cur, u, p, n, m, g, out);
            if (ok == 0) {
                cout << "[" << timestamp << "] " << out << "\n";
            } else {
                cout << "[" << timestamp << "] -1\n";
            }
        }
        else if (cmd == "add_train") {
            const char* i = args["i"].c_str();
            int n = stoi(args["n"]);
            int m = stoi(args["m"]);
            string s = args["s"];
            string p = args["p"];
            string x = args["x"];
            string t = args["t"];
            string o = args["o"];
            string d = args["d"];
            string y = args["y"];
            // 解析 stations
            sjtu::vector<string> stations = split(s, '|');
            // 解析 prices
            sjtu::vector<string> p_str = split(p, '|');
            sjtu:: vector<int> prices;
            for (auto& v : p_str) prices.push_back(stoi(v));
            // 解析 travelTimes
            sjtu::vector<string> t_str = split(t, '|');
            sjtu::vector<int> travelTimes;
            for (auto& v : t_str) travelTimes.push_back(stoi(v));
            // 解析 stopoverTimes
            sjtu::vector<int> stopoverTimes;
            if (o != "_") {
                sjtu::vector<string> o_str = split(o, '|');
                for (auto& v : o_str) stopoverTimes.push_back(stoi(v));
            }
            // 转为二维数组
            char stationArr[100][31];
            for (size_t idx = 0; idx < stations.size(); ++idx) {
                strncpy(stationArr[idx], stations[idx].c_str(), 30);
                stationArr[idx][30] = '\0';
            }
            // 分割售卖日期
            sjtu::vector<string> sale = split(d, '|');
            string saleStart = sale[0];
            string saleEnd = sale[1];
            int ret = trainMgr.addTrain(i, n, m, stationArr, prices.data, x.c_str(),
                                        travelTimes.data, stopoverTimes.data,
                                        saleStart.c_str(), saleEnd.c_str(), y[0]);
            cout << "[" << timestamp << "] " << (ret == 0 ? "0" : "-1") << "\n";
        }
        else if (cmd == "delete_train") {
            const char* i = args["i"].c_str();
            int ret = trainMgr.deleteTrain(i);
            cout << "[" << timestamp << "] " << (ret == 0 ? "0" : "-1") << "\n";
        }
        else if (cmd == "release_train") {
            const char* i = args["i"].c_str();
            int ret = trainMgr.releaseTrain(i);
            cout << "[" << timestamp << "] " << (ret == 0 ? "0" : "-1") << "\n";
        }
        else if (cmd == "query_train") {
            const char* i = args["i"].c_str();
            const char* d = args["d"].c_str();
            cout << "[" << timestamp << "] ";
            trainMgr.queryTrain(i, d);
        }
        else if (cmd == "query_ticket") {
            const char* s = args["s"].c_str();
            const char* t = args["t"].c_str();
            const char* d = args["d"].c_str();
            const char* p = args["p"].c_str();
            cout << "[" << timestamp << "] ";
            orderMgr.query_ticket(trainMgr, s, t, d, p);
        }
        else if (cmd == "query_transfer") {
            const char* s = args["s"].c_str();
            const char* t = args["t"].c_str();
            const char* d = args["d"].c_str();
            const char* p = args["p"].c_str();
            cout << "[" << timestamp << "] ";
            orderMgr.query_transfer(trainMgr, s, t, d, p);
        }
        else if (cmd == "buy_ticket") {
            const char* u = args["u"].c_str();
            if (!userMgr.isOnline(u)) {
                cout << -1 << '\n';
                continue;
            }
            const char* i = args["i"].c_str();
            const char* d = args["d"].c_str();
            int n = stoi(args["n"]);
            const char* f = args["f"].c_str();
            const char* t = args["t"].c_str();
            bool q = args.count("q") && args["q"] == "true";
            cout << "[" << timestamp << "] ";
            orderMgr.buy_ticket(trainMgr, u, i, d, n, f, t, q);
        }
        else if (cmd == "query_order") {
            const char* u = args["u"].c_str();
            cout << "[" << timestamp << "] ";
            orderMgr.query_order(u);
        }
        else if (cmd == "refund_ticket") {
            const char* u = args["u"].c_str();
            if (!userMgr.isOnline(u)) {
                cout << -1 << '\n';
                continue;
            }
            int n = stoi(args["n"]);
            cout << "[" << timestamp << "] ";
            orderMgr.refund_ticket(trainMgr, u, n);
        }
        else if (cmd == "clean") {
            userMgr.clean();
            trainMgr.clean();
            orderMgr.clean();
            cout << "[" << timestamp << "] 0\n";
        }
        else if (cmd == "exit") {
            cout << "[" << timestamp << "] bye\n";
            break;
        }
    }
    return 0;
}