// shsup_mpi.cpp
#include <mpi.h>
#include <omp.h>            // optional; requires -fopenmp
#include <algorithm>
#include <iostream>
#include <set>
#include <string>
#include <utility>
#include <vector>
#include <queue>
#include <cstddef>

using Boolean = bool;
using Size = std::size_t;
using String = std::string;

template <typename T, typename U>
using Pair = std::pair<T, U>;

template <typename T, typename C = std::less<T>>
using Set = std::set<T>;

template <typename T>
using SizeType = typename T::size_type;

inline SizeType<String> get_size(const String &x) { return x.size(); }

// ---------------- string overlap (KMP helper) ----------------

// prefix-function (pi) for KMP
static std::vector<int> prefix_function(const std::string &s) {
    int n = (int)s.size();
    std::vector<int> pi(n);
    for (int i = 1; i < n; ++i) {
        int j = pi[i - 1];
        while (j > 0 && s[i] != s[j]) j = pi[j - 1];
        if (s[i] == s[j]) ++j;
        pi[i] = j;
    }
    return pi;
}

// returns length of longest suffix of `a` that is prefix of `b`
// implemented deterministically and efficiently via KMP
static int overlap_kmp(const std::string &a, const std::string &b) {
    if (a.empty() || b.empty()) return 0;
    int maxk = std::min((int)a.size(), (int)b.size());
    // pattern = b + '#' + suffix_of_a_of_length_maxk
    std::string pattern;
    pattern.reserve((int)b.size() + 1 + maxk);
    pattern.append(b);
    pattern.push_back('#');
    pattern.append(a.data() + ((int)a.size() - maxk), maxk);
    auto pi = prefix_function(pattern);
    return pi.back();
}

inline int overlap_value(const String &s, const String &t) {
    // disallow i == j at caller; here we compute non-negative overlap
    return overlap_kmp(s, t);
}

inline String overlap_string(const String &s, const String &t) {
    int k = overlap_value(s, t);
    if (k <= 0) return s + t;
    // append t from position k
    return s + t.substr(k);
}

// ---------------- deterministic selection structs ----------------

struct LocalBest {
    int overlap; // -1 means no candidate
    int i;
    int j;
    int owner; // rank that proposes i
};

static LocalBest make_localbest(int ov, int i, int j, int owner) {
    return LocalBest{ov, i, j, owner};
}

// deterministic compare: return true if a is better than b
static bool better_localbest(const LocalBest &a, const LocalBest &b) {
    if (a.overlap != b.overlap) return a.overlap > b.overlap;
    if (a.i != b.i) return a.i < b.i;
    if (a.j != b.j) return a.j < b.j;
    return a.owner < b.owner;
}

// ---------------- ownership helpers ----------------
static void compute_ownership_ranges(int n, int world_size, int rank, int &lo, int &hi) {
    int base = n / world_size;
    int rem = n % world_size;
    if (rank < rem) {
        lo = rank * (base + 1);
        hi = lo + (base + 1);
    } else {
        lo = rank * base + rem;
        hi = lo + base;
    }
}

// ---------------- I/O helpers (rank 0 reads, broadcasts to others) ----------------
static void mpi_broadcast_strings(int rank, std::vector<std::string> &strings) {
    if (rank == 0) {
        int n = (int)strings.size();
        MPI_Bcast(&n, 1, MPI_INT, 0, MPI_COMM_WORLD);
        for (int i = 0; i < n; ++i) {
            int len = (int)strings[i].size();
            MPI_Bcast(&len, 1, MPI_INT, 0, MPI_COMM_WORLD);
            MPI_Bcast((void *)strings[i].data(), len, MPI_CHAR, 0, MPI_COMM_WORLD);
        }
    } else {
        int n;
        MPI_Bcast(&n, 1, MPI_INT, 0, MPI_COMM_WORLD);
        strings.resize(n);
        for (int i = 0; i < n; ++i) {
            int len;
            MPI_Bcast(&len, 1, MPI_INT, 0, MPI_COMM_WORLD);
            strings[i].resize(len);
            MPI_Bcast((void *)strings[i].data(), len, MPI_CHAR, 0, MPI_COMM_WORLD);
        }
    }
}

// ---------------- compute local best (owned indices) ----------------
static LocalBest compute_local_best(const std::vector<std::string> &strings,
                                    const std::vector<char> &alive,
                                    int n, int rank, int world_size)
{
    int lo, hi;
    compute_ownership_ranges(n, world_size, rank, lo, hi);

    LocalBest best = make_localbest(-1, -1, -1, rank);

    // Parallelize over i owned by this rank (optional)
    #pragma omp parallel
    {
        LocalBest local_best = make_localbest(-1, -1, -1, rank);
        #pragma omp for schedule(dynamic)
        for (int i = lo; i < hi; ++i) {
            if (!alive[i]) continue;
            // find best j deterministically: j ascending
            int best_j = -1;
            int best_ov = -1;
            for (int j = 0; j < n; ++j) {
                if (j == i) continue;
                if (!alive[j]) continue;
                int ov = overlap_value(strings[i], strings[j]);
                if (ov > best_ov || (ov == best_ov && j < best_j)) {
                    best_ov = ov;
                    best_j = j;
                }
            }
            if (best_j >= 0) {
                LocalBest candidate = make_localbest(best_ov, i, best_j, rank);
                #pragma omp critical
                {
                    if (better_localbest(candidate, local_best)) local_best = candidate;
                }
            }
        } // end omp for
        #pragma omp critical
        {
            if (better_localbest(local_best, best)) best = local_best;
        }
    } // end omp parallel

    return best;
}

// ---------------- gather-best and root-select (deterministic) ----------------
static LocalBest gather_and_select_best(const LocalBest &local, int rank, int world_size) {
    int sendbuf[4] = {local.overlap, local.i, local.j, local.owner};
    std::vector<int> recvbuf;
    if (rank == 0) recvbuf.resize(4 * world_size);
    MPI_Gather(sendbuf, 4, MPI_INT, recvbuf.data(), 4, MPI_INT, 0, MPI_COMM_WORLD);

    LocalBest chosen = make_localbest(-1, -1, -1, -1);
    if (rank == 0) {
        for (int r = 0; r < world_size; ++r) {
            int ov = recvbuf[r * 4 + 0];
            int i = recvbuf[r * 4 + 1];
            int j = recvbuf[r * 4 + 2];
            int owner = recvbuf[r * 4 + 3];
            LocalBest cand = make_localbest(ov, i, j, owner);
            if (better_localbest(cand, chosen)) chosen = cand;
        }
    }
    int chosenbuf[4];
    if (rank == 0) {
        chosenbuf[0] = chosen.overlap;
        chosenbuf[1] = chosen.i;
        chosenbuf[2] = chosen.j;
        chosenbuf[3] = chosen.owner;
    }
    MPI_Bcast(chosenbuf, 4, MPI_INT, 0, MPI_COMM_WORLD);
    if (rank != 0) {
        chosen.overlap = chosenbuf[0];
        chosen.i = chosenbuf[1];
        chosen.j = chosenbuf[2];
        chosen.owner = chosenbuf[3];
    }
    return chosen;
}

// ---------------- owner performs fusion and broadcasts fused string ----------------
static int perform_and_broadcast_fusion(const LocalBest &chosen, int rank,
                                        std::vector<std::string> &strings,
                                        std::vector<char> &alive)
{
    int new_index = -1;
    if (chosen.overlap < 0) return -1;

    if (rank == chosen.owner) {
        std::string fused = overlap_string(strings[chosen.i], strings[chosen.j]);
        int len = (int)fused.size();
        MPI_Bcast(&len, 1, MPI_INT, chosen.owner, MPI_COMM_WORLD);
        MPI_Bcast((void *)fused.data(), len, MPI_CHAR, chosen.owner, MPI_COMM_WORLD);
        new_index = (int)strings.size();
        strings.push_back(fused);
        alive.push_back(1);
        alive[chosen.i] = 0;
        alive[chosen.j] = 0;
    } else {
        int len;
        MPI_Bcast(&len, 1, MPI_INT, chosen.owner, MPI_COMM_WORLD);
        std::string fused;
        fused.resize(len);
        MPI_Bcast((void *)fused.data(), len, MPI_CHAR, chosen.owner, MPI_COMM_WORLD);
        new_index = (int)strings.size();
        strings.push_back(fused);
        alive.push_back(1);
        alive[chosen.i] = 0;
        alive[chosen.j] = 0;
    }
    return new_index;
}

// ---------------- reload local bests (recompute best_pair for affected entries) ----------------
static void reload_heap_local(int new_index,
                              std::vector<std::string> &strings,
                              std::vector<char> &alive,
                              std::vector<int> &best_pair)
{
    int N = (int)strings.size();
    // ensure best_pair resized
    if ((int)best_pair.size() < N) best_pair.resize(N, -1);

    // compute best for new_index
    if (alive[new_index]) {
        int best_j = -1;
        int best_ov = -1;
        #pragma omp parallel
        {
            int loc_best_j = -1;
            int loc_best_ov = -1;
            #pragma omp for nowait
            for (int j = 0; j < N; ++j) {
                if (j == new_index) continue;
                if (!alive[j]) continue;
                int ov = overlap_value(strings[new_index], strings[j]);
                if (ov > loc_best_ov || (ov == loc_best_ov && (loc_best_j == -1 || j < loc_best_j))) {
                    loc_best_ov = ov;
                    loc_best_j = j;
                }
            }
            #pragma omp critical
            {
                if (loc_best_ov > best_ov || (loc_best_ov == best_ov && (best_j == -1 || loc_best_j < best_j))) {
                    best_ov = loc_best_ov;
                    best_j = loc_best_j;
                }
            }
        }
        best_pair[new_index] = best_j;
    }

    // for all i, if their best_pair is dead, recompute
    int M = (int)best_pair.size();
    #pragma omp parallel for schedule(dynamic)
    for (int i = 0; i < M; ++i) {
        if (!alive[i]) continue;
        int curr = best_pair[i];
        if (curr >= 0 && !alive[curr]) {
            int best_j = -1;
            int best_ov = -1;
            for (int j = 0; j < N; ++j) {
                if (j == i) continue;
                if (!alive[j]) continue;
                int ov = overlap_value(strings[i], strings[j]);
                if (ov > best_ov || (ov == best_ov && (best_j == -1 || j < best_j))) {
                    best_ov = ov;
                    best_j = j;
                }
            }
            best_pair[i] = best_j;
        }
    }
}

// ---------------- main loop + helpers ----------------
int main(int argc, char *argv[]) {
    MPI_Init(&argc, &argv);
    int rank = 0, world_size = 1;
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);
    MPI_Comm_size(MPI_COMM_WORLD, &world_size);

    // Read input on rank 0, broadcast to others
    std::vector<std::string> strings;
    if (rank == 0) {
        int n;
        if (!(std::cin >> n)) {
            MPI_Finalize();
            return 0;
        }
        strings.reserve(n);
        for (int i = 0; i < n; ++i) {
            std::string s;
            std::cin >> s;
            strings.push_back(s);
        }
    }
    mpi_broadcast_strings(rank, strings);

    int n = (int)strings.size();
    std::vector<char> alive(n, 1);
    std::vector<int> best_pair(n, -1);

    // initial local best_pair computation (parallelizable per rank)
    // We'll compute nothing global yet; each rank will compute for its owned i when needed
    // Main loop: compute local best per rank -> gather -> choose global -> fuse -> update -> repeat

    while (true) {
        // compute local best candidate
        LocalBest local = compute_local_best(strings, alive, n, rank, world_size);

        // gather and choose global best (deterministic at root)
        LocalBest chosen = gather_and_select_best(local, rank, world_size);

        // termination if no candidate (overlap < 0)
        if (chosen.overlap < 0) break;

        // owner performs fusion and broadcast fused string; all ranks update strings and alive
        int new_index = perform_and_broadcast_fusion(chosen, rank, strings, alive);
        if (new_index < 0) break;

        // reload local bests (parallel inside rank)
        reload_heap_local(new_index, strings, alive, best_pair);

        // update global alive count: each rank counts local alive, then sum
        int local_alive = 0;
        for (char a : alive) if (a) ++local_alive;
        int global_alive = 0;
        MPI_Allreduce(&local_alive, &global_alive, 1, MPI_INT, MPI_SUM, MPI_COMM_WORLD);

        if (global_alive <= 1) break;

        // update n because strings vector grew
        n = (int)strings.size();
    }

    // determine final string deterministically: prefer strings.back() if alive
    std::string final_answer;
    if (!strings.empty() && alive.back()) final_answer = strings.back();
    else {
        for (int i = (int)strings.size() - 1; i >= 0; --i) {
            if (alive[i]) { final_answer = strings[i]; break; }
        }
    }

    // print only from rank 0
    if (rank == 0) {
        if (!final_answer.empty()) std::cout << final_answer << '\n';
    }

    MPI_Finalize();
    return 0;
}
