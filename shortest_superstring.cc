#include <algorithm>
#include <iostream>
#include <set>
#include <string>
#include <utility>

#include <mpi.h>

#include <omp.h>
#include <vector>
#include <cstddef>

#define standard_input  std::cin
#define standard_output std::cout

using Boolean = bool ;
using Size    = std::size_t ;
using String  = std::string ;

using InStream  = std::istream ;
using OutStream = std::ostream ;

using Matrix = std::vector<std::vector<int>> ; 
using StringVector = std::vector<std::string> ;

template <typename T, typename U>
using Pair = std::pair <T, U> ;

template <typename T, typename C = std::less <T>>
using Set = std::set <T> ;

template <typename T>
using SizeType = typename T :: size_type ;

//  -------------------   funcoes base   ----------------------------
struct LocalBest {
    int overlap; //-1 significa nada
    int row;
    int col;
    int owner; //quem mandou o localbest
}

static LocalBest make_localbest(int ov, int rol, int col, int own) {
    return LocalBest{ov, rol, col, own};
}

//criterios apos o 1 nessa funcao servem para desempate e consistencia de resposta :)
static bool better_localbest(const LocalBest &a, const LocalBest &b) {
    if (a.overlap != b.overlap) return a.overlap > b.overlap;   //1 criterio: quem tem maior overlap
    if (a.row != b.row) return a.i < b.i;   //2 criterio: quem tem menor numero de linha
    if (a.col != b.col) return a.col < b.col;   //3 criterio: quem tem menor numero de coluna
    return a.own < b.own;   //ultimo criterio: indice do dono
}

//  -------------------   funcoes utils   ----------------------------

template <typename C> inline auto
get_size (const C& x) -> SizeType <C>
{
    return x.size () ;
}

template <typename C> inline auto
at_least_two_elements_in (const C& c) -> Boolean
{
    return get_size (c) > SizeType <C> (1) ;
}

template <typename T> inline auto
first_element (const Set <T>& x) -> T
{
    return *(x.begin ()) ;
}

template <typename T> inline auto
second_element (const Set <T>& x) -> T
{
    return *(std::next (x.begin ())) ;
}

template <typename T> inline auto
remove (Set <T>& x, const T& e) -> Set <T>&
{
    x.erase (e) ;
    return x ;
}

template <typename T> inline auto
push (Set <T>& x, const T& e) -> Set <T>&
{
    x.insert (e) ;
    return x ;
}

template <typename C> inline auto
is_empty (const C& x) -> Boolean
{
    return x.is_empty () ;
}

//  -------------------   funcoes calculo overlap simples   ----------------------------

Boolean is_prefix (const String& a, const String& b)
{
    if (get_size (a) > get_size (b))
        return false ;
    if ( !
            ( std::mismatch
                ( a.begin ()
                , a.end   ()
                , b.begin () )
                    .first == a.end () ) )
        return false ;
    return true ;
}

inline auto
suffix_from_position (const String& x, SizeType <String> i) -> String
{
    return x.substr (i) ;
}

inline auto
remove_prefix (const String& x, SizeType <String> n) -> String
{
    if (get_size (x) > n)
        return suffix_from_position (x, n) ;
    return x ;
}

auto
all_suffixes (const String& x) -> Set <String>
{
    Set <String> ss ;
    SizeType <String> n = size (x) ;
    while (-- n) {
        ss.insert (x.substr (n)) ;
    }
    return ss ;
}

auto
commom_suffix_and_prefix (const String& a, const String& b) -> String
{
    if (is_empty (a)) return "" ;
    if (is_empty (b)) return "" ;
    String x = "" ;
    for (const String& s : all_suffixes (a)) {
        if (is_prefix (s, b) && get_size (s) > get_size (x)) {
            x = s ;
        }
    }
    return x ;
}

inline auto
overlap_value (const String& s, const String& t) -> SizeType <String>
{
    return get_size (commom_suffix_and_prefix (s, t)) ;
}

auto
overlap (const String& s, const String& t) -> String
{
    String c = commom_suffix_and_prefix (s, t) ;
    return s + remove_prefix (t, get_size (c)) ;
}

//  -------------------   funcoes I/O   ----------------------------

inline auto
write_string_and_break_line (OutStream& out, String s) -> void
{
    out << s << std::endl ;
}

inline auto
read_size (InStream& in) -> Size
{
    Size n ;
    in >>  n ;
    return n ;
}

inline auto
read_string (InStream& in) -> String
{
    String s ;
    in >>  s ;
    return s ;
}

auto
read_strings_from_standard_input () -> Set <String>
{
    using N = SizeType <Set <String>> ;
    Set <String> x ;
    N n = N (read_size (standard_input)) ;
    while (n --) x.insert (read_string (standard_input)) ;
    return x ;
}

inline auto
write_string_to_standard_ouput (const String& s) -> void
{
    write_string_and_break_line (standard_output, s) ;
}

//  -------------------   funcoes MPI utils  ----------------------------

static void mpi_broadcast_strings(int rank, StringVector &strings) {
    if (rank == 0) {    //host
        int n = strings.size();
        MPI_Bcast(&n, 1, MPI_INT, 0, MPI_COMM_WORLD);   //envia quantidade de strings pra todos
        for (int i = 0; i < n; i++){
            int length = strings[i].size();
            MPI_Bcast(&len, 1, MPI_INT, 0 MPI_COMM_WORLD);                              //pra cada string, envia o tamanho
            MPI_Bcast((void*)strings[i].data(), len, MPI_CHAR, 0, MPI_COMM_WORLD);      //seguido da string em si
        }
    } else {    //nao host
        int n;
        MPI_Bcast(&n, 1, MPI_INT, 0, MPI_COMM_WORLD);   //recebe quantidade de strings
        strings.resize(n);                              //ajusta vetor com base na info
        for (int i = 0; i < n; i++){
            int len;
            MPI_Bcast(&len, 1, MPI_INT, 0, MPI_COMM_WORLD);     //pra cada string, ajusta com base no tamanho
            strings[i].resize(len);                             //e depois recebe
            MPI_Bcast((void*)&strings[i].data(), len, MPI_CHAR, 0, MPI_COMM_WORLD);
        }
    }
}

//divisao de tarefas :(
static void compute_ownership_ranges(int n, int world_size, int rank, int &min, int &max) {
    int base = n / world_size;  //carga minima
    int remnant = n % world_size;   //azarados que vao pegar 1 a mais
    if (rank < remnant) {
        min = rank * (base + 1);
        max = min + (base + 1);
    } else {
        min = rank * base + remnant;
        max = min + base;
    }
}

//  -------------------   funcoes matriz   ----------------------------

static LocalBest find_localbest_pair(Matrix &matrix, std::vector<bool> &alive, int rank, int world_size) {
    int n = matrix.size();
    int min, max;
    
    compute_ownership_ranges(n, world_size, rank, min, max);

    LocalBest local_best = make_localbest(-1, -1, -1, rank);

    for (int i = min, i < max; i++){
        if (!alive[i])
            continue;
        
        for (int j = 0; j < n; j++){
            if (i == j || !alive[j])
                continue;

            int current_overlap = matrix[i][j];
            if (current_overlap > local_best.overlap)
                local_best = make_localbest(current_overlap, i, j, rank);
            else if (current_overlap == local_best.overlap && current_overlap != -1) {
                if (i < local_best.row || (i == local_best.row && j < local_best.col))
                    local_best = make_localbest(current_overlap, i, j, rank);
            }
        }
    }
    return local_best;
}

static LocalBest select_best(const LocalBest &local, int rank, int world_size) {
    int sendbuf[4] = { local.overlap, local.row, local.col, local.own };
    std::vector<int> recvbuf;

    if (rank == 0)
        recvbuf.resize(4 * world_size);

    MPI_Gather(sendbuf, 4, MPI_INT, recvbuf)
}

static LocalBest global_best_pair(LocalBest &local_best, int rank, int world_size) {
    return select_best(local_best, rank, world_size);
}

//  -------------------   funcoes matriz   ----------------------------

static void prepare_overlap_matrix(Matrix &matrix, StringVector & strings) {
    int n = strings.size();
    matrix.resize(n, std::vector<int>(n, 0));

    #pragma omp parallel for schedule (dynamic)
    for (int i = 0; i < n; i++){
        for (int j = 0; j < n; j++){
            if (i != j)
                matrix[i][j] = overlap_value(strings[i], strings[j]);
        }
    }
}

static void update_matrix (Matrix &matrix, StringVector &strings, std::vector<bool> &alive, 
                                                int new_index, int removed_i, int removed_j) {
    int n = strings.size();

    if (n > matrix.size()) {
        matrix.resize(n);
        for (auto &row : matrix) {
            row.resize(n, 0);
        }
    }

    #pragma omp parallel for
    for (int k = 0; k < n; k++){
        if (!alive[k] || k == new_index){
            matrix[new_index][k] = 0;
            matrix[k][new_index] = 0;
            continue;
        }

        int value = overlap_value(strings[new_index], strings[k]);
        matrix[new_index][k] = value;
        matrix[k][new_index] = value;
    }

    for (int k = 0; k < n; k++){
        matrix[removed_i][k] = 0;
        matrix[k][removed_i] = 0;
        matrix[removed_j][k] = 0;
        matrix[k][removed_j] = 0;
    }
}

//  -------------------   funcoes matriz main   ----------------------------


inline auto
pop_two_elements_and_push_overlap
        (Set <String>& ss, const Pair <String, String>& p) -> Set <String>&
{
    ss = remove (ss, p.first)  ;
    ss = remove (ss, p.second) ;
    ss = push   (ss, overlap (p.first, p.second)) ;
    return ss ;
}

auto
all_distinct_pairs (const Set <String>& ss) -> Set <Pair <String, String>>
{
    Set <Pair <String, String>> x ;
    for (const String& s1 : ss) {
        for (const String& s2 : ss) {
            if (s1 != s2) x.insert (make_pair (s1, s2)) ;
        }
    }
    return x ;
}

auto
highest_overlap_value
        (const Set <Pair <String, String>>& sp) -> Pair <String, String>
{
    Pair <String, String> x = first_element (sp) ;
    for (const Pair <String, String>& p : sp) {
        if ( overlap_value (p.first, p.second)
                > overlap_value (x.first, x.second) )
        {
            x = p ;
        }
    }
    return x ;
}

auto
pair_of_strings_with_highest_overlap_value
        (const Set <String>& ss) -> Pair <String, String>
{
    return highest_overlap_value (all_distinct_pairs (ss)) ;
}

auto
shortest_superstring (Set <String> t) -> String
{
    if (is_empty (t)) return "" ;
    while (at_least_two_elements_in (t)) {
        t = pop_two_elements_and_push_overlap
            ( t
            , pair_of_strings_with_highest_overlap_value (t) ) ;
    }
    return first_element (t) ;
}

auto
main (int argc, char const* argv[]) -> int
{
    Set <String> ss = read_strings_from_standard_input () ;
    write_string_to_standard_ouput (shortest_superstring (ss)) ;
    return 0 ;
}
