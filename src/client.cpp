#include "client.hpp"
#include "utils.hpp"
#include <unistd.h>
#include <vector>
#include "commands.hpp"

void handle_client(int client_fd) {
  while (true) {
    char buffer[1024];
    ssize_t bytes_received = recv(client_fd, buffer, sizeof(buffer), 0);
    if (bytes_received <= 0) {
      break;
    }
    std::vector<token> parsed_command = parse_command(buffer, bytes_received);

    auto command_it = command_store.find(parsed_command[0].sVal[0]);
    if (command_it != command_store.end()) {
      command_it->second(client_fd, parsed_command);
    }
  }

  close(client_fd);
}