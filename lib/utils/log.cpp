
#include <thread>
#include <unordered_map>

std::string prefix;
bool log_colored = true;

std::unordered_map<std::thread::id, int> id_map;
int counter = 0;

int get_thread_id() {
    std::thread::id id = std::this_thread::get_id();

    if (!id_map.contains(id)) {
        id_map.emplace(id, counter++);
    }

    return id_map.at(id);
}