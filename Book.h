#ifndef BOOK_H
#define BOOK_H

#include <string>
#include <ctime>

class Book {
private:
    int id;
    std::string title;
    std::string author;
    bool available;
    std::string borrowedBy;
    std::time_t borrowDate;
    std::time_t dueDate;
    int borrowCount;

public:
    Book();
    Book(int id, const std::string& title, const std::string& author);
    Book(int id, const std::string& title, const std::string& author,
         bool available, const std::string& borrowedBy,
         std::time_t borrowDate, std::time_t dueDate, int borrowCount);

    int getId() const;
    const std::string& getTitle() const;
    const std::string& getAuthor() const;
    bool isAvailable() const;
    const std::string& getBorrowedBy() const;
    std::time_t getBorrowDate() const;
    std::time_t getDueDate() const;
    int getBorrowCount() const;

    void editDetails(const std::string& newTitle, const std::string& newAuthor);
    void borrow(const std::string& username);
    void giveBack(const std::string& username);
    bool isOverdue() const;
    int daysOverdue() const;
    double fine() const;

    std::string serialize() const;
    static Book deserialize(const std::string& line);
};

#endif
