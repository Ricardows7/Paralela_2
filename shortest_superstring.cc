#include <mpi.h>
#include <omp.h>
#include <algorithm>
#include <iostream>
#include <vector>
#include <string>
#include <utility>
#include <list>
#include <set>
#include <iterator>
#include <cstddef>
#include <climits>

using Boolean = bool;
using Size = std::size_t;
using String = std::string;

#define standard_input  std::cin
#define standard_output std::cout

template <typename T, typename U>
using Pair = std::pair<T, U>;

template <typename T, typename C = std::less<T>>
using Set = std::set<T>;

template <typename T>
using SizeType = typename T::size_type;

using InStream = std::istream;
using OutStream = std::ostream;

//  -------------------   funcoes utils   ----------------------------

template <typename C> inline auto
get_size(const C& x) -> SizeType<C>
{
    return x.size();
}

template <typename C> inline auto
at_least_two_elements_in(const C& c) -> Boolean
{
    return get_size(c) > SizeType<C>(1);
}

template <typename T> inline auto
first_element(const Set<T>& x) -> T
{
    return *(x.begin());
}

template <typename T> inline auto
second_element(const Set<T>& x) -> T
{
    return *(std::next(x.begin()));
}

template <typename T> inline auto
remove(Set<T>& x, const T& e) -> Set<T>&
{
    x.erase(e);
    return x;
}

template <typename T> inline auto
push(Set<T>& x, const T& e) -> Set<T>&
{
    x.insert(e);
    return x;
}

template <typename C> inline auto
is_empty(const C& x) -> Boolean
{
    return x.empty();
}

//  -------------------   funcoes calculo overlap simples   ----------------------------

inline auto
overlap_value(const String& a, const String& b) -> Size //calcula o valor de overlap a + b
{
    if (a.empty() || b.empty()) return 0;

    Size maior_overlap = std::min(a.size(), b.size());
    
    for (Size k = maior_overlap; k > 0; --k) 
        if (a.compare(a.size() - k, k, b, 0, k) == 0) 
            return k;

    return 0;
}

auto
overlap(const String& s, const String& t) -> String
{
    Size k = overlap_value(s, t);
    return s + t.substr(k); 
}

//  -------------------   funcoes I/O   ----------------------------

inline auto
write_string_and_break_line(OutStream& out, String s) -> void
{
    out << s << std::endl;
}

inline auto
read_size(InStream& in) -> Size
{
    Size n;
    in >> n;
    return n;
}

inline auto
read_string(InStream& in) -> String
{
    String s;
    in >> s;
    return s;
}

inline auto
write_string_to_standard_ouput(const String& s) -> void
{
    write_string_and_break_line(standard_output, s);
}

// ----------------- funcoes de slots -----------------

class SlotManager {
private:
    std::vector<int> occupied_slots;    //vetor para iteracao
    std::vector<bool> slot_state;       //vetor para verificacao rapido
    int total_slots;
    
public:
    SlotManager(int n) : total_slots(n) {
        slot_state.resize(n, true);
        occupied_slots.reserve(n);
        for (int i = 0; i < n; ++i) {
            occupied_slots.push_back(i);
        }
    }
    
    int remove_and_get_reusable(int i, int j) { //logica da delecao
        slot_state[i] = false;
        slot_state[j] = false;
        
        auto it = std::find(occupied_slots.begin(), occupied_slots.end(), i);
        if (it != occupied_slots.end()) {
            *it = occupied_slots.back();
            occupied_slots.pop_back();
        }
        
        it = std::find(occupied_slots.begin(), occupied_slots.end(), j);
        if (it != occupied_slots.end()) {
            *it = occupied_slots.back();
            occupied_slots.pop_back();
        }
        
        return std::min(i, j);
    }
    
    void add_reused_slot(int slot) {    //reaproveitar um slot do perdido pro fundido
        slot_state[slot] = true;
        occupied_slots.push_back(slot);
    }
    
    const std::vector<int>& get_occupied_slots() const {
        return occupied_slots;
    }
    
    bool is_alive(int index) const {
        return slot_state[index];
    }
    
    int count_occupied() const {
        return occupied_slots.size();
    }
    
    int total_size() const { return total_slots; }
};

// ----------------- funcoes de matriz -----------------

class FixedOverlapMatrix {
private:
    std::vector<int> data;
    int n;
    const std::vector<std::string>& strings;
    
public:
    FixedOverlapMatrix(int size, const std::vector<std::string>& str_ref) 
        : n(size), strings(str_ref) {
        data.resize(n * n, -1);
        
        #pragma omp parallel for collapse(2) schedule(dynamic, 8)
        for (int i = 0; i < n; ++i) {
            for (int j = 0; j < n; ++j) {
                if (i != j) {
                    data[i * n + j] = overlap_value(strings[i], strings[j]);
                } else {
                    data[i * n + j] = 0;
                }
            }
        }
    }
    
    int& operator()(int i, int j) { //acesso ao vetor que representa matriz (quadratico)
        return data[i * n + j];
    }
    
    const int& operator()(int i, int j) const {
        return data[i * n + j];
    }
    
    int get_overlap(int i, int j) const {
        if (i == j) return 0;
        return data[i * n + j];
    }
    
    //atualiza so o que precisa
    void update_slot(int reused_slot, const std::string& new_string, 
                                    const std::vector<int>& occupied_slots) {

        #pragma omp parallel for schedule(static)
        for (size_t idx = 0; idx < occupied_slots.size(); ++idx) {
            int other_index = occupied_slots[idx];

            if (other_index != reused_slot) {   //atualiza de maneira espelhada na matriz
                data[reused_slot * n + other_index] = overlap_value(new_string, strings[other_index]);
                data[other_index * n + reused_slot] = overlap_value(strings[other_index], new_string);
            }
        }
        
        data[reused_slot * n + reused_slot] = 0;
    }
};

// ----------------- funcoes de melhor local -----------------

struct LocalBest {
    int overlap;
    int i;
    int j;
    int owner;
};

static LocalBest make_localbest(int ov, int i, int j, int owner) {
    return LocalBest{ov, i, j, owner};
}

static bool better_localbest(const LocalBest &a, const LocalBest &b) {
    if (a.overlap != b.overlap) return a.overlap > b.overlap;
    if (a.i != b.i) return a.i < b.i;
    if (a.j != b.j) return a.j < b.j;
    return a.owner < b.owner;
}

// ----------------- funcoes de MPI -----------------

//comunicacao de strings e tamanhos
static void mpi_broadcast_strings(int rank, std::vector<std::string> &strings) {
    int n;
    if (rank == 0) {
        n = (int)strings.size();
    }
    MPI_Bcast(&n, 1, MPI_INT, 0, MPI_COMM_WORLD);
    
    if (rank != 0) {
        strings.resize(n);
    }
    
    for (int i = 0; i < n; ++i) {
        int len;
        if (rank == 0) {
            len = (int)strings[i].size();
        }
        MPI_Bcast(&len, 1, MPI_INT, 0, MPI_COMM_WORLD);
        
        if (rank != 0) {
            strings[i].resize(len);
        }
        MPI_Bcast((void *)strings[i].data(), len, MPI_CHAR, 0, MPI_COMM_WORLD);
    }
}

// ----------------- funcao de achar melhor local -----------------

static LocalBest compute_local_best_rows(
    const std::vector<std::string>& strings,
    const SlotManager& slot_manager,
    const FixedOverlapMatrix& overlap_matrix,
    int rank, int world_size)
{
    const std::vector<int>& occupied_slots = slot_manager.get_occupied_slots();

    int n_occupied = (int)occupied_slots.size();
    if (n_occupied < 2) return make_localbest(-1, -1, -1, rank);

    // divide os blocos por linhas
    int base = n_occupied / world_size;
    int rem  = n_occupied % world_size;
    int startRow = rank * base + std::min(rank, rem);
    int rows = base + ((rank < rem) ? 1 : 0);
    int endRow = startRow + rows;

    LocalBest best = make_localbest(-1, INT_MAX, INT_MAX, rank);

    #pragma omp parallel
    {
        LocalBest local_best = make_localbest(-1, INT_MAX, INT_MAX, rank);

        //pra cada linha, verifica todas as colunas pertencentes (tudo na msm linhad de cache :)
        #pragma omp for schedule(dynamic)
        for (int local_i = startRow; local_i < endRow; ++local_i) {
            int i = occupied_slots[local_i];
            
            for (int local_j = 0; local_j < n_occupied; ++local_j) {
                if (local_j == local_i) continue;

                int j = occupied_slots[local_j];
                int ov = overlap_matrix.get_overlap(i, j);
                
                //teste com varios fallbacks de consistencia pra atualizar a thread local
                if (ov > local_best.overlap || 
                   (ov == local_best.overlap && 
                   (i < local_best.i || (i == local_best.i && j < local_best.j)))) {
                    local_best = make_localbest(ov, i, j, rank);
                }
            }
        }

        #pragma omp critical    //ao final, finca o melhor
        {
            if (better_localbest(local_best, best)) {
                best = local_best;
            }
        }
    }

    return best;
}

// ----------------- funcao de fusao -----------------

static int perform_fusion_with_slot_reuse(const LocalBest &chosen, int rank,
                                         std::vector<std::string>& strings,
                                         SlotManager& slot_manager,
                                         FixedOverlapMatrix& overlap_matrix) 
{
    if (chosen.overlap < 0) return -1;
    
    String fused;

    if (rank == chosen.owner) fused = overlap(strings[chosen.i], strings[chosen.j]);
    
    int len = (int)fused.size();    //avisa geral do tamanho da fusao
    MPI_Bcast(&len, 1, MPI_INT, chosen.owner, MPI_COMM_WORLD);
    
    if (rank != chosen.owner) fused.resize(len);
    
    MPI_Bcast((void *)fused.data(), len, MPI_CHAR, chosen.owner, MPI_COMM_WORLD);
    
    //desabilita as strings escolhidas, usando o slot de uma pro resultado da fusao
    int reused_slot = slot_manager.remove_and_get_reusable(chosen.i, chosen.j);
    strings[reused_slot] = fused;
    slot_manager.add_reused_slot(reused_slot);
    
    //atualiza a matriz com base no slot atualizado (evita mexer no que nao precisa)
    const std::vector<int>& occupied_slots = slot_manager.get_occupied_slots();
    overlap_matrix.update_slot(reused_slot, fused, occupied_slots);
    
    return reused_slot;
}

static LocalBest gather_and_select_best(const LocalBest &local, int rank, int world_size) {
    int sendbuf[4] = {local.overlap, local.i, local.j, local.owner};
    std::vector<int> recvbuf;
    
    if (rank == 0) recvbuf.resize(4 * world_size);  //recebe as opcoes
    MPI_Gather(sendbuf, 4, MPI_INT, (rank == 0) ? recvbuf.data() : nullptr, 4, MPI_INT, 0, MPI_COMM_WORLD);
    
    LocalBest chosen = make_localbest(-1, -1, -1, -1);
    if (rank == 0) {
        for (int r = 0; r < world_size; ++r) {
            LocalBest cand = make_localbest(
                recvbuf[r * 4 + 0], recvbuf[r * 4 + 1], 
                recvbuf[r * 4 + 2], recvbuf[r * 4 + 3]
            );
            if (better_localbest(cand, chosen)) chosen = cand;
        }
    }
    
    int chosenbuf[4];
    if (rank == 0) {
        chosenbuf[0] = chosen.overlap; chosenbuf[1] = chosen.i;
        chosenbuf[2] = chosen.j; chosenbuf[3] = chosen.owner;
    }
    MPI_Bcast(chosenbuf, 4, MPI_INT, 0, MPI_COMM_WORLD);    //comunica a todos a escolha final do round
    
    if (rank != 0) {
        chosen.overlap = chosenbuf[0]; chosen.i = chosenbuf[1];
        chosen.j = chosenbuf[2]; chosen.owner = chosenbuf[3];
    }
    return chosen;
}

// ----------------- funcao main -----------------

int main(int argc, char *argv[]) {
    double start_total, end_total, start_parallel = 0.0, end_parallel = 0.0;
    double parallel_time = 0.0;
    
    start_total = MPI_Wtime();  //ajustes de tempo
    
    int rank = 0;
    int world_size = 1;
    
    #ifndef SEQUENTIAL_PURE //setups paralelos
    MPI_Init(&argc, &argv);
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);
    MPI_Comm_size(MPI_COMM_WORLD, &world_size);
    #endif
    
    std::vector<std::string> strings;   //inputs
    if (rank == 0) {
        Size n = read_size(standard_input); 
        strings.resize(n);
        for (Size i = 0; i < n; ++i) {
            strings[i] = read_string(standard_input);
        }
    }
    
    #ifndef SEQUENTIAL_PURE
    mpi_broadcast_strings(rank, strings);
    #endif
    
    SlotManager slot_manager(strings.size());   //setupds das estruturas de dados
    FixedOverlapMatrix overlap_matrix(strings.size(), strings); 
    
    #ifndef SEQUENTIAL_PURE
    MPI_Barrier(MPI_COMM_WORLD);    //temporal
    start_parallel = MPI_Wtime();
    #else
    start_parallel = start_total;
    #endif
    
    while (true) {
        LocalBest local = compute_local_best_rows(
            strings, slot_manager, overlap_matrix, rank, world_size);
        
        LocalBest chosen;
        
        #ifndef SEQUENTIAL_PURE
        chosen = gather_and_select_best(local, rank, world_size); 
        #else
        chosen = local;
        #endif
        
        // Condição de parada
        if (chosen.overlap < 0 || slot_manager.count_occupied() <= 1) break;
        
        #ifndef SEQUENTIAL_PURE
        perform_fusion_with_slot_reuse(chosen, rank, strings, slot_manager, overlap_matrix);
        #else
        if (chosen.overlap >= 0) {  //vesao sequencial
            String fused = overlap(strings[chosen.i], strings[chosen.j]);
            int reused_slot = slot_manager.remove_and_get_reusable(chosen.i, chosen.j);
            strings[reused_slot] = fused;
            slot_manager.add_reused_slot(reused_slot);
            const std::vector<int>& occupied_slots = slot_manager.get_occupied_slots();
            overlap_matrix.update_slot(reused_slot, fused, occupied_slots);
        }
        #endif

        //condicao de parada
        if (slot_manager.count_occupied() <= 1) break;

    }
    
    #ifndef SEQUENTIAL_PURE
    end_parallel = MPI_Wtime();
    double local_parallel_time = end_parallel - start_parallel;
    MPI_Reduce(&local_parallel_time, &parallel_time, 1, MPI_DOUBLE, MPI_MAX, 0, MPI_COMM_WORLD);
    #else
    end_parallel = MPI_Wtime();
    parallel_time = end_parallel - start_parallel;
    #endif
    
    if (rank == 0 && !strings.empty()) {
        const std::vector<int>& final_occupied = slot_manager.get_occupied_slots();
        if (!final_occupied.empty()) {  //print do resultado
            write_string_to_standard_ouput(strings[final_occupied[0]]);
        }
        
        end_total = MPI_Wtime();
        double total_time = end_total - start_total;

        std::cout.precision(6);
        std::cout << "Tempo total: " << total_time * 1000.0 << " ms" << std::endl;
        std::cout << "Tempo paralelo: " << parallel_time * 1000.0 << " ms" << std::endl;
    }
    
    #ifndef SEQUENTIAL_PURE
    MPI_Finalize();
    #endif
    
    return 0;
}