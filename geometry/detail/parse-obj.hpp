/**
 *  @file geometry/detail/parse-obj.hpp
 *  @author Vaclav Blazek <vaclav.blazek@ext.citationtech.net>
 *
 *  Boost.Spirit-based OBJ file format parser (implementation).
 */

#ifndef geometry_detail_objparser_hpp_included_
#define geometry_detail_objparser_hpp_included_

#include <boost/config/warning_disable.hpp>
#include <boost/spirit/include/qi.hpp>
#include <boost/spirit/include/phoenix_core.hpp>
#include <boost/spirit/include/phoenix_operator.hpp>
#include <boost/spirit/include/phoenix_object.hpp>
#include <boost/spirit/include/phoenix_stl.hpp>
#include <boost/spirit/include/phoenix_fusion.hpp>
#include <boost/spirit/include/phoenix_container.hpp>
#include <boost/fusion/include/adapt_struct.hpp>
#include <boost/fusion/include/io.hpp>
#include <boost/spirit/include/qi_match.hpp>
#include <boost/spirit/include/qi_match_auto.hpp>
#include <boost/spirit/include/qi_stream.hpp>

#include <geometry/parse-obj.hpp>

namespace geometry { namespace detail {

struct Vertex : ObjParserBase::Vector3d {};
struct Normal : ObjParserBase::Vector3d {};
struct Texture : ObjParserBase::Vector3d {};

typedef ObjParserBase::Facet Facet;

class Obj {
public:
    Obj(ObjParserBase &p)
        : p_(&p), vCount_(), tCount_(), nCount_()
    {}

    Obj& operator+=(const detail::Vertex &v) {
        ++vCount_;
        p_->addVertex(v);
        return *this;
    }

    Obj& operator+=(const detail::Texture &t) {
        ++tCount_;
        p_->addTexture(t);
        return *this;
    }

    Obj& operator+=(const detail::Normal &n) {
        ++nCount_;
        p_->addNormal(n);
        return *this;
    }

    Obj& operator+=(Facet f) {
        for (auto &v : f.v) {
            if (v < 0) { v = vCount_ + v + 1; }
        }

        for (auto &t : f.t) {
            if (t < 0) { t = tCount_ + t + 1; }
        }

        for (auto &n : f.n) {
            if (n < 0) { n = nCount_ + n + 1; }
        }

        p_->addFacet(f);
        return *this;
    }

private:
    ObjParserBase *p_;
    int vCount_;
    int tCount_;
    int nCount_;
};

} } // namespace geometry::detail

BOOST_FUSION_ADAPT_STRUCT(
    geometry::detail::Vertex,
    (double, x)
    (double, y)
    (double, z)
)

BOOST_FUSION_ADAPT_STRUCT(
    geometry::detail::Texture,
    (double, x)
    (double, y)
    (double, z)
)

BOOST_FUSION_ADAPT_STRUCT(
    geometry::detail::Normal,
    (double, x)
    (double, y)
    (double, z)
)

BOOST_FUSION_ADAPT_STRUCT(
    geometry::detail::Facet,
    (int, v[0])(int, t[0])(int, n[0])
    (int, v[1])(int, t[1])(int, n[1])
    (int, v[2])(int, t[2])(int, n[2])
)

namespace geometry { namespace detail {

namespace qi = boost::spirit::qi;
namespace ascii = boost::spirit::ascii;

template <typename Iterator, typename Skipper>
struct vertex_parser : qi::grammar<Iterator, Vertex(), Skipper>
{
    vertex_parser() : vertex_parser::base_type(start)  {
        using qi::auto_;

        start %= 'v' >> auto_ >> auto_ >> auto_;
    }

    qi::rule<Iterator, Vertex(), Skipper> start;
};

template <typename Iterator, typename Skipper>
struct texture_parser : qi::grammar<Iterator, Texture(), Skipper>
{
    texture_parser() : texture_parser::base_type(start)  {
        using qi::lit;
        using qi::auto_;

        start %= lit("vt") >> auto_ >> -auto_ >> -auto_;
    }

    qi::rule<Iterator, Texture(), Skipper> start;
};

template <typename Iterator, typename Skipper>
struct normal_parser : qi::grammar<Iterator, Normal(), Skipper>
{
    normal_parser() : normal_parser::base_type(start)  {
        using qi::lit;
        using qi::auto_;

        start %= lit("vn") >> auto_ >> auto_ >> auto_;
    }

    qi::rule<Iterator, Normal(), Skipper> start;
};

template <typename Iterator, typename Skipper>
struct facet_parser : qi::grammar<Iterator, Facet(), Skipper>
{
    facet_parser() : facet_parser::base_type(start)  {
        using qi::auto_;
        using qi::omit;
        using qi::no_skip;
        using qi::lexeme;

        start %= 'f'
            >> lexeme[auto_ >> '/' >> -auto_ >> '/' >> -auto_
                      >> omit[+ascii::space]
                      >> auto_ >> '/' >> -auto_ >> '/' >> -auto_
                      >> omit[+ascii::space]
                      >> auto_ >> '/' >> -auto_ >> '/' >> -auto_];
    }

    qi::rule<Iterator, Facet(), Skipper> start;
};

template <typename Iterator, typename Skipper>
struct Obj_parser : qi::grammar<Iterator, Obj(), Skipper>
{
    Obj_parser() : Obj_parser::base_type(start)  {
        using qi::omit;

        vertex %= vertex_parser<Iterator, Skipper>();
        texture %= texture_parser<Iterator, Skipper>();
        normal %= normal_parser<Iterator, Skipper>();
        facet %= facet_parser<Iterator, Skipper>();

        start %= omit[*(vertex[qi::_val += qi::_1]
                        | texture[qi::_val += qi::_1]
                        | normal[qi::_val += qi::_1]
                        | facet[qi::_val += qi::_1])]
            [qi::_val];
    }

    qi::rule<Iterator, Obj(), Skipper> start;

    vertex_parser<Iterator, Skipper> vertex;
    texture_parser<Iterator, Skipper> texture;
    normal_parser<Iterator, Skipper> normal;
    facet_parser<Iterator, Skipper> facet;
};

template <typename Iterator>
struct skipper : qi::grammar<Iterator>
{
    typedef skipper<Iterator> type;

    skipper() : skipper::base_type(start)  {
        comment %= '#' >> *(qi::char_ - qi::char_("\r\n"))
                       >> (qi::eol | qi::eoi);

        start %= ascii::space | comment;
    }

    qi::rule<Iterator> start;
    qi::rule<Iterator> comment;
};

template <typename Iterator>
bool parse(Iterator begin, Iterator end, ObjParserBase &p)
{
    typedef detail::skipper<Iterator> skipper_type;

    detail::Obj_parser<Iterator, skipper_type> qrammar;
    Obj o(p);

    bool r(phrase_parse(begin, end, qrammar, skipper_type(), o));

    return (r && (begin == end));
}

template<typename CharT, typename Traits>
bool parse(std::basic_istream<CharT, Traits> &is, ObjParserBase &p)
{
    is.unsetf(std::ios::skipws);
    typedef boost::spirit::istream_iterator iterator_type;
    return parse(iterator_type(is), iterator_type(), p);
}

} } // namespace geometry::detail


#endif // geometry_detail_objparser_hpp_included_