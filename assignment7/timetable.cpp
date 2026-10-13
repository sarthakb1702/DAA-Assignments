#include <iostream>
#include <fstream>
#include <sstream>
#include <vector>
#include <set>
#include <map>
#include <algorithm>
#include <chrono>
#include <random>
#include <numeric>
#include <iomanip>
using namespace std;

// ---------------------------------------------------------------
// University Exam Timetable Scheduling - Graph Colouring
// Algorithms : Greedy O(V^2), Welsh-Powell O(V^2), DSATUR O(V^2)
// Extras     : Room allocation (best fit), exact backtracking check,
//              scalability test on large random datasets
// Conflict graph is stored as adjacency lists (memory friendly).
// ---------------------------------------------------------------

struct Course { string id, name, dept; int students = 0; };
struct Room   { string id; int capacity; };
typedef vector<vector<int>> Graph;                 // adjacency lists
typedef chrono::high_resolution_clock Clock;
double msSince(Clock::time_point t0) { return chrono::duration<double, milli>(Clock::now() - t0).count(); }

vector<string> splitStr(const string& line, char d) {
    vector<string> out; string tok; stringstream ss(line);
    while (getline(ss, tok, d)) { if (!tok.empty() && tok.back() == '\r') tok.pop_back(); out.push_back(tok); }
    return out;
}

// ---------- Step 1: build conflict graph from student enrolments ----------
Graph buildGraph(int V, const vector<vector<int>>& enrol) {
    vector<pair<int,int>> edges;
    for (auto& cl : enrol)
        for (size_t i = 0; i < cl.size(); i++)
            for (size_t j = i + 1; j < cl.size(); j++) {
                edges.push_back({cl[i], cl[j]});
                edges.push_back({cl[j], cl[i]});
            }
    sort(edges.begin(), edges.end());
    edges.erase(unique(edges.begin(), edges.end()), edges.end());   // remove duplicate edges
    Graph g(V);
    for (auto& e : edges) g[e.first].push_back(e.second);
    return g;
}

// ---------- Colouring helper: smallest colour not used by neighbours ----------
int smallestFree(const Graph& g, const vector<int>& col, int u, vector<int>& mark, int stamp) {
    for (int v : g[u]) if (col[v] >= 0) mark[col[v]] = stamp;
    int c = 0; while (mark[c] == stamp) c++;
    return c;
}

// ---------- 1. Greedy colouring (vertices in input order) ----------
vector<int> greedyColour(const Graph& g) {
    int V = g.size(); vector<int> col(V, -1), mark(V + 2, -1);
    for (int u = 0; u < V; u++) col[u] = smallestFree(g, col, u, mark, u);
    return col;
}

// ---------- 2. Welsh-Powell (sort by degree descending, then greedy) ----------
vector<int> welshPowell(const Graph& g) {
    int V = g.size(); vector<int> order(V);
    iota(order.begin(), order.end(), 0);
    stable_sort(order.begin(), order.end(), [&](int a, int b) { return g[a].size() > g[b].size(); });
    vector<int> col(V, -1), mark(V + 2, -1);
    for (int k = 0; k < V; k++) col[order[k]] = smallestFree(g, col, order[k], mark, k);
    return col;
}

// ---------- 3. DSATUR (pick vertex with most distinct neighbour colours) ----------
vector<int> dsatur(const Graph& g) {
    int V = g.size(); vector<int> col(V, -1), mark(V + 2, -1);
    vector<set<int>> sat(V);                         // colours seen around each vertex
    for (int step = 0; step < V; step++) {
        int best = -1;
        for (int u = 0; u < V; u++) {
            if (col[u] != -1) continue;
            if (best == -1 || sat[u].size() > sat[best].size() ||
               (sat[u].size() == sat[best].size() && g[u].size() > g[best].size())) best = u;
        }
        col[best] = smallestFree(g, col, best, mark, step);
        for (int v : g[best]) sat[v].insert(col[best]);
    }
    return col;
}

int numColours(const vector<int>& col) { return col.empty() ? 0 : *max_element(col.begin(), col.end()) + 1; }

bool validColouring(const Graph& g, const vector<int>& col) {
    for (size_t u = 0; u < g.size(); u++) {
        if (col[u] < 0) return false;
        for (int v : g[u]) if (col[u] == col[v]) return false;
    }
    return true;
}

// ---------- Exact chromatic number by backtracking (branch and bound) ----------
struct BT {
    const Graph& g; int V; vector<int> col, bestCol; int best; long long nodes = 0, cap; bool complete = true;
    vector<int> order;
    BT(const Graph& gr, int ub, const vector<int>& init, long long c) : g(gr), V(gr.size()), col(gr.size(), -1), bestCol(init), best(ub), cap(c) {
        order.resize(V); iota(order.begin(), order.end(), 0);
        stable_sort(order.begin(), order.end(), [&](int a, int b) { return g[a].size() > g[b].size(); });
    }
    void rec(int k, int used) {
        if (!complete) return;
        if (++nodes > cap) { complete = false; return; }
        if (used >= best) return;                    // prune: cannot beat best
        if (k == V) { best = used; bestCol = col; return; }
        int u = order[k];
        vector<char> bad(used + 1, 0);
        for (int v : g[u]) if (col[v] >= 0) bad[col[v]] = 1;
        for (int c = 0; c <= used; c++) {
            if (c < used && bad[c]) continue;
            if (c == used && used + 1 >= best) continue;      // a new colour would not improve on best
            col[u] = c;
            rec(k + 1, max(used, c + 1));
            col[u] = -1;
            if (!complete) return;
        }
    }
};

// ---------- Step 4: room allocation per slot (best fit: smallest room that fits) ----------
struct Assign { int slot; int room; };
// exams that do not fit in any free room of their slot are moved to extra slots
vector<Assign> allocateRooms(const vector<Course>& C, const vector<int>& col, const vector<Room>& R, int& totalSlots) {
    int V = C.size(); vector<Assign> as(V, {-1, -1});
    int k = numColours(col);
    vector<vector<int>> slots(k);
    for (int c = 0; c < V; c++) slots[col[c]].push_back(c);
    vector<int> roomOrder(R.size()); iota(roomOrder.begin(), roomOrder.end(), 0);
    sort(roomOrder.begin(), roomOrder.end(), [&](int a, int b) { return R[a].capacity < R[b].capacity; });  // ascending
    for (size_t t = 0; t < slots.size(); t++) {                    // slots grows while overflow exists
        auto& ex = slots[t];
        sort(ex.begin(), ex.end(), [&](int a, int b) { return C[a].students > C[b].students; });  // biggest first
        vector<bool> used(R.size(), false); vector<int> overflow;
        for (int e : ex) {
            int pick = -1;
            for (int r : roomOrder) if (!used[r] && R[r].capacity >= C[e].students) { pick = r; break; }
            if (pick == -1) overflow.push_back(e);
            else { used[pick] = true; as[e] = {(int)t, pick}; }
        }
        if (!overflow.empty()) slots.push_back(overflow);          // overflow set is still conflict-free
    }
    totalSlots = slots.size();
    return as;
}

void printTimetable(const vector<Course>& C, const vector<Assign>& as, const vector<Room>& R) {
    vector<int> order(C.size()); iota(order.begin(), order.end(), 0);
    stable_sort(order.begin(), order.end(), [&](int a, int b) { return as[a].slot < as[b].slot; });
    cout << left << setw(8) << "Course" << setw(36) << "Name" << setw(10) << "Students" << setw(10) << "Slot" << setw(8) << "Room" << "Capacity\n" << right;
    cout << "--------------------------------------------------------------------------------\n";
    int prev = -1;
    for (int c : order) {
        if (prev != -1 && as[c].slot != prev) cout << "\n";
        prev = as[c].slot;
        cout << left << setw(8) << C[c].id << setw(36) << C[c].name.substr(0, 34) << setw(10) << C[c].students
             << setw(10) << ("Slot " + to_string(as[c].slot + 1)) << setw(8) << R[as[c].room].id << R[as[c].room].capacity << "\n" << right;
    }
}

// ---------- load data from CSV files ----------
bool loadData(const string& cf, const string& sf, const string& rf, vector<Course>& C, vector<vector<int>>& enrol, vector<Room>& R) {
    ifstream fc(cf), fs(sf), fr(rf);
    if (!fc || !fs || !fr) return false;
    string line; map<string,int> idx;
    getline(fc, line);
    while (getline(fc, line)) { if (line.empty()) continue; auto f = splitStr(line, ','); idx[f[0]] = C.size(); C.push_back({f[0], f[1], f[2], 0}); }
    getline(fs, line);
    while (getline(fs, line)) {
        if (line.empty()) continue;
        auto f = splitStr(line, ','); vector<int> cl;
        for (auto& cid : splitStr(f[1], ';')) { int k = idx.at(cid); cl.push_back(k); C[k].students++; }
        enrol.push_back(cl);
    }
    getline(fr, line);
    while (getline(fr, line)) { if (line.empty()) continue; auto f = splitStr(line, ','); R.push_back({f[0], stoi(f[1])}); }
    return true;
}

int main() {
    // ================= 0. Worked example from the assignment =================
    cout << "==============================================\n     SAMPLE (assignment example)\n==============================================\n";
    {
        vector<Course> C = {{"C1","C1","",0},{"C2","C2","",0},{"C3","C3","",0},{"C4","C4","",0}};
        vector<vector<int>> en = {{0,1},{1,2},{0,2},{3}};
        for (auto& e : en) for (int c : e) C[c].students++;
        vector<Room> R = {{"R1",100},{"R2",80},{"R3",50}};
        Graph g = buildGraph(4, en); vector<int> col = dsatur(g);
        int slots; auto as = allocateRooms(C, col, R, slots);
        cout << "Students enrolled -> S1:C1,C2  S2:C2,C3  S3:C1,C3  S4:C4\nRooms -> R1=100 R2=80 R3=50\n\n";
        cout << left << setw(8) << "Course" << setw(10) << "Slot" << "Room\n" << right;
        for (int c = 0; c < 4; c++) cout << left << setw(8) << C[c].id << setw(10) << ("Slot " + to_string(as[c].slot + 1)) << R[as[c].room].id << "\n" << right;
        cout << "Slots used: " << slots << "\n";
    }

    // ================= 1. Load dataset =================
    vector<Course> C; vector<vector<int>> enrol; vector<Room> R;
    if (!loadData("courses.csv", "students.csv", "rooms.csv", C, enrol, R)) { cout << "Error: courses.csv / students.csv / rooms.csv not found\n"; return 1; }
    int V = C.size();
    Graph g = buildGraph(V, enrol);
    long long E = 0; int maxDeg = 0;
    for (auto& a : g) { E += a.size(); maxDeg = max(maxDeg, (int)a.size()); }
    E /= 2;
    cout << "\n==============================================\n     UNIVERSITY EXAM TIMETABLE SCHEDULING\n==============================================\n";
    cout << "Total courses read  : " << V << "\n";
    cout << "Total students read : " << enrol.size() << "\n";
    cout << "Total rooms read    : " << R.size() << "\n";
    cout << "Conflict graph      : " << V << " vertices, " << E << " edges, max degree " << maxDeg << "\n";

    // ================= 2. Compare the three algorithms =================
    cout << "\n---------- Algorithm comparison (dataset) ----------\n";
    cout << left << setw(18) << "Algorithm" << setw(16) << "Slots(colours)" << setw(12) << "Valid" << "Time(ms)\n" << right;
    cout << fixed << setprecision(3);
    struct Algo { string name; vector<int> col; double ms; };
    vector<Algo> algos;
    auto t0 = Clock::now(); auto c1 = greedyColour(g); algos.push_back({"Greedy", c1, msSince(t0)});
    t0 = Clock::now();      auto c2 = welshPowell(g);  algos.push_back({"Welsh-Powell", c2, msSince(t0)});
    t0 = Clock::now();      auto c3 = dsatur(g);       algos.push_back({"DSATUR", c3, msSince(t0)});
    for (auto& a : algos)
        cout << left << setw(18) << a.name << setw(16) << numColours(a.col) << setw(12) << (validColouring(g, a.col) ? "yes" : "NO") << a.ms << "\n" << right;

    // exact check by backtracking
    int ub = INT32_MAX; vector<int> bestInit;
    for (auto& a : algos) if (numColours(a.col) < ub) { ub = numColours(a.col); bestInit = a.col; }
    t0 = Clock::now();
    BT bt(g, ub, bestInit, 5000000); bt.rec(0, 0);
    double tbt = msSince(t0);
    cout << left << setw(18) << "Backtracking" << setw(16) << bt.best << setw(12) << (validColouring(g, bt.bestCol) ? "yes" : "NO") << tbt
         << (bt.complete ? "  (proved optimal)" : "  (node limit reached)") << "\n" << right;
    int clique = 0; // simple lower bound: max over edges of common-neighbour greedy clique
    for (int u = 0; u < V; u++) {
        vector<int> cl = {u}; vector<int> cand = g[u];
        sort(cand.begin(), cand.end(), [&](int a, int b) { return g[a].size() > g[b].size(); });
        for (int v : cand) { bool ok = true; for (int w : cl) if (!binary_search(g[v].begin(), g[v].end(), w)) { ok = false; break; } if (ok) cl.push_back(v); }
        clique = max(clique, (int)cl.size());
    }
    cout << "Lower bound (clique found) : " << clique << " slots\n";

    // ================= 3. Final timetable with room allocation (DSATUR) =================
    cout << "\n---------- Final timetable (DSATUR + best-fit room allocation) ----------\n";
    int slots; auto as = allocateRooms(C, c3, R, slots);
    printTimetable(C, as, R);
    cout << "\nTotal exam slots used : " << slots << " (colours from DSATUR: " << numColours(c3) << ")\n";
    // verify no student has two exams in one slot and no room is double booked
    bool ok = true;
    for (auto& cl : enrol) { set<int> s; for (int c : cl) if (!s.insert(as[c].slot).second) ok = false; }
    set<pair<int,int>> rs; for (int c = 0; c < V; c++) if (!rs.insert({as[c].slot, as[c].room}).second) ok = false;
    for (int c = 0; c < V; c++) if (R[as[c].room].capacity < C[c].students) ok = false;
    cout << "Verification (no student clash, no room clash, capacity ok): " << (ok ? "PASSED" : "FAILED") << "\n";

    // room utilisation
    long long seats = 0, used = 0; for (int c = 0; c < V; c++) { seats += R[as[c].room].capacity; used += C[c].students; }
    cout << "Room utilisation (students / seats allotted): " << setprecision(1) << 100.0 * used / seats << " %\n" << setprecision(3);

    // ================= 4. Scalability test on large random datasets =================
    cout << "\n---------- Scalability test (random data, 5 courses per student) ----------\n";
    cout << left << setw(10) << "Courses" << setw(10) << "Students" << setw(12) << "Edges"
         << setw(10) << "Build(ms)" << setw(12) << "Greedy" << setw(8) << "(ms)" << setw(12) << "W-Powell" << setw(8) << "(ms)" << setw(10) << "DSATUR" << "(ms)\n" << right;
    mt19937 rng(2026);
    for (int n : {250, 500, 1000, 2000, 4000}) {
        int S = n * 10;
        vector<vector<int>> en(S);
        // skewed popularity: a few very popular courses, long tail of electives
        discrete_distribution<int> pick(n, 0, n, [&](double x) { return 1.0 / (1.0 + 4.0 * x / n); });
        for (auto& cl : en) { set<int> s; while (s.size() < 5) s.insert(pick(rng)); cl.assign(s.begin(), s.end()); }
        t0 = Clock::now(); Graph G = buildGraph(n, en); double tb = msSince(t0);
        long long e = 0; for (auto& a : G) e += a.size(); e /= 2;
        t0 = Clock::now(); auto a1 = greedyColour(G); double m1 = msSince(t0);
        t0 = Clock::now(); auto a2 = welshPowell(G);  double m2 = msSince(t0);
        t0 = Clock::now(); auto a3 = dsatur(G);       double m3 = msSince(t0);
        bool v = validColouring(G, a1) && validColouring(G, a2) && validColouring(G, a3);
        cout << left << setw(10) << n << setw(10) << S << setw(12) << e << setw(10) << tb
             << setw(12) << numColours(a1) << setw(8) << m1 << setw(12) << numColours(a2) << setw(8) << m2 << setw(10) << numColours(a3) << m3
             << (v ? "" : "  INVALID") << "\n" << right;
    }
    cout << "==============================================\n";
    return 0;
}
