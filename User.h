#ifndef USER_H
#define USER_H
#include <string>

class User {
protected:
    std::string username;
    std::string password;
public:
    User(const std::string& username, const std::string& password);
    virtual ~User() = default;
    bool checkPassword(const std::string& candidate) const;
    const std::string& getUsername() const;
    virtual std::string role() const = 0;
};
class Admin : public User {
public:
    Admin(const std::string& username, const std::string& password);
    std::string role() const override;
};
class Student : public User {
public:
    Student(const std::string& username, const std::string& password);
    std::string role() const override;
};
#endif
