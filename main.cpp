#include "Book.h"
#include "User.h"
#include "Database.h"
#include "History.h"
#include "HttpUtil.h"

#include <iostream>
#include <fstream>
#include <sstream>
#include <vector>
#include <algorithm>
#include <csignal>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>

static volatile sig_atomic_t g_running = 1;
static void handleSignal(int) { g_running = 0; }

struct Request {
    std::string method, path, body;
    std::map<std::string, std::string> headers;
};
struct Session { std::string username, role; };
struct Response { int status = 200; std::string type = "application/json; charset=utf-8"; std::string body; };

std::string readFile(const std::string& path) {
    std::ifstream f(path, std::ios::binary);
    if (!f) return "";
    std::ostringstream out; out << f.rdbuf(); return out.str();
}
std::string lowerCopy(std::string s) {
    std::transform(s.begin(), s.end(), s.begin(), [](unsigned char c){ return static_cast<char>(std::tolower(c)); }); return s;
}
std::string trim(const std::string& s) {
    const auto a = s.find_first_not_of(" \t\r\n"); if (a == std::string::npos) return "";
    const auto b = s.find_last_not_of(" \t\r\n"); return s.substr(a, b-a+1);
}
bool readRequest(int fd, Request& req) {
    std::string raw; char buffer[4096]; std::size_t headerEnd = std::string::npos;
    while ((headerEnd = raw.find("\r\n\r\n")) == std::string::npos) {
        ssize_t n = recv(fd, buffer, sizeof(buffer), 0); if (n <= 0) return false;
        raw.append(buffer, static_cast<std::size_t>(n));
        if (raw.size() > 1024*1024) return false;
    }
    std::istringstream head(raw.substr(0, headerEnd));
    std::string first; std::getline(head, first); if (!first.empty() && first.back() == '\r') first.pop_back();
    std::istringstream firstLine(first); firstLine >> req.method >> req.path;
    std::string line;
    while (std::getline(head, line)) {
        if (!line.empty() && line.back() == '\r') line.pop_back();
        auto colon = line.find(':'); if (colon != std::string::npos) req.headers[lowerCopy(trim(line.substr(0, colon)))] = trim(line.substr(colon+1));
    }
    std::size_t contentLength = 0;
    auto it = req.headers.find("content-length");
    if (it != req.headers.end()) { try { contentLength = static_cast<std::size_t>(std::stoul(it->second)); } catch (...) {} }
    req.body = raw.substr(headerEnd+4);
    while (req.body.size() < contentLength) {
        ssize_t n = recv(fd, buffer, sizeof(buffer), 0); if (n <= 0) break;
        req.body.append(buffer, static_cast<std::size_t>(n));
    }
    if (req.body.size() > contentLength) req.body.resize(contentLength);
    return !req.method.empty();
}
std::string statusText(int status) {
    switch(status) { case 200:return "OK"; case 201:return "Created"; case 400:return "Bad Request"; case 401:return "Unauthorized"; case 403:return "Forbidden"; case 404:return "Not Found"; case 409:return "Conflict"; case 500:return "Internal Server Error"; default:return "OK"; }
}
void sendResponse(int fd, const Response& r) {
    std::ostringstream out;
    out << "HTTP/1.1 " << r.status << ' ' << statusText(r.status) << "\r\n"
        << "Content-Type: " << r.type << "\r\n"
        << "Content-Length: " << r.body.size() << "\r\n"
        << "Cache-Control: no-store\r\nConnection: close\r\n"
        << "X-Content-Type-Options: nosniff\r\n\r\n" << r.body;
    const std::string data = out.str(); std::size_t sent = 0;
    while (sent < data.size()) { ssize_t n = send(fd, data.data()+sent, data.size()-sent, 0); if (n <= 0) break; sent += static_cast<std::size_t>(n); }
}
Response jsonResponse(int status, const std::string& body) { return Response{status, "application/json; charset=utf-8", body}; }
Response message(int status, bool ok, const std::string& msg) {
    return jsonResponse(status, std::string("{\"ok\":") + (ok ? "true" : "false") + ",\"message\":" + jsonString(msg) + "}");
}
std::string bookJson(const Book& b) {
    std::ostringstream o;
    o << "{\"id\":" << b.getId() << ",\"title\":" << jsonString(b.getTitle())
      << ",\"author\":" << jsonString(b.getAuthor()) << ",\"available\":" << (b.isAvailable()?"true":"false")
      << ",\"borrowedBy\":" << jsonString(b.getBorrowedBy())
      << ",\"borrowDate\":" << static_cast<long long>(b.getBorrowDate())
      << ",\"dueDate\":" << static_cast<long long>(b.getDueDate())
      << ",\"overdue\":" << (b.isOverdue()?"true":"false")
      << ",\"daysOverdue\":" << b.daysOverdue() << ",\"fine\":" << b.fine()
      << ",\"borrowCount\":" << b.getBorrowCount() << '}';
    return o.str();
}
std::string booksJson(const std::vector<Book>& books) {
    std::string out = "["; bool first = true;
    for (const auto& b : books) { if (!first) out += ','; first = false; out += bookJson(b); }
    return out + ']';
}
std::string queryValue(const std::string& path, const std::string& key) {
    auto q = path.find('?'); if (q == std::string::npos) return "";
    std::string query = path.substr(q+1); std::istringstream parts(query); std::string part;
    while (std::getline(parts, part, '&')) { auto eq=part.find('='); if (eq!=std::string::npos && urlDecode(part.substr(0,eq))==key) return urlDecode(part.substr(eq+1)); }
    return "";
}
std::string bearerToken(const Request& req) {
    auto it=req.headers.find("authorization"); if(it==req.headers.end()) return "";
    const std::string prefix="Bearer "; if(it->second.rfind(prefix,0)!=0) return "";
    return it->second.substr(prefix.size());
}

class Application {
    Database db;
    History history;
    std::vector<Admin> admins;
    std::vector<Student> students;
    std::map<std::string, Session> sessions;
    unsigned long tokenCounter = 1000;
public:
    Application() : db("library.txt"), history("history.txt") {
        db.load();
        admins.emplace_back("admin", "admin123");
        students.emplace_back("student", "student123");
        students.emplace_back("student2", "student123");
    }
    Response handle(const Request& req) {
        std::string path = req.path.substr(0, req.path.find('?'));
        if (req.method == "GET" && path == "/") return html("templates/index.html");
        if (req.method == "GET" && path == "/admin") return html("templates/admin.html");
        if (req.method == "GET" && path == "/student") return html("templates/student.html");
        if (req.method == "GET" && path == "/static/style.css") return asset("static/style.css", "text/css; charset=utf-8");
        if (req.method == "GET" && path == "/static/app.js") return asset("static/app.js", "application/javascript; charset=utf-8");

        if (req.method == "POST" && path == "/api/login") return login(req);
        if (req.method == "POST" && path == "/api/logout") {
            std::string token = bearerToken(req); if (token.empty()) return message(401,false,"Please log in again.");
            sessions.erase(token); return message(200,true,"Logged out.");
        }
        if (path.rfind("/api/", 0) != 0) return message(404,false,"Page not found.");
        Session* session = authenticate(req);
        if (!session) return message(401,false,"Your session expired. Please log in again.");

        try {
            if (req.method == "GET" && path == "/api/books") return jsonResponse(200, booksJson(db.search(queryValue(req.path,"q"))));
            if (req.method == "GET" && path == "/api/my_books") {
                if (session->role != "student") return message(403,false,"Student access only.");
                return jsonResponse(200, booksJson(db.booksBorrowedBy(session->username)));
            }
            if (req.method == "GET" && path == "/api/borrowed") {
                if (!isAdmin(*session)) return message(403,false,"Admin access only.");
                return jsonResponse(200, booksJson(db.borrowedBooks()));
            }
            if (req.method == "GET" && path == "/api/history") {
                if (!isAdmin(*session)) return message(403,false,"Admin access only.");
                return historyJson();
            }
            if (req.method == "GET" && path == "/api/stats") {
                if (!isAdmin(*session)) return message(403,false,"Admin access only.");
                std::ostringstream o; o << "{\"total\":" << db.totalBooks() << ",\"borrowed\":" << db.borrowedCount()
                    << ",\"overdue\":" << db.overdueCount() << ",\"popular\":" << jsonString(db.mostBorrowedTitle()) << '}';
                return jsonResponse(200,o.str());
            }
            if (req.method == "POST" && path == "/api/add_book") {
                if (!isAdmin(*session)) return message(403,false,"Admin access only.");
                std::string title=jsonGetString(req.body,"title"), author=jsonGetString(req.body,"author");
                if(title.empty() || author.empty()) return message(400,false,"Enter both a title and an author.");
                Book b=db.addBook(title,author); history.log("Added",session->username,b.getId(),b.getTitle());
                return jsonResponse(201,"{\"ok\":true,\"message\":\"Book added.\",\"book\":"+bookJson(b)+"}");
            }
            if (req.method == "POST" && path == "/api/edit_book") {
                if (!isAdmin(*session)) return message(403,false,"Admin access only.");
                int id=jsonGetInt(req.body,"id"); std::string title=jsonGetString(req.body,"title"), author=jsonGetString(req.body,"author");
                if(id<1 || title.empty() || author.empty()) return message(400,false,"Enter a valid ID, title and author.");
                db.editBook(id,title,author); history.log("Edited",session->username,id,title);
                return message(200,true,"Book updated.");
            }
            if (req.method == "POST" && path == "/api/delete_book") {
                if (!isAdmin(*session)) return message(403,false,"Admin access only.");
                int id=jsonGetInt(req.body,"id"); auto all=db.allBooks(); auto it=std::find_if(all.begin(),all.end(),[id](const Book& b){return b.getId()==id;});
                if(it==all.end()) return message(404,false,"Book ID was not found.");
                std::string title=it->getTitle(); db.deleteBook(id); history.log("Deleted",session->username,id,title);
                return message(200,true,"Book deleted.");
            }
            if (req.method == "POST" && path == "/api/borrow") {
                if (session->role!="student") return message(403,false,"Student access only.");
                int id=jsonGetInt(req.body,"id"); Book b=db.borrowBook(id,session->username); history.log("Borrowed",session->username,id,b.getTitle());
                return jsonResponse(200,"{\"ok\":true,\"message\":\"Book borrowed. It is due in 14 days.\",\"book\":"+bookJson(b)+"}");
            }
            if (req.method == "POST" && path == "/api/return") {
                if (session->role!="student") return message(403,false,"Student access only.");
                int id=jsonGetInt(req.body,"id"); auto all=db.allBooks(); auto it=std::find_if(all.begin(),all.end(),[id](const Book& b){return b.getId()==id;});
                if(it==all.end()) return message(404,false,"Book ID was not found.");
                double fine=it->fine(); Book b=db.returnBook(id,session->username); history.log("Returned",session->username,id,b.getTitle());
                std::ostringstream msg; msg << "Book returned successfully. Fine: ₹" << fine << ".";
                return message(200,true,msg.str());
            }
            return message(404,false,"API endpoint not found.");
        } catch (const std::exception& e) {
            std::string m=e.what(); int status=(m.find("not found")!=std::string::npos)?404:409;
            return message(status,false,m);
        }
    }
private:
    Response html(const std::string& path) { auto s=readFile(path); if(s.empty()) return Response{500,"text/plain; charset=utf-8","Could not read "+path+". Start the server from the project folder."}; return Response{200,"text/html; charset=utf-8",s}; }
    Response asset(const std::string& path,const std::string& type) { auto s=readFile(path); if(s.empty()) return Response{404,"text/plain; charset=utf-8","Asset not found. Start the server from the project folder."}; return Response{200,type,s}; }
    bool isAdmin(const Session& s) const { return s.role=="admin"; }
    Session* authenticate(const Request& req) {
        std::string token=bearerToken(req); auto it=sessions.find(token); return it==sessions.end()?nullptr:&it->second;
    }
    Response login(const Request& req) {
        std::string u=jsonGetString(req.body,"username"), p=jsonGetString(req.body,"password"), role="";
        for(const auto& a:admins) if(a.getUsername()==u && a.checkPassword(p)) role=a.role();
        for(const auto& s:students) if(s.getUsername()==u && s.checkPassword(p)) role=s.role();
        if(role.empty()) return message(401,false,"Incorrect username or password.");
        std::string token="local-session-"+std::to_string(++tokenCounter)+"-"+std::to_string(std::time(nullptr));
        sessions[token]=Session{u,role};
        return jsonResponse(200,"{\"ok\":true,\"token\":"+jsonString(token)+",\"username\":"+jsonString(u)+",\"role\":"+jsonString(role)+"}");
    }
    Response historyJson() {
        auto lines=history.readNewestFirst(); std::string out="["; bool first=true;
        for(const auto& line:lines) {
            std::vector<std::string> p; std::stringstream ss(line); std::string part; while(std::getline(ss,part,'|')) p.push_back(part);
            if(p.size()<5) continue;
            if (!first) out += ',';
            first = false;
            out += "{\"timestamp\":"+p[0]+",\"action\":"+jsonString(p[1])+",\"username\":"+jsonString(p[2])+",\"bookId\":"+p[3]+",\"title\":"+jsonString(p[4])+"}";
        }
        return jsonResponse(200,out+"]");
    }
};

int main() {
    const int port=18080;
    int server=socket(AF_INET,SOCK_STREAM,0);
    if(server<0) { std::cerr << "Could not create server socket.\n"; return 1; }
    int yes=1; setsockopt(server,SOL_SOCKET,SO_REUSEADDR,&yes,sizeof(yes));
    sockaddr_in address{}; address.sin_family=AF_INET; address.sin_addr.s_addr=htonl(INADDR_ANY); address.sin_port=htons(port);
    if(bind(server,reinterpret_cast<sockaddr*>(&address),sizeof(address))<0) { std::cerr << "Could not bind port "<<port<<". Is another server using it?\n"; close(server); return 1; }
    if(listen(server,20)<0) { std::cerr << "Could not listen on port.\n"; close(server); return 1; }
    std::signal(SIGINT,  handleSignal);
    std::signal(SIGTERM, handleSignal);
    Application app;
    std::cout << "Library server running at http://localhost:" << port << "\nPress Ctrl+C to stop.\n";
    while(g_running) {
        sockaddr_in client{}; socklen_t len=sizeof(client); int fd=accept(server,reinterpret_cast<sockaddr*>(&client),&len);
        if(fd<0) continue;
        Request req;
        Response response = readRequest(fd, req) ? app.handle(req) : message(400, false, "Invalid HTTP request.");
        sendResponse(fd, response); close(fd);
    }
    close(server); return 0;
}
