#include <arpa/inet.h>
#include <chrono>
#include <iostream>
#include <map>
#include <netdb.h>
#include <string>
#include <sys/socket.h>
#include <sys/types.h>
#include <thread>
#include <unistd.h>
#include <vector>

class token {
public:
  int type;
  std::vector<std::string> sVal;
  int iVal;

  token(std::vector<std::string> s) : type(0), sVal(s) {}
  token(int i) : type(2), iVal(i) {}
};

std::map<std::string, std::tuple<int, std::vector<std::string>, int, long long>>
    mp;

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
  // std::cout << "--" << numStr << "--\n";
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

void handle_client(int client_fd) {
  while (true) {
    char buffer[1024];
    ssize_t bytes_received = recv(client_fd, buffer, sizeof(buffer), 0);
    if (bytes_received <= 0) {
      break;
    }
    std::vector<token> parsed_command = parse_command(buffer, bytes_received);

    if (parsed_command[0].sVal[0] == "ECHO") {
      std::string resp = bulk_str(parsed_command[1].sVal[0]);
      send(client_fd, resp.c_str(), resp.size(), 0);
    } else if (parsed_command[0].sVal[0] == "PING") {
      std::string resp = simple_string("PONG");
      send(client_fd, resp.c_str(), resp.size(), 0);
    } else if (parsed_command[0].sVal[0] == "SET") {
      long long expiry = -1;
      if (parsed_command.size() == 5) {
        if (parsed_command[3].sVal[0] == "EX") {
          expiry = now_ms() + std::stoll(parsed_command[4].sVal[0]) * 1000;
        } else if (parsed_command[3].sVal[0] == "PX") {
          expiry = now_ms() + std::stoll(parsed_command[4].sVal[0]);
        }
      }
      mp[parsed_command[1].sVal[0]] = {
          0, {parsed_command[2].sVal[0]}, 0, expiry};
      ok(client_fd);
    } else if (parsed_command[0].sVal[0] == "GET") {
      auto it = mp.find(parsed_command[1].sVal[0]);
      if (it == mp.end()) {
        null(client_fd);
      } else {
        long long expiry = std::get<3>(it->second);
        if (expiry != -1 && now_ms() >= expiry) {
          mp.erase(it);
          null(client_fd);
        } else {
          std::string resp = bulk_str(std::get<1>(it->second)[0]);
          send(client_fd, resp.c_str(), resp.size(), 0);
        }
      }
    } else if (parsed_command[0].sVal[0] == "RPUSH") {
      for (int i = 2; i < parsed_command.size(); i++) {
        (std::get<1>(mp[parsed_command[1].sVal[0]]))
            .push_back(parsed_command[i].sVal[0]);
      }
      std::string resp =
          resp_int((std::get<1>(mp[parsed_command[1].sVal[0]])).size());
      send(client_fd, resp.c_str(), resp.size(), 0);
    } else if (parsed_command[0].sVal[0] == "LRANGE") {

      std::vector<std::string> v = std::get<1>(mp[parsed_command[1].sVal[0]]);
      int sz = v.size();
      int start_idx = std::stoi(parsed_command[2].sVal[0]);
      int end_idx = std::stoi(parsed_command[3].sVal[0]);

      if (sz == 0) {
        std::string resp = resp_array({});
        send(client_fd, resp.c_str(), resp.size(), 0);
        continue;
      }

      if (start_idx < 0)
        start_idx += sz;
      if (end_idx < 0)
        end_idx += sz;

      if (start_idx < 0)
        start_idx = 0;
      if (end_idx < 0)
        end_idx = 0;
      if (start_idx >= sz)
        start_idx = sz;
      if (end_idx >= sz)
        end_idx = sz - 1;

      std::vector<std::string> resp_v;
      if (start_idx <= end_idx && start_idx < sz) {
        for (int i = start_idx; i <= end_idx; i++) {
          resp_v.push_back(v[i]);
        }
      }

      std::string resp = resp_array(resp_v);
      send(client_fd, resp.c_str(), resp.size(), 0);
    }
  }

  close(client_fd);
}

int main(int argc, char **argv) {
  // Flush after every std::cout / std::cerr
  std::cout << std::unitbuf;
  std::cerr << std::unitbuf;

  int server_fd = socket(AF_INET, SOCK_STREAM, 0);
  if (server_fd < 0) {
    std::cerr << "Failed to create server socket\n";
    return 1;
  }

  // Since the tester restarts your program quite often, setting SO_REUSEADDR
  // ensures that we don't run into 'Address already in use' errors
  int reuse = 1;
  if (setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse)) <
      0) {
    std::cerr << "setsockopt failed\n";
    return 1;
  }

  struct sockaddr_in server_addr;
  server_addr.sin_family = AF_INET;
  server_addr.sin_addr.s_addr = INADDR_ANY;
  server_addr.sin_port = htons(6379);

  if (bind(server_fd, (struct sockaddr *)&server_addr, sizeof(server_addr)) !=
      0) {
    std::cerr << "Failed to bind to port 6379\n";
    return 1;
  }

  int connection_backlog = 5;
  if (listen(server_fd, connection_backlog) != 0) {
    std::cerr << "listen failed\n";
    return 1;
  }

  std::cout << "Listening for connections!" << std::endl;
  while (true) {
    struct sockaddr_in client_addr;
    int client_addr_len = sizeof(client_addr);

    int client_fd = accept(server_fd, (struct sockaddr *)&client_addr,
                           (socklen_t *)&client_addr_len);
    std::cout << "Client connected\n";

    std::thread(handle_client, client_fd).detach();
  }
  close(server_fd);

  return 0;
}
