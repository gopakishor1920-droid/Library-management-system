#include "History.h"
#include <fstream>
#include <sstream>
#include <vector>
#include <ctime>
#include <algorithm>
History::History(const std::string& f) : filename(f) {}
void History::log(const std::string& action, const std::string& username, int id, const std::string& title) {
    std::ofstream file(filename, std::ios::app);
    if (!file) return;
    std::time_t now = std::time(nullptr);
    std::string safeTitle = title;
    std::replace(safeTitle.begin(), safeTitle.end(), '|', '/');
    file << static_cast<long long>(now) << '|' << action << '|' << username << '|' << id << '|' << safeTitle << '\n';
}
std::vector<std::string> History::readNewestFirst() const {
    std::ifstream file(filename);
    std::vector<std::string> lines;
    std::string line;
    while (std::getline(file, line)) if (!line.empty()) lines.push_back(line);
    std::reverse(lines.begin(), lines.end());
    return lines;
}
