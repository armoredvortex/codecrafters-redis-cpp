#include "utils.hpp"
#include <chrono>
#include <string>
#include <sys/socket.h>
#include <vector>

long long now_ms() {
  return std::chrono::duration_cast<std::chrono::milliseconds>(
             std::chrono::steady_clock::now().time_since_epoch())
      .count();
}

std::string bulk_str(std::string str) {
  std::string resp;
  resp += '$' + std::to_string(str.size()) + "\r\n" + str + "\r\n";
  return resp;
}

std::string simple_string(std::string str) {
  std::string resp;
  resp += '+' + str + "\r\n";
  return resp;
}

std::string resp_int(int n) {
  std::string resp;
  resp += ':' + std::to_string(n) + "\r\n";
  return resp;
}

std::string resp_array(std::vector<std::string> v) {
  std::string resp;
  resp += '*' + std::to_string(v.size()) + "\r\n";
  for (auto e : v) {
    resp += bulk_str(e);
  }

  return resp;
}

void ok(int client_fd) {
  send(client_fd, "+OK\r\n", 5, 0);
  return;
}

void null(int client_fd) {
  std::string resp = "$-1\r\n";
  send(client_fd, resp.c_str(), resp.size(), 0);
  return;
}

int parse_tokens(int i, const std::string &buf, std::vector<token> &ans) {
  char type = buf[i];
  std::string numStr;
  i++;
  while (buf[i] != '\r') {
    numStr += buf[i];
    i++;
  }

  int num = std::stoi(numStr);
  i += 2;
  if (type == '*') {
    for (int j = 0; j < num; j++) {
      i = parse_tokens(i, buf, ans);
    }
  } else if (type == '$') {
    std::string str;
    for (int j = 0; j < num; j++) {
      str += buf[i];
      i++;
    }
    i += 2;
    ans.push_back(token({str}));
  }
  return i;
}

std::vector<token> parse_command(char buffer[1024], ssize_t bytes_received) {
  std::string buf(buffer, bytes_received);
  std::vector<token> tokenList;
  for (int i = 0; i < buf.size(); i++) {
    i = parse_tokens(i, buf, tokenList);
  }
  return tokenList;
}
