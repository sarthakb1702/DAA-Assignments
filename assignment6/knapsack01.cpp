#include <iostream>
#include <fstream>
#include <sstream>
#include <vector>
#include <algorithm>
#include <chrono>
#include <cmath>
#include <iomanip>
using namespace std;

// ---------------------------------------------------------------
// Disaster Relief Resource Allocation - 0/1 Knapsack
// Approaches: Brute Force O(2^n), Dynamic Programming O(N*W), Greedy O(n log n)
// Perishable items (medicine, food) get a priority boost on utility.
// ---------------------------------------------------------------

const double PRIORITY_FACTOR = 1.25;      // boost for perishable / life-saving items

struct Item {
    int id;
    string name, category;
    int weight;
    int baseUtility;
    bool perishable;
    int utility;                           // effective utility after priority boost
};

struct Result { int utility = 0; int weight = 0; vector<int> chosen; };   // chosen = indices into items

typedef chrono::high_resolution_clock Clock;
double msSince(Clock::time_point t0) { return chrono::duration<double, milli>(Clock::now() - t0).count(); }

vector<string> splitCSV(const string& line) {
    vector<string> out; string tok; stringstream ss(line);
    while (getline(ss, tok, ',')) { if (!tok.empty() && tok.back() == '\r') tok.pop_back(); out.push_back(tok); }
    return out;
}

// ---------- 1. Dynamic Programming (bottom-up table + traceback) ----------
Result knapsackDP(const vector<Item>& items, const vector<int>& idx, int W) {
    int n = idx.size();
    vector<vector<int>> dp(n + 1, vector<int>(W + 1, 0));
    for (int i = 1; i <= n; i++) {
        const Item& it = items[idx[i - 1]];
        for (int w = 1; w <= W; w++) {
            if (it.weight <= w) dp[i][w] = max(it.utility + dp[i - 1][w - it.weight], dp[i - 1][w]);
            else                dp[i][w] = dp[i - 1][w];
        }
    }
    Result r; r.utility = dp[n][W];
    int w = W;
    for (int i = n; i >= 1; i--)
        if (dp[i][w] != dp[i - 1][w]) {             // item i was included
            r.chosen.push_back(idx[i - 1]);
            w -= items[idx[i - 1]].weight;
            r.weight += items[idx[i - 1]].weight;
        }
    reverse(r.chosen.begin(), r.chosen.end());
    return r;
}

// ---------- 2. Brute Force (all 2^n subsets) ----------
void bfRec(const vector<Item>& items, int n, int i, int W, int w, int v, int mask, int& bestV, int& bestMask) {
    if (i == n) { if (w <= W && v > bestV) { bestV = v; bestMask = mask; } return; }
    bfRec(items, n, i + 1, W, w, v, mask, bestV, bestMask);                                       // exclude
    bfRec(items, n, i + 1, W, w + items[i].weight, v + items[i].utility, mask | (1 << i), bestV, bestMask);  // include
}
Result knapsackBrute(const vector<Item>& items, int n, int W) {
    int bestV = 0, bestMask = 0;
    bfRec(items, n, 0, W, 0, 0, 0, bestV, bestMask);
    Result r; r.utility = bestV;
    for (int i = 0; i < n; i++) if (bestMask >> i & 1) { r.chosen.push_back(i); r.weight += items[i].weight; }
    return r;
}

// ---------- 3. Greedy (by utility/weight ratio, no fractions) ----------
Result knapsackGreedy(const vector<Item>& items, const vector<int>& idx, int W) {
    vector<int> order = idx;
    sort(order.begin(), order.end(), [&](int a, int b) {
        return (double)items[a].utility / items[a].weight > (double)items[b].utility / items[b].weight; });
    Result r; int rem = W;
    for (int k : order)
        if (items[k].weight <= rem) { r.chosen.push_back(k); rem -= items[k].weight; r.utility += items[k].utility; r.weight += items[k].weight; }
    return r;
}

void printPlan(const vector<Item>& items, const Result& r, int W) {
    cout << "Truck Capacity      : " << W << " kg\n";
    cout << "Used Capacity       : " << r.weight << " kg\n";
    cout << "Remaining Capacity  : " << W - r.weight << " kg\n";
    cout << "Total Utility       : " << r.utility << "\n";
    cout << "Selected Items:\n----------------------------------------------\n";
    for (int k : r.chosen) {
        const Item& it = items[k];
        cout << "ID: " << setw(2) << it.id << " | " << left << setw(30) << it.name << right
             << " | Weight: " << setw(2) << it.weight << " kg | Utility: " << setw(3) << it.utility
             << (it.perishable ? " | PRIORITY" : "") << "\n";
    }
}

int main() {
    ifstream fi("items.csv"), ft("trucks.csv");
    if (!fi || !ft) { cout << "Error: items.csv / trucks.csv not found\n"; return 1; }
    vector<Item> items; string line;
    getline(fi, line);
    while (getline(fi, line)) {
        if (line.empty()) continue;
        auto f = splitCSV(line);
        Item it{stoi(f[0]), f[1], f[2], stoi(f[3]), stoi(f[4]), f[5] == "yes", 0};
        it.utility = it.perishable ? (int)lround(it.baseUtility * PRIORITY_FACTOR) : it.baseUtility;
        items.push_back(it);
    }
    vector<pair<string,int>> trucks;
    getline(ft, line);
    while (getline(ft, line)) { if (line.empty()) continue; auto f = splitCSV(line); trucks.push_back({f[1], stoi(f[2])}); }

    int perish = 0; for (auto& it : items) perish += it.perishable;
    cout << "Total items read    : " << items.size() << " (perishable/priority: " << perish << ")\n";
    cout << "Total trucks read   : " << trucks.size() << "\n";

    int W;
    cout << "\nEnter truck capacity W (kg): ";
    cin >> W;
    vector<int> all(items.size()); for (size_t i = 0; i < items.size(); i++) all[i] = i;

    cout << "\n==============================================\n";
    cout << "     DISASTER RELIEF RESOURCE ALLOCATION\n";
    cout << "==============================================\n";
    cout << "---------- 0/1 Knapsack (Dynamic Programming) ----------\n";
    Result best = knapsackDP(items, all, W);
    printPlan(items, best, W);

    // ---------- APPROACH COMPARISON ----------
    cout << "\n---------- Approach comparison (first 20 items, W = " << W << ") ----------\n";
    vector<int> first20(all.begin(), all.begin() + 20);
    auto t0 = Clock::now(); Result rb = knapsackBrute(items, 20, W); double tb = msSince(t0);
    t0 = Clock::now();      Result rd = knapsackDP(items, first20, W); double td = msSince(t0);
    t0 = Clock::now();      Result rg = knapsackGreedy(items, first20, W); double tg = msSince(t0);
    cout << left << setw(22) << "Approach" << setw(10) << "Utility" << setw(12) << "Weight(kg)" << "Time(ms)\n" << right;
    cout << fixed << setprecision(3);
    auto row = [&](const string& n, const Result& r, double t) {
        cout << left << setw(22) << n << setw(10) << r.utility << setw(12) << r.weight << t << "\n" << right; };
    row("Brute Force", rb, tb); row("Dynamic Programming", rd, td); row("Greedy (ratio)", rg, tg);
    cout << "Greedy optimality gap: " << rd.utility - rg.utility << " utility\n";

    cout << "\n---------- Approach comparison (all " << items.size() << " items, W = " << W << ") ----------\n";
    Result gAll = knapsackGreedy(items, all, W);
    cout << "Dynamic Programming : utility " << best.utility << "\n";
    cout << "Greedy (ratio)      : utility " << gAll.utility << "  (gap " << best.utility - gAll.utility << ")\n";

    // ---------- MULTIPLE TRUCKS ----------
    cout << "\n---------- Multi-truck allocation (DP, truck by truck) ----------\n";
    vector<int> remaining = all; int grand = 0;
    for (auto& tr : trucks) {
        Result r = knapsackDP(items, remaining, tr.second);
        cout << tr.first << " (" << tr.second << " kg): utility " << r.utility << ", load " << r.weight << " kg, items:";
        for (int k : r.chosen) cout << " " << items[k].id;
        cout << "\n";
        grand += r.utility;
        vector<int> next;
        for (int k : remaining) if (find(r.chosen.begin(), r.chosen.end(), k) == r.chosen.end()) next.push_back(k);
        remaining = next;
    }
    cout << "Total utility across trucks: " << grand << "  |  Items left at base: " << remaining.size() << "\n";

    // ---------- REAL-TIME DECISION ----------
    int uw, uu; string up;
    cout << "\nEnter urgent item (weight utility perishable[yes/no]): ";
    cin >> uw >> uu >> up;
    Item urgent{(int)items.size() + 1, "URGENT Item", "Urgent", uw, uu, up == "yes", 0};
    urgent.utility = urgent.perishable ? (int)lround(uu * PRIORITY_FACTOR) : uu;
    items.push_back(urgent); all.push_back(items.size() - 1);
    cout << "\n>> ALERT: urgent item arrived (" << uw << " kg, utility " << urgent.utility << "). Re-running DP...\n\n";
    Result nb = knapsackDP(items, all, W);
    printPlan(items, nb, W);
    cout << "Utility change      : " << nb.utility - best.utility << "\n";
    cout << "Dropped items       :";
    bool any = false;
    for (int k : best.chosen) if (find(nb.chosen.begin(), nb.chosen.end(), k) == nb.chosen.end()) { cout << " " << items[k].id; any = true; }
    cout << (any ? "\n" : " none\n");
    items.pop_back();

    // ---------- PERFORMANCE ANALYSIS ----------
    cout << "\n---------- Performance analysis: Brute Force vs DP ----------\n";
    cout << left << setw(6) << "n" << setw(16) << "BruteForce(ms)" << setw(12) << "DP(ms)" << "Same utility\n" << right;
    for (int n : {10, 14, 18, 20, 22, 24}) {
        vector<Item> pool = items;
        vector<int> idx(n); for (int i = 0; i < n; i++) idx[i] = i;
        auto s = Clock::now(); Result a = knapsackBrute(pool, n, W); double ta = msSince(s);
        s = Clock::now();      Result b = knapsackDP(pool, idx, W);  double tb2 = msSince(s);
        cout << left << setw(6) << n << setw(16) << ta << setw(12) << tb2 << (a.utility == b.utility ? "yes" : "NO") << "\n" << right;
    }
    cout << "==============================================\n";
    return 0;
}
