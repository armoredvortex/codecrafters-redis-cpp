#pragma once

#include "utils.hpp"
#include <condition_variable>
#include <functional>
#include <map>
#include <mutex>
#include <string>

using CommandHandler = std::function<void(int, const std::vector<token> &)>;

extern std::mutex store_mutex;
extern std::condition_variable store_cv;
extern std::map<std::string, CommandHandler> command_store;
extern std::map<std::string,
                std::tuple<int, std::vector<std::string>, int, long long>>
    store;

void c_ping(int client_fd, const std::vector<token> &args);
void c_echo(int client_fd, const std::vector<token> &args);
void c_set(int client_fd, const std::vector<token> &args);
void c_get(int client_fd, const std::vector<token> &args);
void c_rpush(int client_fd, const std::vector<token> &args);
void c_lpush(int client_fd, const std::vector<token> &args);
void c_lrange(int client_fd, const std::vector<token> &args);
void c_llen(int client_fd, const std::vector<token> &args);
void c_lpop(int client_fd, const std::vector<token> &args);
void c_blpop(int client_fd, const std::vector<token> &args);
