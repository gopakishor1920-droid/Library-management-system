#ifndef HISTORY_H
#define HISTORY_H
#include <string>
#include <vector>
class History {
private:
    std::string filename;
public:
    explicit History(const std::string& filename = "history.txt");
    void log(const std::string& action, const std::string& username, int bookId, const std::string& title);
    std::vector<std::string> readNewestFirst() const;
};
#endif
