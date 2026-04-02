#include "commands.hpp"
#include <chrono>
#include "utils.hpp"
#include <string>

void c_ping(int client_fd, const std::vector<token> &args) {
  std::string resp = simple_string("PONG");
  send(client_fd, resp.c_str(), resp.size(), 0);
}

void c_echo(int client_fd, const std::vector<token> &args) {
  std::string resp = bulk_str(args[1].sVal[0]);
  send(client_fd, resp.c_str(), resp.size(), 0);
}

void c_set(int client_fd, const std::vector<token> &args) {
  store_mutex.lock();
  long long expiry = -1;
  if (args.size() == 5) {
    if (args[3].sVal[0] == "EX") {
      expiry = now_ms() + std::stoll(args[4].sVal[0]) * 1000;
    } else if (args[3].sVal[0] == "PX") {
      expiry = now_ms() + std::stoll(args[4].sVal[0]);
    }
  }
  store[args[1].sVal[0]] = {0, {args[2].sVal[0]}, 0, expiry};
  store_mutex.unlock();
  ok(client_fd);
}

void c_get(int client_fd, const std::vector<token> &args) {
  store_mutex.lock();
  auto it = store.find(args[1].sVal[0]);
  if (it == store.end()) {
    null(client_fd);
  } else {
    long long expiry = std::get<3>(it->second);
    if (expiry != -1 && now_ms() >= expiry) {
      store.erase(it);
      null(client_fd);
    } else {
      std::string resp = bulk_str(std::get<1>(it->second)[0]);
      send(client_fd, resp.c_str(), resp.size(), 0);
    }
  }
  store_mutex.unlock();
}

void c_rpush(int client_fd, const std::vector<token> &args) {
  store_mutex.lock();
  for (int i = 2; i < args.size(); i++) {
    (std::get<1>(store[args[1].sVal[0]])).push_back(args[i].sVal[0]);
  }
  std::string resp = resp_int((std::get<1>(store[args[1].sVal[0]])).size());
  store_mutex.unlock();
  store_cv.notify_all();
  send(client_fd, resp.c_str(), resp.size(), 0);
}

void c_lpush(int client_fd, const std::vector<token> &args) {
  store_mutex.lock();
  for (int i = 2; i < args.size(); i++) {
    auto &v = std::get<1>(store[args[1].sVal[0]]);
    v.insert(v.begin(), args[i].sVal[0]);
  }
  std::string resp = resp_int((std::get<1>(store[args[1].sVal[0]])).size());
  store_mutex.unlock();
  store_cv.notify_all();
  send(client_fd, resp.c_str(), resp.size(), 0);
}

void c_lrange(int client_fd, const std::vector<token> &args) {
  store_mutex.lock();
  std::vector<std::string> v = std::get<1>(store[args[1].sVal[0]]);
  int sz = v.size();
  int start_idx = std::stoi(args[2].sVal[0]);
  int end_idx = std::stoi(args[3].sVal[0]);

  if (sz == 0) {
    std::string resp = resp_array({});
    send(client_fd, resp.c_str(), resp.size(), 0);
    store_mutex.unlock();
    return;
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
  store_mutex.unlock();
  send(client_fd, resp.c_str(), resp.size(), 0);
}

void c_llen(int client_fd, const std::vector<token> &args) {
  store_mutex.lock();
  std::string resp = resp_int(std::get<1>(store[args[1].sVal[0]]).size());
  store_mutex.unlock();
  send(client_fd, resp.c_str(), resp.size(), 0);
}

void c_lpop(int client_fd, const std::vector<token> &args) {
  store_mutex.lock();
  if (std::get<1>(store[args[1].sVal[0]]).size() == 0) {
    null(client_fd);
  } else if (args.size() == 2) {
    auto &v = std::get<1>(store[args[1].sVal[0]]);
    std::string popped = v[0];
    v.erase(v.begin());
    std::string resp = bulk_str(popped);
    send(client_fd, resp.c_str(), resp.size(), 0);
  } else {
    size_t toPop = std::stoi(args[2].sVal[0]);
    std::vector<std::string> resp;
    auto &v = std::get<1>(store[args[1].sVal[0]]);
    for (int i = 0; i < std::min(toPop, v.size()); i++) {
      resp.push_back(v[i]);
    }
    v.erase(v.begin(), v.begin() + std::min(toPop,v.size()));
    std::string resp_str = resp_array(resp);
    send(client_fd, resp_str.c_str(), resp_str.size(), 0);
  }
  store_mutex.unlock();
}

void c_blpop(int client_fd, const std::vector<token> &args) {
  if (args.size() < 3) {
    null(client_fd);
    return;
  }

  std::string key = args[1].sVal[0];
  double timeout_seconds = std::stod(args[2].sVal[0]);

  std::unique_lock<std::mutex> lock(store_mutex);
  
  // Ensure key exists in store
  if (store.find(key) == store.end()) {
    store[key] = {0, {}, 0, -1};
  }
  
  auto has_value = [&]() {
    auto it = store.find(key);
    return it != store.end() && !std::get<1>(it->second).empty();
  };

  bool ready = false;
  if (timeout_seconds <= 0) {
    store_cv.wait(lock, has_value);
    ready = true;
  } else {
    auto timeout = std::chrono::milliseconds(
        static_cast<long long>(timeout_seconds * 1000));
    ready = store_cv.wait_for(lock, timeout, has_value);
  }

  if (!ready) {
    lock.unlock();
    null_arr(client_fd);
    return;
  }

  auto &v = std::get<1>(store[key]);
  std::string popped = v[0];
  v.erase(v.begin());
  lock.unlock();

  std::string resp = resp_array({key, popped});
  send(client_fd, resp.c_str(), resp.size(), 0);
}
