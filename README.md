# PageTurn — C++ Library Management System

A student-friendly, web-based Library Management System. The backend is C++17 and uses a small POSIX HTTP server so the project can be built on macOS without installing Crow or another C++ web framework. The frontend is plain HTML, CSS and JavaScript. Book records and history are saved in text files.

> **Learning/demo project:** the server is intended for local use on your own computer, not deployment on the public internet. Demo passwords are stored in source code, sessions are in memory, and this minimal server is not a production-grade HTTP implementation.

## Features

- Separate admin and student login pages and dashboards
- Admin: add, edit, delete and search books
- Admin: view current borrowers, overdue count, most-borrowed title and recent activity
- Student: search books, borrow and return books, view personal borrowed books
- 14-day loan period and a demo fine of ₹5 per overdue day
- Case-insensitive title/author search
- Persistent `library.txt` records and append-only `history.txt` activity log
- OOP: `Book`, abstract `User`, `Admin`, `Student`, `Database`, and `History`
- C++ features: inheritance, virtual functions, STL vectors, algorithms/lambdas, exceptions, file I/O and serialization
- Responsive frontend with search, status labels, modal form, and toast notifications

## Requirements

- macOS with Xcode Command Line Tools (provides `clang++`), or another POSIX system with a C++17 compiler
- VS Code is recommended but not required

Install Apple's compiler tools if needed:

```bash
xcode-select --install
```

## Run on macOS

1. Extract the ZIP.
2. Open the extracted `cpp-library-web` folder in VS Code.
3. Open **Terminal → New Terminal**. Make sure the terminal is in the folder containing `main.cpp`.
4. Compile:

```bash
clang++ -std=c++17 -Wall -Wextra main.cpp Book.cpp User.cpp Database.cpp History.cpp -o library
```

5. Run:

```bash
./library
```

6. Open **http://localhost:18080** in your browser.
7. Stop the server with **Ctrl+C** in the terminal.

Run the program from the project folder. The HTML templates, CSS, JavaScript and data files are loaded using relative paths.

### Demo accounts

| Role | Username | Password |
|---|---|---|
| Admin | `admin` | `admin123` |
| Student | `student` | `student123` |
| Second student (useful for testing ownership) | `student2` | `student123` |

The first run creates `library.txt` with three sample books if it doesn't already exist. The activity file `history.txt` is created when the first action is logged.

## Project structure

```text
cpp-library-web/
├── main.cpp          # Small HTTP server, routes, API and sessions
├── Book.h/.cpp       # Book data, borrowing rules, due dates, fine and serialization
├── User.h/.cpp       # Abstract User, Admin and Student
├── Database.h/.cpp   # In-memory book collection + file persistence/search
├── History.h/.cpp    # Append-only activity log
├── HttpUtil.h        # Small JSON/string helper functions
├── templates/
│   ├── index.html    # Login page
│   ├── admin.html    # Admin dashboard
│   └── student.html  # Student dashboard
├── static/
│   ├── style.css     # Visual design and responsive styles
│   └── app.js        # Frontend interactions and API requests
├── library.txt      # Created at runtime; persistent book data
└── history.txt       # Created at runtime; activity log
```

## How file handling works

Each line in `library.txt` represents one book, with fields separated by `|`:

```text
1|Clean Code|Robert C. Martin|1||0|0|0
```

Fields: `id | title | author | available(1/0) | borrowedBy | borrowDate | dueDate | borrowCount`.

- `Database::load()` uses `ifstream` and `getline()` to read records when the server starts.
- `Book::deserialize()` converts each line into a `Book` object.
- `Database::save()` uses `ofstream` to rewrite all current book records after changes.
- `History::log()` opens `history.txt` with `ios::app`, so each action is appended rather than erasing previous entries.
- `Book::serialize()` converts a book object back into one text line.

The `|` character and newline are not allowed in book titles/authors because this simple text format uses them as separators.

## API overview

All API requests except login need the `Authorization: Bearer <token>` header. The browser adds it automatically after login.

| Method | Endpoint | Access | Purpose |
|---|---|---|---|
| POST | `/api/login` | Public | Verify demo credentials and create session |
| POST | `/api/logout` | Logged-in | End session |
| GET | `/api/books?q=clean` | Logged-in | List/search books |
| POST | `/api/add_book` | Admin | Add a book |
| POST | `/api/edit_book` | Admin | Edit title and author |
| POST | `/api/delete_book` | Admin | Delete an available book |
| GET | `/api/stats` | Admin | Dashboard totals |
| GET | `/api/borrowed` | Admin | Current borrowed books |
| GET | `/api/history` | Admin | Recent activity records |
| POST | `/api/borrow` | Student | Borrow an available book |
| POST | `/api/return` | Student | Return a book borrowed by that student |
| GET | `/api/my_books` | Student | List that student's borrowed books |

## Suggested learning order

1. Read `Book.h` and `Book.cpp`; understand constructors, private fields, getters and `serialize()`.
2. Follow `Database::load()` and `Database::save()` to understand file I/O.
3. Read `User.h` and `User.cpp` for inheritance and polymorphism.
4. Read `Application::handle()` in `main.cpp` to understand how a browser request calls C++ functions.
5. Open `static/app.js` and follow one button's `fetch()` call to its matching API route.
6. Test the cases below before your viva.

## Test checklist

- Log in with a correct and an incorrect password.
- Add a book, refresh the browser, and confirm it remains in the catalog.
- Search with different letter casing.
- Edit and delete an available book.
- Borrow a book as `student`; confirm it becomes unavailable.
- Try borrowing it again; the backend should reject it.
- Log in as `student2`; confirm they cannot return the first student's book.
- Return the book and check the history.
- Stop and restart the server; confirm books and history remain.
- To test an overdue fine without waiting 14 days, make a temporary test copy and change a borrowed book's `dueDate` field in `library.txt` to an old Unix timestamp.

## Limitations worth mentioning in a viva

- This is a local teaching server that handles one connection at a time. It is not a hardened public web server.
- Demo passwords are hardcoded and are not hashed; don't use real passwords.
- Sessions disappear when the server restarts.
- JSON parsing is intentionally minimal to keep the backend readable. A production app should use a tested JSON library and a mature HTTP framework.
- The browser stores a session token in `sessionStorage`; role checks are also enforced on the backend, not only by hiding UI elements.
- The data file is simple text storage, not a database. Concurrent writes and crash-safe transactions are not implemented.
