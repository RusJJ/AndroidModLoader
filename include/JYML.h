#ifndef __JYML_HEADER
#define __JYML_HEADER



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
#include <cstdlib>
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

class JYML
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
        rebuild();
        return true;
    }

    void parse( const std::string& text )
    {
        clear();
        split_lines( text );
        rebuild();
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

    size_t document_count() const
    {
        return docs_.empty() ? 0 : docs_.size();
    }

    bool has( std::string_view path, size_t doc = 0 ) const
    {
        return resolve( path, doc ) != nullptr;
    }

    bool is_scalar( std::string_view path, size_t doc = 0 ) const
    {
        const Node* node = resolve( path, doc );
        return node && node->type == Type::Scalar;
    }

    bool is_sequence( std::string_view path, size_t doc = 0 ) const
    {
        const Node* node = resolve( path, doc );
        return node && node->type == Type::Sequence;
    }

    bool is_mapping( std::string_view path, size_t doc = 0 ) const
    {
        const Node* node = resolve( path, doc );
        return node && node->type == Type::Mapping;
    }

    size_t size( std::string_view path, size_t doc = 0 ) const
    {
        const Node* node = resolve( path, doc );
        if ( !node )
            return 0;
        if ( node->type == Type::Sequence )
            return node->seq.size();
        if ( node->type == Type::Mapping )
            return node->map.size();
        return 0;
    }

    std::vector<std::string> keys( std::string_view path = {}, size_t doc = 0 ) const
    {
        std::vector<std::string> out;
        const Node* node = resolve( path, doc );
        if ( node && node->type == Type::Mapping )
        {
            out.reserve( node->map.size() );
            for ( const auto& entry : node->map )
                out.push_back( entry.first );
        }
        return out;
    }

    std::string get( std::string_view path, std::string_view fallback = {}, size_t doc = 0 ) const
    {
        const Node* node = resolve( path, doc );
        if ( !node || node->type != Type::Scalar )
            return std::string( fallback );
        return node->scalar;
    }

    bool get_into( std::string_view path, std::string& out, size_t doc = 0 ) const
    {
        const Node* node = resolve( path, doc );
        if ( !node || node->type != Type::Scalar )
            return false;
        out = node->scalar;
        return true;
    }

    template <typename T>
    T get_as( std::string_view path, T fallback = T{}, size_t doc = 0 ) const
    {
        const Node* node = resolve( path, doc );
        if ( !node || node->type != Type::Scalar )
            return fallback;
        return convert<T>( node->scalar, fallback );
    }

    std::string tag( std::string_view path, size_t doc = 0 ) const
    {
        const Node* node = resolve( path, doc );
        return node ? node->tag : std::string();
    }

    std::string anchor( std::string_view path, size_t doc = 0 ) const
    {
        const Node* node = resolve( path, doc );
        return node ? node->anchor : std::string();
    }

    bool get_comment( std::string_view path, std::string& out, size_t doc = 0 ) const
    {
        const Node* node = resolve( path, doc );
        if ( !node )
            return false;

        out = leading_comment( node->line_begin );
        return leading_comment_begin( node->line_begin ) != node->line_begin;
    }

    std::string get_comment( std::string_view path, size_t doc = 0 ) const
    {
        std::string out;
        get_comment( path, out, doc );
        return out;
    }

    bool get_inline_comment( std::string_view path, std::string& out, size_t doc = 0 ) const
    {
        const Node* node = resolve( path, doc );
        if ( !node || node->key_line == npos )
            return false;

        const std::string& text = lines_[node->key_line].text;
        const size_t pos = inline_comment_pos( text, 0 );
        if ( pos == std::string_view::npos )
            return false;

        out = comment_body( text.substr( pos ) );
        return true;
    }

    bool set( std::string_view path, std::string_view value, size_t doc = 0 )
    {
        const std::vector<Segment> segs = parse_path( path );
        size_t depth = 0;
        if ( const Node* flow = find_flow( segs, doc, depth ) )
        {
            Node tmp = *flow;
            Node* target = flow_target( tmp, segs, depth );
            if ( !target || flow->value_end == 0 )
                return false;

            Node scalar;
            scalar.type = Type::Scalar;
            scalar.scalar = std::string( value );
            scalar.anchor = target->anchor;
            scalar.tag = target->tag;
            *target = std::move( scalar );
            replace_flow( *flow, emit_flow( tmp, true ) );
            return true;
        }

        const Node* raw = resolve_raw( segs, doc );
        if ( raw && raw->type == Type::Alias && raw->value_line != npos )
        {
            replace_inline_value( *raw, value );
            rebuild();
            return true;
        }

        Node* node = resolve( path, doc );
        if ( node )
        {
            if ( node->type == Type::Scalar && node->value_line != npos )
            {
                replace_inline_value( *node, value );
                rebuild();
                return true;
            }

            replace_node_block( *node, scalar_emit( value ) );
            rebuild();
            return true;
        }

        return insert_path( path, value, doc );
    }

    template <typename T>
    bool set_as( std::string_view path, const T& value, size_t doc = 0 )
    {
        if constexpr ( std::is_same_v<T, bool> )
        {
            return set( path, value ? "true" : "false", doc );
        }
        else
        {
            std::ostringstream stream;
            stream << value;
            return set( path, stream.str(), doc );
        }
    }

    bool remove( std::string_view path, size_t doc = 0 )
    {
        std::vector<Segment> segs = parse_path( path );
        size_t depth = 0;
        const Node* flow = find_flow( segs, doc, depth );
        if ( flow && depth < segs.size() )
        {
            if ( !resolve( path, doc ) || flow->value_end == 0 )
                return false;

            const Segment last = segs.back();
            segs.pop_back();

            Node tmp = *flow;
            Node* parent = flow_target( tmp, segs, depth );
            if ( !parent )
                return false;

            if ( last.index )
            {
                if ( parent->type != Type::Sequence || last.i >= parent->seq.size() )
                    return false;
                parent->seq.erase( parent->seq.begin() + static_cast<std::ptrdiff_t>( last.i ) );
            }
            else
            {
                const auto it = std::find_if( parent->map.begin(), parent->map.end(),
                    [&]( const auto& entry ) { return entry.first == last.key; } );
                if ( it == parent->map.end() )
                    return false;
                parent->map.erase( it );
            }

            replace_flow( *flow, emit_flow( tmp, true ) );
            return true;
        }

        const Node* node = segs.empty() ? resolve( path, doc ) : resolve_raw( segs, doc );
        if ( !node )
            return false;

        const size_t begin = node->line_begin;
        const size_t end = node->line_end;
        if ( begin >= end || end > lines_.size() )
            return false;

        lines_.erase( lines_.begin() + static_cast<std::ptrdiff_t>( begin ),
            lines_.begin() + static_cast<std::ptrdiff_t>( end ) );
        rebuild();
        return true;
    }

    bool push( std::string_view path, std::string_view value, size_t doc = 0 )
    {
        if ( docs_.empty() )
            rebuild();
        if ( doc >= docs_.size() )
            return false;

        const std::vector<Segment> segs = parse_path( path );
        size_t depth = 0;
        if ( const Node* flow = find_flow( segs, doc, depth ) )
        {
            Node tmp = *flow;
            Node* target = flow_target( tmp, segs, depth );
            if ( !target || flow->value_end == 0 )
                return false;
            if ( target->type == Type::Null )
            {
                target->type = Type::Sequence;
                target->style = Style::Flow;
            }
            if ( target->type != Type::Sequence )
                return false;

            Node item;
            item.type = Type::Scalar;
            item.scalar = std::string( value );
            target->seq.push_back( std::move( item ) );
            replace_flow( *flow, emit_flow( tmp, true ) );
            return true;
        }

        Node* node = resolve( path, doc );
        if ( node && node->type != Type::Sequence && node->type != Type::Null )
            return false;

        if ( !node )
        {
            if ( segs.empty() || segs.back().index )
                return false;

            Node* parent = ensure_parent( segs, doc );
            if ( !parent )
                return false;

            insert_key_line( parent, segs.back().key, {} );
            node = resolve( path, doc );
            if ( !node )
                return false;
        }

        append_seq_item( node, value );
        return true;
    }

    bool insert_at( std::string_view path, size_t index, std::string_view value, size_t doc = 0 )
    {
        const std::vector<Segment> segs = parse_path( path );
        size_t depth = 0;
        if ( const Node* flow = find_flow( segs, doc, depth ) )
        {
            Node tmp = *flow;
            Node* target = flow_target( tmp, segs, depth );
            if ( !target || flow->value_end == 0 || target->type != Type::Sequence )
                return false;

            Node item;
            item.type = Type::Scalar;
            item.scalar = std::string( value );
            target->seq.insert( target->seq.begin() + static_cast<std::ptrdiff_t>( std::min( index, target->seq.size() ) ), std::move( item ) );
            replace_flow( *flow, emit_flow( tmp, true ) );
            return true;
        }

        Node* node = resolve( path, doc );
        if ( !node || node->type != Type::Sequence )
            return false;

        const size_t indent = node->indent;
        const size_t at = index >= node->seq.size() ? node->line_end : node->seq[index].line_begin;

        Line line;
        line.text = std::string( indent, ' ' ) + "- " + scalar_emit( value );
        line.newline = newline_style();

        if ( at > 0 && at <= lines_.size() && lines_[at - 1].newline.empty() )
            lines_[at - 1].newline = newline_style();

        lines_.insert( lines_.begin() + static_cast<std::ptrdiff_t>( std::min( at, lines_.size() ) ), line );
        rebuild();
        return true;
    }

private:
    enum class Type
    {
        Null,
        Scalar,
        Sequence,
        Mapping,
        Alias
    };

    enum class Style
    {
        Plain,
        Single,
        Double,
        Literal,
        Folded,
        Flow
    };

    struct Node
    {
        Type type = Type::Null;
        Style style = Style::Plain;
        std::string scalar;
        std::string tag;
        std::string anchor;
        std::string alias;
        std::vector<std::pair<std::string, Node>> map;
        std::vector<Node> seq;
        size_t indent = 0;
        size_t line_begin = 0;
        size_t line_end = 0;
        size_t key_line = npos;
        size_t value_line = npos;
        size_t value_begin = 0;
        size_t value_end = 0;
    };

    struct Document
    {
        Node root;
        size_t line_begin = 0;
        size_t line_end = 0;
    };

    struct Line
    {
        std::string text;
        std::string newline;
    };

    struct Content
    {
        size_t line = 0;
        size_t indent = 0;
    };

    static constexpr size_t npos = static_cast<size_t>( -1 );

    void clear()
    {
        lines_.clear();
        docs_.clear();
        source_path_.clear();
        newline_style_.clear();
    }

    static bool is_space( char ch )
    {
        return ch == ' ' || ch == '\t';
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

    static bool is_blank( std::string_view text )
    {
        return ltrim_pos( text ) >= text.size();
    }

    static bool is_comment_line( std::string_view text )
    {
        const size_t first = ltrim_pos( text );
        return first < text.size() && text[first] == '#';
    }

    static bool is_doc_marker( std::string_view text )
    {
        return text.rfind( "---", 0 ) == 0 || text.rfind( "...", 0 ) == 0;
    }

    static bool is_doc_start( std::string_view text )
    {
        return text.rfind( "---", 0 ) == 0;
    }

    static bool is_doc_end( std::string_view text )
    {
        return text.rfind( "...", 0 ) == 0;
    }

    static bool is_directive( std::string_view text )
    {
        return !text.empty() && text[0] == '%';
    }

    static bool is_structural( std::string_view text )
    {
        return !is_blank( text ) && !is_comment_line( text ) && !is_directive( text );
    }

    static std::string comment_body( std::string_view text )
    {
        size_t pos = ltrim_pos( text );
        if ( pos >= text.size() || text[pos] != '#' )
            return {};

        ++pos;
        if ( pos < text.size() && is_space( text[pos] ) )
            ++pos;

        return std::string( text.substr( pos ) );
    }

    static size_t inline_comment_pos( std::string_view text, size_t begin )
    {
        bool single = false;
        bool dbl = false;
        for ( size_t i = begin; i < text.size(); ++i )
        {
            const char ch = text[i];
            if ( dbl )
            {
                if ( ch == '\\' )
                    ++i;
                else if ( ch == '"' )
                    dbl = false;
                continue;
            }
            if ( single )
            {
                if ( ch == '\'' )
                    single = false;
                continue;
            }
            if ( ch == '"' )
                dbl = true;
            else if ( ch == '\'' )
                single = true;
            else if ( ch == '#' && i > begin && is_space( text[i - 1] ) )
                return i;
        }
        return std::string_view::npos;
    }

    static size_t find_map_colon( std::string_view text, size_t start )
    {
        bool single = false;
        bool dbl = false;
        int flow = 0;
        for ( size_t i = start; i < text.size(); ++i )
        {
            const char ch = text[i];
            if ( dbl )
            {
                if ( ch == '\\' )
                    ++i;
                else if ( ch == '"' )
                    dbl = false;
                continue;
            }
            if ( single )
            {
                if ( ch == '\'' )
                    single = false;
                continue;
            }
            if ( ch == '"' )
                dbl = true;
            else if ( ch == '\'' )
                single = true;
            else if ( ch == '[' || ch == '{' )
                ++flow;
            else if ( ch == ']' || ch == '}' )
            {
                if ( flow > 0 )
                    --flow;
            }
            else if ( ch == '#' && i > start && is_space( text[i - 1] ) )
                return std::string_view::npos;
            else if ( ch == ':' && flow == 0 && ( i + 1 >= text.size() || is_space( text[i + 1] ) ) )
                return i;
        }
        return std::string_view::npos;
    }

    static bool is_seq_entry( std::string_view text, size_t col )
    {
        return col < text.size() && text[col] == '-' &&
            ( col + 1 >= text.size() || is_space( text[col + 1] ) );
    }

    template <typename T>
    static T convert( const std::string& value, T fallback )
    {
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
        else if constexpr ( std::is_same_v<T, std::string> )
        {
            return value;
        }
        else
        {
            std::istringstream stream( value );
            T out{};
            stream >> out;
            return stream && stream.eof() ? out : fallback;
        }
    }

    std::string newline_style() const
    {
        return newline_style_.empty() ? std::string( "\n" ) : newline_style_;
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

    void rebuild()
    {
        docs_.clear();
        anchors_.clear();
        content_.clear();

        for ( size_t i = 0; i < lines_.size(); ++i )
        {
            if ( is_structural( lines_[i].text ) && !is_doc_marker( lines_[i].text ) )
                content_.push_back( Content{ i, ltrim_pos( lines_[i].text ) } );
        }

        std::vector<std::pair<size_t, size_t>> ranges;
        size_t begin = 0;
        for ( size_t i = 0; i < lines_.size(); ++i )
        {
            const std::string_view text( lines_[i].text );
            if ( is_doc_start( text ) )
            {
                if ( i > begin )
                    ranges.emplace_back( begin, i );
                begin = i;
            }
            else if ( is_doc_end( text ) )
            {
                ranges.emplace_back( begin, i + 1 );
                begin = i + 1;
            }
        }
        if ( begin < lines_.size() )
            ranges.emplace_back( begin, lines_.size() );

        std::vector<std::pair<size_t, size_t>> kept;
        for ( const auto& range : ranges )
        {
            for ( size_t i = range.first; i < range.second; ++i )
            {
                if ( is_structural( lines_[i].text ) && !is_doc_marker( lines_[i].text ) )
                {
                    kept.push_back( range );
                    break;
                }
            }
        }
        if ( kept.empty() )
            kept.emplace_back( 0, lines_.size() );

        for ( const auto& range : kept )
        {
            Document document;
            document.line_begin = range.first;
            document.line_end = range.second;

            cur_ = first_content_in( range.first, range.second );
            limit_ = range.second;
            document.root = parse_block( 0 );
            docs_.push_back( std::move( document ) );
        }

        for ( const Document& document : docs_ )
            collect_anchors( document.root );
    }

    size_t first_content_in( size_t begin, size_t end ) const
    {
        for ( size_t i = 0; i < content_.size(); ++i )
        {
            if ( content_[i].line >= begin && content_[i].line < end )
                return i;
        }
        return content_.size();
    }

    bool cursor_valid() const
    {
        return cur_ < content_.size() && content_[cur_].line < limit_;
    }

    Node parse_block( size_t min_indent )
    {
        if ( !cursor_valid() || content_[cur_].indent < min_indent )
            return Node{};

        const size_t ind = content_[cur_].indent;
        const std::string& text = lines_[content_[cur_].line].text;

        if ( is_seq_entry( text, ind ) )
            return parse_sequence( ind );

        if ( find_map_colon( text, ind ) != std::string_view::npos )
            return parse_mapping( ind );

        return parse_scalar_node( content_[cur_].line, ind, ind );
    }

    Node parse_mapping( size_t ind )
    {
        Node node;
        node.type = Type::Mapping;
        node.indent = ind;
        node.line_begin = content_[cur_].line;
        node.line_end = content_[cur_].line + 1;

        while ( cursor_valid() && content_[cur_].indent == ind )
        {
            const size_t phys = content_[cur_].line;
            const std::string& text = lines_[phys].text;

            if ( is_seq_entry( text, ind ) )
                break;

            const size_t colon = find_map_colon( text, ind );
            if ( colon == std::string_view::npos )
                break;

            std::string key = decode_key( text.substr( ind, colon - ind ) );

            size_t vcol = colon + 1;
            while ( vcol < text.size() && is_space( text[vcol] ) )
                ++vcol;

            const bool empty_inline = vcol >= text.size() ||
                ( text[vcol] == '#' && ( vcol == 0 || is_space( text[vcol - 1] ) ) );

            Node child;
            if ( empty_inline )
            {
                ++cur_;
                if ( cursor_valid() && content_[cur_].indent > ind )
                    child = parse_block( ind + 1 );
                if ( child.type == Type::Null )
                    child.indent = ind;
                child.key_line = phys;
                child.line_begin = phys;
                if ( child.line_end <= phys )
                    child.line_end = phys + 1;
            }
            else
            {
                child = parse_value( phys, vcol, ind );
                child.key_line = phys;
                child.line_begin = phys;
                child.indent = ind;
            }

            node.line_end = child.line_end;
            node.map.emplace_back( std::move( key ), std::move( child ) );
        }

        return node;
    }

    Node parse_sequence( size_t ind )
    {
        Node node;
        node.type = Type::Sequence;
        node.indent = ind;
        node.line_begin = content_[cur_].line;
        node.line_end = content_[cur_].line + 1;

        while ( cursor_valid() && content_[cur_].indent == ind &&
            is_seq_entry( lines_[content_[cur_].line].text, ind ) )
        {
            const size_t phys = content_[cur_].line;
            const std::string& text = lines_[phys].text;

            size_t after = ind + 1;
            while ( after < text.size() && is_space( text[after] ) )
                ++after;

            Node item;
            if ( after >= text.size() || ( text[after] == '#' && is_space( text[after - 1] ) ) )
            {
                ++cur_;
                if ( cursor_valid() && content_[cur_].indent > ind )
                    item = parse_block( ind + 1 );
                item.line_begin = phys;
                if ( item.line_end <= phys )
                    item.line_end = phys + 1;
            }
            else if ( find_map_colon( text, after ) != std::string_view::npos || is_seq_entry( text, after ) )
            {
                item = parse_compact( phys, after );
                item.line_begin = phys;
            }
            else
            {
                item = parse_value( phys, after, ind );
                item.line_begin = phys;
            }

            node.line_end = item.line_end;
            node.seq.push_back( std::move( item ) );
        }

        return node;
    }

    Node parse_compact( size_t phys, size_t col )
    {
        content_[cur_].indent = col;
        if ( is_seq_entry( lines_[phys].text, col ) )
            return parse_sequence( col );
        return parse_mapping( col );
    }

    Node parse_value( size_t phys, size_t col, size_t owner_indent )
    {
        const std::string& text = lines_[phys].text;

        Node prefix;
        col = consume_prefixes( text, col, prefix );

        while ( col < text.size() && is_space( text[col] ) )
            ++col;

        if ( col >= text.size() || ( text[col] == '#' && col > 0 && is_space( text[col - 1] ) ) )
        {
            ++cur_;
            Node child;
            if ( cursor_valid() && content_[cur_].indent > owner_indent )
                child = parse_block( owner_indent + 1 );
            child.line_begin = phys;
            if ( child.line_end <= phys )
                child.line_end = phys + 1;
            apply_prefix( child, prefix );
            return child;
        }

        const char lead = text[col];
        if ( lead == '|' || lead == '>' )
        {
            Node node = parse_block_scalar( phys, col, owner_indent );
            apply_prefix( node, prefix );
            return node;
        }
        if ( lead == '[' || lead == '{' )
        {
            Node node = parse_flow( phys, col );
            apply_prefix( node, prefix );
            return node;
        }
        if ( lead == '*' )
        {
            Node node;
            node.type = Type::Alias;
            const size_t end = scan_token_end( text, col + 1 );
            node.alias = std::string( text.substr( col + 1, end - col - 1 ) );
            node.value_line = phys;
            node.value_begin = col;
            node.value_end = end;
            node.line_begin = phys;
            node.line_end = phys + 1;
            ++cur_;
            return node;
        }

        Node node = parse_scalar_inline( phys, col );
        apply_prefix( node, prefix );
        return node;
    }

    size_t consume_prefixes( std::string_view text, size_t col, Node& prefix ) const
    {
        for ( ;; )
        {
            while ( col < text.size() && is_space( text[col] ) )
                ++col;
            if ( col >= text.size() )
                break;

            if ( text[col] == '&' )
            {
                const size_t end = scan_token_end( text, col + 1 );
                prefix.anchor = std::string( text.substr( col + 1, end - col - 1 ) );
                col = end;
            }
            else if ( text[col] == '!' )
            {
                const size_t end = scan_token_end( text, col );
                prefix.tag = std::string( text.substr( col, end - col ) );
                col = end;
            }
            else
            {
                break;
            }
        }
        return col;
    }

    static void apply_prefix( Node& node, const Node& prefix )
    {
        if ( !prefix.anchor.empty() )
            node.anchor = prefix.anchor;
        if ( !prefix.tag.empty() )
            node.tag = prefix.tag;
    }

    static size_t scan_token_end( std::string_view text, size_t pos )
    {
        while ( pos < text.size() && !is_space( text[pos] ) &&
            text[pos] != ',' && text[pos] != ']' && text[pos] != '}' )
            ++pos;
        return pos;
    }

    Node parse_scalar_inline( size_t phys, size_t col )
    {
        const std::string& text = lines_[phys].text;
        Node node;
        node.type = Type::Scalar;
        node.line_begin = phys;
        node.line_end = phys + 1;

        if ( text[col] == '"' || text[col] == '\'' )
        {
            const char quote = text[col];
            size_t end = col + 1;
            bool closed = false;
            while ( end < text.size() )
            {
                if ( quote == '"' && text[end] == '\\' )
                {
                    end += 2;
                    continue;
                }
                if ( text[end] == quote )
                {
                    if ( quote == '\'' && end + 1 < text.size() && text[end + 1] == '\'' )
                    {
                        end += 2;
                        continue;
                    }
                    closed = true;
                    ++end;
                    break;
                }
                ++end;
            }

            node.style = quote == '"' ? Style::Double : Style::Single;
            if ( closed )
            {
                node.scalar = unquote( text.substr( col, end - col ), quote );
                node.value_line = phys;
                node.value_begin = col;
                node.value_end = end;
                ++cur_;
                return node;
            }
        }

        const size_t comment = inline_comment_pos( text, col );
        const size_t tail = comment == std::string_view::npos ? text.size() : comment;
        const size_t end = rtrim_pos( text, tail );

        node.style = Style::Plain;
        node.scalar = std::string( text.substr( col, end - col ) );
        node.value_line = phys;
        node.value_begin = col;
        node.value_end = end;
        ++cur_;
        return node;
    }

    Node parse_scalar_node( size_t phys, size_t col, size_t owner_indent )
    {
        return parse_value( phys, col, owner_indent );
    }

    Node parse_block_scalar( size_t phys, size_t col, size_t owner_indent )
    {
        const std::string& header = lines_[phys].text;
        Node node;
        node.type = Type::Scalar;
        node.style = header[col] == '|' ? Style::Literal : Style::Folded;
        node.line_begin = phys;

        char chomp = ' ';
        size_t explicit_indent = 0;
        for ( size_t i = col + 1; i < header.size(); ++i )
        {
            const char ch = header[i];
            if ( ch == '-' || ch == '+' )
                chomp = ch;
            else if ( ch >= '1' && ch <= '9' )
                explicit_indent = owner_indent + static_cast<size_t>( ch - '0' );
            else
                break;
        }

        ++cur_;
        std::vector<std::string> raw;
        size_t block_indent = explicit_indent;
        size_t last = phys + 1;

        for ( size_t i = phys + 1; i < limit_; ++i )
        {
            const std::string_view text( lines_[i].text );
            if ( is_blank( text ) )
            {
                raw.emplace_back();
                continue;
            }

            const size_t indent = ltrim_pos( text );
            if ( indent <= owner_indent )
                break;

            if ( block_indent == 0 )
                block_indent = indent;
            if ( indent < block_indent )
                break;

            raw.push_back( std::string( text.substr( block_indent ) ) );
            last = i + 1;
        }

        while ( !raw.empty() && raw.back().empty() )
            raw.pop_back();

        std::string value;
        if ( node.style == Style::Literal )
        {
            for ( size_t i = 0; i < raw.size(); ++i )
            {
                value += raw[i];
                if ( i + 1 < raw.size() )
                    value.push_back( '\n' );
            }
        }
        else
        {
            bool prev_empty = false;
            for ( size_t i = 0; i < raw.size(); ++i )
            {
                if ( i == 0 )
                {
                    value += raw[i];
                }
                else if ( raw[i].empty() )
                {
                    value.push_back( '\n' );
                }
                else if ( prev_empty )
                {
                    value += raw[i];
                }
                else
                {
                    value.push_back( ' ' );
                    value += raw[i];
                }
                prev_empty = raw[i].empty();
            }
        }

        if ( chomp != '-' && !raw.empty() )
            value.push_back( '\n' );

        node.scalar = std::move( value );
        node.line_end = last;

        cur_ = advance_to( last );
        return node;
    }

    Node parse_flow( size_t phys, size_t col )
    {
        std::string joined;
        std::vector<size_t> line_of;
        size_t last = phys;
        int depth = 0;
        bool single = false;
        bool dbl = false;

        for ( size_t i = phys; i < limit_; ++i )
        {
            const std::string& text = lines_[i].text;
            const size_t from = i == phys ? col : ltrim_pos( text );
            for ( size_t j = from; j < text.size(); ++j )
            {
                const char ch = text[j];
                joined.push_back( ch );
                line_of.push_back( i );
                if ( dbl )
                {
                    if ( ch == '\\' )
                    {
                        if ( j + 1 < text.size() )
                        {
                            joined.push_back( text[++j] );
                            line_of.push_back( i );
                        }
                    }
                    else if ( ch == '"' )
                        dbl = false;
                    continue;
                }
                if ( single )
                {
                    if ( ch == '\'' )
                        single = false;
                    continue;
                }
                if ( ch == '"' )
                    dbl = true;
                else if ( ch == '\'' )
                    single = true;
                else if ( ch == '[' || ch == '{' )
                    ++depth;
                else if ( ch == ']' || ch == '}' )
                {
                    --depth;
                    if ( depth == 0 )
                    {
                        last = i;
                        size_t pos = 0;
                        Node node = parse_flow_node( joined, pos );
                        node.line_begin = phys;
                        node.line_end = last + 1;
                        node.style = Style::Flow;
                        node.value_begin = col;
                        node.value_end = j + 1;
                        cur_ = advance_to( last + 1 );
                        return node;
                    }
                }
            }
            joined.push_back( ' ' );
            line_of.push_back( i );
        }

        Node node;
        node.type = Type::Scalar;
        node.line_begin = phys;
        node.line_end = phys + 1;
        ++cur_;
        return node;
    }

    Node parse_flow_node( const std::string& text, size_t& pos )
    {
        skip_flow_space( text, pos );
        Node prefix;
        pos = consume_prefixes( text, pos, prefix );
        skip_flow_space( text, pos );

        Node node;
        if ( pos < text.size() && text[pos] == '[' )
        {
            node.type = Type::Sequence;
            node.style = Style::Flow;
            ++pos;
            skip_flow_space( text, pos );
            while ( pos < text.size() && text[pos] != ']' )
            {
                node.seq.push_back( parse_flow_node( text, pos ) );
                skip_flow_space( text, pos );
                if ( pos < text.size() && text[pos] == ',' )
                {
                    ++pos;
                    skip_flow_space( text, pos );
                }
            }
            if ( pos < text.size() )
                ++pos;
        }
        else if ( pos < text.size() && text[pos] == '{' )
        {
            node.type = Type::Mapping;
            node.style = Style::Flow;
            ++pos;
            skip_flow_space( text, pos );
            while ( pos < text.size() && text[pos] != '}' )
            {
                const std::string key = decode_key( read_flow_scalar( text, pos, true ) );
                skip_flow_space( text, pos );
                Node value;
                if ( pos < text.size() && text[pos] == ':' )
                {
                    ++pos;
                    skip_flow_space( text, pos );
                    value = parse_flow_node( text, pos );
                }
                node.map.emplace_back( key, std::move( value ) );
                skip_flow_space( text, pos );
                if ( pos < text.size() && text[pos] == ',' )
                {
                    ++pos;
                    skip_flow_space( text, pos );
                }
            }
            if ( pos < text.size() )
                ++pos;
        }
        else if ( pos < text.size() && text[pos] == '*' )
        {
            node.type = Type::Alias;
            ++pos;
            const size_t start = pos;
            while ( pos < text.size() && !is_flow_break( text[pos] ) && !is_space( text[pos] ) )
                ++pos;
            node.alias = text.substr( start, pos - start );
        }
        else
        {
            node.type = Type::Scalar;
            if ( pos < text.size() && ( text[pos] == '"' || text[pos] == '\'' ) )
                node.style = text[pos] == '"' ? Style::Double : Style::Single;
            node.scalar = read_flow_scalar( text, pos, false );
        }

        apply_prefix( node, prefix );
        return node;
    }

    static void skip_flow_space( const std::string& text, size_t& pos )
    {
        while ( pos < text.size() && ( is_space( text[pos] ) || text[pos] == '\n' ) )
            ++pos;
    }

    static bool is_flow_break( char ch )
    {
        return ch == ',' || ch == ']' || ch == '}' || ch == ':';
    }

    std::string read_flow_scalar( const std::string& text, size_t& pos, bool as_key ) const
    {
        skip_flow_space( text, pos );
        if ( pos >= text.size() )
            return {};

        if ( text[pos] == '"' || text[pos] == '\'' )
        {
            const char quote = text[pos];
            size_t end = pos + 1;
            while ( end < text.size() )
            {
                if ( quote == '"' && text[end] == '\\' )
                {
                    end += 2;
                    continue;
                }
                if ( text[end] == quote )
                {
                    if ( quote == '\'' && end + 1 < text.size() && text[end + 1] == '\'' )
                    {
                        end += 2;
                        continue;
                    }
                    ++end;
                    break;
                }
                ++end;
            }
            std::string out = unquote( text.substr( pos, end - pos ), quote );
            pos = end;
            return out;
        }

        const size_t start = pos;
        while ( pos < text.size() && !is_flow_break( text[pos] ) )
            ++pos;
        if ( as_key )
        {
            // keys stop at ':' which is_flow_break already covers
        }
        return trimmed_copy( text.substr( start, pos - start ) );
    }

    static std::string decode_key( std::string_view raw )
    {
        std::string key = trimmed_copy( raw );
        if ( key.size() >= 2 && ( key.front() == '"' || key.front() == '\'' ) && key.back() == key.front() )
            return unquote( key, key.front() );
        return key;
    }

    static std::string unquote( std::string_view text, char quote )
    {
        if ( text.size() < 2 )
            return std::string( text );

        std::string_view body = text.substr( 1, text.size() - 2 );
        std::string out;
        out.reserve( body.size() );

        if ( quote == '\'' )
        {
            for ( size_t i = 0; i < body.size(); ++i )
            {
                if ( body[i] == '\'' && i + 1 < body.size() && body[i + 1] == '\'' )
                {
                    out.push_back( '\'' );
                    ++i;
                }
                else
                {
                    out.push_back( body[i] );
                }
            }
            return out;
        }

        for ( size_t i = 0; i < body.size(); ++i )
        {
            if ( body[i] != '\\' || i + 1 >= body.size() )
            {
                out.push_back( body[i] );
                continue;
            }

            const char esc = body[++i];
            switch ( esc )
            {
            case 'n': out.push_back( '\n' ); break;
            case 't': out.push_back( '\t' ); break;
            case 'r': out.push_back( '\r' ); break;
            case '0': out.push_back( '\0' ); break;
            case 'b': out.push_back( '\b' ); break;
            case 'f': out.push_back( '\f' ); break;
            case '"': out.push_back( '"' ); break;
            case '\\': out.push_back( '\\' ); break;
            case '/': out.push_back( '/' ); break;
            default: out.push_back( esc ); break;
            }
        }
        return out;
    }

    size_t advance_to( size_t phys_line )
    {
        size_t i = cur_;
        while ( i < content_.size() && content_[i].line < phys_line )
            ++i;
        return i;
    }

    void collect_anchors( const Node& node )
    {
        if ( !node.anchor.empty() )
            anchors_[node.anchor] = &node;

        for ( const auto& entry : node.map )
            collect_anchors( entry.second );
        for ( const Node& item : node.seq )
            collect_anchors( item );
    }

    const Node* deref( const Node* node ) const
    {
        if ( node && node->type == Type::Alias )
        {
            const auto it = anchors_.find( node->alias );
            if ( it != anchors_.end() )
                return it->second;
        }
        return node;
    }

    struct Segment
    {
        bool index = false;
        size_t i = 0;
        std::string key;
    };

    static std::vector<Segment> parse_path( std::string_view path )
    {
        std::vector<Segment> out;
        size_t i = 0;
        while ( i < path.size() )
        {
            if ( path[i] == '.' )
            {
                ++i;
                continue;
            }

            if ( path[i] == '[' )
            {
                const size_t close = path.find( ']', i );
                if ( close == std::string_view::npos )
                    break;

                std::string_view inner = path.substr( i + 1, close - i - 1 );
                if ( !inner.empty() && ( inner.front() == '"' || inner.front() == '\'' ) )
                {
                    Segment seg;
                    seg.key = decode_key( inner );
                    out.push_back( std::move( seg ) );
                }
                else
                {
                    Segment seg;
                    seg.index = true;
                    seg.i = static_cast<size_t>( std::strtoull( std::string( inner ).c_str(), nullptr, 10 ) );
                    out.push_back( std::move( seg ) );
                }
                i = close + 1;
                continue;
            }

            size_t end = i;
            while ( end < path.size() && path[end] != '.' && path[end] != '[' )
                ++end;

            Segment seg;
            seg.key = std::string( path.substr( i, end - i ) );
            out.push_back( std::move( seg ) );
            i = end;
        }
        return out;
    }

    const Node* resolve( std::string_view path, size_t doc ) const
    {
        if ( doc >= docs_.size() )
            return nullptr;

        const Node* node = deref( &docs_[doc].root );
        for ( const Segment& seg : parse_path( path ) )
        {
            if ( !node )
                return nullptr;
            node = step( node, seg );
        }
        return node;
    }

    // like resolve, but the last segment is not dereferenced
    const Node* resolve_raw( const std::vector<Segment>& segs, size_t doc ) const
    {
        if ( segs.empty() || doc >= docs_.size() )
            return nullptr;

        const Node* node = deref( &docs_[doc].root );
        for ( size_t i = 0; node && i + 1 < segs.size(); ++i )
            node = step( node, segs[i] );
        if ( !node )
            return nullptr;

        const Segment& seg = segs.back();
        if ( seg.index )
            return node->type == Type::Sequence && seg.i < node->seq.size() ? &node->seq[seg.i] : nullptr;

        if ( node->type != Type::Mapping )
            return nullptr;

        for ( const auto& entry : node->map )
        {
            if ( entry.first == seg.key )
                return &entry.second;
        }
        return nullptr;
    }

    const Node* step( const Node* node, const Segment& seg ) const
    {
        if ( seg.index )
        {
            if ( node->type != Type::Sequence || seg.i >= node->seq.size() )
                return nullptr;
            return deref( &node->seq[seg.i] );
        }

        if ( node->type != Type::Mapping )
            return nullptr;

        for ( const auto& entry : node->map )
        {
            if ( entry.first == seg.key )
                return deref( &entry.second );
        }
        return nullptr;
    }

    // outermost flow collection on the path, depth = segments consumed to reach it
    const Node* find_flow( const std::vector<Segment>& segs, size_t doc, size_t& depth ) const
    {
        if ( doc >= docs_.size() )
            return nullptr;

        const Node* node = deref( &docs_[doc].root );
        for ( size_t i = 0; node; ++i )
        {
            if ( node->style == Style::Flow && ( node->type == Type::Mapping || node->type == Type::Sequence ) )
            {
                depth = i;
                return node;
            }
            if ( i >= segs.size() )
                break;
            node = step( node, segs[i] );
        }
        return nullptr;
    }

    static Node* flow_target( Node& node, const std::vector<Segment>& segs, size_t i )
    {
        if ( i >= segs.size() )
            return &node;

        const Segment& seg = segs[i];
        if ( seg.index )
        {
            if ( node.type != Type::Sequence || seg.i >= node.seq.size() )
                return nullptr;
            return flow_target( node.seq[seg.i], segs, i + 1 );
        }

        if ( node.type == Type::Null )
        {
            node.type = Type::Mapping;
            node.style = Style::Flow;
        }
        if ( node.type != Type::Mapping )
            return nullptr;

        for ( auto& entry : node.map )
        {
            if ( entry.first == seg.key )
                return flow_target( entry.second, segs, i + 1 );
        }
        node.map.emplace_back( seg.key, Node{} );
        return flow_target( node.map.back().second, segs, i + 1 );
    }

    std::string emit_flow( const Node& node, bool top ) const
    {
        std::string out;
        if ( !top )
        {
            if ( !node.anchor.empty() )
                out += "&" + node.anchor + " ";
            if ( !node.tag.empty() )
                out += node.tag + " ";
        }

        if ( node.type == Type::Mapping )
        {
            out.push_back( '{' );
            for ( size_t i = 0; i < node.map.size(); ++i )
            {
                if ( i > 0 )
                    out += ", ";
                out += flow_scalar_emit( node.map[i].first );
                if ( node.map[i].second.type != Type::Null )
                    out += ": " + emit_flow( node.map[i].second, false );
            }
            out.push_back( '}' );
        }
        else if ( node.type == Type::Sequence )
        {
            out.push_back( '[' );
            for ( size_t i = 0; i < node.seq.size(); ++i )
            {
                if ( i > 0 )
                    out += ", ";
                out += emit_flow( node.seq[i], false );
            }
            out.push_back( ']' );
        }
        else if ( node.type == Type::Alias )
        {
            out += "*" + node.alias;
        }
        else if ( node.style == Style::Double )
        {
            out += quote_double( node.scalar );
        }
        else if ( node.style == Style::Single )
        {
            out += quote_single( node.scalar );
        }
        else
        {
            out += flow_scalar_emit( node.scalar );
        }
        return out;
    }

    void replace_flow( const Node& flow, const std::string& text )
    {
        const size_t begin = flow.line_begin;
        const size_t end = flow.line_end;

        Line line;
        line.text = lines_[begin].text.substr( 0, flow.value_begin ) + text + lines_[end - 1].text.substr( flow.value_end );
        line.newline = lines_[end - 1].newline;

        lines_.erase( lines_.begin() + static_cast<std::ptrdiff_t>( begin ),
            lines_.begin() + static_cast<std::ptrdiff_t>( end ) );
        lines_.insert( lines_.begin() + static_cast<std::ptrdiff_t>( begin ), line );
        rebuild();
    }

    Node* resolve( std::string_view path, size_t doc )
    {
        return const_cast<Node*>( static_cast<const JYML*>( this )->resolve( path, doc ) );
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

    std::string scalar_emit( std::string_view value ) const
    {
        if ( needs_quoting( value ) )
            return quote_double( value );
        return std::string( value );
    }

    static std::string flow_scalar_emit( std::string_view value )
    {
        if ( value.empty() || needs_quoting( value ) || value.find_first_of( ",[]{}" ) != std::string_view::npos )
            return quote_double( value );
        return std::string( value );
    }

    static std::string quote_double( std::string_view value )
    {
        std::string out;
        out.reserve( value.size() + 2 );
        out.push_back( '"' );
        for ( char ch : value )
        {
            if ( ch == '"' || ch == '\\' )
                out.push_back( '\\' );
            if ( ch == '\n' )
            {
                out += "\\n";
                continue;
            }
            out.push_back( ch );
        }
        out.push_back( '"' );
        return out;
    }

    static std::string quote_single( std::string_view value )
    {
        std::string out;
        out.reserve( value.size() + 2 );
        out.push_back( '\'' );
        for ( char ch : value )
        {
            if ( ch == '\'' )
                out.push_back( '\'' );
            out.push_back( ch );
        }
        out.push_back( '\'' );
        return out;
    }

    static bool needs_quoting( std::string_view value )
    {
        if ( value.empty() )
            return false;
        if ( is_space( value.front() ) || is_space( value.back() ) )
            return true;

        const char first = value.front();
        if ( first == '!' || first == '&' || first == '*' || first == '[' || first == '{' ||
            first == '#' || first == '%' || first == '@' || first == '`' || first == '>' ||
            first == '|' || first == '\'' || first == '"' || first == '-' || first == '?' ||
            first == ':' || first == ',' )
            return true;

        for ( char ch : value )
        {
            if ( ch == '\n' || ch == '#' || ch == ':' )
                return true;
        }
        return false;
    }

    void replace_inline_value( const Node& node, std::string_view value )
    {
        Line& line = lines_[node.value_line];
        const std::string replacement = scalar_emit( value );
        line.text.replace( node.value_begin, node.value_end - node.value_begin, replacement );
    }

    void replace_node_block( Node& node, std::string_view value )
    {
        if ( node.key_line != npos )
        {
            const std::string& key_text = lines_[node.key_line].text;
            const size_t colon = find_map_colon( key_text, ltrim_pos( key_text ) );
            if ( colon != std::string_view::npos )
            {
                std::string head = key_text.substr( 0, colon + 1 );
                head.push_back( ' ' );
                head += value;

                Line replacement{ head, newline_style() };
                lines_.erase( lines_.begin() + static_cast<std::ptrdiff_t>( node.line_begin ),
                    lines_.begin() + static_cast<std::ptrdiff_t>( node.line_end ) );
                lines_.insert( lines_.begin() + static_cast<std::ptrdiff_t>( node.line_begin ), replacement );
                return;
            }
        }

        std::string indent( node.indent, ' ' );
        Line replacement{ indent + std::string( value ), newline_style() };
        lines_.erase( lines_.begin() + static_cast<std::ptrdiff_t>( node.line_begin ),
            lines_.begin() + static_cast<std::ptrdiff_t>( node.line_end ) );
        lines_.insert( lines_.begin() + static_cast<std::ptrdiff_t>( node.line_begin ), replacement );
    }

    static std::string join_path( const std::vector<Segment>& segs, size_t count )
    {
        std::string out;
        for ( size_t i = 0; i < count; ++i )
        {
            if ( segs[i].index )
                out += "[" + std::to_string( segs[i].i ) + "]";
            else
            {
                if ( !out.empty() )
                    out.push_back( '.' );
                out += segs[i].key;
            }
        }
        return out;
    }

    void insert_key_line( Node* parent, std::string_view key, std::string_view value )
    {
        size_t indent = 0;
        size_t insert_at = parent->line_end;

        if ( parent->type == Type::Mapping && !parent->map.empty() &&
            parent->map.back().second.key_line != npos )
        {
            indent = parent->indent;
            insert_at = parent->line_end;
        }
        else if ( parent->key_line != npos )
        {
            indent = parent->indent + 2;
            insert_at = parent->key_line + 1;
        }

        Line line;
        line.text = std::string( indent, ' ' ) + std::string( key );
        line.text += value.empty() ? ":" : ": " + scalar_emit( value );
        line.newline = newline_style();

        if ( insert_at > 0 && insert_at <= lines_.size() && lines_[insert_at - 1].newline.empty() )
            lines_[insert_at - 1].newline = newline_style();

        lines_.insert( lines_.begin() + static_cast<std::ptrdiff_t>( std::min( insert_at, lines_.size() ) ), line );
        rebuild();
    }

    void append_seq_item( Node* node, std::string_view value )
    {
        size_t indent = 0;
        size_t insert_at = node->line_end;

        if ( node->type == Type::Sequence )
        {
            indent = node->indent;
            insert_at = node->line_end;
        }
        else if ( node->key_line != npos )
        {
            indent = node->indent + 2;
            insert_at = node->key_line + 1;
        }

        Line line;
        line.text = std::string( indent, ' ' ) + "- " + scalar_emit( value );
        line.newline = newline_style();

        if ( insert_at > 0 && insert_at <= lines_.size() && lines_[insert_at - 1].newline.empty() )
            lines_[insert_at - 1].newline = newline_style();

        lines_.insert( lines_.begin() + static_cast<std::ptrdiff_t>( std::min( insert_at, lines_.size() ) ), line );
        rebuild();
    }

    Node* ensure_parent( const std::vector<Segment>& segs, size_t doc )
    {
        for ( size_t i = 0; i + 1 < segs.size(); ++i )
        {
            const Node* cur = resolve( join_path( segs, i + 1 ), doc );
            if ( cur )
            {
                if ( cur->type == Type::Scalar )
                    return nullptr;
                continue;
            }

            if ( segs[i].index )
                return nullptr;

            Node* parent = resolve( join_path( segs, i ), doc );
            if ( !parent || parent->type == Type::Scalar || parent->type == Type::Sequence )
                return nullptr;

            insert_key_line( parent, segs[i].key, {} );
        }

        Node* parent = resolve( join_path( segs, segs.size() - 1 ), doc );
        if ( !parent || parent->type == Type::Scalar || parent->type == Type::Sequence )
            return nullptr;
        return parent;
    }

    bool insert_path( std::string_view path, std::string_view value, size_t doc )
    {
        if ( docs_.empty() )
            rebuild();
        if ( doc >= docs_.size() )
            return false;

        const std::vector<Segment> segs = parse_path( path );
        if ( segs.empty() || segs.back().index )
            return false;

        Node* parent = ensure_parent( segs, doc );
        if ( !parent )
            return false;

        insert_key_line( parent, segs.back().key, value );
        return true;
    }

    std::vector<Line> lines_;
    std::vector<Document> docs_;
    std::unordered_map<std::string, const Node*> anchors_;
    std::vector<Content> content_;
    std::string source_path_;
    std::string newline_style_;
    size_t cur_ = 0;
    size_t limit_ = 0;
};

#endif // __JYML_HEADER
