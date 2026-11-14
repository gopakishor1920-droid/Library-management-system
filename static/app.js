const $ = (selector) => document.querySelector(selector);
const page = document.body.dataset.page || "login";
const tokenKey = "pageturn-token";
const userKey = "pageturn-user";
const roleKey = "pageturn-role";
const token = () => sessionStorage.getItem(tokenKey) || "";
const currentUser = () => sessionStorage.getItem(userKey) || "Reader";
const currentRole = () => sessionStorage.getItem(roleKey) || "";

async function api(path, options = {}) {
  const headers = { ...(options.headers || {}) };
  if (options.body) headers["Content-Type"] = "application/json";
  if (token()) headers.Authorization = `Bearer ${token()}`;
  const response = await fetch(path, { ...options, headers });
  const data = await response.json().catch(() => ({}));
  if (response.status === 401) { sessionStorage.clear(); if (page !== "login") location.href = "/"; }
  if (!response.ok) throw new Error(data.message || `Request failed (${response.status})`);
  return data;
}
function escapeHTML(value) {
  return String(value ?? "").replace(/[&<>"']/g, c => ({"&":"&amp;","<":"&lt;",">":"&gt;",'"':"&quot;","'":"&#39;"}[c]));
}
function notify(message, kind = "success") {
  const region = $("#toast-region"); if (!region) return;
  const toast = document.createElement("div"); toast.className = `toast toast-${kind}`;
  toast.innerHTML = `<span>${kind === "success" ? "✓" : "!"}</span><p>${escapeHTML(message)}</p>`;
  region.appendChild(toast); setTimeout(() => { toast.classList.add("toast-out"); setTimeout(() => toast.remove(), 250); }, 3200);
}
function dateText(epoch) {
  if (!epoch || Number(epoch) <= 0) return "—";
  return new Date(Number(epoch) * 1000).toLocaleDateString(undefined, { month: "short", day: "numeric", year: "numeric" });
}
function money(value) { return `₹${Number(value || 0).toFixed(0)}`; }
function requireRole(role) {
  if (!token() || currentRole() !== role) { sessionStorage.clear(); location.href = "/"; return false; }
  return true;
}

if (page === "login") {
  if (token() && currentRole() === "admin") location.href = "/admin";
  if (token() && currentRole() === "student") location.href = "/student";
  const form = $("#login-form");
  $("#toggle-password")?.addEventListener("click", () => {
    const input = $("#password"); input.type = input.type === "password" ? "text" : "password";
    $("#toggle-password").textContent = input.type === "password" ? "Show" : "Hide";
  });
  document.querySelectorAll(".demo-account").forEach(button => button.addEventListener("click", () => {
    $("#username").value = button.dataset.user; $("#password").value = button.dataset.pass; $("#login-error").textContent = "";
  }));
  form?.addEventListener("submit", async event => {
    event.preventDefault(); const submit = form.querySelector("button[type=submit]");
    submit.disabled = true; submit.innerHTML = "Signing in…"; $("#login-error").textContent = "";
    try {
      const result = await api("/api/login", { method: "POST", body: JSON.stringify({ username: $("#username").value.trim(), password: $("#password").value }) });
      sessionStorage.setItem(tokenKey, result.token); sessionStorage.setItem(userKey, result.username); sessionStorage.setItem(roleKey, result.role);
      location.href = result.role === "admin" ? "/admin" : "/student";
    } catch (error) { $("#login-error").textContent = error.message; }
    finally { submit.disabled = false; submit.innerHTML = "<span>Sign in</span><span>↗</span>"; }
  });
}

async function logout() {
  try { if (token()) await api("/api/logout", { method: "POST", body: "{}" }); } catch (_) {}
  sessionStorage.clear(); location.href = "/";
}
$("#logout-btn")?.addEventListener("click", logout);

if (page === "admin" && requireRole("admin")) {
  $("#profile-name").textContent = currentUser(); $("#greeting-name").textContent = currentUser();
  let allBooks = [], filter = "all", query = "", editingId = null;
  const modal = $("#book-modal");
  async function loadBooks() { allBooks = await api(`/api/books?q=${encodeURIComponent(query)}`); renderBooks(); }
  function renderBooks() {
    const books = allBooks.filter(b => filter === "all" || (filter === "available" ? b.available : !b.available));
    $("#book-count").textContent = `${books.length} ${books.length === 1 ? "book" : "books"} shown`;
    const tbody = $("#admin-books");
    if (!books.length) { tbody.innerHTML = `<tr><td colspan="6" class="empty-cell">No books found. Try another search or add a title.</td></tr>`; return; }
    tbody.innerHTML = books.map(b => `<tr><td><div class="book-cell"><div class="mini-cover cover-color-${b.id % 5}">${escapeHTML((b.title[0] || "B").toUpperCase())}</div><div><b>${escapeHTML(b.title)}</b><small>${escapeHTML(b.author)}</small></div></div></td><td><span class="id-chip">BK-${String(b.id).padStart(3,"0")}</span></td><td><span class="status-badge ${b.available ? "status-available" : b.overdue ? "status-overdue" : "status-borrowed"}"><i></i>${b.available ? "Available" : b.overdue ? "Overdue" : "Borrowed"}</span></td><td>${b.available ? `<span class="muted">—</span>` : `<span class="borrower">${escapeHTML(b.borrowedBy)}</span>${b.overdue ? `<small class="overdue-note">${b.daysOverdue}d overdue</small>` : ""}`}</td><td>${b.available ? `<span class="muted">—</span>` : `<span class="${b.overdue ? "overdue-note" : "muted"}">${dateText(b.dueDate)}</span>`}</td><td><div class="row-actions"><button class="small-action" data-edit="${b.id}" title="Edit book">Edit</button><button class="small-action danger-action" data-delete="${b.id}" ${!b.available ? "disabled title='Return the book before deleting'" : ""}>Delete</button></div></td></tr>`).join("");
  }
  async function loadStats() {
    const s = await api("/api/stats"); $("#stat-total").textContent=s.total; $("#stat-borrowed").textContent=s.borrowed;
    $("#stat-overdue").textContent=s.overdue; $("#stat-popular").textContent=s.popular; $("#stat-popular").title=s.popular;
  }
  async function loadHistory() {
    const items = await api("/api/history"), el = $("#history-list");
    if (!items.length) { el.innerHTML = `<div class="empty-state">No activity yet. Add a book or wait for a student to borrow one.</div>`; return; }
    el.innerHTML = items.slice(0,8).map(item => {
      const action = item.action.toLowerCase(); const icon = action === "borrowed" ? "↗" : action === "returned" ? "↙" : action === "deleted" ? "−" : action === "edited" ? "✎" : "+";
      return `<div class="history-item"><div class="history-icon history-${escapeHTML(action)}">${icon}</div><div class="history-copy"><p><b>${escapeHTML(item.username)}</b> ${escapeHTML(action)} <b>${escapeHTML(item.title)}</b></p><small>Book ID BK-${String(item.bookId).padStart(3,"0")}</small></div><time>${dateText(item.timestamp)}</time></div>`;
    }).join("");
  }
  function openModal(book = null) {
    editingId = book ? book.id : null; $("#edit-id").value = book ? book.id : "";
    $("#modal-title").textContent = book ? "Edit book details" : "Add a new book";
    $("#modal-eyebrow").textContent = book ? `EDIT BOOK / BK-${String(book.id).padStart(3,"0")}` : "ADD TO COLLECTION";
    $("#book-title").value = book ? book.title : ""; $("#book-author").value = book ? book.author : "";
    $("#book-error").textContent = ""; modal.hidden = false; setTimeout(() => $("#book-title").focus(), 30);
  }
  function closeModal() { modal.hidden = true; }
  $("#add-open").addEventListener("click", () => openModal()); $("#modal-close").addEventListener("click", closeModal);
  $("#cancel-modal").addEventListener("click", closeModal); modal.addEventListener("click", e => { if (e.target === modal) closeModal(); });
  $("#book-form").addEventListener("submit", async e => {
    e.preventDefault(); const button = $("#save-book"); button.disabled=true; $("#book-error").textContent="";
    const body = { title: $("#book-title").value.trim(), author: $("#book-author").value.trim() };
    try {
      if (editingId) await api("/api/edit_book", { method:"POST", body:JSON.stringify({ ...body, id:editingId }) });
      else await api("/api/add_book", { method:"POST", body:JSON.stringify(body) });
      closeModal(); notify(editingId ? "Book details updated." : "A new book joined the shelves."); await Promise.all([loadBooks(),loadStats(),loadHistory()]);
    } catch (error) { $("#book-error").textContent=error.message; }
    finally { button.disabled=false; }
  });
  $("#admin-books").addEventListener("click", async e => {
    const edit = e.target.closest("[data-edit]"), del = e.target.closest("[data-delete]");
    if (edit) { const b=allBooks.find(x=>x.id===Number(edit.dataset.edit)); if(b) openModal(b); }
    if (del) {
      const id=Number(del.dataset.delete); if(!confirm("Delete this book from the library? This cannot be undone.")) return;
      try { const result=await api("/api/delete_book",{method:"POST",body:JSON.stringify({id})}); notify(result.message); await Promise.all([loadBooks(),loadStats(),loadHistory()]); }
      catch(error){notify(error.message,"error");}
    }
  });
  $("#book-search").addEventListener("input", e => { query=e.target.value; clearTimeout(window.searchTimer); window.searchTimer=setTimeout(()=>loadBooks().catch(err=>notify(err.message,"error")),180); });
  document.querySelectorAll("[data-filter]").forEach(btn=>btn.addEventListener("click",()=>{document.querySelectorAll("[data-filter]").forEach(b=>b.classList.remove("active"));btn.classList.add("active");filter=btn.dataset.filter;renderBooks();}));
  $("#refresh-books").addEventListener("click",()=>Promise.all([loadBooks(),loadStats(),loadHistory()]).then(()=>notify("Library refreshed." )).catch(e=>notify(e.message,"error")));
  Promise.all([loadBooks(),loadStats(),loadHistory()]).catch(e=>notify(e.message,"error"));
}

if (page === "student" && requireRole("student")) {
  const name=currentUser(); $("#profile-name").textContent=name; $("#student-avatar").textContent=name[0].toUpperCase(); $("#top-avatar").textContent=name[0].toUpperCase();
  let query="", searchTimer;
  async function loadStudentBooks() {
    const books=await api(`/api/books?q=${encodeURIComponent(query)}`); $("#student-book-count").textContent=`${books.length} ${books.length===1?"book":"books"}`;
    const grid=$("#student-books");
    if(!books.length){grid.innerHTML=`<div class="empty-state grid-empty">No books match that search. Try another title or author.</div>`;return;}
    grid.innerHTML=books.map(b=>`<article class="book-card"><div class="book-card-art art-${b.id%5}"><span class="book-card-id">BK-${String(b.id).padStart(3,"0")}</span><div class="cover-illustration"><span>${escapeHTML((b.title.split(" ")[0]||"BOOK").toUpperCase())}</span><b>${escapeHTML(b.title)}</b><small>${escapeHTML(b.author)}</small></div><span class="art-spark">✳</span></div><div class="book-card-body"><div class="book-card-meta"><span class="status-badge ${b.available?"status-available":b.overdue?"status-overdue":"status-borrowed"}"><i></i>${b.available?"Available":b.overdue?"Overdue":"Borrowed"}</span><span class="book-id-small">#${b.id}</span></div><h3>${escapeHTML(b.title)}</h3><p class="book-author">by ${escapeHTML(b.author)}</p><div class="book-card-footer">${b.available?`<span class="availability-note">Ready for you</span><button class="btn btn-primary btn-small" data-borrow="${b.id}">Borrow <span>↗</span></button>`:b.borrowedBy===name?`<span class="availability-note">On your shelf</span><button class="btn btn-ghost btn-small" data-return="${b.id}">Return</button>`:`<span class="availability-note">With another reader</span><button class="btn btn-ghost btn-small" disabled>Unavailable</button>`}</div></div></article>`).join("");
  }
  async function loadMyBooks() {
    const books=await api("/api/my_books"); $("#my-book-total").textContent=`${books.length} ${books.length===1?"BOOK":"BOOKS"}`;
    const el=$("#my-books-list");
    if(!books.length){el.innerHTML=`<div class="empty-state">You haven't borrowed a book yet. Your next read is waiting above.</div>`;return;}
    el.innerHTML=books.map(b=>`<div class="my-book-row"><div class="mini-cover cover-color-${b.id%5}">${escapeHTML(b.title[0].toUpperCase())}</div><div class="my-book-info"><b>${escapeHTML(b.title)}</b><small>${escapeHTML(b.author)}</small></div><div class="due-info ${b.overdue?"due-late":""}"><small>${b.overdue?`${b.daysOverdue} day(s) overdue`:`DUE DATE`}</small><b>${dateText(b.dueDate)}</b>${b.fine>0?`<small class="fine-note">Fine so far: ${money(b.fine)}</small>`:""}</div><button class="btn btn-ghost btn-small" data-return="${b.id}">Return book</button></div>`).join("");
  }
  async function refreshStudent() { await Promise.all([loadStudentBooks(),loadMyBooks()]); }
  $("#book-search").addEventListener("input",e=>{query=e.target.value;clearTimeout(searchTimer);searchTimer=setTimeout(()=>loadStudentBooks().catch(err=>notify(err.message,"error")),150);});
  document.addEventListener("keydown",e=>{if((e.metaKey||e.ctrlKey)&&e.key.toLowerCase()==="k"){e.preventDefault();$("#book-search").focus();}});
  document.addEventListener("click",async e=>{
    const borrow=e.target.closest("[data-borrow]"), ret=e.target.closest("[data-return]");
    if(borrow){borrow.disabled=true;try{const r=await api("/api/borrow",{method:"POST",body:JSON.stringify({id:Number(borrow.dataset.borrow)})});notify(r.message);await refreshStudent();}catch(err){notify(err.message,"error");borrow.disabled=false;}}
    if(ret){ret.disabled=true;try{const r=await api("/api/return",{method:"POST",body:JSON.stringify({id:Number(ret.dataset.return)})});notify(r.message);await refreshStudent();}catch(err){notify(err.message,"error");ret.disabled=false;}}
  });
  refreshStudent().catch(e=>notify(e.message,"error"));
}
