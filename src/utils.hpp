#pragma once

#include <string>
#include <sys/socket.h>
#include <vector>

class token {
public:
  int type;
  std::vector<std::string> sVal;
  int iVal;

  token(std::vector<std::string> s) : type(0), sVal(s) {}
  token(int i) : type(1), iVal(i) {}
};

long long now_ms();

std::string bulk_str(std::string str);

std::string simple_string(std::string str);

std::string resp_int(int n);

std::string resp_array(std::vector<std::string> v);

void ok(int client_fd);

void null(int client_fd);

int parse_tokens(int i, const std::string &buf, std::vector<token> &ans);

std::vector<token> parse_command(char buffer[1024], ssize_t bytes_received);
