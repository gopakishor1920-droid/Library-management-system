#include "Book.h"
#include <sstream>
#include <vector>
#include <stdexcept>

namespace {
const int LOAN_DAYS = 14;
const double FINE_PER_DAY = 5.0;
std::vector<std::string> split(const std::string& line) {
    std::vector<std::string> parts;
    std::stringstream ss(line);
    std::string part;
    while (std::getline(ss, part, '|')) parts.push_back(part);
    return parts;
}
}

Book::Book() : id(0), available(true), borrowDate(0), dueDate(0), borrowCount(0) {}
Book::Book(int id, const std::string& title, const std::string& author)
    : id(id), title(title), author(author), available(true), borrowedBy(""),
      borrowDate(0), dueDate(0), borrowCount(0) {}
Book::Book(int id, const std::string& title, const std::string& author,
           bool available, const std::string& borrowedBy,
           std::time_t borrowDate, std::time_t dueDate, int borrowCount)
    : id(id), title(title), author(author), available(available), borrowedBy(borrowedBy),
      borrowDate(borrowDate), dueDate(dueDate), borrowCount(borrowCount) {}

int Book::getId() const { return id; }
const std::string& Book::getTitle() const { return title; }
const std::string& Book::getAuthor() const { return author; }
bool Book::isAvailable() const { return available; }
const std::string& Book::getBorrowedBy() const { return borrowedBy; }
std::time_t Book::getBorrowDate() const { return borrowDate; }
std::time_t Book::getDueDate() const { return dueDate; }
int Book::getBorrowCount() const { return borrowCount; }

void Book::editDetails(const std::string& newTitle, const std::string& newAuthor) {
    title = newTitle; author = newAuthor;
}
void Book::borrow(const std::string& username) {
    if (!available) throw std::runtime_error("This book is already borrowed.");
    available = false;
    borrowedBy = username;
    borrowDate = std::time(nullptr);
    dueDate = borrowDate + LOAN_DAYS * 24 * 60 * 60;
    ++borrowCount;
}
void Book::giveBack(const std::string& username) {
    if (available) throw std::runtime_error("This book is not currently borrowed.");
    if (borrowedBy != username) throw std::runtime_error("Only the student who borrowed this book can return it.");
    available = true;
    borrowedBy.clear();
    borrowDate = 0;
    dueDate = 0;
}
bool Book::isOverdue() const { return !available && dueDate > 0 && std::time(nullptr) > dueDate; }
int Book::daysOverdue() const {
    if (!isOverdue()) return 0;
    return static_cast<int>((std::time(nullptr) - dueDate + 86399) / 86400);
}
double Book::fine() const { return daysOverdue() * FINE_PER_DAY; }

std::string Book::serialize() const {
    std::ostringstream out;
    out << id << '|' << title << '|' << author << '|' << (available ? 1 : 0) << '|'
        << borrowedBy << '|' << static_cast<long long>(borrowDate) << '|'
        << static_cast<long long>(dueDate) << '|' << borrowCount;
    return out.str();
}
Book Book::deserialize(const std::string& line) {
    auto p = split(line);
    if (p.size() != 8) throw std::runtime_error("Invalid book record");
    return Book(std::stoi(p[0]), p[1], p[2], p[3] == "1", p[4],
                static_cast<std::time_t>(std::stoll(p[5])),
                static_cast<std::time_t>(std::stoll(p[6])), std::stoi(p[7]));
}
