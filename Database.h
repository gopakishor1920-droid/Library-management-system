#ifndef DATABASE_H
#define DATABASE_H
#include "Book.h"
#include <string>
#include <vector>

class Database {
private:
    std::vector<Book> books;
    std::string filename;
    int nextId;
    Book& findBook(int id);
public:
    explicit Database(const std::string& filename = "library.txt");
    void load();
    void save() const;
    std::vector<Book> allBooks() const;
    std::vector<Book> search(const std::string& query) const;
    std::vector<Book> booksBorrowedBy(const std::string& username) const;
    std::vector<Book> borrowedBooks() const;
    Book addBook(const std::string& title, const std::string& author);
    void editBook(int id, const std::string& title, const std::string& author);
    void deleteBook(int id);
    Book borrowBook(int id, const std::string& username);
    Book returnBook(int id, const std::string& username);
    int totalBooks() const;
    int borrowedCount() const;
    int overdueCount() const;
    std::string mostBorrowedTitle() const;
};
#endif
