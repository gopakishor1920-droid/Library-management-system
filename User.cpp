#include "User.h"
User::User(const std::string& u, const std::string& p) : username(u), password(p) {}
bool User::checkPassword(const std::string& candidate) const { return password == candidate; }
const std::string& User::getUsername() const { return username; }
Admin::Admin(const std::string& u, const std::string& p) : User(u, p) {}
std::string Admin::role() const { return "admin"; }
Student::Student(const std::string& u, const std::string& p) : User(u, p) {}
std::string Student::role() const { return "student"; }
