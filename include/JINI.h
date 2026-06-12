#ifndef __JINI_HEADER
#define __JINI_HEADER



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
#include <cctype>
#include <charconv>
#include <cstddef>
#include <fstream>
#include <sstream>
#include <string>
#include <string_view>
#include <system_error>
#include <type_traits>
#include <unordered_map>
#include <utility>
#include <vector>

class JINI
{
public:
    bool load( const std::string& path )
    {
        clear();
        source_path_ = path;

        std::ifstream file( path, std::ios::binary );
        if ( !file )
            return false;

        std::ostringstream buffer;
        buffer << file.rdbuf();
        split_lines( buffer.str() );
        rebuild_index();
        return true;
    }

    void parse( const std::string& text )
    {
        clear();
        split_lines( text );
        rebuild_index();
    }

    bool save() const
    {
        return !source_path_.empty() && save_as( source_path_ );
    }

    bool save_as( const std::string& path ) const
    {
        std::ofstream file( path, std::ios::binary | std::ios::trunc );
        if ( !file )
            return false;

        for ( const Line& line : lines_ )
        {
            file.write( line.text.data(), static_cast<std::streamsize>( line.text.size() ) );
            file.write( line.newline.data(), static_cast<std::streamsize>( line.newline.size() ) );
        }

        return static_cast<bool>( file );
    }

    std::string to_string() const
    {
        std::string out;
        size_t size = 0;
        for ( const Line& line : lines_ )
            size += line.text.size() + line.newline.size();

        out.reserve( size );
        for ( const Line& line : lines_ )
        {
            out += line.text;
            out += line.newline;
        }
        return out;
    }

    bool has( std::string_view section, std::string_view key ) const
    {
        return find_key( section, key ) != nullptr;
    }

    bool has_section( std::string_view section ) const
    {
        return find_section_line( section ) != npos;
    }

    std::string get( std::string_view section, std::string_view key, std::string_view fallback = {} ) const
    {
        const KeyRef* ref = find_key( section, key );
        if ( !ref )
            return std::string( fallback );

        const Line& line = lines_[ ref->line ];
        return line.text.substr( ref->value_begin, ref->value_end - ref->value_begin );
    }

    bool get_into( std::string_view section, std::string_view key, std::string& out ) const
    {
        const KeyRef* ref = find_key( section, key );
        if ( !ref )
            return false;

        const Line& line = lines_[ ref->line ];
        out.assign( line.text, ref->value_begin, ref->value_end - ref->value_begin );
        return true;
    }

    bool set( std::string_view section, std::string_view key, std::string_view value )
    {
        KeyRef* ref = find_key( section, key );
        if ( ref )
        {
            Line& line = lines_[ ref->line ];
            std::string replacement( value.data(), value.size() );
            if ( ref->value_begin == ref->value_end && ref->value_end < line.text.size() &&
                ( line.text[ref->value_end] == ';' || line.text[ref->value_end] == '#' ) &&
                !replacement.empty() && !is_space( replacement.back() ) )
            {
                replacement.push_back( ' ' );
            }

            line.text.replace( ref->value_begin, ref->value_end - ref->value_begin, replacement );
            ref->value_end = ref->value_begin + value.size();
            line.value_end = ref->value_end;
            normalize_section_spacing();
            rebuild_index();
            return true;
        }

        insert_key( section, key, value );
        return false;
    }

    void prettify_sections()
    {
        normalize_section_spacing();
        rebuild_index();
    }

    bool set_key_comment( std::string_view section, std::string_view key, std::string_view comment, char marker = ';' )
    {
        const KeyRef* ref = find_key( section, key );
        if ( !ref )
            return false;

        replace_leading_comment( ref->line, comment, marker );
        normalize_section_spacing();
        rebuild_index();
        return true;
    }

    bool get_key_comment( std::string_view section, std::string_view key, std::string& out ) const
    {
        const KeyRef* ref = find_key( section, key );
        if ( !ref )
            return false;

        out = leading_comment( ref->line );
        return leading_comment_begin( ref->line ) != ref->line;
    }

    std::string get_key_comment( std::string_view section, std::string_view key ) const
    {
        std::string out;
        get_key_comment( section, key, out );
        return out;
    }

    bool set_section_comment( std::string_view section, std::string_view comment, char marker = ';' )
    {
        const size_t line = find_section_line( section );
        if ( line == npos )
            return false;

        replace_leading_comment( line, comment, marker );
        normalize_section_spacing();
        rebuild_index();
        return true;
    }

    bool get_section_comment( std::string_view section, std::string& out ) const
    {
        const size_t line = find_section_line( section );
        if ( line == npos )
            return false;

        out = leading_comment( line );
        return leading_comment_begin( line ) != line;
    }

    std::string get_section_comment( std::string_view section ) const
    {
        std::string out;
        get_section_comment( section, out );
        return out;
    }

    template <typename T>
    T get_as( std::string_view section, std::string_view key, T fallback = T{} ) const
    {
        const std::string value = get( section, key );
        if ( value.empty() && !has( section, key ) )
            return fallback;

        if constexpr ( std::is_same_v<T, bool> )
        {
            std::string lower = value;
            std::transform( lower.begin(), lower.end(), lower.begin(), []( unsigned char ch ) {
                return static_cast<char>( std::tolower( ch ) );
            } );

            if ( lower == "1" || lower == "true" || lower == "yes" || lower == "on" )
                return true;
            if ( lower == "0" || lower == "false" || lower == "no" || lower == "off" )
                return false;
            return fallback;
        }
        else if constexpr ( std::is_integral_v<T> )
        {
            T out{};
            const char* begin = value.data();
            const char* end = begin + value.size();
            const auto result = std::from_chars( begin, end, out );
            return result.ec == std::errc{} && result.ptr == end ? out : fallback;
        }
        else
        {
            std::istringstream stream( value );
            T out{};
            stream >> out;
            return stream && stream.eof() ? out : fallback;
        }
    }

    template <typename T>
    bool set_as( std::string_view section, std::string_view key, const T& value )
    {
        if constexpr ( std::is_same_v<T, bool> )
        {
            return set( section, key, value ? "true" : "false" );
        }
        else
        {
            std::ostringstream stream;
            stream << value;
            return set( section, key, stream.str() );
        }
    }

private:
    enum class Kind
    {
        Other,
        Section,
        Key
    };

    struct Line
    {
        std::string text;
        std::string newline;
        Kind kind = Kind::Other;
        std::string section;
        std::string key;
        size_t value_begin = 0;
        size_t value_end = 0;
    };

    struct KeyRef
    {
        size_t line = 0;
        size_t value_begin = 0;
        size_t value_end = 0;
    };

    struct ValueSpan
    {
        size_t begin = 0;
        size_t end = 0;
    };

    static constexpr size_t npos = static_cast<size_t>( -1 );

    void clear()
    {
        lines_.clear();
        keys_.clear();
        source_path_.clear();
        newline_style_.clear();
    }

    static bool is_space( char ch )
    {
        return ch == ' ' || ch == '\t' || ch == '\f' || ch == '\v';
    }

    static size_t ltrim_pos( std::string_view text )
    {
        size_t pos = 0;
        while ( pos < text.size() && is_space( text[pos] ) )
            ++pos;
        return pos;
    }

    static size_t rtrim_pos( std::string_view text, size_t end )
    {
        while ( end > 0 && is_space( text[end - 1] ) )
            --end;
        return end;
    }

    static std::string trimmed_copy( std::string_view text )
    {
        const size_t begin = ltrim_pos( text );
        const size_t end = rtrim_pos( text, text.size() );
        if ( end <= begin )
            return {};
        return std::string( text.substr( begin, end - begin ) );
    }

    static bool is_comment_after( std::string_view text, size_t pos )
    {
        for ( size_t i = pos; i < text.size(); ++i )
        {
            if ( is_space( text[i] ) )
                continue;
            return text[i] == ';' || text[i] == '#';
        }
        return true;
    }

    static bool is_blank_or_comment( std::string_view text )
    {
        const size_t first = ltrim_pos( text );
        return first >= text.size() || text[first] == ';' || text[first] == '#';
    }

    static bool is_comment_line( std::string_view text )
    {
        const size_t first = ltrim_pos( text );
        return first < text.size() && ( text[first] == ';' || text[first] == '#' );
    }

    static char safe_comment_marker( char marker )
    {
        return marker == '#' ? '#' : ';';
    }

    static std::string comment_body( std::string_view text )
    {
        size_t pos = ltrim_pos( text );
        if ( pos >= text.size() || ( text[pos] != ';' && text[pos] != '#' ) )
            return {};

        ++pos;
        if ( pos < text.size() && is_space( text[pos] ) )
            ++pos;

        return std::string( text.substr( pos ) );
    }

    static std::string comment_line_text( std::string_view text, char marker )
    {
        std::string stripped;
        const size_t first = ltrim_pos( text );
        if ( first < text.size() && ( text[first] == ';' || text[first] == '#' ) )
        {
            stripped = comment_body( text );
            text = stripped;
        }

        std::string out;
        out.push_back( safe_comment_marker( marker ) );
        if ( !text.empty() )
        {
            out.push_back( ' ' );
            out.append( text.data(), text.size() );
        }
        return out;
    }

    static size_t inline_comment_pos( std::string_view text, size_t begin )
    {
        for ( size_t i = begin; i < text.size(); ++i )
        {
            if ( text[i] == ';' || text[i] == '#' )
                return i;
        }
        return std::string_view::npos;
    }

    static ValueSpan parse_value_span( std::string_view text, size_t equals )
    {
        size_t begin = equals + 1;
        while ( begin < text.size() && is_space( text[begin] ) )
            ++begin;

        const size_t comment = inline_comment_pos( text, begin );
        const size_t tail = comment == std::string_view::npos ? text.size() : comment;
        size_t end = rtrim_pos( text, tail );

        if ( end < begin )
        {
            if ( comment != std::string_view::npos )
            {
                begin = comment;
                end = comment;
            }
            else
            {
                begin = tail;
                end = tail;
            }
        }

        return ValueSpan{ begin, end };
    }

    void split_lines( const std::string& text )
    {
        size_t pos = 0;
        while ( pos < text.size() )
        {
            size_t end = pos;
            while ( end < text.size() && text[end] != '\r' && text[end] != '\n' )
                ++end;

            Line line;
            line.text.assign( text, pos, end - pos );

            if ( end < text.size() )
            {
                if ( text[end] == '\r' && end + 1 < text.size() && text[end + 1] == '\n' )
                {
                    line.newline = "\r\n";
                    end += 2;
                }
                else
                {
                    line.newline.assign( 1, text[end] );
                    ++end;
                }

                if ( newline_style_.empty() )
                    newline_style_ = line.newline;
            }

            lines_.push_back( std::move( line ) );
            pos = end;
        }
    }

    void rebuild_index()
    {
        keys_.clear();
        std::string current_section;

        for ( size_t i = 0; i < lines_.size(); ++i )
        {
            Line& line = lines_[i];
            line.kind = Kind::Other;
            line.section.clear();
            line.key.clear();
            line.value_begin = 0;
            line.value_end = 0;

            const std::string_view text( line.text );
            const size_t first = ltrim_pos( text );
            if ( first >= text.size() )
                continue;

            if ( text[first] == ';' || text[first] == '#' )
                continue;

            if ( text[first] == '[' )
            {
                const size_t close = text.find( ']', first + 1 );
                if ( close == std::string_view::npos || close == first + 1 || !is_comment_after( text, close + 1 ) )
                    continue;

                current_section = std::string( text.substr( first + 1, close - first - 1 ) );
                line.kind = Kind::Section;
                line.section = current_section;
                continue;
            }

            if ( current_section.empty() )
                continue;

            const size_t equals = text.find( '=', first );
            if ( equals == std::string_view::npos )
                continue;

            const size_t key_end = rtrim_pos( text, equals );
            if ( key_end <= first )
                continue;

            const std::string key = trimmed_copy( text.substr( first, equals - first ) );
            if ( key.empty() )
                continue;

            const ValueSpan value = parse_value_span( text, equals );

            line.kind = Kind::Key;
            line.section = current_section;
            line.key = key;
            line.value_begin = value.begin;
            line.value_end = value.end;

            keys_[make_key( current_section, key )] = KeyRef{ i, value.begin, value.end };
        }
    }

    const KeyRef* find_key( std::string_view section, std::string_view key ) const
    {
        const auto it = keys_.find( make_key( section, key ) );
        return it == keys_.end() ? nullptr : &it->second;
    }

    KeyRef* find_key( std::string_view section, std::string_view key )
    {
        const auto it = keys_.find( make_key( section, key ) );
        return it == keys_.end() ? nullptr : &it->second;
    }

    size_t find_section_line( std::string_view section ) const
    {
        for ( size_t i = 0; i < lines_.size(); ++i )
        {
            if ( lines_[i].kind == Kind::Section && lines_[i].section == section )
                return i;
        }
        return npos;
    }

    static std::string make_key( std::string_view section, std::string_view key )
    {
        std::string out;
        out.reserve( section.size() + key.size() + 1 );
        out.append( section.data(), section.size() );
        out.push_back( '\0' );
        out.append( key.data(), key.size() );
        return out;
    }

    std::string newline_style() const
    {
        return newline_style_.empty() ? std::string( "\n" ) : newline_style_;
    }

    size_t leading_comment_begin( size_t line ) const
    {
        size_t begin = line;
        while ( begin > 0 && is_comment_line( lines_[begin - 1].text ) )
            --begin;
        return begin;
    }

    std::string leading_comment( size_t line ) const
    {
        if ( line >= lines_.size() )
            return {};

        const size_t begin = leading_comment_begin( line );
        std::string out;
        for ( size_t i = begin; i < line; ++i )
        {
            if ( !out.empty() )
                out.push_back( '\n' );
            out += comment_body( lines_[i].text );
        }
        return out;
    }

    std::vector<Line> make_comment_lines( std::string_view comment, char marker ) const
    {
        std::vector<Line> out;
        if ( comment.empty() )
            return out;

        const std::string newline( newline_style() );
        size_t pos = 0;
        while ( pos <= comment.size() )
        {
            size_t end = pos;
            while ( end < comment.size() && comment[end] != '\r' && comment[end] != '\n' )
                ++end;

            Line line;
            line.text = comment_line_text( comment.substr( pos, end - pos ), marker );
            line.newline = newline;
            out.push_back( std::move( line ) );

            if ( end >= comment.size() )
                break;

            if ( comment[end] == '\r' && end + 1 < comment.size() && comment[end + 1] == '\n' )
                pos = end + 2;
            else
                pos = end + 1;
        }

        return out;
    }

    void replace_leading_comment( size_t line, std::string_view comment, char marker )
    {
        const size_t begin = leading_comment_begin( line );
        lines_.erase( lines_.begin() + static_cast<std::ptrdiff_t>( begin ),
            lines_.begin() + static_cast<std::ptrdiff_t>( line ) );

        const std::vector<Line> comments = make_comment_lines( comment, marker );
        if ( comments.empty() )
            return;

        if ( begin > 0 && lines_[begin - 1].newline.empty() )
            lines_[begin - 1].newline = newline_style();

        lines_.insert( lines_.begin() + static_cast<std::ptrdiff_t>( begin ), comments.begin(), comments.end() );
    }

    void insert_key( std::string_view section, std::string_view key, std::string_view value )
    {
        size_t insert_at = lines_.size();
        bool found_section = false;

        for ( size_t i = 0; i < lines_.size(); ++i )
        {
            if ( lines_[i].kind == Kind::Section && lines_[i].section == section )
            {
                found_section = true;
                insert_at = i + 1;
                continue;
            }

            if ( found_section && lines_[i].kind == Kind::Section )
            {
                while ( insert_at > 0 && is_blank_or_comment( lines_[insert_at - 1].text ) )
                    --insert_at;
                break;
            }

            if ( found_section )
                insert_at = i + 1;
        }

        if ( !found_section )
        {
            const std::string newline( newline_style() );

            if ( !lines_.empty() )
            {
                if ( lines_.back().newline.empty() )
                    lines_.back().newline = newline;

                if ( !lines_.back().text.empty() )
                    lines_.push_back( Line{ "", newline } );
            }

            Line section_line;
            section_line.text = "[" + std::string( section ) + "]";
            section_line.newline = newline;
            section_line.kind = Kind::Section;
            section_line.section = std::string( section );
            lines_.push_back( std::move( section_line ) );
            insert_at = lines_.size();
        }

        Line line;
        line.text.reserve( key.size() + value.size() + 3 );
        line.text.append( key.data(), key.size() );
        line.text += " = ";
        const size_t value_begin = line.text.size();
        line.text.append( value.data(), value.size() );
        line.newline = std::string( newline_style() );
        line.kind = Kind::Key;
        line.section = std::string( section );
        line.key = std::string( key );
        line.value_begin = value_begin;
        line.value_end = value_begin + value.size();

        lines_.insert( lines_.begin() + static_cast<std::ptrdiff_t>( insert_at ), std::move( line ) );
        normalize_section_spacing();
        rebuild_index();
    }

    void normalize_section_spacing()
    {
        const std::string newline( newline_style() );

        for ( size_t i = 1; i + 1 < lines_.size(); )
        {
            if ( lines_[i].text.empty() && lines_[i - 1].kind == Kind::Key &&
                lines_[i + 1].kind == Kind::Key && lines_[i - 1].section == lines_[i + 1].section )
            {
                lines_.erase( lines_.begin() + static_cast<std::ptrdiff_t>( i ) );
                continue;
            }

            ++i;
        }

        for ( size_t i = 1; i < lines_.size(); ++i )
        {
            if ( lines_[i].kind != Kind::Section )
                continue;

            size_t prefix_begin = i;
            while ( prefix_begin > 0 && is_comment_line( lines_[prefix_begin - 1].text ) )
                --prefix_begin;

            if ( prefix_begin == 0 || lines_[prefix_begin - 1].text.empty() )
                continue;

            if ( lines_[prefix_begin - 1].newline.empty() )
                lines_[prefix_begin - 1].newline = newline;

            lines_.insert( lines_.begin() + static_cast<std::ptrdiff_t>( prefix_begin ), Line{ "", newline } );
            ++i;
        }
    }

    std::vector<Line> lines_;
    std::unordered_map<std::string, KeyRef> keys_;
    std::string source_path_;
    std::string newline_style_;
};

#endif // __JINI_HEADER