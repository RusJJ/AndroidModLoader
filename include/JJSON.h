#ifndef __JJSON_HEADER
#define __JJSON_HEADER



// MIT License
//
// Copyright (c) 2026 RusJJ
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in all
// copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
// SOFTWARE.



#include <algorithm>
#include <charconv>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <functional>
#include <limits>
#include <locale>
#include <memory>
#include <sstream>
#include <string>
#include <string_view>
#include <system_error>
#include <type_traits>
#include <unordered_map>
#include <utility>
#include <vector>

class JJSON
{
public:
    // Syntax accepted when reading and editing the document.
    enum class SyntaxMode
    {
        JSON,
        JSONC,
        JSON5
    };

    enum class ValueType
    {
        Missing, // No value at the requested path.
        Object,
        Array,
        String,
        Number,
        Boolean,
        Null
    };

    struct Options
    {
        SyntaxMode mode = SyntaxMode::JSON;
        bool allow_trailing_commas = false; // JSONC only.
        size_t max_depth = 512; // The recursive parser accepts up to 1024 levels.
    };

    struct Error
    {
        const char* message = nullptr;
        size_t offset = 0;
        size_t line = 0;
        size_t column = 0;
    };

    JJSON() = default;
    explicit JJSON( Options options ) : options_( options ) {}

    bool load( const std::string& path )
    {
        if ( path.find( '\0' ) != npos )
            return fail( "Invalid file path", 0 );

        const auto close_file = []( std::FILE* file ) { std::fclose( file ); };
        std::unique_ptr<std::FILE, decltype( close_file )> file( std::fopen( path.c_str(), "rb" ), close_file );
        if ( !file )
            return fail( "Could not open file", 0 );

        std::string text;
        char buffer[16384];
        size_t count;
        do
        {
            count = std::fread( buffer, 1, sizeof( buffer ), file.get() );
            text.append( buffer, count );
        }
        while ( count == sizeof( buffer ) );
        if ( std::ferror( file.get() ) )
            return fail( "Could not read file", 0 );
        file.reset();

        if ( !parse( std::move( text ) ) )
            return false;

        source_path_ = path;
        return true;
    }

    bool parse( std::string text )
    {
        JJSON next( options_ );
        next.text_ = std::move( text );
        if ( !next.scan() )
        {
            error_ = next.error_;
            return false;
        }

        *this = std::move( next );
        return true;
    }

    bool save() const
    {
        return !source_path_.empty() && save_as( source_path_ );
    }

    bool save_as( const std::string& path ) const
    {
        if ( nodes_.empty() || path.find( '\0' ) != npos )
            return false;
        std::FILE* file = std::fopen( path.c_str(), "wb" );
        if ( !file )
            return false;
        const size_t written = std::fwrite( text_.data(), 1, text_.size(), file );
        return std::fclose( file ) == 0 && written == text_.size();
    }

    const std::string& to_string() const { return text_; }
    const Error& error() const { return error_; }

    // Paths are JSON Pointers. An empty path refers to the whole document.
    ValueType value_type( std::string_view path = {} ) const
    {
        const size_t id = resolve( path );
        return id == npos ? ValueType::Missing : nodes_[id].type;
    }

    bool has( std::string_view path ) const { return value_type( path ) != ValueType::Missing; }

    size_t size( std::string_view path = {} ) const
    {
        const size_t id = resolve( path );
        return id == npos ? 0 : nodes_[id].count;
    }

    std::vector<std::string> keys( std::string_view path = {} ) const
    {
        std::vector<std::string> out;
        const size_t id = resolve( path );
        if ( id == npos || nodes_[id].type != ValueType::Object )
            return out;

        out.reserve( nodes_[id].count );
        for ( size_t child = first_child( id ); child != npos; child = nodes_[child].next )
            out.push_back( decode_key( nodes_[child] ) );
        return out;
    }

    std::string_view raw( std::string_view path = {} ) const
    {
        const size_t id = resolve( path );
        return id == npos ? std::string_view{} : span( nodes_[id].begin, nodes_[id].end );
    }

    bool get_into( std::string_view path, std::string& out ) const
    {
        const size_t id = resolve( path );
        if ( id == npos )
            return false;

        const Node& node = nodes_[id];
        if ( node.type == ValueType::String )
            decode_into( span( node.begin + 1, node.end - 1 ), node.escaped, out );
        else
            out = span( node.begin, node.end );
        return true;
    }

    std::string get( std::string_view path, std::string_view fallback = {} ) const
    {
        std::string out;
        return get_into( path, out ) ? std::move( out ) : std::string( fallback );
    }

    template <typename T>
    T get_as( std::string_view path, T fallback = T{} ) const
    {
        if constexpr ( std::is_same_v<T, std::string> )
            return get( path, fallback );

        const size_t id = resolve( path );
        if ( id == npos )
            return fallback;

        const Node& node = nodes_[id];
        std::string_view value = span( node.begin, node.end );
        if constexpr ( std::is_same_v<T, bool> )
            return node.type == ValueType::Boolean ? value == "true" : fallback;
        else if constexpr ( std::is_arithmetic_v<T> )
            return node.type == ValueType::Number ? number_as( value, fallback ) : fallback;
        else
            return fallback;
    }

    bool set( std::string_view path, std::string_view value )
    {
        return set_raw( path, quote( value ) );
    }

    template <typename T>
    bool set_as( std::string_view path, const T& value )
    {
        if constexpr ( std::is_same_v<T, bool> )
            return set_value( path, value ? "true" : "false", ValueType::Boolean );
        else if constexpr ( std::is_same_v<T, std::nullptr_t> )
            return set_value( path, "null", ValueType::Null );
        else if constexpr ( std::is_integral_v<T> )
        {
            char buffer[std::numeric_limits<T>::digits10 + 4];
            const auto result = std::to_chars( buffer, buffer + sizeof( buffer ), +value );
            return result.ec == std::errc{} &&
                set_value( path, std::string_view( buffer, result.ptr - buffer ), ValueType::Number );
        }
        else if constexpr ( std::is_floating_point_v<T> )
        {
            if ( !std::isfinite( value ) )
            {
                if ( options_.mode != SyntaxMode::JSON5 )
                    return false;
                return set_value( path, std::isnan( value ) ? "NaN" : value < 0 ? "-Infinity" : "Infinity", ValueType::Number );
            }
            return set_number( path, value, 0 );
        }
        else
            return set( path, value );
    }

    // Raw edits take a single value. Array insertion also uses raw values.
    bool set_raw( std::string_view path, std::string_view value )
    {
        Node parsed;
        return check_value( value, &parsed ) && set_value( path, value, parsed.type, parsed.escaped );
    }

    bool insert_at( std::string_view path, size_t index, std::string_view value )
    {
        const size_t id = resolve( path );
        return id != npos && nodes_[id].type == ValueType::Array && index <= nodes_[id].count &&
            check_value( value ) && insert_value( id, index, value );
    }

    bool push( std::string_view path, std::string_view value )
    {
        const size_t id = resolve( path );
        return id != npos && nodes_[id].type == ValueType::Array && check_value( value ) &&
            insert_value( id, nodes_[id].count, value );
    }

    bool remove( std::string_view path )
    {
        std::vector<size_t> parents;
        const size_t id = resolve( path, &parents );
        if ( id == npos || id == 0 )
            return false;

        const Node& node = nodes_[id];
        const size_t begin = node.key_begin == npos ? node.begin : node.key_begin;
        std::string text = text_;
        if ( node.comma != npos )
            text.erase( node.comma, 1 );
        text.erase( begin, node.end - begin );
        if ( node.comma == npos )
        {
            const size_t parent = parents.back();
            size_t previous = first_child( parent );
            if ( previous != id )
            {
                while ( nodes_[previous].next != id )
                    previous = nodes_[previous].next;
                text.erase( nodes_[previous].comma, 1 );
            }
        }
        return commit( std::move( text ) );
    }

    size_t comment_count() const { return comments_.size(); }

    // Raw and comment views are invalidated by a successful parse or edit.
    std::string_view get_comment( size_t index ) const
    {
        return index < comments_.size() ? span( comments_[index].begin, comments_[index].end ) : std::string_view{};
    }

    bool set_comment( size_t index, std::string_view comment )
    {
        if ( index >= comments_.size() || !check_comment( comment ) )
            return false;

        std::string replacement( comment );
        const Comment& old = comments_[index];
        if ( replacement[1] == '/' && old.end < text_.size() && !line_end( old.end, true ) )
            replacement += newline();
        return replace( old.begin, old.end, replacement );
    }

    bool insert_comment( std::string_view path, std::string_view comment )
    {
        const size_t id = resolve( path );
        if ( id == npos || !check_comment( comment ) )
            return false;

        const Node& node = nodes_[id];
        const size_t begin = node.key_begin == npos ? node.begin : node.key_begin;
        std::string replacement( comment );
        if ( replacement[1] == '/' )
            replacement += newline();
        return replace( begin, begin, replacement );
    }

    bool remove_comment( size_t index )
    {
        if ( index >= comments_.size() )
            return false;
        const Comment& comment = comments_[index];
        return replace( comment.begin, comment.end, " " );
    }

private:
    static constexpr size_t npos = static_cast<size_t>( -1 );

    struct Node
    {
        size_t begin = 0, end = 0;
        size_t key_begin = npos, key_end = npos;
        size_t last = npos, next = npos, count = 0;
        size_t comma = npos;
        ValueType type = ValueType::Missing;
        bool escaped = false, key_escaped = false;
    };

    struct Comment { size_t begin, end; };
    struct KeyIndex { size_t hash, child; };

    static constexpr size_t index_threshold = 32;

    std::string_view span( size_t begin, size_t end ) const
    {
        return std::string_view( text_ ).substr( begin, end - begin );
    }

    static std::string quote( std::string_view value )
    {
        constexpr char hex[] = "0123456789abcdef";
        std::string out = "\"";
        out.reserve( value.size() + 2 );
        for ( unsigned char ch : value )
        {
            if ( ch == '"' || ch == '\\' )
                out += '\\';
            if ( ch < 0x20 )
            {
                out += "\\u00";
                out += hex[ch >> 4];
                out += hex[ch & 15];
            }
            else
                out += static_cast<char>( ch );
        }
        out += '"';
        return out;
    }

    static bool digit( char ch ) { return ch >= '0' && ch <= '9'; }

    static bool pointer_part( std::string_view value, std::string& out )
    {
        out.clear();
        for ( size_t i = 0; i < value.size(); ++i )
        {
            if ( value[i] != '~' )
                out += value[i];
            else
            {
                if ( ++i == value.size() || ( value[i] != '0' && value[i] != '1' ) )
                    return false;
                out += value[i] == '0' ? '~' : '/';
            }
        }
        return true;
    }

    size_t first_child( size_t id ) const
    {
        // Nodes are stored in preorder, so a nonempty collection's first child follows it.
        return nodes_[id].count ? id + 1 : npos;
    }

    size_t child_at( size_t id, size_t index ) const
    {
        const Node& node = nodes_[id];
        if ( index >= node.count )
            return npos;
        if ( node.last - id == node.count )
            return id + 1 + index;
        if ( index == node.count - 1 )
            return node.last;
        if ( node.count >= index_threshold )
        {
            const auto it = array_index_.find( id );
            if ( it != array_index_.end() )
                return it->second[index];
        }
        size_t child = first_child( id );
        while ( index-- )
            child = nodes_[child].next;
        return child;
    }

    std::string_view key_span( const Node& node ) const
    {
        const char first = text_[node.key_begin];
        if ( first == '"' || first == '\'' )
            return span( node.key_begin + 1, node.key_end - 1 );
        return span( node.key_begin, node.key_end );
    }

    std::string decode_key( const Node& node, bool preserve_surrogates = false ) const
    {
        std::string out;
        decode_into( key_span( node ), node.key_escaped, out, preserve_surrogates );
        return out;
    }

    bool key_equals( size_t id, std::string_view key ) const
    {
        const Node& node = nodes_[id];
        return node.key_escaped ? decode_key( node, true ) == key : key_span( node ) == key;
    }

    size_t find_member( size_t id, std::string_view key ) const
    {
        const Node& node = nodes_[id];
        if ( node.count >= index_threshold )
        {
            const auto index = object_index_.find( id );
            if ( index != object_index_.end() )
            {
                const auto& entries = index->second;
                const size_t hash = std::hash<std::string_view>{}( key );
                auto it = std::lower_bound( entries.begin(), entries.end(), KeyIndex{ hash, 0 },
                    []( const KeyIndex& a, const KeyIndex& b ) { return a.hash < b.hash; } );
                for ( ; it != entries.end() && it->hash == hash; ++it )
                {
                    if ( key_equals( it->child, key ) )
                        return it->child;
                }
                return npos;
            }
        }

        // Keep duplicate members in the source. Lookup uses the last occurrence.
        size_t found = npos;
        for ( size_t child = first_child( id ); child != npos; child = nodes_[child].next )
        {
            if ( key_equals( child, key ) )
                found = child;
        }
        return found;
    }

    size_t resolve( std::string_view path, std::vector<size_t>* parents = nullptr ) const
    {
        if ( nodes_.empty() )
            return npos;
        size_t id = 0;
        std::string decoded;
        while ( !path.empty() )
        {
            if ( path.front() != '/' )
                return npos;
            path.remove_prefix( 1 );
            const size_t separator = path.find( '/' );
            std::string_view part = path.substr( 0, separator );
            if ( part.find( '~' ) != npos )
            {
                if ( !pointer_part( part, decoded ) )
                    return npos;
                part = decoded;
            }

            const size_t parent = id;
            const Node& node = nodes_[parent];
            if ( parents )
                parents->push_back( parent );
            id = npos;
            if ( node.type == ValueType::Object )
                id = find_member( parent, part );
            else if ( node.type == ValueType::Array && !part.empty() && digit( part.front() ) &&
                ( part.size() == 1 || part.front() != '0' ) )
            {
                size_t index = 0;
                const auto result = std::from_chars( part.data(), part.data() + part.size(), index );
                if ( result.ec == std::errc{} && result.ptr == part.data() + part.size() )
                    id = child_at( parent, index );
            }

            if ( id == npos || separator == npos )
                return id;
            path.remove_prefix( separator );
        }
        return id;
    }

    template <typename T>
    static T number_as( std::string_view value, T fallback )
    {
        const bool negative = value.front() == '-';
        if ( value.front() == '+' || negative )
            value.remove_prefix( 1 );

        const bool hex = value.size() > 2 && value[0] == '0' && ( value[1] == 'x' || value[1] == 'X' );
        if ( hex )
            value.remove_prefix( 2 );

        if constexpr ( std::is_integral_v<T> )
        {
            if ( negative && !std::is_signed_v<T> )
                return fallback;
            std::make_unsigned_t<T> out{};
            const auto result = std::from_chars( value.data(), value.data() + value.size(), out, hex ? 16 : 10 );
            if ( result.ec != std::errc{} || result.ptr != value.data() + value.size() )
                return fallback;
            const auto limit = static_cast<std::make_unsigned_t<T>>( std::numeric_limits<T>::max() );
            if ( negative && out > limit )
                return out == limit + 1u ? std::numeric_limits<T>::min() : fallback;
            if ( out > limit )
                return fallback;
            return negative ? static_cast<T>( -static_cast<T>( out ) ) : static_cast<T>( out );
        }
        else
        {
            if ( value == "Infinity" )
                return negative ? -std::numeric_limits<T>::infinity() : std::numeric_limits<T>::infinity();
            if ( value == "NaN" )
                return std::copysign( std::numeric_limits<T>::quiet_NaN(), negative ? T{-1} : T{1} );
            if ( hex )
            {
                long double out = 0;
                for ( char ch : value )
                    out = out * 16 + hex_digit( ch );
                if ( out > std::numeric_limits<T>::max() )
                    return fallback;
                return static_cast<T>( negative ? -out : out );
            }
            T out{};
            return read_decimal( value, out, 0 ) ? ( negative ? -out : out ) : fallback;
        }
    }

    // Older NDK libc++ versions provide charconv for integers only.
    template <typename T>
    static auto read_decimal( std::string_view value, T& out, int )
        -> decltype( std::from_chars( value.data(), value.data(), out ), bool{} )
    {
        const char* end = value.data() + value.size();
        const auto result = std::from_chars( value.data(), end, out );
        if ( result.ec == std::errc::result_out_of_range )
            return read_decimal( value, out, 0L );
        return result.ec == std::errc{} && result.ptr == end;
    }

    template <typename T>
    static bool read_decimal( std::string_view value, T& out, long )
    {
        std::istringstream stream{ std::string( value ) };
        stream.imbue( std::locale::classic() );
        stream >> std::noskipws >> out;
        // The token is already valid. Some libraries report underflow as a stream error.
        const T minimum = std::numeric_limits<T>::min();
        const T maximum = std::numeric_limits<T>::max();
        return stream.eof() && out >= -maximum && out <= maximum &&
            ( stream || ( out >= -minimum && out <= minimum ) );
    }

    template <typename T>
    auto set_number( std::string_view path, T value, int )
        -> decltype( std::to_chars( static_cast<char*>( nullptr ), static_cast<char*>( nullptr ),
            value, std::chars_format::general, 1 ), bool{} )
    {
        char buffer[std::numeric_limits<T>::max_digits10 + 16];
        const auto result = std::to_chars( buffer, buffer + sizeof( buffer ), value, std::chars_format::general,
            std::numeric_limits<T>::max_digits10 );
        return result.ec == std::errc{} &&
            set_value( path, std::string_view( buffer, result.ptr - buffer ), ValueType::Number );
    }

    template <typename T>
    bool set_number( std::string_view path, T value, long )
    {
        std::ostringstream stream;
        stream.imbue( std::locale::classic() );
        stream.precision( std::numeric_limits<T>::max_digits10 );
        stream << value;
        return stream && set_value( path, stream.str(), ValueType::Number );
    }

    std::string_view newline() const
    {
        const size_t lf = text_.find( '\n' );
        return lf != npos && lf && text_[lf - 1] == '\r' ? "\r\n" : "\n";
    }

    bool check_value( std::string_view value, Node* parsed = nullptr )
    {
        JJSON fragment( options_ );
        fragment.text_ = value;
        if ( fragment.scan( false ) )
        {
            if ( parsed )
                *parsed = fragment.nodes_[0];
            return true;
        }
        error_ = fragment.error_;
        return false;
    }

    bool check_comment( std::string_view comment )
    {
        if ( options_.mode == SyntaxMode::JSON || comment.size() < 2 || comment[0] != '/' )
            return false;
        if ( comment[1] == '*' )
            return comment.size() >= 4 && comment.find( "*/", 2 ) == comment.size() - 2;
        return comment[1] == '/' && comment.find_first_of( "\r\n" ) == npos &&
            ( options_.mode != SyntaxMode::JSON5 ||
                ( comment.find( "\xE2\x80\xA8" ) == npos && comment.find( "\xE2\x80\xA9" ) == npos ) );
    }

    size_t indent_start( size_t at ) const
    {
        size_t begin = at;
        while ( begin && ( text_[begin - 1] == ' ' || text_[begin - 1] == '\t' ) )
            --begin;
        return begin && ( text_[begin - 1] == '\n' || text_[begin - 1] == '\r' ) ? begin : npos;
    }

    bool insert_value( size_t id, size_t index, std::string_view value )
    {
        const Node& node = nodes_[id];
        if ( index == node.count )
        {
            const size_t closing = node.end - 1;
            const size_t closing_line = indent_start( closing );
            if ( closing_line != npos )
            {
                std::string line;
                size_t last_end = 0;
                bool trailing = false;
                if ( node.count )
                {
                    const Node& first = nodes_[id + 1];
                    const size_t first_at = first.key_begin == npos ? first.begin : first.key_begin;
                    const size_t first_line = indent_start( first_at );
                    if ( first_line != npos )
                        line = span( first_line, first_at );
                    last_end = nodes_[node.last].end;
                    trailing = nodes_[node.last].comma != npos;
                }
                if ( line.empty() )
                    line = std::string( span( closing_line, closing ) ) + "  ";
                line += value;
                if ( trailing )
                    line += ',';
                line += newline();

                std::string text = text_;
                text.insert( closing_line, line );
                if ( node.count && !trailing )
                    text.insert( last_end, 1, ',' );
                return commit( std::move( text ) );
            }

            if ( !node.count )
                return replace( node.begin + 1, node.begin + 1, value );

            const size_t at = nodes_[node.last].end;
            return replace( at, at, ", " + std::string( value ) );
        }

        const size_t at = nodes_[child_at( id, index )].begin;
        const size_t line = indent_start( at );
        if ( line == npos )
            return replace( at, at, std::string( value ) + ", " );

        std::string replacement( value );
        replacement += ',';
        replacement += newline();
        replacement += span( line, at );
        return replace( at, at, replacement );
    }

    static bool scalar( ValueType type )
    {
        return type != ValueType::Array && type != ValueType::Object;
    }

    bool set_value( std::string_view path, std::string_view value, ValueType type, bool escaped = false )
    {
        if ( nodes_.empty() && path.empty() )
            return replace( 0, 0, value );

        const size_t id = resolve( path );
        if ( id != npos )
        {
            Node& node = nodes_[id];
            if ( scalar( node.type ) && scalar( type ) )
                return replace_scalar( id, path, value, type, escaped );
            return replace( node.begin, node.end, value );
        }

        const size_t slash = path.rfind( '/' );
        if ( slash == npos )
            return false;
        const size_t parent = resolve( path.substr( 0, slash ) );
        if ( parent == npos )
            return false;

        std::string key;
        if ( !pointer_part( path.substr( slash + 1 ), key ) )
            return false;
        const Node& node = nodes_[parent];
        if ( node.type == ValueType::Object )
            return insert_value( parent, node.count, quote( key ) + ": " + std::string( value ) );
        if ( node.type == ValueType::Array && key == "-" )
            return insert_value( parent, node.count, value );
        return false;
    }

    bool replace_scalar( size_t id, std::string_view path, std::string_view value, ValueType type, bool escaped )
    {
        Node& node = nodes_[id];
        const size_t old_end = node.end;
        const size_t old_size = old_end - node.begin;
        const size_t new_size = value.size();
        std::vector<size_t> parents;
        if ( old_size != new_size && id )
            resolve( path, &parents );

        text_.replace( node.begin, old_size, value );
        node.type = type;
        node.escaped = escaped;

        if ( old_size != new_size )
        {
            // Scalar edits keep the tree intact. Only source offsets after the edit move.
            const auto shift = [=]( size_t& offset ) {
                if ( offset != npos && offset >= old_end )
                    offset = offset - old_size + new_size;
            };

            for ( size_t parent : parents )
            {
                shift( nodes_[parent].end );
                shift( nodes_[parent].comma );
            }
            shift( node.end );
            shift( node.comma );

            for ( size_t i = id + 1; i < nodes_.size(); ++i )
            {
                Node& entry = nodes_[i];
                shift( entry.begin );
                shift( entry.end );
                shift( entry.key_begin );
                shift( entry.key_end );
                shift( entry.comma );
            }

            auto comment = std::lower_bound( comments_.begin(), comments_.end(), old_end,
                []( const Comment& entry, size_t offset ) { return entry.begin < offset; } );
            for ( ; comment != comments_.end(); ++comment )
            {
                shift( comment->begin );
                shift( comment->end );
            }
        }
        error_ = {};
        return true;
    }

    bool replace( size_t begin, size_t end, std::string_view value )
    {
        std::string text = text_;
        text.replace( begin, end - begin, value );
        return commit( std::move( text ) );
    }

    bool commit( std::string text )
    {
        JJSON next( options_ );
        if ( !next.parse( std::move( text ) ) )
        {
            error_ = next.error_;
            return false;
        }

        next.source_path_ = source_path_;
        *this = std::move( next );
        return true;
    }

    bool fail( const char* message, size_t offset )
    {
        error_ = { message, offset, 1, 1 };
        for ( size_t i = 0; i < offset; )
        {
            const size_t ending = line_end( i, true );
            if ( ending && i + ending <= offset )
            {
                i += ending;
                ++error_.line;
                error_.column = 1;
            }
            else
            {
                ++i;
                ++error_.column;
            }
        }
        return false;
    }

    char peek() const { return pos_ < text_.size() ? text_[pos_] : '\0'; }
    bool json5() const { return options_.mode == SyntaxMode::JSON5; }

    size_t line_end( size_t pos, bool pair ) const
    {
        if ( pos >= text_.size() )
            return 0;
        if ( text_[pos] == '\n' )
            return 1;
        if ( text_[pos] == '\r' )
            return pair && pos + 1 < text_.size() && text_[pos + 1] == '\n' ? 2 : 1;
        if ( json5() && std::string_view( text_ ).substr( pos, 2 ) == "\xE2\x80" && pos + 2 < text_.size() )
        {
            const auto ch = static_cast<unsigned char>( text_[pos + 2] );
            if ( ch == 0xA8 || ch == 0xA9 )
                return 3;
        }
        return 0;
    }

    static unsigned hex_digit( char ch )
    {
        if ( digit( ch ) )
            return static_cast<unsigned>( ch - '0' );
        if ( ch >= 'A' && ch <= 'F' )
            return static_cast<unsigned>( ch - 'A' + 10 );
        if ( ch >= 'a' && ch <= 'f' )
            return static_cast<unsigned>( ch - 'a' + 10 );
        return 16;
    }

    static bool read_hex( std::string_view text, size_t& pos, size_t count, uint32_t& out )
    {
        if ( count > text.size() - pos )
            return false;
        out = 0;
        while ( count-- )
        {
            const unsigned digit = hex_digit( text[pos++] );
            if ( digit == 16 )
                return false;
            out = out * 16 + digit;
        }
        return true;
    }

    static bool read_utf8( std::string_view text, size_t& pos, uint32_t& out )
    {
        if ( pos == text.size() )
            return false;
        const uint32_t first = static_cast<unsigned char>( text[pos++] );
        if ( first < 0x80 )
        {
            out = first;
            return true;
        }

        if ( first < 0xC2 || first > 0xF4 )
            return false;
        const size_t count = first < 0xE0 ? 1 : first < 0xF0 ? 2 : 3;
        if ( count > text.size() - pos )
            return false;

        out = first & ( 0x7F >> ( count + 1 ) );
        for ( size_t i = 0; i < count; ++i )
        {
            const uint32_t next = static_cast<unsigned char>( text[pos++] );
            if ( ( next & 0xC0 ) != 0x80 )
                return false;
            out = ( out << 6 ) | ( next & 0x3F );
        }

        const uint32_t minimum = count == 1 ? 0x80 : count == 2 ? 0x800 : 0x10000;
        return out >= minimum && out <= 0x10FFFF && ( out < 0xD800 || out > 0xDFFF );
    }

    static void append_utf8( std::string& out, uint32_t code )
    {
        if ( code < 0x80 )
            out += static_cast<char>( code );
        else if ( code < 0x800 )
        {
            out += static_cast<char>( 0xC0 | ( code >> 6 ) );
            out += static_cast<char>( 0x80 | ( code & 0x3F ) );
        }
        else
        {
            if ( code >= 0x10000 )
            {
                out += static_cast<char>( 0xF0 | ( code >> 18 ) );
                out += static_cast<char>( 0x80 | ( ( code >> 12 ) & 0x3F ) );
            }
            else
                out += static_cast<char>( 0xE0 | ( code >> 12 ) );

            out += static_cast<char>( 0x80 | ( ( code >> 6 ) & 0x3F ) );
            out += static_cast<char>( 0x80 | ( code & 0x3F ) );
        }
    }

    static bool json5_space( uint32_t ch )
    {
        return ch == 0x09 || ch == 0x0B || ch == 0x0C || ch == 0x20 || ch == 0xA0 ||
            ch == 0x1680 || ( ch >= 0x2000 && ch <= 0x200A ) ||
            ch == 0x202F || ch == 0x205F || ch == 0x3000 || ch == 0xFEFF;
    }

    bool trivia()
    {
        while ( pos_ < text_.size() )
        {
            const char ch = peek();
            if ( ch == ' ' || ch == '\t' || ch == '\r' || ch == '\n' )
            {
                ++pos_;
                continue;
            }
            if ( options_.mode != SyntaxMode::JSON && ch == '/' && pos_ + 1 < text_.size() )
            {
                const char next = text_[pos_ + 1];
                if ( next == '/' || next == '*' )
                {
                    const size_t begin = pos_;
                    pos_ += 2;
                    if ( next == '/' )
                    {
                        while ( pos_ < text_.size() && !line_end( pos_, false ) )
                            ++pos_;
                    }
                    else
                    {
                        const size_t end = text_.find( "*/", pos_ );
                        if ( end == npos )
                            return fail( "Unterminated comment", begin );
                        pos_ = end + 2;
                    }

                    for ( size_t i = begin + 2; i < pos_; )
                    {
                        const size_t at = i;
                        uint32_t code;
                        if ( !read_utf8( text_, i, code ) )
                            return fail( "Invalid UTF-8 in comment", at );
                    }
                    comments_.push_back( { begin, pos_ } );
                    continue;
                }
            }
            if ( json5() )
            {
                if ( ch == '\v' || ch == '\f' )
                {
                    ++pos_;
                    continue;
                }
                if ( static_cast<unsigned char>( ch ) >= 0x80 )
                {
                    size_t end = pos_;
                    uint32_t code;
                    if ( read_utf8( text_, end, code ) && ( json5_space( code ) || code == 0x2028 || code == 0x2029 ) )
                    {
                        pos_ = end;
                        continue;
                    }
                }
            }
            break;
        }
        return true;
    }

    bool scan_string( bool& escaped )
    {
        const size_t begin = pos_;
        const char quote = text_[pos_++];
        while ( pos_ < text_.size() )
        {
            const size_t at = pos_;
            const auto ch = static_cast<unsigned char>( text_[pos_++] );
            if ( ch == static_cast<unsigned char>( quote ) )
                return true;

            if ( ch == '\\' )
            {
                escaped = true;
                if ( pos_ == text_.size() )
                    break;
                const size_t ending = json5() ? line_end( pos_, true ) : 0;
                if ( ending )
                {
                    pos_ += ending;
                    continue;
                }

                const char escape = text_[pos_++];
                if ( escape == 'u' || ( json5() && escape == 'x' ) )
                {
                    uint32_t code;
                    if ( !read_hex( text_, pos_, escape == 'u' ? 4 : 2, code ) )
                        return fail( "Invalid hexadecimal escape", at );
                }
                else if ( json5() )
                {
                    if ( digit( escape ) && ( escape != '0' || digit( peek() ) ) )
                        return fail( "Invalid numeric escape", at );
                    if ( static_cast<unsigned char>( escape ) >= 0x80 )
                    {
                        uint32_t code;
                        --pos_;
                        if ( !read_utf8( text_, pos_, code ) )
                            return fail( "Invalid UTF-8 in string", at );
                    }
                }
                else if ( escape != '"' && escape != '\\' && escape != '/' && escape != 'b' &&
                    escape != 'f' && escape != 'n' && escape != 'r' && escape != 't' )
                    return fail( "Invalid string escape", at );
            }
            else if ( ch == '\r' || ch == '\n' || ( !json5() && ch < 0x20 ) )
                return fail( "Unescaped control character in string", at );
            else if ( ch >= 0x80 )
            {
                uint32_t code;
                --pos_;
                if ( !read_utf8( text_, pos_, code ) )
                    return fail( "Invalid UTF-8 in string", at );
            }
        }
        return fail( "Unterminated string", begin );
    }

    void decode_into( std::string_view value, bool escaped, std::string& out, bool preserve_surrogates = false ) const
    {
        if ( !escaped )
        {
            out = value;
            return;
        }

        out.clear();
        out.reserve( value.size() );
        for ( size_t i = 0; i < value.size(); )
        {
            const char ch = value[i++];
            if ( ch != '\\' )
            {
                out += ch;
                continue;
            }

            const size_t next = i;
            const char escape = value[i++];
            switch ( escape )
            {
                case 'b': out += '\b'; break;
                case 'f': out += '\f'; break;
                case 'n': out += '\n'; break;
                case 'r': out += '\r'; break;
                case 't': out += '\t'; break;
                case 'v': out += '\v'; break;
                case '0': out += '\0'; break;
                case '\n': break;
                case '\r':
                    if ( i < value.size() && value[i] == '\n' )
                        ++i;
                    break;
                case 'x':
                case 'u':
                {
                    uint32_t code = 0;
                    read_hex( value, i, escape == 'u' ? 4 : 2, code );
                    if ( code >= 0xD800 && code <= 0xDBFF && value.substr( i, 2 ) == "\\u" )
                    {
                        size_t end = i + 2;
                        uint32_t low = 0;
                        if ( read_hex( value, end, 4, low ) && low >= 0xDC00 && low <= 0xDFFF )
                        {
                            i = end;
                            code = 0x10000 + ( ( code - 0xD800 ) << 10 ) + low - 0xDC00;
                        }
                    }
                    // Keep invalid surrogate keys distinct from a literal replacement character.
                    if ( !preserve_surrogates && code >= 0xD800 && code <= 0xDFFF )
                        code = 0xFFFD;
                    append_utf8( out, code );
                    break;
                }
                default:
                    if ( value.substr( next, 3 ) == "\xE2\x80\xA8" || value.substr( next, 3 ) == "\xE2\x80\xA9" )
                        i = next + 3;
                    else
                        out += escape;
                    break;
            }
        }
    }

    bool scan_identifier( bool& escaped )
    {
        bool first = true;
        while ( pos_ < text_.size() )
        {
            size_t end = pos_;
            uint32_t code;
            const bool escape = peek() == '\\';
            if ( escape )
            {
                if ( span( pos_, pos_ + 2 ) != "\\u" )
                    return fail( "Invalid identifier escape", pos_ );
                end += 2;
                if ( !read_hex( text_, end, 4, code ) )
                    return fail( "Invalid identifier escape", pos_ );
            }
            else if ( !read_utf8( text_, end, code ) )
                return fail( "Invalid UTF-8 in identifier", pos_ );

            if ( !identifier_character( code, first ) )
            {
                if ( escape || first )
                    return fail( "Invalid identifier character", pos_ );
                break;
            }

            escaped = escaped || escape;
            first = false;
            pos_ = end;
        }
        return !first || fail( "Expected object key", pos_ );
    }

    bool scan_number()
    {
        const size_t begin = pos_;
        if ( peek() == '-' || ( json5() && peek() == '+' ) )
            ++pos_;
        if ( json5() )
        {
            if ( span( pos_, pos_ + 8 ) == "Infinity" )
            {
                pos_ += 8;
                return true;
            }
            if ( span( pos_, pos_ + 3 ) == "NaN" )
            {
                pos_ += 3;
                return true;
            }
            if ( span( pos_, pos_ + 2 ) == "0x" || span( pos_, pos_ + 2 ) == "0X" )
            {
                pos_ += 2;
                const size_t start = pos_;
                while ( hex_digit( peek() ) < 16 )
                    ++pos_;
                return pos_ != start || fail( "Expected hexadecimal digit", pos_ );
            }
        }

        const size_t integer = pos_;
        if ( peek() == '0' )
            ++pos_;
        else
        {
            while ( digit( peek() ) )
                ++pos_;
        }
        const bool has_integer = pos_ != integer;

        if ( !has_integer && !json5() )
            return fail( "Expected integer digit", pos_ );
        if ( peek() == '.' )
        {
            const size_t fraction = ++pos_;
            while ( digit( peek() ) )
                ++pos_;
            if ( fraction == pos_ && ( !json5() || !has_integer ) )
                return fail( "Expected fractional digit", pos_ );
        }
        else if ( !has_integer )
            return fail( "Expected number", begin );

        if ( peek() == 'e' || peek() == 'E' )
        {
            ++pos_;
            if ( peek() == '+' || peek() == '-' )
                ++pos_;
            const size_t exponent = pos_;
            while ( digit( peek() ) )
                ++pos_;
            if ( exponent == pos_ )
                return fail( "Expected exponent digit", pos_ );
        }
        return true;
    }

    bool scan_value( size_t depth )
    {
        const size_t id = nodes_.size();
        nodes_.emplace_back();
        nodes_[id].begin = pos_;

        const char ch = peek();
        ValueType type;
        if ( ch == '{' || ch == '[' )
        {
            if ( depth >= options_.max_depth || depth >= 1024 )
                return fail( "Maximum nesting depth exceeded", pos_ );
            type = ch == '{' ? ValueType::Object : ValueType::Array;
            if ( !scan_collection( id, depth + 1, type == ValueType::Object ) )
                return false;
        }
        else if ( ch == '"' || ( json5() && ch == '\'' ) )
        {
            type = ValueType::String;
            if ( !scan_string( nodes_[id].escaped ) )
                return false;
        }
        else if ( ( ch == 't' && span( pos_, pos_ + 4 ) == "true" ) ||
            ( ch == 'f' && span( pos_, pos_ + 5 ) == "false" ) )
        {
            type = ValueType::Boolean;
            pos_ += ch == 't' ? 4 : 5;
        }
        else if ( ch == 'n' && span( pos_, pos_ + 4 ) == "null" )
        {
            type = ValueType::Null;
            pos_ += 4;
        }
        else
        {
            type = ValueType::Number;
            if ( !scan_number() )
                return false;
        }

        nodes_[id].type = type;
        nodes_[id].end = pos_;
        if ( nodes_[id].count >= index_threshold )
            index_collection( id );
        return true;
    }

    bool scan_collection( size_t id, size_t depth, bool object )
    {
        const char close = object ? '}' : ']';
        ++pos_;
        if ( !trivia() )
            return false;
        if ( peek() == close )
        {
            ++pos_;
            return true;
        }

        for ( ;; )
        {
            size_t key_begin = npos, key_end = npos;
            bool key_escaped = false;
            if ( object )
            {
                key_begin = pos_;
                if ( peek() == '"' || ( json5() && peek() == '\'' ) )
                {
                    if ( !scan_string( key_escaped ) )
                        return false;
                }
                else if ( json5() )
                {
                    if ( !scan_identifier( key_escaped ) )
                        return false;
                }
                else
                    return fail( "Expected quoted object key", pos_ );
                key_end = pos_;
                if ( !trivia() )
                    return false;
                if ( peek() != ':' )
                    return fail( "Expected ':' after object key", pos_ );
                ++pos_;
                if ( !trivia() )
                    return false;
            }

            const size_t child = nodes_.size();
            if ( !scan_value( depth ) || !trivia() )
                return false;

            Node& parent = nodes_[id];
            Node& entry = nodes_[child];
            entry.key_begin = key_begin;
            entry.key_end = key_end;
            entry.key_escaped = key_escaped;
            if ( parent.count++ )
                nodes_[parent.last].next = child;
            parent.last = child;

            if ( peek() == close )
            {
                ++pos_;
                return true;
            }
            if ( peek() != ',' )
                return fail( "Expected ',' or closing delimiter", pos_ );

            entry.comma = pos_++;
            if ( !trivia() )
                return false;
            if ( peek() == close )
            {
                if ( !json5() && ( options_.mode != SyntaxMode::JSONC || !options_.allow_trailing_commas ) )
                    return fail( "Trailing comma is disabled", entry.comma );
                ++pos_;
                return true;
            }
        }
    }

    void index_collection( size_t id )
    {
        const Node& node = nodes_[id];
        if ( node.type == ValueType::Object )
        {
            auto& index = object_index_[id];
            index.reserve( node.count );
            for ( size_t child = first_child( id ); child != npos; child = nodes_[child].next )
            {
                const Node& entry = nodes_[child];
                const size_t hash = entry.key_escaped ? std::hash<std::string_view>{}( decode_key( entry, true ) ) :
                    std::hash<std::string_view>{}( key_span( entry ) );
                index.push_back( { hash, child } );
            }
            std::sort( index.begin(), index.end(), []( const KeyIndex& a, const KeyIndex& b ) {
                return a.hash == b.hash ? a.child > b.child : a.hash < b.hash;
            } );
        }
        else if ( node.type == ValueType::Array && node.last - id != node.count )
        {
            auto& index = array_index_[id];
            index.reserve( node.count );
            for ( size_t child = first_child( id ); child != npos; child = nodes_[child].next )
                index.push_back( child );
        }
    }

    bool scan( bool document = true )
    {
        if ( document && span( 0, 3 ) == "\xEF\xBB\xBF" )
            pos_ = 3;
        if ( !trivia() || !scan_value( 0 ) || !trivia() )
            return false;
        if ( pos_ != text_.size() )
            return fail( "Unexpected content after JSON value", pos_ );

        // A value edit cannot insert trivia that would swallow its surrounding punctuation.
        if ( !document && ( nodes_[0].begin != 0 || nodes_[0].end != text_.size() ) )
            return fail( "Expected a value without surrounding whitespace or comments", nodes_[0].end );
        return true;
    }

    static bool identifier_character( uint32_t code, bool first );

    Options options_;
    Error error_;
    std::string source_path_;
    std::string text_;
    std::vector<Node> nodes_;
    std::vector<Comment> comments_;
    std::unordered_map<size_t, std::vector<KeyIndex>> object_index_;
    std::unordered_map<size_t, std::vector<size_t>> array_index_;
    size_t pos_ = 0;
};

inline bool JJSON::identifier_character( uint32_t code, bool first )
{
    if ( code < 0x80 )
        return ( code >= 'a' && code <= 'z' ) || ( code >= 'A' && code <= 'Z' ) ||
            code == '_' || code == '$' || ( !first && digit( static_cast<char>( code ) ) );

    // ECMAScript 5.1 identifier categories, from Unicode 15.0.0.
    // The low bit of the encoded end marks ranges restricted to identifier continuation.
    static constexpr uint32_t ranges[][2] = {
        { 0xAA, 0x154 }, { 0xB5, 0x16A }, { 0xBA, 0x174 }, { 0xC0, 0x1AC },
        { 0xD8, 0x1EC }, { 0xF8, 0x582 }, { 0x2C6, 0x5A2 }, { 0x2E0, 0x5C8 },
        { 0x2EC, 0x5D8 }, { 0x2EE, 0x5DC }, { 0x300, 0x6DF }, { 0x370, 0x6E8 },
        { 0x376, 0x6EE }, { 0x37A, 0x6FA }, { 0x37F, 0x6FE }, { 0x386, 0x70C },
        { 0x388, 0x714 }, { 0x38C, 0x718 }, { 0x38E, 0x742 }, { 0x3A3, 0x7EA },
        { 0x3F7, 0x902 }, { 0x483, 0x90F }, { 0x48A, 0xA5E }, { 0x531, 0xAAC },
        { 0x559, 0xAB2 }, { 0x560, 0xB10 }, { 0x591, 0xB7B }, { 0x5BF, 0xB7F },
        { 0x5C1, 0xB85 }, { 0x5C4, 0xB8B }, { 0x5C7, 0xB8F }, { 0x5D0, 0xBD4 },
        { 0x5EF, 0xBE4 }, { 0x610, 0xC35 }, { 0x620, 0xC94 }, { 0x64B, 0xCD3 },
        { 0x66E, 0xCDE }, { 0x670, 0xCE1 }, { 0x671, 0xDA6 }, { 0x6D5, 0xDAA },
        { 0x6D6, 0xDB9 }, { 0x6DF, 0xDC9 }, { 0x6E5, 0xDCC }, { 0x6E7, 0xDD1 },
        { 0x6EA, 0xDDB }, { 0x6EE, 0xDDE }, { 0x6F0, 0xDF3 }, { 0x6FA, 0xDF8 },
        { 0x6FF, 0xDFE }, { 0x710, 0xE20 }, { 0x711, 0xE23 }, { 0x712, 0xE5E },
        { 0x730, 0xE95 }, { 0x74D, 0xF4A }, { 0x7A6, 0xF61 }, { 0x7B1, 0xF62 },
        { 0x7C0, 0xF93 }, { 0x7CA, 0xFD4 }, { 0x7EB, 0xFE7 }, { 0x7F4, 0xFEA },
        { 0x7FA, 0xFF4 }, { 0x7FD, 0xFFB }, { 0x800, 0x102A }, { 0x816, 0x1033 },
        { 0x81A, 0x1034 }, { 0x81B, 0x1047 }, { 0x824, 0x1048 }, { 0x825, 0x104F },
        { 0x828, 0x1050 }, { 0x829, 0x105B }, { 0x840, 0x10B0 }, { 0x859, 0x10B7 },
        { 0x860, 0x10D4 }, { 0x870, 0x110E }, { 0x889, 0x111C }, { 0x898, 0x113F },
        { 0x8A0, 0x1192 }, { 0x8CA, 0x11C3 }, { 0x8E3, 0x1207 }, { 0x904, 0x1272 },
        { 0x93A, 0x1279 }, { 0x93D, 0x127A }, { 0x93E, 0x129F }, { 0x950, 0x12A0 },
        { 0x951, 0x12AF }, { 0x958, 0x12C2 }, { 0x962, 0x12C7 }, { 0x966, 0x12DF },
        { 0x971, 0x1300 }, { 0x981, 0x1307 }, { 0x985, 0x1318 }, { 0x98F, 0x1320 },
        { 0x993, 0x1350 }, { 0x9AA, 0x1360 }, { 0x9B2, 0x1364 }, { 0x9B6, 0x1372 },
        { 0x9BC, 0x1379 }, { 0x9BD, 0x137A }, { 0x9BE, 0x1389 }, { 0x9C7, 0x1391 },
        { 0x9CB, 0x139B }, { 0x9CE, 0x139C }, { 0x9D7, 0x13AF }, { 0x9DC, 0x13BA },
        { 0x9DF, 0x13C2 }, { 0x9E2, 0x13C7 }, { 0x9E6, 0x13DF }, { 0x9F0, 0x13E2 },
        { 0x9FC, 0x13F8 }, { 0x9FE, 0x13FD }, { 0xA01, 0x1407 }, { 0xA05, 0x1414 },
        { 0xA0F, 0x1420 }, { 0xA13, 0x1450 }, { 0xA2A, 0x1460 }, { 0xA32, 0x1466 },
        { 0xA35, 0x146C }, { 0xA38, 0x1472 }, { 0xA3C, 0x1479 }, { 0xA3E, 0x1485 },
        { 0xA47, 0x1491 }, { 0xA4B, 0x149B }, { 0xA51, 0x14A3 }, { 0xA59, 0x14B8 },
        { 0xA5E, 0x14BC }, { 0xA66, 0x14E3 }, { 0xA72, 0x14E8 }, { 0xA75, 0x14EB },
        { 0xA81, 0x1507 }, { 0xA85, 0x151A }, { 0xA8F, 0x1522 }, { 0xA93, 0x1550 },
        { 0xAAA, 0x1560 }, { 0xAB2, 0x1566 }, { 0xAB5, 0x1572 }, { 0xABC, 0x1579 },
        { 0xABD, 0x157A }, { 0xABE, 0x158B }, { 0xAC7, 0x1593 }, { 0xACB, 0x159B },
        { 0xAD0, 0x15A0 }, { 0xAE0, 0x15C2 }, { 0xAE2, 0x15C7 }, { 0xAE6, 0x15DF },
        { 0xAF9, 0x15F2 }, { 0xAFA, 0x15FF }, { 0xB01, 0x1607 }, { 0xB05, 0x1618 },
        { 0xB0F, 0x1620 }, { 0xB13, 0x1650 }, { 0xB2A, 0x1660 }, { 0xB32, 0x1666 },
        { 0xB35, 0x1672 }, { 0xB3C, 0x1679 }, { 0xB3D, 0x167A }, { 0xB3E, 0x1689 },
        { 0xB47, 0x1691 }, { 0xB4B, 0x169B }, { 0xB55, 0x16AF }, { 0xB5C, 0x16BA },
        { 0xB5F, 0x16C2 }, { 0xB62, 0x16C7 }, { 0xB66, 0x16DF }, { 0xB71, 0x16E2 },
        { 0xB82, 0x1705 }, { 0xB83, 0x1706 }, { 0xB85, 0x1714 }, { 0xB8E, 0x1720 },
        { 0xB92, 0x172A }, { 0xB99, 0x1734 }, { 0xB9C, 0x1738 }, { 0xB9E, 0x173E },
        { 0xBA3, 0x1748 }, { 0xBA8, 0x1754 }, { 0xBAE, 0x1772 }, { 0xBBE, 0x1785 },
        { 0xBC6, 0x1791 }, { 0xBCA, 0x179B }, { 0xBD0, 0x17A0 }, { 0xBD7, 0x17AF },
        { 0xBE6, 0x17DF }, { 0xC00, 0x1809 }, { 0xC05, 0x1818 }, { 0xC0E, 0x1820 },
        { 0xC12, 0x1850 }, { 0xC2A, 0x1872 }, { 0xC3C, 0x1879 }, { 0xC3D, 0x187A },
        { 0xC3E, 0x1889 }, { 0xC46, 0x1891 }, { 0xC4A, 0x189B }, { 0xC55, 0x18AD },
        { 0xC58, 0x18B4 }, { 0xC5D, 0x18BA }, { 0xC60, 0x18C2 }, { 0xC62, 0x18C7 },
        { 0xC66, 0x18DF }, { 0xC80, 0x1900 }, { 0xC81, 0x1907 }, { 0xC85, 0x1918 },
        { 0xC8E, 0x1920 }, { 0xC92, 0x1950 }, { 0xCAA, 0x1966 }, { 0xCB5, 0x1972 },
        { 0xCBC, 0x1979 }, { 0xCBD, 0x197A }, { 0xCBE, 0x1989 }, { 0xCC6, 0x1991 },
        { 0xCCA, 0x199B }, { 0xCD5, 0x19AD }, { 0xCDD, 0x19BC }, { 0xCE0, 0x19C2 },
        { 0xCE2, 0x19C7 }, { 0xCE6, 0x19DF }, { 0xCF1, 0x19E4 }, { 0xCF3, 0x19E7 },
        { 0xD00, 0x1A07 }, { 0xD04, 0x1A18 }, { 0xD0E, 0x1A20 }, { 0xD12, 0x1A74 },
        { 0xD3B, 0x1A79 }, { 0xD3D, 0x1A7A }, { 0xD3E, 0x1A89 }, { 0xD46, 0x1A91 },
        { 0xD4A, 0x1A9B }, { 0xD4E, 0x1A9C }, { 0xD54, 0x1AAC }, { 0xD57, 0x1AAF },
        { 0xD5F, 0x1AC2 }, { 0xD62, 0x1AC7 }, { 0xD66, 0x1ADF }, { 0xD7A, 0x1AFE },
        { 0xD81, 0x1B07 }, { 0xD85, 0x1B2C }, { 0xD9A, 0x1B62 }, { 0xDB3, 0x1B76 },
        { 0xDBD, 0x1B7A }, { 0xDC0, 0x1B8C }, { 0xDCA, 0x1B95 }, { 0xDCF, 0x1BA9 },
        { 0xDD6, 0x1BAD }, { 0xDD8, 0x1BBF }, { 0xDE6, 0x1BDF }, { 0xDF2, 0x1BE7 },
        { 0xE01, 0x1C60 }, { 0xE31, 0x1C63 }, { 0xE32, 0x1C66 }, { 0xE34, 0x1C75 },
        { 0xE40, 0x1C8C }, { 0xE47, 0x1C9D }, { 0xE50, 0x1CB3 }, { 0xE81, 0x1D04 },
        { 0xE84, 0x1D08 }, { 0xE86, 0x1D14 }, { 0xE8C, 0x1D46 }, { 0xEA5, 0x1D4A },
        { 0xEA7, 0x1D60 }, { 0xEB1, 0x1D63 }, { 0xEB2, 0x1D66 }, { 0xEB4, 0x1D79 },
        { 0xEBD, 0x1D7A }, { 0xEC0, 0x1D88 }, { 0xEC6, 0x1D8C }, { 0xEC8, 0x1D9D },
        { 0xED0, 0x1DB3 }, { 0xEDC, 0x1DBE }, { 0xF00, 0x1E00 }, { 0xF18, 0x1E33 },
        { 0xF20, 0x1E53 }, { 0xF35, 0x1E6B }, { 0xF37, 0x1E6F }, { 0xF39, 0x1E73 },
        { 0xF3E, 0x1E7F }, { 0xF40, 0x1E8E }, { 0xF49, 0x1ED8 }, { 0xF71, 0x1F09 },
        { 0xF86, 0x1F0F }, { 0xF88, 0x1F18 }, { 0xF8D, 0x1F2F }, { 0xF99, 0x1F79 },
        { 0xFC6, 0x1F8D }, { 0x1000, 0x2054 }, { 0x102B, 0x207D }, { 0x103F, 0x207E },
        { 0x1040, 0x2093 }, { 0x1050, 0x20AA }, { 0x1056, 0x20B3 }, { 0x105A, 0x20BA },
        { 0x105E, 0x20C1 }, { 0x1061, 0x20C2 }, { 0x1062, 0x20C9 }, { 0x1065, 0x20CC },
        { 0x1067, 0x20DB }, { 0x106E, 0x20E0 }, { 0x1071, 0x20E9 }, { 0x1075, 0x2102 },
        { 0x1082, 0x211B }, { 0x108E, 0x211C }, { 0x108F, 0x213B }, { 0x10A0, 0x218A },
        { 0x10C7, 0x218E }, { 0x10CD, 0x219A }, { 0x10D0, 0x21F4 }, { 0x10FC, 0x2490 },
        { 0x124A, 0x249A }, { 0x1250, 0x24AC }, { 0x1258, 0x24B0 }, { 0x125A, 0x24BA },
        { 0x1260, 0x2510 }, { 0x128A, 0x251A }, { 0x1290, 0x2560 }, { 0x12B2, 0x256A },
        { 0x12B8, 0x257C }, { 0x12C0, 0x2580 }, { 0x12C2, 0x258A }, { 0x12C8, 0x25AC },
        { 0x12D8, 0x2620 }, { 0x1312, 0x262A }, { 0x1318, 0x26B4 }, { 0x135D, 0x26BF },
        { 0x1380, 0x271E }, { 0x13A0, 0x27EA }, { 0x13F8, 0x27FA }, { 0x1401, 0x2CD8 },
        { 0x166F, 0x2CFE }, { 0x1681, 0x2D34 }, { 0x16A0, 0x2DD4 }, { 0x16EE, 0x2DF0 },
        { 0x1700, 0x2E22 }, { 0x1712, 0x2E2B }, { 0x171F, 0x2E62 }, { 0x1732, 0x2E69 },
        { 0x1740, 0x2EA2 }, { 0x1752, 0x2EA7 }, { 0x1760, 0x2ED8 }, { 0x176E, 0x2EE0 },
        { 0x1772, 0x2EE7 }, { 0x1780, 0x2F66 }, { 0x17B4, 0x2FA7 }, { 0x17D7, 0x2FAE },
        { 0x17DC, 0x2FB8 }, { 0x17DD, 0x2FBB }, { 0x17E0, 0x2FD3 }, { 0x180B, 0x301B },
        { 0x180F, 0x3033 }, { 0x1820, 0x30F0 }, { 0x1880, 0x3108 }, { 0x1885, 0x310D },
        { 0x1887, 0x3150 }, { 0x18A9, 0x3153 }, { 0x18AA, 0x3154 }, { 0x18B0, 0x31EA },
        { 0x1900, 0x323C }, { 0x1920, 0x3257 }, { 0x1930, 0x3277 }, { 0x1946, 0x329F },
        { 0x1950, 0x32DA }, { 0x1970, 0x32E8 }, { 0x1980, 0x3356 }, { 0x19B0, 0x3392 },
        { 0x19D0, 0x33B3 }, { 0x1A00, 0x342C }, { 0x1A17, 0x3437 }, { 0x1A20, 0x34A8 },
        { 0x1A55, 0x34BD }, { 0x1A60, 0x34F9 }, { 0x1A7F, 0x3513 }, { 0x1A90, 0x3533 },
        { 0x1AA7, 0x354E }, { 0x1AB0, 0x357B }, { 0x1ABF, 0x359D }, { 0x1B00, 0x3609 },
        { 0x1B05, 0x3666 }, { 0x1B34, 0x3689 }, { 0x1B45, 0x3698 }, { 0x1B50, 0x36B3 },
        { 0x1B6B, 0x36E7 }, { 0x1B80, 0x3705 }, { 0x1B83, 0x3740 }, { 0x1BA1, 0x375B },
        { 0x1BAE, 0x375E }, { 0x1BB0, 0x3773 }, { 0x1BBA, 0x37CA }, { 0x1BE6, 0x37E7 },
        { 0x1C00, 0x3846 }, { 0x1C24, 0x386F }, { 0x1C40, 0x3893 }, { 0x1C4D, 0x389E },
        { 0x1C50, 0x38B3 }, { 0x1C5A, 0x38FA }, { 0x1C80, 0x3910 }, { 0x1C90, 0x3974 },
        { 0x1CBD, 0x397E }, { 0x1CD0, 0x39A5 }, { 0x1CD4, 0x39D1 }, { 0x1CE9, 0x39D8 },
        { 0x1CED, 0x39DB }, { 0x1CEE, 0x39E6 }, { 0x1CF4, 0x39E9 }, { 0x1CF5, 0x39EC },
        { 0x1CF7, 0x39F3 }, { 0x1CFA, 0x39F4 }, { 0x1D00, 0x3B7E }, { 0x1DC0, 0x3BFF },
        { 0x1E00, 0x3E2A }, { 0x1F18, 0x3E3A }, { 0x1F20, 0x3E8A }, { 0x1F48, 0x3E9A },
        { 0x1F50, 0x3EAE }, { 0x1F59, 0x3EB2 }, { 0x1F5B, 0x3EB6 }, { 0x1F5D, 0x3EBA },
        { 0x1F5F, 0x3EFA }, { 0x1F80, 0x3F68 }, { 0x1FB6, 0x3F78 }, { 0x1FBE, 0x3F7C },
        { 0x1FC2, 0x3F88 }, { 0x1FC6, 0x3F98 }, { 0x1FD0, 0x3FA6 }, { 0x1FD6, 0x3FB6 },
        { 0x1FE0, 0x3FD8 }, { 0x1FF2, 0x3FE8 }, { 0x1FF6, 0x3FF8 }, { 0x200C, 0x401B },
        { 0x203F, 0x4081 }, { 0x2054, 0x40A9 }, { 0x2071, 0x40E2 }, { 0x207F, 0x40FE },
        { 0x2090, 0x4138 }, { 0x20D0, 0x41B9 }, { 0x20E1, 0x41C3 }, { 0x20E5, 0x41E1 },
        { 0x2102, 0x4204 }, { 0x2107, 0x420E }, { 0x210A, 0x4226 }, { 0x2115, 0x422A },
        { 0x2119, 0x423A }, { 0x2124, 0x4248 }, { 0x2126, 0x424C }, { 0x2128, 0x4250 },
        { 0x212A, 0x425A }, { 0x212F, 0x4272 }, { 0x213C, 0x427E }, { 0x2145, 0x4292 },
        { 0x214E, 0x429C }, { 0x2160, 0x4310 }, { 0x2C00, 0x59C8 }, { 0x2CEB, 0x59DC },
        { 0x2CEF, 0x59E3 }, { 0x2CF2, 0x59E6 }, { 0x2D00, 0x5A4A }, { 0x2D27, 0x5A4E },
        { 0x2D2D, 0x5A5A }, { 0x2D30, 0x5ACE }, { 0x2D6F, 0x5ADE }, { 0x2D7F, 0x5AFF },
        { 0x2D80, 0x5B2C }, { 0x2DA0, 0x5B4C }, { 0x2DA8, 0x5B5C }, { 0x2DB0, 0x5B6C },
        { 0x2DB8, 0x5B7C }, { 0x2DC0, 0x5B8C }, { 0x2DC8, 0x5B9C }, { 0x2DD0, 0x5BAC },
        { 0x2DD8, 0x5BBC }, { 0x2DE0, 0x5BFF }, { 0x2E2F, 0x5C5E }, { 0x3005, 0x600E },
        { 0x3021, 0x6052 }, { 0x302A, 0x605F }, { 0x3031, 0x606A }, { 0x3038, 0x6078 },
        { 0x3041, 0x612C }, { 0x3099, 0x6135 }, { 0x309D, 0x613E }, { 0x30A1, 0x61F4 },
        { 0x30FC, 0x61FE }, { 0x3105, 0x625E }, { 0x3131, 0x631C }, { 0x31A0, 0x637E },
        { 0x31F0, 0x63FE }, { 0x3400, 0x9B7E }, { 0x4E00, 0x14918 }, { 0xA4D0, 0x149FA },
        { 0xA500, 0x14C18 }, { 0xA610, 0x14C3E }, { 0xA620, 0x14C53 }, { 0xA62A, 0x14C56 },
        { 0xA640, 0x14CDC }, { 0xA66F, 0x14CDF }, { 0xA674, 0x14CFB }, { 0xA67F, 0x14D3A },
        { 0xA69E, 0x14D3F }, { 0xA6A0, 0x14DDE }, { 0xA6F0, 0x14DE3 }, { 0xA717, 0x14E3E },
        { 0xA722, 0x14F10 }, { 0xA78B, 0x14F94 }, { 0xA7D0, 0x14FA2 }, { 0xA7D3, 0x14FA6 },
        { 0xA7D5, 0x14FB2 }, { 0xA7F2, 0x15002 }, { 0xA802, 0x15005 }, { 0xA803, 0x1500A },
        { 0xA806, 0x1500D }, { 0xA807, 0x15014 }, { 0xA80B, 0x15017 }, { 0xA80C, 0x15044 },
        { 0xA823, 0x1504F }, { 0xA82C, 0x15059 }, { 0xA840, 0x150E6 }, { 0xA880, 0x15103 },
        { 0xA882, 0x15166 }, { 0xA8B4, 0x1518B }, { 0xA8D0, 0x151B3 }, { 0xA8E0, 0x151E3 },
        { 0xA8F2, 0x151EE }, { 0xA8FB, 0x151F6 }, { 0xA8FD, 0x151FC }, { 0xA8FF, 0x15213 },
        { 0xA90A, 0x1524A }, { 0xA926, 0x1525B }, { 0xA930, 0x1528C }, { 0xA947, 0x152A7 },
        { 0xA960, 0x152F8 }, { 0xA980, 0x15307 }, { 0xA984, 0x15364 }, { 0xA9B3, 0x15381 },
        { 0xA9CF, 0x1539E }, { 0xA9D0, 0x153B3 }, { 0xA9E0, 0x153C8 }, { 0xA9E5, 0x153CB },
        { 0xA9E6, 0x153DE }, { 0xA9F0, 0x153F3 }, { 0xA9FA, 0x153FC }, { 0xAA00, 0x15450 },
        { 0xAA29, 0x1546D }, { 0xAA40, 0x15484 }, { 0xAA43, 0x15487 }, { 0xAA44, 0x15496 },
        { 0xAA4C, 0x1549B }, { 0xAA50, 0x154B3 }, { 0xAA60, 0x154EC }, { 0xAA7A, 0x154F4 },
        { 0xAA7B, 0x154FB }, { 0xAA7E, 0x1555E }, { 0xAAB0, 0x15561 }, { 0xAAB1, 0x15562 },
        { 0xAAB2, 0x15569 }, { 0xAAB5, 0x1556C }, { 0xAAB7, 0x15571 }, { 0xAAB9, 0x1557A },
        { 0xAABE, 0x1557F }, { 0xAAC0, 0x15580 }, { 0xAAC1, 0x15583 }, { 0xAAC2, 0x15584 },
        { 0xAADB, 0x155BA }, { 0xAAE0, 0x155D4 }, { 0xAAEB, 0x155DF }, { 0xAAF2, 0x155E8 },
        { 0xAAF5, 0x155ED }, { 0xAB01, 0x1560C }, { 0xAB09, 0x1561C }, { 0xAB11, 0x1562C },
        { 0xAB20, 0x1564C }, { 0xAB28, 0x1565C }, { 0xAB30, 0x156B4 }, { 0xAB5C, 0x156D2 },
        { 0xAB70, 0x157C4 }, { 0xABE3, 0x157D5 }, { 0xABEC, 0x157DB }, { 0xABF0, 0x157F3 },
        { 0xAC00, 0x1AF46 }, { 0xD7B0, 0x1AF8C }, { 0xD7CB, 0x1AFF6 }, { 0xF900, 0x1F4DA },
        { 0xFA70, 0x1F5B2 }, { 0xFB00, 0x1F60C }, { 0xFB13, 0x1F62E }, { 0xFB1D, 0x1F63A },
        { 0xFB1E, 0x1F63D }, { 0xFB1F, 0x1F650 }, { 0xFB2A, 0x1F66C }, { 0xFB38, 0x1F678 },
        { 0xFB3E, 0x1F67C }, { 0xFB40, 0x1F682 }, { 0xFB43, 0x1F688 }, { 0xFB46, 0x1F762 },
        { 0xFBD3, 0x1FA7A }, { 0xFD50, 0x1FB1E }, { 0xFD92, 0x1FB8E }, { 0xFDF0, 0x1FBF6 },
        { 0xFE00, 0x1FC1F }, { 0xFE20, 0x1FC5F }, { 0xFE33, 0x1FC69 }, { 0xFE4D, 0x1FC9F },
        { 0xFE70, 0x1FCE8 }, { 0xFE76, 0x1FDF8 }, { 0xFF10, 0x1FE33 }, { 0xFF21, 0x1FE74 },
        { 0xFF3F, 0x1FE7F }, { 0xFF41, 0x1FEB4 }, { 0xFF66, 0x1FF7C }, { 0xFFC2, 0x1FF8E },
        { 0xFFCA, 0x1FF9E }, { 0xFFD2, 0x1FFAE }, { 0xFFDA, 0x1FFB8 }, { 0x10000, 0x20016 },
        { 0x1000D, 0x2004C }, { 0x10028, 0x20074 }, { 0x1003C, 0x2007A }, { 0x1003F, 0x2009A },
        { 0x10050, 0x200BA }, { 0x10080, 0x201F4 }, { 0x10140, 0x202E8 }, { 0x101FD, 0x203FB },
        { 0x10280, 0x20538 }, { 0x102A0, 0x205A0 }, { 0x102E0, 0x205C1 }, { 0x10300, 0x2063E },
        { 0x1032D, 0x20694 }, { 0x10350, 0x206EA }, { 0x10376, 0x206F5 }, { 0x10380, 0x2073A },
        { 0x103A0, 0x20786 }, { 0x103C8, 0x2079E }, { 0x103D1, 0x207AA }, { 0x10400, 0x2093A },
        { 0x104A0, 0x20953 }, { 0x104B0, 0x209A6 }, { 0x104D8, 0x209F6 }, { 0x10500, 0x20A4E },
        { 0x10530, 0x20AC6 }, { 0x10570, 0x20AF4 }, { 0x1057C, 0x20B14 }, { 0x1058C, 0x20B24 },
        { 0x10594, 0x20B2A }, { 0x10597, 0x20B42 }, { 0x105A3, 0x20B62 }, { 0x105B3, 0x20B72 },
        { 0x105BB, 0x20B78 }, { 0x10600, 0x20E6C }, { 0x10740, 0x20EAA }, { 0x10760, 0x20ECE },
        { 0x10780, 0x20F0A }, { 0x10787, 0x20F60 }, { 0x107B2, 0x20F74 }, { 0x10800, 0x2100A },
        { 0x10808, 0x21010 }, { 0x1080A, 0x2106A }, { 0x10837, 0x21070 }, { 0x1083C, 0x21078 },
        { 0x1083F, 0x210AA }, { 0x10860, 0x210EC }, { 0x10880, 0x2113C }, { 0x108E0, 0x211E4 },
        { 0x108F4, 0x211EA }, { 0x10900, 0x2122A }, { 0x10920, 0x21272 }, { 0x10980, 0x2136E },
        { 0x109BE, 0x2137E }, { 0x10A00, 0x21400 }, { 0x10A01, 0x21407 }, { 0x10A05, 0x2140D },
        { 0x10A0C, 0x2141F }, { 0x10A10, 0x21426 }, { 0x10A15, 0x2142E }, { 0x10A19, 0x2146A },
        { 0x10A38, 0x21475 }, { 0x10A3F, 0x2147F }, { 0x10A60, 0x214F8 }, { 0x10A80, 0x21538 },
        { 0x10AC0, 0x2158E }, { 0x10AC9, 0x215C8 }, { 0x10AE5, 0x215CD }, { 0x10B00, 0x2166A },
        { 0x10B40, 0x216AA }, { 0x10B60, 0x216E4 }, { 0x10B80, 0x21722 }, { 0x10C00, 0x21890 },
        { 0x10C80, 0x21964 }, { 0x10CC0, 0x219E4 }, { 0x10D00, 0x21A46 }, { 0x10D24, 0x21A4F },
        { 0x10D30, 0x21A73 }, { 0x10E80, 0x21D52 }, { 0x10EAB, 0x21D59 }, { 0x10EB0, 0x21D62 },
        { 0x10EFD, 0x21DFF }, { 0x10F00, 0x21E38 }, { 0x10F27, 0x21E4E }, { 0x10F30, 0x21E8A },
        { 0x10F46, 0x21EA1 }, { 0x10F70, 0x21F02 }, { 0x10F82, 0x21F0B }, { 0x10FB0, 0x21F88 },
        { 0x10FE0, 0x21FEC }, { 0x11000, 0x22005 }, { 0x11003, 0x2206E }, { 0x11038, 0x2208D },
        { 0x11066, 0x220E1 }, { 0x11071, 0x220E4 }, { 0x11073, 0x220E9 }, { 0x11075, 0x220EA },
        { 0x1107F, 0x22105 }, { 0x11083, 0x2215E }, { 0x110B0, 0x22175 }, { 0x110C2, 0x22185 },
        { 0x110D0, 0x221D0 }, { 0x110F0, 0x221F3 }, { 0x11100, 0x22205 }, { 0x11103, 0x2224C },
        { 0x11127, 0x22269 }, { 0x11136, 0x2227F }, { 0x11144, 0x22288 }, { 0x11145, 0x2228D },
        { 0x11147, 0x2228E }, { 0x11150, 0x222E4 }, { 0x11173, 0x222E7 }, { 0x11176, 0x222EC },
        { 0x11180, 0x22305 }, { 0x11183, 0x22364 }, { 0x111B3, 0x22381 }, { 0x111C1, 0x22388 },
        { 0x111C9, 0x22399 }, { 0x111CE, 0x223B3 }, { 0x111DA, 0x223B4 }, { 0x111DC, 0x223B8 },
        { 0x11200, 0x22422 }, { 0x11213, 0x22456 }, { 0x1122C, 0x2246F }, { 0x1123E, 0x2247D },
        { 0x1123F, 0x22480 }, { 0x11241, 0x22483 }, { 0x11280, 0x2250C }, { 0x11288, 0x22510 },
        { 0x1128A, 0x2251A }, { 0x1128F, 0x2253A }, { 0x1129F, 0x22550 }, { 0x112B0, 0x225BC },
        { 0x112DF, 0x225D5 }, { 0x112F0, 0x225F3 }, { 0x11300, 0x22607 }, { 0x11305, 0x22618 },
        { 0x1130F, 0x22620 }, { 0x11313, 0x22650 }, { 0x1132A, 0x22660 }, { 0x11332, 0x22666 },
        { 0x11335, 0x22672 }, { 0x1133B, 0x22679 }, { 0x1133D, 0x2267A }, { 0x1133E, 0x22689 },
        { 0x11347, 0x22691 }, { 0x1134B, 0x2269B }, { 0x11350, 0x226A0 }, { 0x11357, 0x226AF },
        { 0x1135D, 0x226C2 }, { 0x11362, 0x226C7 }, { 0x11366, 0x226D9 }, { 0x11370, 0x226E9 },
        { 0x11400, 0x22868 }, { 0x11435, 0x2288D }, { 0x11447, 0x22894 }, { 0x11450, 0x228B3 },
        { 0x1145E, 0x228BD }, { 0x1145F, 0x228C2 }, { 0x11480, 0x2295E }, { 0x114B0, 0x22987 },
        { 0x114C4, 0x2298A }, { 0x114C7, 0x2298E }, { 0x114D0, 0x229B3 }, { 0x11580, 0x22B5C },
        { 0x115AF, 0x22B6B }, { 0x115B8, 0x22B81 }, { 0x115D8, 0x22BB6 }, { 0x115DC, 0x22BBB },
        { 0x11600, 0x22C5E }, { 0x11630, 0x22C81 }, { 0x11644, 0x22C88 }, { 0x11650, 0x22CB3 },
        { 0x11680, 0x22D54 }, { 0x116AB, 0x22D6F }, { 0x116B8, 0x22D70 }, { 0x116C0, 0x22D93 },
        { 0x11700, 0x22E34 }, { 0x1171D, 0x22E57 }, { 0x11730, 0x22E73 }, { 0x11740, 0x22E8C },
        { 0x11800, 0x23056 }, { 0x1182C, 0x23075 }, { 0x118A0, 0x231BE }, { 0x118E0, 0x231D3 },
        { 0x118FF, 0x2320C }, { 0x11909, 0x23212 }, { 0x1190C, 0x23226 }, { 0x11915, 0x2322C },
        { 0x11918, 0x2325E }, { 0x11930, 0x2326B }, { 0x11937, 0x23271 }, { 0x1193B, 0x2327D },
        { 0x1193F, 0x2327E }, { 0x11940, 0x23281 }, { 0x11941, 0x23282 }, { 0x11942, 0x23287 },
        { 0x11950, 0x232B3 }, { 0x119A0, 0x2334E }, { 0x119AA, 0x233A0 }, { 0x119D1, 0x233AF },
        { 0x119DA, 0x233C1 }, { 0x119E1, 0x233C2 }, { 0x119E3, 0x233C6 }, { 0x119E4, 0x233C9 },
        { 0x11A00, 0x23400 }, { 0x11A01, 0x23415 }, { 0x11A0B, 0x23464 }, { 0x11A33, 0x23473 },
        { 0x11A3A, 0x23474 }, { 0x11A3B, 0x2347D }, { 0x11A47, 0x2348F }, { 0x11A50, 0x234A0 },
        { 0x11A51, 0x234B7 }, { 0x11A5C, 0x23512 }, { 0x11A8A, 0x23533 }, { 0x11A9D, 0x2353A },
        { 0x11AB0, 0x235F0 }, { 0x11C00, 0x23810 }, { 0x11C0A, 0x2385C }, { 0x11C2F, 0x2386D },
        { 0x11C38, 0x2387F }, { 0x11C40, 0x23880 }, { 0x11C50, 0x238B3 }, { 0x11C72, 0x2391E },
        { 0x11C92, 0x2394F }, { 0x11CA9, 0x2396D }, { 0x11D00, 0x23A0C }, { 0x11D08, 0x23A12 },
        { 0x11D0B, 0x23A60 }, { 0x11D31, 0x23A6D }, { 0x11D3A, 0x23A75 }, { 0x11D3C, 0x23A7B },
        { 0x11D3F, 0x23A8B }, { 0x11D46, 0x23A8C }, { 0x11D47, 0x23A8F }, { 0x11D50, 0x23AB3 },
        { 0x11D60, 0x23ACA }, { 0x11D67, 0x23AD0 }, { 0x11D6A, 0x23B12 }, { 0x11D8A, 0x23B1D },
        { 0x11D90, 0x23B23 }, { 0x11D93, 0x23B2F }, { 0x11D98, 0x23B30 }, { 0x11DA0, 0x23B53 },
        { 0x11EE0, 0x23DE4 }, { 0x11EF3, 0x23DED }, { 0x11F00, 0x23E03 }, { 0x11F02, 0x23E04 },
        { 0x11F03, 0x23E07 }, { 0x11F04, 0x23E20 }, { 0x11F12, 0x23E66 }, { 0x11F34, 0x23E75 },
        { 0x11F3E, 0x23E85 }, { 0x11F50, 0x23EB3 }, { 0x11FB0, 0x23F60 }, { 0x12000, 0x24732 },
        { 0x12400, 0x248DC }, { 0x12480, 0x24A86 }, { 0x12F90, 0x25FE0 }, { 0x13000, 0x2685E },
        { 0x13440, 0x26881 }, { 0x13441, 0x2688C }, { 0x13447, 0x268AB }, { 0x14400, 0x28C8C },
        { 0x16800, 0x2D470 }, { 0x16A40, 0x2D4BC }, { 0x16A60, 0x2D4D3 }, { 0x16A70, 0x2D57C },
        { 0x16AC0, 0x2D593 }, { 0x16AD0, 0x2D5DA }, { 0x16AF0, 0x2D5E9 }, { 0x16B00, 0x2D65E },
        { 0x16B30, 0x2D66D }, { 0x16B40, 0x2D686 }, { 0x16B50, 0x2D6B3 }, { 0x16B63, 0x2D6EE },
        { 0x16B7D, 0x2D71E }, { 0x16E40, 0x2DCFE }, { 0x16F00, 0x2DE94 }, { 0x16F4F, 0x2DE9F },
        { 0x16F50, 0x2DEA0 }, { 0x16F51, 0x2DF0F }, { 0x16F8F, 0x2DF25 }, { 0x16F93, 0x2DF3E },
        { 0x16FE0, 0x2DFC2 }, { 0x16FE3, 0x2DFC6 }, { 0x16FE4, 0x2DFC9 }, { 0x16FF0, 0x2DFE3 },
        { 0x17000, 0x30FEE }, { 0x18800, 0x319AA }, { 0x18D00, 0x31A10 }, { 0x1AFF0, 0x35FE6 },
        { 0x1AFF5, 0x35FF6 }, { 0x1AFFD, 0x35FFC }, { 0x1B000, 0x36244 }, { 0x1B132, 0x36264 },
        { 0x1B150, 0x362A4 }, { 0x1B155, 0x362AA }, { 0x1B164, 0x362CE }, { 0x1B170, 0x365F6 },
        { 0x1BC00, 0x378D4 }, { 0x1BC70, 0x378F8 }, { 0x1BC80, 0x37910 }, { 0x1BC90, 0x37932 },
        { 0x1BC9D, 0x3793D }, { 0x1CF00, 0x39E5B }, { 0x1CF30, 0x39E8D }, { 0x1D165, 0x3A2D3 },
        { 0x1D16D, 0x3A2E5 }, { 0x1D17B, 0x3A305 }, { 0x1D185, 0x3A317 }, { 0x1D1AA, 0x3A35B },
        { 0x1D242, 0x3A489 }, { 0x1D400, 0x3A8A8 }, { 0x1D456, 0x3A938 }, { 0x1D49E, 0x3A93E },
        { 0x1D4A2, 0x3A944 }, { 0x1D4A5, 0x3A94C }, { 0x1D4A9, 0x3A958 }, { 0x1D4AE, 0x3A972 },
        { 0x1D4BB, 0x3A976 }, { 0x1D4BD, 0x3A986 }, { 0x1D4C5, 0x3AA0A }, { 0x1D507, 0x3AA14 },
        { 0x1D50D, 0x3AA28 }, { 0x1D516, 0x3AA38 }, { 0x1D51E, 0x3AA72 }, { 0x1D53B, 0x3AA7C },
        { 0x1D540, 0x3AA88 }, { 0x1D546, 0x3AA8C }, { 0x1D54A, 0x3AAA0 }, { 0x1D552, 0x3AD4A },
        { 0x1D6A8, 0x3AD80 }, { 0x1D6C2, 0x3ADB4 }, { 0x1D6DC, 0x3ADF4 }, { 0x1D6FC, 0x3AE28 },
        { 0x1D716, 0x3AE68 }, { 0x1D736, 0x3AE9C }, { 0x1D750, 0x3AEDC }, { 0x1D770, 0x3AF10 },
        { 0x1D78A, 0x3AF50 }, { 0x1D7AA, 0x3AF84 }, { 0x1D7C4, 0x3AF96 }, { 0x1D7CE, 0x3AFFF },
        { 0x1DA00, 0x3B46D }, { 0x1DA3B, 0x3B4D9 }, { 0x1DA75, 0x3B4EB }, { 0x1DA84, 0x3B509 },
        { 0x1DA9B, 0x3B53F }, { 0x1DAA1, 0x3B55F }, { 0x1DF00, 0x3BE3C }, { 0x1DF25, 0x3BE54 },
        { 0x1E000, 0x3C00D }, { 0x1E008, 0x3C031 }, { 0x1E01B, 0x3C043 }, { 0x1E023, 0x3C049 },
        { 0x1E026, 0x3C055 }, { 0x1E030, 0x3C0DA }, { 0x1E08F, 0x3C11F }, { 0x1E100, 0x3C258 },
        { 0x1E130, 0x3C26D }, { 0x1E137, 0x3C27A }, { 0x1E140, 0x3C293 }, { 0x1E14E, 0x3C29C },
        { 0x1E290, 0x3C55A }, { 0x1E2AE, 0x3C55D }, { 0x1E2C0, 0x3C5D6 }, { 0x1E2EC, 0x3C5F3 },
        { 0x1E4D0, 0x3C9D6 }, { 0x1E4EC, 0x3C9F3 }, { 0x1E7E0, 0x3CFCC }, { 0x1E7E8, 0x3CFD6 },
        { 0x1E7ED, 0x3CFDC }, { 0x1E7F0, 0x3CFFC }, { 0x1E800, 0x3D188 }, { 0x1E8D0, 0x3D1AD },
        { 0x1E900, 0x3D286 }, { 0x1E944, 0x3D295 }, { 0x1E94B, 0x3D296 }, { 0x1E950, 0x3D2B3 },
        { 0x1EE00, 0x3DC06 }, { 0x1EE05, 0x3DC3E }, { 0x1EE21, 0x3DC44 }, { 0x1EE24, 0x3DC48 },
        { 0x1EE27, 0x3DC4E }, { 0x1EE29, 0x3DC64 }, { 0x1EE34, 0x3DC6E }, { 0x1EE39, 0x3DC72 },
        { 0x1EE3B, 0x3DC76 }, { 0x1EE42, 0x3DC84 }, { 0x1EE47, 0x3DC8E }, { 0x1EE49, 0x3DC92 },
        { 0x1EE4B, 0x3DC96 }, { 0x1EE4D, 0x3DC9E }, { 0x1EE51, 0x3DCA4 }, { 0x1EE54, 0x3DCA8 },
        { 0x1EE57, 0x3DCAE }, { 0x1EE59, 0x3DCB2 }, { 0x1EE5B, 0x3DCB6 }, { 0x1EE5D, 0x3DCBA },
        { 0x1EE5F, 0x3DCBE }, { 0x1EE61, 0x3DCC4 }, { 0x1EE64, 0x3DCC8 }, { 0x1EE67, 0x3DCD4 },
        { 0x1EE6C, 0x3DCE4 }, { 0x1EE74, 0x3DCEE }, { 0x1EE79, 0x3DCF8 }, { 0x1EE7E, 0x3DCFC },
        { 0x1EE80, 0x3DD12 }, { 0x1EE8B, 0x3DD36 }, { 0x1EEA1, 0x3DD46 }, { 0x1EEA5, 0x3DD52 },
        { 0x1EEAB, 0x3DD76 }, { 0x1FBF0, 0x3F7F3 }, { 0x20000, 0x54DBE }, { 0x2A700, 0x56E72 },
        { 0x2B740, 0x5703A }, { 0x2B820, 0x59D42 }, { 0x2CEB0, 0x5D7C0 }, { 0x2F800, 0x5F43A },
        { 0x30000, 0x62694 }, { 0x31350, 0x6475E }, { 0xE0100, 0x1C03DF },
    };

    size_t begin = 0, end = sizeof( ranges ) / sizeof( ranges[0] );
    while ( begin < end )
    {
        const size_t middle = begin + ( end - begin ) / 2;
        if ( code < ranges[middle][0] )
            end = middle;
        else if ( code > ( ranges[middle][1] >> 1 ) )
            begin = middle + 1;
        else
            return !first || !( ranges[middle][1] & 1 );
    }
    return false;
}

#endif // __JJSON_HEADER
