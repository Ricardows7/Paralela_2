#include <algorithm>
#include <iostream>
#include <set>
#include <string>
#include <utility>
#include <vector>
#include <map>

#define standard_input  std::cin
#define standard_output std::cout

#define SAMESTRINGVALUE -1

using Boolean = bool ;
using Size    = std::size_t ;
using String  = std::string ;
using Map = std::map<int, String> ;
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
    SizeType <String> n = get_size (x) ;
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


int pop_two_elements_and_push_overlap
        (const Pair <String, String>& p)
{
    String place_holder = overlap (p.first, p.second) ;
    delete_by_string(p.second);
    return sub_string(p.first, place_holder);
}

// auto
// all_distinct_pairs (const Set <String>& ss) -> Set <Pair <String, String>>
// {
//     Set <Pair <String, String>> x ;
//     for (const String& s1 : ss) {
//         for (const String& s2 : ss) {
//             if (s1 != s2) x.insert (make_pair (s1, s2)) ;
//         }
//     }
//     return x ;
// }

// auto
// highest_overlap_value
//         (const Set <Pair <String, String>>& sp) -> Pair <String, String>
// {
//     Pair <String, String> x = first_element (sp) ;
//     for (const Pair <String, String>& p : sp) {
//         if ( overlap_value (p.first, p.second)
//                 > overlap_value (x.first, x.second) )
//         {
//             x = p ;
//         }
//     }
//     return x ;
// }

auto
pair_of_strings_with_highest_overlap_value() -> Pair <String, String>
{
    Pair <String, String> result;

    int perm_row = 0,  perm_column = 0, temp_row, temp_column;
    int max_overlap_value = -1, temp_overlap_value;
   
    for (const auto& row : relation_coordinate_string){
        temp_row = row.first;
        for (const auto& column : relation_coordinate_string){
            temp_column = column.first;

            temp_overlap_value = matrix[get_coordinates(row.first, column.first)];
            if (temp_overlap_value > max_overlap_value){
                perm_row = temp_row;
                perm_column = temp_column;
                max_overlap_value = temp_overlap_value;
            }
        }
    }

    result = { relation_coordinate_string.at(perm_row), relation_coordinate_string.at(perm_column) };
    return result ;
}

// auto
// shortest_superstring (std::map<int,String> t) -> String
// {
//     if (is_empty (t)) return "" ;
//     while (t.size > 1) {
//         t = pop_two_elements_and_push_overlap
//             ( t
//             , pair_of_strings_with_highest_overlap_value (t) ) ;
//     }
//     return first_element (t) ;
// }

auto
shortest_superstring () -> String
{
    if (is_empty (relation_strig_coordinate)) return "" ;
    while (relation_strig_coordinate.size() > 1) {
        int new_index = pop_two_elements_and_push_overlap (pair_of_strings_with_highest_overlap_value()) ;
        load_matrix_string(new_index);
    }
    return relation_coordinate_string.begin()->second;
}


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
read_strings_from_standard_input ()
{
    using N = SizeType <Set <String>> ;
    
    int index = 0;
    dimension = N (read_size (standard_input)) ;
    N n = dimension;

    relation_strig_coordinate.clear();
    
    while (n --){
        String s = read_string(standard_input);
        add_string(s, index);
        index++;
    } 
    return;
}

inline auto
write_string_to_standard_ouput (const String& s) -> void
{
    write_string_and_break_line (standard_output, s) ;
}

int dimension;
std::vector<int> matrix;
std::map<int, String> relation_coordinate_string;
std::map<String, int> relation_strig_coordinate;

auto
main (int argc, char const* argv[]) -> int
{
    read_strings_from_standard_input () ;
    load_matrix_full();
    write_string_to_standard_ouput (shortest_superstring ()) ;
    return 0 ;
}                                                                                          

auto
get_coordinates(int row, int column){
    return (row * dimension) + column;
}

auto 
get_row_and_column_by_index(int index) -> std::pair<int,int>{
    int row = index / dimension;
    int column = index % dimension;

    return { row, column };
}

void add_string (String s, int index) {
    relation_strig_coordinate[s] = index;
    relation_coordinate_string[index] = s;
}

int sub_string (String original, String newbie) {
    int coordinate = relation_strig_coordinate[original];
    relation_strig_coordinate.erase(original);
    relation_coordinate_string[coordinate] = newbie;
    relation_strig_coordinate[newbie] = coordinate;
    
    return coordinate;
}

void delete_by_string (String s) {
    int coordinate = relation_strig_coordinate[s];
    relation_strig_coordinate.erase(s);
    relation_coordinate_string.erase(coordinate);

    return;
}

void load_matrix_string (int index) {
    String fav = relation_coordinate_string.at(index);

    for (const auto& row : relation_coordinate_string){
        matrix[get_coordinates(row.first, index)] = overlap_value(
            relation_coordinate_string.at(row.first), fav);
    }

    for (const auto& col : relation_coordinate_string){
        if (col.first == index){
            matrix[get_coordinates(index,index)] = SAMESTRINGVALUE;
            continue;
        }
        matrix[get_coordinates(index, col.first)] = overlap_value(
            fav, relation_coordinate_string.at(col.first));
    }
}

void load_matrix_full() {
    for (const auto& row : relation_coordinate_string){
        int row_ind = row.first;
        for (const auto& col : relation_coordinate_string){
            int col_ind = col.first;
            if (row_ind == col.first){
                matrix[get_coordinates(row_ind, row_ind)] = SAMESTRINGVALUE;
                continue;
            }
            matrix[get_coordinates(row_ind, col_ind)] = overlap_value(
                relation_coordinate_string.at(row_ind), relation_coordinate_string.at(col_ind));
        }
    }
}