#include <map>
#include <string>
#include <vector>

void handle_client(int client_fd);

extern std::map<std::string,
                std::tuple<int, std::vector<std::string>, int, long long>>
    store;
