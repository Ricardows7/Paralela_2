#include <algorithm>
#include <iostream>
#include <set>
#include <string>
#include <utility>
#include <vector>
#include <map>
#include <iterator>   // std::next
#include <cstddef>    // std::size_t
#include <queue>

#define standard_input  std::cin
#define standard_output std::cout

#define SAMESTRINGVALUE -1

using Boolean = bool ;
using Size    = std::size_t ;
using String  = std::string ;
using Vector = std::vector<int> ;

using InStream  = std::istream ;
using OutStream = std::ostream ;

template <typename T, typename U>
using Pair = std::pair <T, U> ;

template <typename T, typename C = std::less <T>>
using Set = std::set <T> ;

template <typename T>
using SizeType = typename T :: size_type ;

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
    return x.empty () ;
}

// forward-declare Entry (já definido mais abaixo no arquivo)
struct Entry;

// ---- protótipos alinhados com as definições atuais ----
Boolean is_prefix (const String& a, const String& b);
inline auto suffix_from_position (const String& x, SizeType <String> i) -> String;
inline auto remove_prefix (const String& x, SizeType <String> n) -> String;
auto all_suffixes (const String& x) -> Set <String>;
auto common_suffix_and_prefix (const String& a, const String& b) -> String;
inline auto overlap_value (const String& s, const String& t) -> SizeType <String>;
auto overlap (const String& s, const String& t) -> String;

int pop_two_elements_and_push_overlap (Entry e, Pair<String, String> p);
auto pair_of_strings_with_highest_overlap_value(Entry best) -> Pair<String, String>;
auto shortest_superstring () -> String;

inline auto write_string_and_break_line (OutStream& out, const String& s) -> void;
inline auto read_size (InStream& in) -> Size;
inline auto read_string (InStream& in) -> String;
void read_strings_from_standard_input ();
inline auto write_string_to_standard_ouput (const String& s) -> void;

void create_heap();
void update_heap(int index);
void reload_heap(int index);

bool get_valid_top(Entry &out);

// ----------------- estado global -----------------

struct Entry {
    int i, j;
    int overlap; // was overlap_value
     bool operator<(const Entry& o) const {
        if (overlap != o.overlap) return overlap < o.overlap; // maior overlap first
        if (i != o.i) return i > o.i;   // smaller i wins -> make larger (=false) lose
        return j > o.j;                 // smaller j wins
    }
};


std::priority_queue<Entry> Holder;

int dimension = 0;
std::vector<bool> alive;
std::vector<std::string> strings;
std::vector<int> best_pair;

// ----------------- implementações -----------------

Boolean is_prefix (const String& a, const String& b)
{
    if (get_size(a) > get_size(b)) return false;
    return b.compare(0, a.size(), a) == 0;
}

inline auto suffix_from_position (const String& x, SizeType <String> i) -> String
{
    return x.substr (i) ;
}

inline auto remove_prefix (const String& x, SizeType <String> n) -> String
{
    if (get_size (x) > n)
        return suffix_from_position (x, n) ;
    return std::string{}; // retorna string vazia quando não há sufixo
}

auto all_suffixes (const String& x) -> Set <String>
{
    Set <String> ss ;
    SizeType <String> n = get_size (x) ;
    // gera sufixos não-vazios (evita o próprio string completo)
    while (n-- > 0) {
        ss.insert (x.substr (n)) ;
    }
    return ss ;
}

auto common_suffix_and_prefix (const String& a, const String& b) -> String
{
    if (is_empty (a)) return "" ;
    if (is_empty (b)) return "" ;
    String best = "" ;
    for (const String& s : all_suffixes (a)) {
        if (is_prefix (s, b) && get_size (s) > get_size (best)) {
            best = s ;
        }
    }
    return best ;
}

inline auto overlap_value (const String& s, const String& t) -> SizeType <String>
{
    return get_size (common_suffix_and_prefix (s, t)) ;
}

auto overlap (const String& s, const String& t) -> String
{
    String c = common_suffix_and_prefix (s, t) ;
    return s + remove_prefix (t, get_size (c)) ;
}

int pop_two_elements_and_push_overlap (Entry e, Pair<String, String> p)
{
    String place_holder = overlap (p.first, p.second) ;

    alive[e.i] = false;
    alive[e.j] = false;

    int size = strings.size();
    strings.push_back(place_holder);
    alive.push_back(true);
    
    return size;
}

auto pair_of_strings_with_highest_overlap_value(Entry best) -> Pair<String, String>
{
    return { strings[best.i], strings[best.j] };
}

auto shortest_superstring () -> String
{
    int alive_count = 0;
    for (bool a : alive) if (a) alive_count++;

    if (is_empty (Holder)) return "" ;
    // garantir que existe pelo menos 1 elemento

    while (alive_count > 1) {
        Entry best;
        if (!get_valid_top(best)) break;
        Holder.pop();
        Pair<String,String> best_pair = pair_of_strings_with_highest_overlap_value(best);
        int new_index = pop_two_elements_and_push_overlap (best, best_pair) ;
        if (new_index < 0) continue;
        alive_count -= 1;   //1 string criada, 2 consumidas
        //reload_heap(new_index);
        update_heap(new_index);
    }

    if (!strings.empty() && alive.back()) {
        return strings.back();
    }

    // fallback: varre de trás pra frente (mais rápido que do início)
    for (int i = (int)strings.size() - 1; i >= 0; --i) {
        if (alive[i]) return strings[i];
    }
    return std::string{};
}

bool get_valid_top(Entry &out){
    while (!Holder.empty()){
        Entry e = Holder.top();
        
        if (!alive[e.i] || !alive[e.j]) { Holder.pop(); continue; }
        out = e;
        return true;
    }
    return false;
}

inline auto
write_string_and_break_line (OutStream& out, const String& s) -> void
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

void read_strings_from_standard_input ()
{
    dimension = static_cast<int>(read_size (standard_input)) ;
    if (dimension <= 0) return;

    for (int i = 0; i < dimension; ++i) {
        String s = read_string(standard_input);
        strings.push_back(s);
        alive.push_back(true);
    }
}

void create_heap() {
    int N = strings.size();
    for (int i = 0; i < N; i++)
        update_heap(i);
}

void update_heap(int index) {
    if (!alive[index]) return;

    int best_j = -1;
    String &base = strings[index];
    int best_overlap = -1;

    int N = strings.size();
    for (int j = 0; j < N; j++){
        if (j == index) continue;
        if (!alive[j]) continue;

        int local_overlap = overlap_value(base, strings[j]);
        if (local_overlap > best_overlap){
            best_overlap = local_overlap;
            best_j = j;
        }
    }
    if (best_j >= 0){
        Holder.push({ index, best_j, best_overlap });
        //best_pair.push_back(best_j);
    }
}

void reload_heap(int index) {
    update_heap(index);
    int N = best_pair.size();
    for (int i = 0; i < N; i++){
        if (!alive[best_pair[i]])
            update_heap(i);
    }
}

inline auto
write_string_to_standard_ouput (const String& s) -> void
{
    write_string_and_break_line (standard_output, s) ;
}

auto
main (int argc, char const* argv[]) -> int
{
    read_strings_from_standard_input() ;
    if (dimension > 0) create_heap();
    write_string_to_standard_ouput (shortest_superstring ()) ;
    return 0 ;
}