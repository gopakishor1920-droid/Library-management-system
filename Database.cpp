#include "Database.h"
#include <fstream>
#include <algorithm>
#include <cctype>
#include <stdexcept>

namespace {
std::string lower(std::string s) {
    std::transform(s.begin(), s.end(), s.begin(), [](unsigned char c){ return static_cast<char>(std::tolower(c)); });
    return s;
}
bool safeField(const std::string& s) {
    return !s.empty() && s.find('|') == std::string::npos && s.find('\n') == std::string::npos && s.find('\r') == std::string::npos;
}
}
Database::Database(const std::string& f) : filename(f), nextId(1) {}
void Database::load() {
    books.clear(); nextId = 1;
    std::ifstream file(filename);
    if (!file) {
        books.emplace_back(nextId++, "Clean Code", "Robert C. Martin");
        books.emplace_back(nextId++, "Atomic Habits", "James Clear");
        books.emplace_back(nextId++, "The Alchemist", "Paulo Coelho");
        save(); return;
    }
    std::string line;
    while (std::getline(file, line)) {
        if (line.empty()) continue;
        try {
            Book b = Book::deserialize(line);
            nextId = std::max(nextId, b.getId() + 1);
            books.push_back(b);
        } catch (...) {
            // Ignore a damaged line instead of preventing the rest of the library from loading.
        }
    }
}
void Database::save() const {
    std::ofstream file(filename);
    if (!file) throw std::runtime_error("Could not open library.txt for writing.");
    for (const auto& b : books) file << b.serialize() << '\n';
    if (!file) throw std::runtime_error("Could not finish saving the library.");
}
Book& Database::findBook(int id) {
    auto it = std::find_if(books.begin(), books.end(), [id](const Book& b){ return b.getId() == id; });
    if (it == books.end()) throw std::runtime_error("Book ID was not found.");
    return *it;
}
std::vector<Book> Database::allBooks() const { return books; }
std::vector<Book> Database::search(const std::string& q) const {
    std::vector<Book> result;
    const std::string needle = lower(q);
    for (const auto& b : books) {
        if (needle.empty() || lower(b.getTitle()).find(needle) != std::string::npos ||
            lower(b.getAuthor()).find(needle) != std::string::npos) result.push_back(b);
    }
    return result;
}
std::vector<Book> Database::booksBorrowedBy(const std::string& u) const {
    std::vector<Book> out;
    for (const auto& b : books) if (!b.isAvailable() && b.getBorrowedBy() == u) out.push_back(b);
    return out;
}
std::vector<Book> Database::borrowedBooks() const {
    std::vector<Book> out;
    for (const auto& b : books) if (!b.isAvailable()) out.push_back(b);
    return out;
}
Book Database::addBook(const std::string& title, const std::string& author) {
    if (!safeField(title) || !safeField(author)) throw std::runtime_error("Title and author are required; do not use the | character.");
    books.emplace_back(nextId++, title, author); save(); return books.back();
}
void Database::editBook(int id, const std::string& title, const std::string& author) {
    if (!safeField(title) || !safeField(author)) throw std::runtime_error("Title and author are required; do not use the | character.");
    findBook(id).editDetails(title, author); save();
}
void Database::deleteBook(int id) {
    auto it = std::find_if(books.begin(), books.end(), [id](const Book& b){ return b.getId() == id; });
    if (it == books.end()) throw std::runtime_error("Book ID was not found.");
    if (!it->isAvailable()) throw std::runtime_error("Return this book before deleting it.");
    books.erase(it); save();
}
Book Database::borrowBook(int id, const std::string& username) {
    Book& b = findBook(id); b.borrow(username); save(); return b;
}
Book Database::returnBook(int id, const std::string& username) {
    Book& b = findBook(id); b.giveBack(username); save(); return b;
}
int Database::totalBooks() const { return static_cast<int>(books.size()); }
int Database::borrowedCount() const { return static_cast<int>(borrowedBooks().size()); }
int Database::overdueCount() const {
    int n = 0; for (const auto& b : books) if (b.isOverdue()) ++n; return n;
}
std::string Database::mostBorrowedTitle() const {
    if (books.empty()) return "—";
    const Book* best = &books.front();
    for (const auto& b : books) if (b.getBorrowCount() > best->getBorrowCount()) best = &b;
    return best->getBorrowCount() == 0 ? "No borrowing yet" : best->getTitle();
}
