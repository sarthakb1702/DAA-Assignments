#include <iostream>
#include <fstream>
#include <sstream>
#include <vector>
#include <queue>
#include <algorithm>
#include <chrono>
#include <cmath>
#include <iomanip>
#include <cstdint>
#include <map>
using namespace std;

// ---------------------------------------------------------------
// SwiftShip delivery routes - Travelling Salesman Problem
// Exact solution with Least Cost (LC) Branch and Bound using matrix reduction
// Extras : fuel cost depends on road condition, delivery time limit,
//          comparison with brute force O((N-1)!) and nearest neighbour heuristic
// ---------------------------------------------------------------

typedef int16_t Cell;
const Cell INF = 30000;                      // "infinity" for blocked edges
const double FUEL_RS_PER_KM = 9.0;           // diesel truck, Rs per km on a good road
const double STOP_HOURS = 0.5;               // unloading time at every delivery city
typedef chrono::high_resolution_clock Clock;
double msSince(Clock::time_point t0) { return chrono::duration<double, milli>(Clock::now() - t0).count(); }

struct City { int id; string name; double lat, lon; };
struct Problem {
    int n = 0;
    vector<vector<int>> cost;                // fuel cost in Rs
    vector<vector<double>> hours;            // driving time in hours
    vector<vector<int>> km;
    vector<string> name;
};

vector<string> splitCSV(const string& line) {
    vector<string> out; string tok; stringstream ss(line);
    while (getline(ss, tok, ',')) { if (!tok.empty() && tok.back() == '\r') tok.pop_back(); out.push_back(tok); }
    return out;
}

// ---------- matrix reduction: subtract row minima then column minima ----------
int reduceMatrix(vector<Cell>& m, int n) {
    int total = 0;
    for (int i = 0; i < n; i++) {
        Cell mn = INF;
        for (int j = 0; j < n; j++) mn = min(mn, m[i * n + j]);
        if (mn != INF && mn > 0) { for (int j = 0; j < n; j++) if (m[i * n + j] != INF) m[i * n + j] -= mn; total += mn; }
    }
    for (int j = 0; j < n; j++) {
        Cell mn = INF;
        for (int i = 0; i < n; i++) mn = min(mn, m[i * n + j]);
        if (mn != INF && mn > 0) { for (int i = 0; i < n; i++) if (m[i * n + j] != INF) m[i * n + j] -= mn; total += mn; }
    }
    return total;
}

struct Node {
    vector<Cell> m;                          // reduced matrix
    vector<uint8_t> path;
    int lb = 0, level = 0;
    double hrs = 0;                          // driving + stop time so far
};
struct NodeCmp {                             // min-heap on lb, deeper nodes first on ties
    bool operator()(const Node& a, const Node& b) const { return a.lb != b.lb ? a.lb > b.lb : a.level < b.level; }
};

struct Stats {
    int best = -1; vector<int> route; double hours = 0;
    long long generated = 0, expanded = 0, prunedBound = 0, prunedTime = 0, maxPQ = 0;
    double ms = 0; bool complete = true; int rootLB = 0;
};

// ---------- LC Branch and Bound ----------
// timeLimit <= 0 means "no time constraint"
Stats solveBB(const Problem& P, int start, double timeLimit, long long maxQueue = 3000000, bool verbose = false) {
    int n = P.n; Stats st; auto t0 = Clock::now();
    double minT = 1e18;
    for (int i = 0; i < n; i++) for (int j = 0; j < n; j++) if (i != j) minT = min(minT, P.hours[i][j]);
    Node root; root.m.assign(n * n, INF);
    for (int i = 0; i < n; i++) for (int j = 0; j < n; j++) if (i != j) root.m[i * n + j] = (Cell)P.cost[i][j];
    if (verbose) {
        cout << "Initial cost matrix:\n";
        for (int i = 0; i < n; i++) { for (int j = 0; j < n; j++) { if (root.m[i*n+j] == INF) cout << setw(5) << "inf"; else cout << setw(5) << root.m[i*n+j]; } cout << "\n"; }
    }
    root.lb = reduceMatrix(root.m, n); st.rootLB = root.lb;
    if (verbose) {
        cout << "Reduced cost matrix (lb0 = " << root.lb << "):\n";
        for (int i = 0; i < n; i++) { for (int j = 0; j < n; j++) { if (root.m[i*n+j] == INF) cout << setw(5) << "inf"; else cout << setw(5) << root.m[i*n+j]; } cout << "\n"; }
    }
    root.path = {(uint8_t)start}; root.level = 0;
    int best = INT32_MAX; vector<uint8_t> bestPath;
    priority_queue<Node, vector<Node>, NodeCmp> pq;
    pq.push(root);
    while (!pq.empty()) {
        Node node = pq.top(); pq.pop();
        if (node.lb >= best) { st.prunedBound++; continue; }                 // prune
        if (node.level == n - 1) {                                           // complete tour, lb is exact
            best = node.lb; bestPath = node.path; st.hours = node.hrs; continue;
        }
        st.expanded++;
        int u = node.path.back();
        for (int v = 0; v < n; v++) {
            if (find(node.path.begin(), node.path.end(), v) != node.path.end()) continue;
            if (node.m[u * n + v] == INF) continue;
            double hr = node.hrs + P.hours[u][v] + STOP_HOURS;
            int remaining = n - 1 - (node.level + 1);                         // cities still unvisited after v
            if (timeLimit > 0) {
                double need = hr + (remaining + 1) * minT + remaining * STOP_HOURS;   // optimistic completion time
                if (node.level + 1 == n - 1) need = hr + P.hours[v][start];
                if (need > timeLimit) { st.prunedTime++; continue; }
            }
            Node ch; ch.m = node.m;
            for (int k = 0; k < n; k++) { ch.m[u * n + k] = INF; ch.m[k * n + v] = INF; }   // block row u, column v
            if (node.level + 1 < n - 1) ch.m[v * n + start] = INF;                          // no early return to start
            int edge = node.m[u * n + v];                                                    // reduced cost of u->v
            ch.lb = node.lb + edge + reduceMatrix(ch.m, n);
            ch.path = node.path; ch.path.push_back(v); ch.level = node.level + 1; ch.hrs = hr;
            if (ch.level == n - 1) ch.hrs = hr + P.hours[v][start];
            st.generated++;
            if (ch.lb < best) pq.push(move(ch)); else st.prunedBound++;
        }
        st.maxPQ = max<long long>(st.maxPQ, pq.size());
        if ((long long)pq.size() > maxQueue) { st.complete = false; break; }                // memory guard
    }
    st.ms = msSince(t0);
    if (best != INT32_MAX) { st.best = best; for (auto c : bestPath) st.route.push_back(c); st.route.push_back(start); }
    return st;
}

// ---------- Brute force: all (N-1)! permutations ----------
long long bfLeaves;
void bfRec(const Problem& P, int start, vector<bool>& seen, int cur, int cnt, int cost, int& best) {
    int n = P.n;
    if (cnt == n) { bfLeaves++; best = min(best, cost + P.cost[cur][start]); return; }
    for (int v = 0; v < n; v++) if (!seen[v]) { seen[v] = true; bfRec(P, start, seen, v, cnt + 1, cost + P.cost[cur][v], best); seen[v] = false; }
}
int bruteForce(const Problem& P, int start, double& ms) {
    auto t0 = Clock::now(); vector<bool> seen(P.n, false); seen[start] = true; int best = INT32_MAX; bfLeaves = 0;
    bfRec(P, start, seen, start, 1, 0, best); ms = msSince(t0); return best;
}

// ---------- Nearest neighbour heuristic ----------
int nearestNeighbour(const Problem& P, int start) {
    vector<bool> seen(P.n, false); seen[start] = true; int cur = start, total = 0;
    for (int k = 1; k < P.n; k++) {
        int b = -1; for (int v = 0; v < P.n; v++) if (!seen[v] && (b == -1 || P.cost[cur][v] < P.cost[cur][b])) b = v;
        total += P.cost[cur][b]; seen[b] = true; cur = b;
    }
    return total + P.cost[cur][start];
}

// ---------- Held-Karp dynamic programming (independent check, O(N^2 2^N)) ----------
int heldKarp(const Problem& P, int start) {
    int n = P.n; const int BIG = 1e9; vector<int> dp((size_t(1) << n) * n, BIG);
    dp[(size_t(1) << start) * n + start] = 0;
    for (size_t mask = 0; mask < (size_t(1) << n); mask++) {
        if (!(mask >> start & 1)) continue;
        for (int j = 0; j < n; j++) {
            int cur = dp[mask * n + j]; if (cur == BIG) continue;
            for (int k = 0; k < n; k++) if (!(mask >> k & 1)) { size_t nm = mask | (size_t(1) << k); dp[nm * n + k] = min(dp[nm * n + k], cur + P.cost[j][k]); }
        }
    }
    int best = BIG; size_t full = (size_t(1) << n) - 1;
    for (int j = 0; j < n; j++) if (j != start && dp[full * n + j] != BIG) best = min(best, dp[full * n + j] + P.cost[j][start]);
    return best;
}

// ---------- Brute force with a time limit (check for the constrained B&B) ----------
void bfTimed(const Problem& P, int start, vector<bool>& seen, int cur, int cnt, int cost, double hrs, double limit, int& best) {
    int n = P.n;
    if (cnt == n) { if (hrs + P.hours[cur][start] <= limit + 1e-9) best = min(best, cost + P.cost[cur][start]); return; }
    for (int v = 0; v < n; v++) if (!seen[v]) { seen[v] = true; bfTimed(P, start, seen, v, cnt + 1, cost + P.cost[cur][v], hrs + P.hours[cur][v] + STOP_HOURS, limit, best); seen[v] = false; }
}

Problem subProblem(const Problem& F, int n) {
    Problem P; P.n = n;
    P.cost.assign(n, vector<int>(n, 0)); P.hours.assign(n, vector<double>(n, 0)); P.km.assign(n, vector<int>(n, 0));
    for (int i = 0; i < n; i++) { P.name.push_back(F.name[i]); for (int j = 0; j < n; j++) { P.cost[i][j] = F.cost[i][j]; P.hours[i][j] = F.hours[i][j]; P.km[i][j] = F.km[i][j]; } }
    return P;
}

void printRoute(const Problem& P, const Stats& s) {
    int totKm = 0;
    cout << "Route : ";
    for (size_t i = 0; i < s.route.size(); i++) cout << P.name[s.route[i]] << (i + 1 < s.route.size() ? " -> " : "\n");
    cout << left << setw(26) << "Leg" << setw(10) << "Km" << setw(12) << "Fuel(Rs)" << "Drive(h)\n" << right;
    for (size_t i = 0; i + 1 < s.route.size(); i++) {
        int a = s.route[i], b = s.route[i + 1]; totKm += P.km[a][b];
        cout << left << setw(26) << (P.name[a] + " -> " + P.name[b]) << setw(10) << P.km[a][b] << setw(12) << P.cost[a][b] << fixed << setprecision(1) << P.hours[a][b] << "\n" << right;
    }
    cout << "Total distance      : " << totKm << " km\n";
    cout << "Total fuel cost     : Rs " << s.best << "\n";
    cout << fixed << setprecision(1) << "Total trip time     : " << s.hours << " h (driving + " << STOP_HOURS << " h stop in every city)\n";
}

int main(int argc, char** argv) {
    cout << "==============================================\n     SWIFTSHIP ROUTE OPTIMIZATION (TSP)\n     Least Cost Branch and Bound\n==============================================\n";
    // ================= 0. Worked example (classic 5-city matrix) =================
    cout << "\n---------- Sample: 5 cities, cost matrix from the notes ----------\n";
    {
        int S[5][5] = {{0,20,30,10,11},{15,0,16,4,2},{3,5,0,2,4},{19,6,18,0,3},{16,4,7,16,0}};
        Problem P; P.n = 5; P.cost.assign(5, vector<int>(5)); P.hours.assign(5, vector<double>(5, 1)); P.km.assign(5, vector<int>(5, 0));
        for (int i = 0; i < 5; i++) { P.name.push_back("C" + to_string(i + 1)); for (int j = 0; j < 5; j++) P.cost[i][j] = S[i][j]; }
        Stats s = solveBB(P, 0, 0, 3000000, true);
        double ms; int bf = bruteForce(P, 0, ms);
        cout << "Optimal tour cost   : " << s.best << "   (brute force: " << bf << ", " << (s.best == bf ? "match" : "MISMATCH") << ")\n";
        cout << "Optimal tour        : "; for (size_t i = 0; i < s.route.size(); i++) cout << P.name[s.route[i]] << (i + 1 < s.route.size() ? " -> " : "\n");
        cout << "Nodes generated " << s.generated << ", expanded " << s.expanded << ", pruned " << s.prunedBound << "  (brute force would check " << bfLeaves << " tours)\n";
    }

    // ================= 1. Load dataset =================
    ifstream fc("cities.csv"), fr("roads.csv");
    if (!fc || !fr) { cout << "Error: cities.csv / roads.csv not found\n"; return 1; }
    Problem F; string line; getline(fc, line);
    while (getline(fc, line)) { if (line.empty()) continue; auto f = splitCSV(line); F.name.push_back(f[1]); }
    int NF = F.name.size();
    F.n = NF; F.cost.assign(NF, vector<int>(NF, 0)); F.hours.assign(NF, vector<double>(NF, 0)); F.km.assign(NF, vector<int>(NF, 0));
    map<string, pair<double,double>> cond = {{"good", {1.0, 55.0}}, {"fair", {1.2, 45.0}}, {"poor", {1.3, 25.0}}, {"ghat", {1.2, 25.0}}, {"expressway", {2.2, 85.0}}};   // {fuel multiplier, speed km/h}
    map<string,int> cnt;
    getline(fr, line);
    while (getline(fr, line)) {
        if (line.empty()) continue;
        auto f = splitCSV(line); int a = stoi(f[0]), b = stoi(f[1]), d = stoi(f[2]); auto c = cond.at(f[3]);
        cnt[f[3]]++;
        int rs = (int)lround(d * FUEL_RS_PER_KM * c.first);
        double h = d / c.second;
        F.cost[a][b] = F.cost[b][a] = rs; F.hours[a][b] = F.hours[b][a] = h; F.km[a][b] = F.km[b][a] = d;
    }
    int N = 15; int start = 0;
    if (argc > 1) N = min(stoi(argv[1]), NF);
    Problem P = subProblem(F, N);
    cout << "\nCities in file      : " << NF << "   (roads: good " << cnt["good"] << ", fair " << cnt["fair"] << ", poor " << cnt["poor"] << ", ghat " << cnt["ghat"] << ", expressway " << cnt["expressway"] << ")\n";
    cout << "Cities in this run  : " << N << "   start/end depot: " << P.name[start] << "\n";
    cout << "Fuel cost model     : Rs " << FUEL_RS_PER_KM << "/km x road factor (good 1.0, fair 1.2, poor 1.3, ghat 1.2, expressway 2.2)\n                      speed (km/h): good 55, fair 45, poor 25, ghat 25, expressway 85\n";

    // ================= 2. LC Branch and Bound, no time limit =================
    cout << "\n---------- LC Branch and Bound (no time limit) ----------\n";
    Stats s1 = solveBB(P, start, 0);
    printRoute(P, s1);
    cout << "Root lower bound    : Rs " << s1.rootLB << "\n";
    cout << "Nodes generated     : " << s1.generated << "\nNodes expanded      : " << s1.expanded << "\nPruned by bound     : " << s1.prunedBound
         << "\nMax priority queue  : " << s1.maxPQ << "\nTime taken          : " << setprecision(3) << s1.ms << " ms\n";

    // ================= 3. With delivery time limit =================
    // fastest possible tour (B&B on driving time) tells us what limits are feasible at all
    Problem T = P; for (int i = 0; i < N; i++) for (int j = 0; j < N; j++) T.cost[i][j] = (int)lround(P.hours[i][j] * 10);
    Stats sT = solveBB(T, start, 0);
    double limit = floor((sT.hours + s1.hours) / 2 * 10) / 10.0;
    if (argc > 2) limit = stod(argv[2]);
    cout << "\n---------- LC Branch and Bound with time limit " << setprecision(1) << limit << " h ----------\n";
    cout << "Cheapest route takes " << s1.hours << " h; the fastest possible tour takes " << sT.hours << " h.\n";
    cout << "(limit = halfway between the two, so the cheapest route is no longer allowed)\n";
    Stats s2 = solveBB(P, start, limit);
    if (s2.best < 0) cout << "No tour finishes within the limit.\n";
    else {
        printRoute(P, s2);
        cout << "Extra fuel cost vs unconstrained optimum : Rs " << s2.best - s1.best << "  (time saved " << setprecision(1) << s1.hours - s2.hours << " h)\n";
        cout << "Nodes generated     : " << s2.generated << "   pruned by time: " << s2.prunedTime << "   Time taken: " << setprecision(3) << s2.ms << " ms\n";
    }

    // ================= 4. Compare approaches =================
    cout << "\n---------- Brute force vs LC Branch and Bound (same cost model) ----------\n";
    cout << left << setw(4) << "N" << setw(14) << "(N-1)!" << setw(12) << "BF(ms)" << setw(12) << "BB(ms)" << setw(12) << "BB nodes" << setw(12) << "BB/(N-1)!" << setw(10) << "Cost(Rs)" << "Match\n" << right;
    cout << fixed << setprecision(3);
    for (int n : {6, 8, 9, 10, 11, 12}) {
        Problem Q = subProblem(F, n); double bms; int bf = bruteForce(Q, start, bms);
        Stats sb = solveBB(Q, start, 0);
        cout << left << setw(4) << n << setw(14) << bfLeaves << setw(12) << bms << setw(12) << sb.ms << setw(12) << sb.generated
             << setw(12) << setprecision(6) << (double)sb.generated / bfLeaves << setprecision(3) << setw(10) << sb.best << (sb.best == bf ? "yes" : "NO") << "\n" << right;
    }
    cout << "\n---------- Check of the time-limited search (N = 10, brute force with the same limit) ----------\n";
    {
        Problem Q = subProblem(F, 10); Stats a = solveBB(Q, start, 0);
        Problem TQ = Q; for (int i = 0; i < 10; i++) for (int j = 0; j < 10; j++) TQ.cost[i][j] = (int)lround(Q.hours[i][j] * 10);
        Stats f = solveBB(TQ, start, 0); double lim = floor((a.hours + f.hours) / 2 * 10) / 10.0;
        Stats b = solveBB(Q, start, lim);
        vector<bool> seen(10, false); seen[start] = true; int bf = INT32_MAX; bfTimed(Q, start, seen, start, 1, 0, 0.0, lim, bf);
        cout << "Cheapest tour " << a.best << " Rs in " << setprecision(1) << a.hours << " h, fastest tour " << f.hours << " h, limit " << lim << " h\n";
        cout << "B&B with limit: Rs " << b.best << "   brute force with limit: Rs " << bf << "   " << (b.best == bf ? "match" : "MISMATCH") << "\n";
    }
    cout << "\n---------- LC Branch and Bound on larger instances (brute force infeasible) ----------\n";
    cout << left << setw(4) << "N" << setw(20) << "(N-1)!" << setw(12) << "BB(ms)" << setw(12) << "Generated" << setw(12) << "Expanded" << setw(10) << "MaxPQ" << setw(10) << "Cost(Rs)" << setw(10) << "NN(Rs)" << setw(10) << "NN gap" << "Held-Karp\n" << right;
    for (int n : {13, 14, 15, 16, 17, 18}) {
        if (n > NF) break;
        Problem Q = subProblem(F, n); Stats sb = solveBB(Q, start, 0);
        double fact = 1; for (int k = 2; k < n; k++) fact *= k;
        int nn = nearestNeighbour(Q, start);
        ostringstream fs; fs << scientific << setprecision(2) << fact;
        cout << left << setw(4) << n << setw(20) << fs.str() << fixed << setprecision(1) << setw(12) << sb.ms << setw(12) << sb.generated << setw(12) << sb.expanded << setw(10) << sb.maxPQ;
        if (sb.complete) cout << setw(10) << sb.best; else cout << setw(10) << "limit";
        cout << setw(10) << nn;
        ostringstream gs; if (sb.complete) gs << "+" << setprecision(1) << fixed << 100.0 * (nn - sb.best) / sb.best << " %";
        cout << setw(10) << gs.str() << (sb.complete && heldKarp(Q, start) == sb.best ? "match" : "NO") << "\n" << right;
    }
    cout << "==============================================\n";
    return 0;
}
