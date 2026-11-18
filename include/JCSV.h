#ifndef __JCSV_HEADER
#define __JCSV_HEADER



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
#include <cstddef>
#include <cstdio>
#include <limits>
#include <locale>
#include <memory>
#include <sstream>
#include <string>
#include <string_view>
#include <system_error>
#include <type_traits>
#include <utility>
#include <vector>

class JCSV
{
public:
    struct Options
    {
        char delimiter = ',';
        char comment = '\0';
        bool header = false;
        bool allow_lf = false;
        bool allow_bom = false;
        bool allow_ragged_rows = false;
        bool allow_control_characters = false;
    };

    struct Error
    {
        const char* message = nullptr;
        size_t offset = 0;
        size_t line = 0;
        size_t column = 0;
    };

    JCSV() = default;
    explicit JCSV( Options options ) : options_( options ) {}

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
        JCSV next( options_ );
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
        if ( !valid_ || path.find( '\0' ) != npos )
            return false;
        std::FILE* file = std::fopen( path.c_str(), "wb" );
        if ( !file )
            return false;
        const size_t written = std::fwrite( text_.data(), 1, text_.size(), file );
        return std::fclose( file ) == 0 && written == text_.size();
    }

    const std::string& to_string() const { return text_; }
    const Error& error() const { return error_; }
    // Row indices exclude the optional header and all comment lines.
    size_t row_count() const { return rows_.size() - header_rows(); }
    size_t column_count() const { return rows_.empty() ? 0 : rows_.front().count; }

    size_t column_count( size_t row ) const
    {
        return row < row_count() ? rows_[row + header_rows()].count : 0;
    }

    std::string column_name( size_t column ) const
    {
        return header_rows() && column < column_count() ? decode( cells_[column] ) : std::string{};
    }

    size_t find_column( std::string_view name ) const
    {
        const auto it = std::lower_bound( columns_.begin(), columns_.end(), name,
            []( const Column& column, std::string_view key ) { return std::string_view( column.name ) < key; } );
        return it != columns_.end() && it->name == name ? it->index : npos;
    }

    bool has( size_t row, size_t column ) const { return find_cell( row, column ) != nullptr; }

    bool get_into( size_t row, size_t column, std::string& out ) const
    {
        const Cell* cell = find_cell( row, column );
        if ( !cell )
            return false;
        decode_into( *cell, out );
        return true;
    }

    std::string get( size_t row, size_t column, std::string_view fallback = {} ) const
    {
        const Cell* cell = find_cell( row, column );
        return cell ? decode( *cell ) : std::string( fallback );
    }

    std::string get( size_t row, std::string_view column, std::string_view fallback = {} ) const
    {
        return get( row, find_column( column ), fallback );
    }

    template <typename T>
    T get_as( size_t row, size_t column, T fallback = T{} ) const
    {
        const Cell* cell = find_cell( row, column );
        if ( !cell )
            return fallback;

        std::string decoded;
        std::string_view value = content( *cell );
        if ( cell->escaped )
        {
            decoded = decode( *cell );
            value = decoded;
        }

        if constexpr ( std::is_same_v<T, std::string> )
            return std::string( value );
        else if constexpr ( std::is_same_v<T, bool> )
        {
            if ( value == "true" || value == "1" )
                return true;
            if ( value == "false" || value == "0" )
                return false;
            return fallback;
        }
        else if constexpr ( std::is_integral_v<T> )
        {
            std::conditional_t<std::is_signed_v<T>, std::make_signed_t<T>, std::make_unsigned_t<T>> out{};
            const auto result = std::from_chars( value.data(), value.data() + value.size(), out );
            return result.ec == std::errc{} && result.ptr == value.data() + value.size() ? static_cast<T>( out ) : fallback;
        }
        else
        {
            T out{};
            if constexpr ( std::is_floating_point_v<T> )
                return read_decimal( value, out, 0 ) ? out : fallback;
            else
                return read_decimal( value, out, 0L ) ? out : fallback;
        }
    }

    template <typename T>
    T get_as( size_t row, std::string_view column, T fallback = T{} ) const
    {
        return get_as<T>( row, find_column( column ), fallback );
    }

    bool set( size_t row, size_t column, std::string_view value )
    {
        const size_t record = row + header_rows();
        if ( row >= row_count() || column >= rows_[record].count )
            return false;

        const size_t index = rows_[record].first + column;
        Cell& cell = cells_[index];
        bool quoted = cell.quoted || ( rows_[record].count == 1 && value.empty() );
        quoted = quoted || ( column == 0 && needs_first_field_quotes( value ) );
        bool escaped = false;
        for ( unsigned char ch : value )
        {
            if ( ch == '\r' || ch == '\n' )
                quoted = true;
            else if ( !valid_field_character( ch ) )
                return fail( "Control character in field", cell.begin );
            else if ( ch == '"' )
                quoted = escaped = true;
            else if ( ch == static_cast<unsigned char>( options_.delimiter ) )
                quoted = true;
        }

        std::string storage;
        std::string_view encoded = value;
        if ( quoted )
        {
            storage = quote( value );
            encoded = storage;
        }

        const size_t old_end = cell.end;
        const size_t old_size = old_end - cell.begin;
        const size_t new_size = encoded.size();
        text_.replace( cell.begin, old_size, encoded );
        cell.quoted = quoted;
        cell.escaped = escaped;

        if ( old_size != new_size )
        {
            // A cell edit preserves record boundaries, even when it adds quoted newlines.
            const auto shift = [=]( size_t& offset ) { offset = offset - old_size + new_size; };
            shift( cell.end );
            shift( rows_[record].end );
            for ( size_t i = index + 1; i < cells_.size(); ++i )
            {
                shift( cells_[i].begin );
                shift( cells_[i].end );
            }
            for ( size_t i = record + 1; i < rows_.size(); ++i )
            {
                shift( rows_[i].begin );
                shift( rows_[i].end );
            }
            for ( Comment& comment : comments_ )
            {
                if ( comment.begin >= old_end )
                {
                    shift( comment.begin );
                    shift( comment.end );
                    shift( comment.after );
                }
            }
        }
        error_ = {};
        return true;
    }

    bool set( size_t row, std::string_view column, std::string_view value )
    {
        return set( row, find_column( column ), value );
    }

    template <typename T>
    bool set_as( size_t row, size_t column, const T& value )
    {
        if constexpr ( std::is_same_v<T, bool> )
            return set( row, column, value ? "true" : "false" );
        else if constexpr ( std::is_integral_v<T> )
        {
            char buffer[std::numeric_limits<T>::digits10 + 4];
            const auto result = std::to_chars( buffer, buffer + sizeof( buffer ), +value );
            return result.ec == std::errc{} && set( row, column, std::string_view( buffer, result.ptr - buffer ) );
        }
        else if constexpr ( std::is_convertible_v<const T&, std::string_view> )
            return set( row, column, value );
        else if constexpr ( std::is_floating_point_v<T> )
            return set_number( row, column, value, 0 );
        else
            return set_number( row, column, value, 0L );
    }

    template <typename T>
    bool set_as( size_t row, std::string_view column, const T& value )
    {
        return set_as( row, find_column( column ), value );
    }

    bool set_column_name( size_t column, std::string_view name )
    {
        if ( !header_rows() || column >= column_count() )
            return false;
        const Cell& cell = cells_[column];
        const bool quoted = cell.quoted || ( column_count() == 1 && name.empty() );
        return replace( cell.begin, cell.end, encode( name, column == 0, quoted ) );
    }

    bool insert_row( size_t row, const std::vector<std::string_view>& values )
    {
        if ( row > row_count() || values.empty() || ( options_.header && !header_rows() ) )
            return false;

        std::string line = encode_row( values );
        if ( row < row_count() )
        {
            const size_t at = rows_[row + header_rows()].begin;
            line += newline();
            return replace( at, at, line );
        }

        if ( text_.size() > bom_ && text_.back() != '\n' )
            line.insert( 0, newline() );
        else if ( text_.size() > bom_ )
            line += newline();

        return replace( text_.size(), text_.size(), line );
    }

    bool push( const std::vector<std::string_view>& values )
    {
        return insert_row( row_count(), values );
    }

    bool set_header( const std::vector<std::string_view>& values )
    {
        if ( !options_.header || values.empty() )
            return false;
        if ( header_rows() )
        {
            const Row& row = rows_.front();
            return replace( row.begin, cells_[row.first + row.count - 1].end, encode_row( values ) );
        }

        std::string line = encode_row( values );
        if ( text_.size() > bom_ )
            line += newline();
        return replace( bom_, bom_, line );
    }

    bool remove( size_t row )
    {
        if ( row >= row_count() )
            return false;
        const Row& entry = rows_[row + header_rows()];
        return replace( entry.begin, entry.end, {} );
    }

    size_t comment_count() const { return comments_.size(); }

    // The view excludes the marker and is invalidated by a successful parse or edit.
    std::string_view get_comment( size_t index ) const
    {
        if ( index >= comments_.size() )
            return {};
        const Comment& comment = comments_[index];
        return std::string_view( text_ ).substr( comment.begin + 1, comment.end - comment.begin - 1 );
    }

    bool set_comment( size_t index, std::string_view value )
    {
        if ( index >= comments_.size() || value.find_first_of( "\r\n" ) != std::string_view::npos )
            return false;
        const Comment& comment = comments_[index];
        return replace( comment.begin + 1, comment.end, value );
    }

    bool insert_comment( size_t row, std::string_view value )
    {
        if ( !options_.comment || row > row_count() || value.find_first_of( "\r\n" ) != std::string_view::npos )
            return false;

        std::string line( 1, options_.comment );
        line += value;
        if ( row < row_count() )
        {
            const size_t at = rows_[row + header_rows()].begin;
            line += newline();
            return replace( at, at, line );
        }

        if ( text_.size() > bom_ && text_.back() != '\n' )
            line.insert( 0, newline() );
        else if ( text_.size() > bom_ )
            line += newline();
        return replace( text_.size(), text_.size(), line );
    }

    bool remove_comment( size_t index )
    {
        if ( index >= comments_.size() )
            return false;
        const Comment& comment = comments_[index];
        return replace( comment.begin, comment.after, {} );
    }

    static constexpr size_t npos = static_cast<size_t>( -1 );

private:
    struct Cell
    {
        size_t begin, end;
        bool quoted, escaped;
    };

    struct Row
    {
        size_t begin, end, first, count;
    };

    struct Comment
    {
        size_t begin, end, after;
    };

    struct Column
    {
        std::string name;
        size_t index;
    };

    // Older NDK libc++ versions provide charconv for integers only.
    template <typename T>
    static auto read_decimal( std::string_view value, T& out, int )
        -> decltype( std::from_chars( value.data(), value.data(), out ), bool{} )
    {
        std::string_view number = value;
        if ( !number.empty() && ( number.front() == '+' || number.front() == '-' ) )
            number.remove_prefix( 1 );
        if ( number.empty() || ( ( number.front() < '0' || number.front() > '9' ) && number.front() != '.' ) )
            return false;

        const char* end = value.data() + value.size();
        const char* begin = value.data() + ( value.front() == '+' ? 1 : 0 );
        const auto result = std::from_chars( begin, end, out );
        if ( result.ec == std::errc::result_out_of_range )
            return read_decimal( value, out, 0L );
        return result.ec == std::errc{} && result.ptr == end;
    }

    template <typename T>
    static bool read_decimal( std::string_view value, T& out, long )
    {
        if constexpr ( std::is_floating_point_v<T> )
        {
            if ( !is_decimal( value ) )
                return false;
        }

        std::istringstream stream{ std::string( value ) };
        stream.imbue( std::locale::classic() );
        stream >> std::noskipws >> out;
        if ( !stream.eof() )
            return false;
        if constexpr ( std::is_floating_point_v<T> )
        {
            // A valid decimal can underflow even though the rounded value is usable.
            const T minimum = std::numeric_limits<T>::min();
            const T maximum = std::numeric_limits<T>::max();
            return out >= -maximum && out <= maximum && ( stream || ( out >= -minimum && out <= minimum ) );
        }
        else
            return static_cast<bool>( stream );
    }

    static bool is_decimal( std::string_view value )
    {
        size_t pos = 0;
        const auto sign = [&] {
            if ( pos < value.size() && ( value[pos] == '+' || value[pos] == '-' ) )
                ++pos;
        };
        const auto digits = [&] {
            const size_t begin = pos;
            while ( pos < value.size() && value[pos] >= '0' && value[pos] <= '9' )
                ++pos;
            return pos != begin;
        };

        sign();
        bool seen_digit = digits();
        if ( pos < value.size() && value[pos] == '.' )
        {
            ++pos;
            seen_digit = digits() || seen_digit;
        }
        if ( pos < value.size() && ( value[pos] == 'e' || value[pos] == 'E' ) )
        {
            ++pos;
            sign();
            if ( !digits() )
                return false;
        }
        return seen_digit && pos == value.size();
    }

    template <typename T>
    auto set_number( size_t row, size_t column, T value, int )
        -> decltype( std::to_chars( static_cast<char*>( nullptr ), static_cast<char*>( nullptr ),
            value, std::chars_format::general, 1 ), bool{} )
    {
        char buffer[std::numeric_limits<T>::max_digits10 + 16];
        const auto result = std::to_chars( buffer, buffer + sizeof( buffer ), value, std::chars_format::general,
            std::numeric_limits<T>::max_digits10 );
        return result.ec == std::errc{} && set( row, column, std::string_view( buffer, result.ptr - buffer ) );
    }

    template <typename T>
    bool set_number( size_t row, size_t column, const T& value, long )
    {
        std::ostringstream stream;
        stream.imbue( std::locale::classic() );
        if constexpr ( std::is_floating_point_v<T> )
            stream.precision( std::numeric_limits<T>::max_digits10 );
        stream << value;
        return stream && set( row, column, stream.str() );
    }

    size_t header_rows() const { return options_.header && !rows_.empty() ? 1 : 0; }

    const Cell* find_cell( size_t row, size_t column ) const
    {
        if ( row >= row_count() )
            return nullptr;
        const Row& entry = rows_[row + header_rows()];
        return column < entry.count ? &cells_[entry.first + column] : nullptr;
    }

    std::string_view content( const Cell& cell ) const
    {
        const size_t quotes = cell.quoted ? 1 : 0;
        return std::string_view( text_ ).substr( cell.begin + quotes, cell.end - cell.begin - 2 * quotes );
    }

    void decode_into( const Cell& cell, std::string& out ) const
    {
        const std::string_view value = content( cell );
        if ( !cell.escaped )
        {
            out = value;
            return;
        }

        out.clear();
        out.reserve( value.size() );
        for ( size_t i = 0; i < value.size(); ++i )
        {
            out += value[i];
            if ( value[i] == '"' )
                ++i;
        }
    }

    std::string decode( const Cell& cell ) const
    {
        std::string out;
        decode_into( cell, out );
        return out;
    }

    bool needs_first_field_quotes( std::string_view value ) const
    {
        return ( options_.comment && !value.empty() && value.front() == options_.comment ) ||
            value.substr( 0, 3 ) == "\xEF\xBB\xBF";
    }

    static std::string quote( std::string_view value )
    {
        std::string out;
        out.reserve( value.size() + 2 );
        out += '"';
        for ( char ch : value )
        {
            out += ch;
            if ( ch == '"' )
                out += ch;
        }
        out += '"';
        return out;
    }

    std::string encode( std::string_view value, bool first, bool quoted = false ) const
    {
        quoted = quoted || ( first && needs_first_field_quotes( value ) ) ||
            value.find( options_.delimiter ) != std::string_view::npos ||
            value.find_first_of( "\"\r\n" ) != std::string_view::npos;
        return quoted ? quote( value ) : std::string( value );
    }

    std::string encode_row( const std::vector<std::string_view>& values ) const
    {
        std::string out;
        for ( size_t i = 0; i < values.size(); ++i )
        {
            if ( i )
                out += options_.delimiter;
            // A single empty field needs quotes to distinguish it from an empty file.
            out += encode( values[i], i == 0, values.size() == 1 && values[i].empty() );
        }
        return out;
    }

    bool replace( size_t begin, size_t end, std::string_view value )
    {
        std::string text = text_;
        text.replace( begin, end - begin, value );
        JCSV next( options_ );
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
        for ( size_t i = bom_; i < offset; ++i )
        {
            if ( text_[i] == '\n' || ( text_[i] == '\r' && ( i + 1 == offset || text_[i + 1] != '\n' ) ) )
            {
                ++error_.line;
                error_.column = 1;
            }
            else
                ++error_.column;
        }
        return false;
    }

    std::string_view newline() const { return lf_ ? "\n" : "\r\n"; }

    bool scan_newline( size_t& pos )
    {
        if ( pos == text_.size() )
            return true;

        const bool lf = text_[pos] == '\n';
        if ( lf ? !options_.allow_lf : pos + 1 == text_.size() || text_[pos + 1] != '\n' )
            return fail( "Invalid record ending", pos );
        if ( !saw_newline_ )
        {
            lf_ = lf;
            saw_newline_ = true;
        }
        pos += lf ? 1 : 2;
        return true;
    }

    bool valid_field_character( unsigned char ch ) const
    {
        return options_.allow_control_characters || ( ch >= 0x20 && ch != 0x7F ) || ch == options_.delimiter;
    }

    bool scan_cell( size_t& pos )
    {
        const size_t begin = pos;
        const bool quoted = pos < text_.size() && text_[pos] == '"';
        bool escaped = false;
        if ( quoted )
        {
            ++pos;
            for ( ;; )
            {
                const size_t quote = text_.find( '"', pos );
                if ( quote == npos )
                    return fail( "Unterminated quoted field", begin );

                if ( !options_.allow_control_characters )
                {
                    for ( size_t i = pos; i < quote; ++i )
                    {
                        const char ch = text_[i];
                        if ( ch != '\r' && ch != '\n' && !valid_field_character( static_cast<unsigned char>( ch ) ) )
                            return fail( "Control character in field", i );
                    }
                }
                pos = quote + 1;
                if ( pos == text_.size() || text_[pos] != '"' )
                    break;
                ++pos;
                escaped = true;
            }

            if ( pos < text_.size() && text_[pos] != options_.delimiter && text_[pos] != '\r' && text_[pos] != '\n' )
                return fail( "Unexpected character after quoted field", pos );
        }
        else
        {
            while ( pos < text_.size() && text_[pos] != options_.delimiter && text_[pos] != '\r' && text_[pos] != '\n' )
            {
                if ( text_[pos] == '"' )
                    return fail( "Quote in unquoted field", pos );
                if ( !valid_field_character( static_cast<unsigned char>( text_[pos] ) ) )
                    return fail( "Control character in field", pos );
                ++pos;
            }
        }

        cells_.push_back( { begin, pos, quoted, escaped } );
        return true;
    }

    bool scan()
    {
        const char delimiter = options_.delimiter;
        const char comment = options_.comment;
        if ( !delimiter || delimiter == '"' || delimiter == '\r' || delimiter == '\n' ||
            comment == delimiter || comment == '"' || comment == '\r' || comment == '\n' )
            return fail( "Invalid delimiter or comment marker", 0 );

        if ( std::string_view( text_ ).substr( 0, 3 ) == "\xEF\xBB\xBF" )
        {
            if ( !options_.allow_bom )
                return fail( "Unexpected UTF-8 BOM", 0 );
            bom_ = 3;
        }

        size_t pos = bom_;
        while ( pos < text_.size() )
        {
            const size_t begin = pos;
            if ( comment && text_[pos] == comment )
            {
                pos = text_.find_first_of( "\r\n", pos );
                if ( pos == npos )
                    pos = text_.size();
                const size_t end = pos;
                if ( !scan_newline( pos ) )
                    return false;
                comments_.push_back( { begin, end, pos } );
                continue;
            }

            const size_t first = cells_.size();
            for ( ;; )
            {
                if ( !scan_cell( pos ) )
                    return false;
                if ( pos == text_.size() || text_[pos] != delimiter )
                    break;
                ++pos;
            }

            if ( !scan_newline( pos ) )
                return false;
            const size_t count = cells_.size() - first;
            if ( !options_.allow_ragged_rows && !rows_.empty() && count != rows_.front().count )
                return fail( "Inconsistent column count", begin );
            rows_.push_back( { begin, pos, first, count } );
        }

        if ( header_rows() )
        {
            columns_.reserve( column_count() );
            for ( size_t i = 0; i < column_count(); ++i )
                columns_.push_back( { decode( cells_[i] ), i } );
            std::sort( columns_.begin(), columns_.end(), []( const Column& a, const Column& b ) {
                return a.name == b.name ? a.index < b.index : a.name < b.name;
            } );
        }
        valid_ = true;
        return true;
    }

    Options options_;
    Error error_;
    std::string source_path_;
    std::string text_;
    std::vector<Cell> cells_;
    std::vector<Row> rows_;
    std::vector<Comment> comments_;
    std::vector<Column> columns_;
    size_t bom_ = 0;
    bool lf_ = false;
    bool saw_newline_ = false;
    bool valid_ = false;
};

#endif // __JCSV_HEADER
