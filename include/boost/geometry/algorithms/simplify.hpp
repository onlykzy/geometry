// Boost.Geometry (aka GGL, Generic Geometry Library)

// Copyright (c) 2007-2015 Barend Gehrels, Amsterdam, the Netherlands.
// Copyright (c) 2008-2015 Bruno Lalande, Paris, France.
// Copyright (c) 2009-2015 Mateusz Loskot, London, UK.

// This file was modified by Oracle on 2018-2021.
// Modifications copyright (c) 2018-2021 Oracle and/or its affiliates.
// Contributed and/or modified by Adam Wulkiewicz, on behalf of Oracle

// Parts of Boost.Geometry are redesigned from Geodan's Geographic Library
// (geolib/GGL), copyright (c) 1995-2010 Geodan, Amsterdam, the Netherlands.

// Use, modification and distribution is subject to the Boost Software License,
// Version 1.0. (See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

#ifndef BOOST_GEOMETRY_ALGORITHMS_SIMPLIFY_HPP
#define BOOST_GEOMETRY_ALGORITHMS_SIMPLIFY_HPP

#include <cstddef>
#ifdef BOOST_GEOMETRY_DEBUG_DOUGLAS_PEUCKER
#include <iostream>
#endif
#include <set>
#include <vector>

#include <boost/core/ignore_unused.hpp>
#include <boost/range/begin.hpp>
#include <boost/range/end.hpp>
#include <boost/range/size.hpp>
#include <boost/range/value_type.hpp>
#include <boost/variant/apply_visitor.hpp>
#include <boost/variant/static_visitor.hpp>
#include <boost/variant/variant_fwd.hpp>

#include <boost/geometry/core/cs.hpp>
#include <boost/geometry/core/closure.hpp>
#include <boost/geometry/core/exterior_ring.hpp>
#include <boost/geometry/core/interior_rings.hpp>
#include <boost/geometry/core/mutable_range.hpp>
#include <boost/geometry/core/tags.hpp>

#include <boost/geometry/geometries/concepts/check.hpp>
#include <boost/geometry/strategies/concepts/simplify_concept.hpp>
#include <boost/geometry/strategies/default_strategy.hpp>
#include <boost/geometry/strategies/distance.hpp>

#include <boost/geometry/algorithms/area.hpp>
#include <boost/geometry/algorithms/clear.hpp>
#include <boost/geometry/algorithms/convert.hpp>
#include <boost/geometry/algorithms/detail/equals/point_point.hpp>
#include <boost/geometry/algorithms/not_implemented.hpp>
#include <boost/geometry/algorithms/is_empty.hpp>
#include <boost/geometry/algorithms/perimeter.hpp>

#include <boost/geometry/algorithms/detail/distance/default_strategies.hpp>

#include <boost/geometry/strategies/simplify/cartesian.hpp>
#include <boost/geometry/strategies/simplify/geographic.hpp>
#include <boost/geometry/strategies/simplify/spherical.hpp>

#ifdef BOOST_GEOMETRY_DEBUG_DOUGLAS_PEUCKER
#include <boost/geometry/io/dsv/write.hpp>
#endif

namespace boost { namespace geometry
{

#ifndef DOXYGEN_NO_DETAIL
namespace detail { namespace simplify
{

/*!
\brief Small wrapper around a point, with an extra member "included"
\details
    It has a const-reference to the original point (so no copy here)
\tparam the enclosed point type
*/
template<typename Point>
struct douglas_peucker_point
{
    Point const& p;
    bool included;

    inline douglas_peucker_point(Point const& ap)
        : p(ap)
        , included(false)
    {}

    // Necessary for proper compilation
    inline douglas_peucker_point<Point> operator=(douglas_peucker_point<Point> const& )
    {
        return douglas_peucker_point<Point>(*this);
    }
};

template
<
    typename Point,
    typename PointDistanceStrategy,
    typename LessCompare
        = std::less
            <
                typename strategy::distance::services::return_type
                    <
                        PointDistanceStrategy,
                        Point, Point
                    >::type
            >
>
class douglas_peucker_
    : LessCompare // for empty base optimization
{
public :

    // See also ticket 5954 https://svn.boost.org/trac/boost/ticket/5954
    // Comparable is currently not possible here because it has to be compared to the squared of max_distance, and more.
    // For now we have to take the real distance.
    typedef PointDistanceStrategy distance_strategy_type;
    // typedef typename strategy::distance::services::comparable_type<PointDistanceStrategy>::type distance_strategy_type;

    typedef typename strategy::distance::services::return_type
                     <
                         distance_strategy_type,
                         Point, Point
                     >::type distance_type;

    douglas_peucker_()
    {}

    douglas_peucker_(LessCompare const& less_compare)
        : LessCompare(less_compare)
    {}

private :
    typedef douglas_peucker_point<Point> dp_point_type;
    typedef typename std::vector<dp_point_type>::iterator iterator_type;


    LessCompare const& less() const
    {
        return *this;
    }

    inline void consider(iterator_type begin,
                         iterator_type end,
                         distance_type const& max_dist,
                         int& n,
                         distance_strategy_type const& ps_distance_strategy) const
    {
        std::size_t size = end - begin;

        // size must be at least 3
        // because we want to consider a candidate point in between
        if (size <= 2)
        {
#ifdef BOOST_GEOMETRY_DEBUG_DOUGLAS_PEUCKER
            if (begin != end)
            {
                std::cout << "ignore between " << dsv(begin->p)
                    << " and " << dsv((end - 1)->p)
                    << " size=" << size << std::endl;
            }
            std::cout << "return because size=" << size << std::endl;
#endif
            return;
        }

        iterator_type last = end - 1;

#ifdef BOOST_GEOMETRY_DEBUG_DOUGLAS_PEUCKER
        std::cout << "find between " << dsv(begin->p)
            << " and " << dsv(last->p)
            << " size=" << size << std::endl;
#endif


        // Find most far point, compare to the current segment
        //geometry::segment<Point const> s(begin->p, last->p);
        distance_type md(-1.0); // any value < 0
        iterator_type candidate;
        for(iterator_type it = begin + 1; it != last; ++it)
        {
            distance_type dist = ps_distance_strategy.apply(it->p, begin->p, last->p);

#ifdef BOOST_GEOMETRY_DEBUG_DOUGLAS_PEUCKER
            std::cout << "consider " << dsv(it->p)
                << " at " << double(dist)
                << ((dist > max_dist) ? " maybe" : " no")
                << std::endl;

#endif
            if ( less()(md, dist) )
            {
                md = dist;
                candidate = it;
            }
        }

        // If a point is found, set the include flag
        // and handle segments in between recursively
        if ( less()(max_dist, md) )
        {
#ifdef BOOST_GEOMETRY_DEBUG_DOUGLAS_PEUCKER
            std::cout << "use " << dsv(candidate->p) << std::endl;
#endif

            candidate->included = true;
            n++;

            consider(begin, candidate + 1, max_dist, n, ps_distance_strategy);
            consider(candidate, end, max_dist, n, ps_distance_strategy);
        }
    }


public :

    template <typename Range, typename OutputIterator>
    inline OutputIterator apply(Range const& range,
                                OutputIterator out,
                                distance_type max_distance) const
    {
#ifdef BOOST_GEOMETRY_DEBUG_DOUGLAS_PEUCKER
            std::cout << "max distance: " << max_distance
                      << std::endl << std::endl;
#endif

        // TODO: get strategy from Strategies

        distance_strategy_type strategy;

        // Copy coordinates, a vector of references to all points
        std::vector<dp_point_type> ref_candidates(boost::begin(range),
                        boost::end(range));

        // Include first and last point of line,
        // they are always part of the line
        int n = 2;
        ref_candidates.front().included = true;
        ref_candidates.back().included = true;

        // Get points, recursively, including them if they are further away
        // than the specified distance
        consider(boost::begin(ref_candidates), boost::end(ref_candidates), max_distance, n, strategy);

        // Copy included elements to the output
        for(typename std::vector<dp_point_type>::const_iterator it
                        = boost::begin(ref_candidates);
            it != boost::end(ref_candidates);
            ++it)
        {
            if (it->included)
            {
                // copy-coordinates does not work because OutputIterator
                // does not model Point (??)
                //geometry::convert(it->p, *out);
                *out = it->p;
                out++;
            }
        }
        return out;
    }
};


/*!
\brief Implements the simplify algorithm.
\details The douglas_peucker policy simplifies a linestring, ring or
    vector of points using the well-known Douglas-Peucker algorithm.
\tparam Point the point type
\tparam PointDistanceStrategy point-segment distance strategy to be used
\note This strategy uses itself a point-segment-distance strategy which
    can be specified
\author Barend and Maarten, 1995/1996
\author Barend, revised for Generic Geometry Library, 2008
*/

/*
For the algorithm, see for example:
 - http://en.wikipedia.org/wiki/Ramer-Douglas-Peucker_algorithm
 - http://www2.dcs.hull.ac.uk/CISRG/projects/Royal-Inst/demos/dp.html
*/
template
<
    typename Point,
    typename PointDistanceStrategy
>
class douglas_peucker
{
public :

    typedef PointDistanceStrategy distance_strategy_type;

    typedef typename douglas_peucker_
        <
            Point,
            PointDistanceStrategy
        >::distance_type distance_type;

    template <typename Range, typename OutputIterator>
    static inline OutputIterator apply(Range const& range,
                                       OutputIterator out,
                                       distance_type const& max_distance)
    {
        namespace services = strategy::distance::services;

        typedef typename services::comparable_type
            <
                PointDistanceStrategy
            >::type comparable_distance_strategy_type;

        return douglas_peucker_
            <
                Point, comparable_distance_strategy_type
            >().apply(range, out,
                      services::result_from_distance
                          <
                              comparable_distance_strategy_type, Point, Point
                          >::apply(comparable_distance_strategy_type(),
                                   max_distance)
                      );
    }

};


template <typename Range, typename Strategies>
inline bool is_degenerate(Range const& range, Strategies const& strategies)
{
    return boost::size(range) == 2
        && detail::equals::equals_point_point(geometry::range::front(range),
                                              geometry::range::back(range),
                                              strategies);
}

struct simplify_range_insert
{
    template
    <
        typename Range, typename OutputIterator, typename Distance,
        typename Impl, typename Strategies
    >
    static inline void apply(Range const& range, OutputIterator out,
                             Distance const& max_distance,
                             Impl const& impl,
                             Strategies const& strategies)
    {
        if (is_degenerate(range, strategies))
        {
            std::copy(boost::begin(range), boost::begin(range) + 1, out);
        }
        else if (boost::size(range) <= 2 || max_distance < 0)
        {
            std::copy(boost::begin(range), boost::end(range), out);
        }
        else
        {
            impl.apply(range, out, max_distance); // TODO: pass strategies
        }
    }
};


struct simplify_copy
{
    template
    <
        typename RangeIn, typename RangeOut, typename Distance,
        typename Impl, typename Strategies
    >
    static inline void apply(RangeIn const& range, RangeOut& out,
                             Distance const& ,
                             Impl const& ,
                             Strategies const& )
    {
        std::copy(boost::begin(range), boost::end(range),
                  geometry::range::back_inserter(out));
    }
};


template <std::size_t MinimumToUseStrategy>
struct simplify_range
{
    template
    <
        typename RangeIn, typename RangeOut, typename Distance,
        typename Impl, typename Strategies
    >
    static inline void apply(RangeIn const& range, RangeOut& out,
                             Distance const& max_distance,
                             Impl const& impl,
                             Strategies const& strategies)
    {
        // For a RING:
        // Note that, especially if max_distance is too large,
        // the output ring might be self intersecting while the input ring is
        // not, although chances are low in normal polygons

        if (boost::size(range) <= MinimumToUseStrategy || max_distance < 0)
        {
            simplify_copy::apply(range, out, max_distance, impl, strategies);
        }
        else
        {
            simplify_range_insert::apply(range, geometry::range::back_inserter(out),
                                         max_distance, impl, strategies);
        }

        // Verify the two remaining points are equal. If so, remove one of them.
        // This can cause the output being under the minimum size
        if (is_degenerate(out, strategies))
        {
            range::resize(out, 1);
        }
    }
};

struct simplify_ring
{
private :
    template <typename Area>
    static inline int area_sign(Area const& area)
    {
        return area > 0 ? 1 : area < 0 ? -1 : 0;
    }

    template <typename Ring, typename Impl, typename Strategies>
    static std::size_t get_opposite(std::size_t index, Ring const& ring,
                                    Impl const& , Strategies const& strategies)
    {
        // TODO: get strategy from Strategies

        typename Impl::distance_strategy_type distance_strategy;

        // Verify if it is NOT the case that all points are less than the
        // simplifying distance. If so, output is empty.
        typename Impl::distance_type max_distance(-1);

        typename geometry::point_type<Ring>::type point = range::at(ring, index);
        std::size_t i = 0;
        for (typename boost::range_iterator<Ring const>::type
                it = boost::begin(ring); it != boost::end(ring); ++it, ++i)
        {
            // This actually is point-segment distance but will result
            // in point-point distance
            typename Impl::distance_type dist = distance_strategy.apply(*it, point, point);
            if (dist > max_distance)
            {
                max_distance = dist;
                index = i;
            }
        }
        return index;
    }

public :
    template <typename Ring, typename Distance, typename Impl, typename Strategies>
    static inline void apply(Ring const& ring, Ring& out, Distance const& max_distance,
                             Impl const& impl, Strategies const& strategies)
    {
        std::size_t const size = boost::size(ring);
        if (size == 0)
        {
            return;
        }

        // TODO: use calculate_point_order
        // TODO: pass strategies

        int const input_sign = area_sign(geometry::area(ring));

        std::set<std::size_t> visited_indexes;

        // Rotate it into a copied vector
        // (vector, because source type might not support rotation)
        // (duplicate end point will be simplified away)
        typedef typename geometry::point_type<Ring>::type point_type;

        std::vector<point_type> rotated(size);

        // Closing point (but it will not start here)
        std::size_t index = 0;

        // Iterate (usually one iteration is enough)
        for (std::size_t iteration = 0; iteration < 4u; iteration++)
        {
            // Always take the opposite. Opposite guarantees that no point
            // "halfway" is chosen, creating an artefact (very narrow triangle)
            // Iteration 0: opposite to closing point (1/2, = on convex hull)
            //              (this will start simplification with that point
            //               and its opposite ~0)
            // Iteration 1: move a quarter on that ring, then opposite to 1/4
            //              (with its opposite 3/4)
            // Iteration 2: move an eight on that ring, then opposite (1/8)
            // Iteration 3: again move a quarter, then opposite (7/8)
            // So finally 8 "sides" of the ring have been examined (if it were
            // a semi-circle). Most probably, there are only 0 or 1 iterations.
            switch (iteration)
            {
                case 1 : index = (index + size / 4) % size; break;
                case 2 : index = (index + size / 8) % size; break;
                case 3 : index = (index + size / 4) % size; break;
            }
            index = get_opposite(index, ring, impl, strategies);

            if (visited_indexes.count(index) > 0)
            {
                // Avoid trying the same starting point more than once
                continue;
            }

            std::rotate_copy(boost::begin(ring), range::pos(ring, index),
                             boost::end(ring), rotated.begin());

            // Close the rotated copy
            rotated.push_back(range::at(ring, index));

            simplify_range<0>::apply(rotated, out, max_distance, impl, strategies);

            // TODO: use calculate_point_order
            // TODO: pass strategies

            // Verify that what was positive, stays positive (or goes to 0)
            // and what was negative stays negative (or goes to 0)
            int const output_sign = area_sign(geometry::area(out));
            if (output_sign == input_sign)
            {
                // Result is considered as satisfactory (usually this is the
                // first iteration - only for small rings, having a scale
                // similar to simplify_distance, next iterations are tried
                return;
            }

            // Original is simplified away. Possibly there is a solution
            // when another starting point is used
            geometry::clear(out);

            // TODO: pass strategies

            if (iteration == 0
                && geometry::perimeter(ring) < 3 * max_distance)
            {
                // Check if it is useful to iterate. A minimal triangle has a
                // perimeter of a bit more than 3 times the simplify distance
                return;
            }

            // Prepare next try
            visited_indexes.insert(index);
            rotated.resize(size);
        }
    }
};


struct simplify_polygon
{
private:

    template
    <
        typename IteratorIn,
        typename InteriorRingsOut,
        typename Distance,
        typename Impl,
        typename Strategies
    >
    static inline void iterate(IteratorIn begin, IteratorIn end,
                               InteriorRingsOut& interior_rings_out,
                               Distance const& max_distance,
                               Impl const& impl, Strategies const& strategies)
    {
        typedef typename boost::range_value<InteriorRingsOut>::type single_type;
        for (IteratorIn it = begin; it != end; ++it)
        {
            single_type out;
            simplify_ring::apply(*it, out, max_distance, impl, strategies);
            if (! geometry::is_empty(out))
            {
                range::push_back(interior_rings_out, out);
            }
        }
    }

    template
    <
        typename InteriorRingsIn,
        typename InteriorRingsOut,
        typename Distance,
        typename Impl,
        typename Strategies
    >
    static inline void apply_interior_rings(InteriorRingsIn const& interior_rings_in,
                                            InteriorRingsOut& interior_rings_out,
                                            Distance const& max_distance,
                                            Impl const& impl, Strategies const& strategies)
    {
        range::clear(interior_rings_out);

        iterate(boost::begin(interior_rings_in), boost::end(interior_rings_in),
                interior_rings_out,
                max_distance,
                impl, strategies);
    }

public:
    template <typename Polygon, typename Distance, typename Impl, typename Strategies>
    static inline void apply(Polygon const& poly_in, Polygon& poly_out,
                             Distance const& max_distance,
                             Impl const& impl, Strategies const& strategies)
    {
        // Note that if there are inner rings, and distance is too large,
        // they might intersect with the outer ring in the output,
        // while it didn't in the input.
        simplify_ring::apply(exterior_ring(poly_in), exterior_ring(poly_out),
                             max_distance, impl, strategies);

        apply_interior_rings(interior_rings(poly_in), interior_rings(poly_out),
                             max_distance, impl, strategies);
    }
};


template<typename Policy>
struct simplify_multi
{
    template <typename MultiGeometry, typename Distance, typename Impl, typename Strategies>
    static inline void apply(MultiGeometry const& multi, MultiGeometry& out,
                             Distance const& max_distance,
                             Impl const& impl, Strategies const& strategies)
    {
        range::clear(out);

        typedef typename boost::range_value<MultiGeometry>::type single_type;

        for (typename boost::range_iterator<MultiGeometry const>::type
                it = boost::begin(multi); it != boost::end(multi); ++it)
        {
            single_type single_out;
            Policy::apply(*it, single_out, max_distance, impl, strategies);
            if (! geometry::is_empty(single_out))
            {
                range::push_back(out, single_out);
            }
        }
    }
};


}} // namespace detail::simplify
#endif // DOXYGEN_NO_DETAIL


#ifndef DOXYGEN_NO_DISPATCH
namespace dispatch
{

template
<
    typename Geometry,
    typename Tag = typename tag<Geometry>::type
>
struct simplify: not_implemented<Tag>
{};

template <typename Point>
struct simplify<Point, point_tag>
{
    template <typename Distance, typename Impl, typename Strategy>
    static inline void apply(Point const& point, Point& out, Distance const& ,
                             Impl const& , Strategy const& )
    {
        geometry::convert(point, out);
    }
};

// Linestring, keep 2 points (unless those points are the same)
template <typename Linestring>
struct simplify<Linestring, linestring_tag>
    : detail::simplify::simplify_range<2>
{};

template <typename Ring>
struct simplify<Ring, ring_tag>
    : detail::simplify::simplify_ring
{};

template <typename Polygon>
struct simplify<Polygon, polygon_tag>
    : detail::simplify::simplify_polygon
{};


template
<
    typename Geometry,
    typename Tag = typename tag<Geometry>::type
>
struct simplify_insert: not_implemented<Tag>
{};


template <typename Linestring>
struct simplify_insert<Linestring, linestring_tag>
    : detail::simplify::simplify_range_insert
{};

template <typename Ring>
struct simplify_insert<Ring, ring_tag>
    : detail::simplify::simplify_range_insert
{};

template <typename MultiPoint>
struct simplify<MultiPoint, multi_point_tag>
    : detail::simplify::simplify_copy
{};


template <typename MultiLinestring>
struct simplify<MultiLinestring, multi_linestring_tag>
    : detail::simplify::simplify_multi<detail::simplify::simplify_range<2> >
{};


template <typename MultiPolygon>
struct simplify<MultiPolygon, multi_polygon_tag>
    : detail::simplify::simplify_multi<detail::simplify::simplify_polygon>
{};


} // namespace dispatch
#endif // DOXYGEN_NO_DISPATCH


namespace resolve_strategy
{

template
<
    typename Strategies,
    bool IsUmbrella = strategies::detail::is_umbrella_strategy<Strategies>::value
>
struct simplify
{
    template <typename Geometry, typename Distance>
    static inline void apply(Geometry const& geometry,
                             Geometry& out,
                             Distance const& max_distance,
                             Strategies const& strategies)
    {
        typedef typename point_type<Geometry>::type point_type;
        typedef model::segment<point_type> segment_type; // dummy

        dispatch::simplify
            <
                Geometry
            >::apply(geometry, out, max_distance,
                     detail::simplify::douglas_peucker
                        <
                            point_type,
                            decltype(strategies.distance(std::declval<point_type>(),
                                                         std::declval<segment_type>()))
                        >(),
                     strategies);
    }
};

template <typename Strategy>
struct simplify<Strategy, false>
{
    template <typename Geometry, typename Distance, typename Strategy>
    static inline void apply(Geometry const& geometry,
                             Geometry& out,
                             Distance const& max_distance,
                             Strategy const& strategy)
    {
        using strategies::simplify::services::strategy_converter;

        simplify
            <
                decltype(strategy_converter<Strategy>::get(strategy))
            >::apply(geometry, out, max_distance,
                     strategy_converter<Strategy>::get(strategy));
    }
};

template <>
struct simplify<default_strategy, false>
{
    template <typename Geometry, typename Distance>
    static inline void apply(Geometry const& geometry,
                             Geometry& out,
                             Distance const& max_distance,
                             default_strategy)
    {
        typedef typename strategies::simplify::services::default_strategy
            <
                Geometry
            >::type strategy_type;

        simplify
            <
                strategy_type
            >::apply(geometry, out, max_distance, strategy_type());
    }
};

template
<
    typename Strategies,
    bool IsUmbrella = strategies::detail::is_umbrella_strategy<Strategies>::value
>
struct simplify_insert
{
    template<typename Geometry, typename OutputIterator, typename Distance>
    static inline void apply(Geometry const& geometry,
                             OutputIterator& out,
                             Distance const& max_distance,
                             Strategies const& strategies)
    {
        typedef typename point_type<Geometry>::type point_type;
        typedef model::segment<point_type> segment_type; // dummy

        dispatch::simplify_insert
            <
                Geometry
            >::apply(geometry, out, max_distance,
                     detail::simplify::douglas_peucker
                        <
                            point_type,
                            decltype(strategies.distance(std::declval<point_type>(),
                                                         std::declval<segment_type>()))
                        >(),
                     strategies);
    }
};

template <typename Strategy>
struct simplify_insert<Strategy, false>
{
    template<typename Geometry, typename OutputIterator, typename Distance>
    static inline void apply(Geometry const& geometry,
                             OutputIterator& out,
                             Distance const& max_distance,
                             Strategy const& strategy)
    {
        using strategies::simplify::services::strategy_converter;

        simplify_insert
            <
                decltype(strategy_converter<Strategy>::get(strategy))
            >::apply(geometry, out, max_distance,
                     decltype(strategy_converter<Strategy>::get(strategy)));
    }
};

template <>
struct simplify_insert<default_strategy, false>
{
    template <typename Geometry, typename OutputIterator, typename Distance>
    static inline void apply(Geometry const& geometry,
                             OutputIterator& out,
                             Distance const& max_distance,
                             default_strategy)
    {
        typedef typename strategies::simplify::services::default_strategy
            <
                Geometry
            >::type strategy_type;
        
        simplify_insert
            <
                strategy_type
            >::apply(geometry, out, max_distance, strategy_type());
    }
};

} // namespace resolve_strategy


namespace resolve_variant {

template <typename Geometry>
struct simplify
{
    template <typename Distance, typename Strategy>
    static inline void apply(Geometry const& geometry,
                             Geometry& out,
                             Distance const& max_distance,
                             Strategy const& strategy)
    {
        resolve_strategy::simplify<Strategy>::apply(geometry, out, max_distance, strategy);
    }
};

template <BOOST_VARIANT_ENUM_PARAMS(typename T)>
struct simplify<boost::variant<BOOST_VARIANT_ENUM_PARAMS(T)> >
{
    template <typename Distance, typename Strategy>
    struct visitor: boost::static_visitor<void>
    {
        Distance const& m_max_distance;
        Strategy const& m_strategy;

        visitor(Distance const& max_distance, Strategy const& strategy)
            : m_max_distance(max_distance)
            , m_strategy(strategy)
        {}

        template <typename Geometry>
        void operator()(Geometry const& geometry, Geometry& out) const
        {
            simplify<Geometry>::apply(geometry, out, m_max_distance, m_strategy);
        }
    };

    template <typename Distance, typename Strategy>
    static inline void
    apply(boost::variant<BOOST_VARIANT_ENUM_PARAMS(T)> const& geometry,
          boost::variant<BOOST_VARIANT_ENUM_PARAMS(T)>& out,
          Distance const& max_distance,
          Strategy const& strategy)
    {
        boost::apply_visitor(
            visitor<Distance, Strategy>(max_distance, strategy),
            geometry,
            out
        );
    }
};

} // namespace resolve_variant


/*!
\brief Simplify a geometry using a specified strategy
\ingroup simplify
\tparam Geometry \tparam_geometry
\tparam Distance A numerical distance measure
\tparam Strategy A type fulfilling a SimplifyStrategy concept
\param strategy A strategy to calculate simplification
\param geometry input geometry, to be simplified
\param out output geometry, simplified version of the input geometry
\param max_distance distance (in units of input coordinates) of a vertex
    to other segments to be removed
\param strategy simplify strategy to be used for simplification, might
    include point-distance strategy

\image html svg_simplify_country.png "The image below presents the simplified country"
\qbk{distinguish,with strategy}
*/
template<typename Geometry, typename Distance, typename Strategy>
inline void simplify(Geometry const& geometry, Geometry& out,
                     Distance const& max_distance, Strategy const& strategy)
{
    concepts::check<Geometry>();

    geometry::clear(out);

    resolve_variant::simplify<Geometry>::apply(geometry, out, max_distance, strategy);
}




/*!
\brief Simplify a geometry
\ingroup simplify
\tparam Geometry \tparam_geometry
\tparam Distance \tparam_numeric
\note This version of simplify simplifies a geometry using the default
    strategy (Douglas Peucker),
\param geometry input geometry, to be simplified
\param out output geometry, simplified version of the input geometry
\param max_distance distance (in units of input coordinates) of a vertex
    to other segments to be removed

\qbk{[include reference/algorithms/simplify.qbk]}
 */
template<typename Geometry, typename Distance>
inline void simplify(Geometry const& geometry, Geometry& out,
                     Distance const& max_distance)
{
    concepts::check<Geometry>();

    geometry::simplify(geometry, out, max_distance, default_strategy());
}


#ifndef DOXYGEN_NO_DETAIL
namespace detail { namespace simplify
{


/*!
\brief Simplify a geometry, using an output iterator
    and a specified strategy
\ingroup simplify
\tparam Geometry \tparam_geometry
\param geometry input geometry, to be simplified
\param out output iterator, outputs all simplified points
\param max_distance distance (in units of input coordinates) of a vertex
    to other segments to be removed
\param strategy simplify strategy to be used for simplification,
    might include point-distance strategy

\qbk{distinguish,with strategy}
\qbk{[include reference/algorithms/simplify.qbk]}
*/
template<typename Geometry, typename OutputIterator, typename Distance, typename Strategy>
inline void simplify_insert(Geometry const& geometry, OutputIterator out,
                            Distance const& max_distance, Strategy const& strategy)
{
    concepts::check<Geometry const>();

    resolve_strategy::simplify_insert<Strategy>::apply(geometry, out, max_distance, strategy);
}

/*!
\brief Simplify a geometry, using an output iterator
\ingroup simplify
\tparam Geometry \tparam_geometry
\param geometry input geometry, to be simplified
\param out output iterator, outputs all simplified points
\param max_distance distance (in units of input coordinates) of a vertex
    to other segments to be removed

\qbk{[include reference/algorithms/simplify_insert.qbk]}
 */
template<typename Geometry, typename OutputIterator, typename Distance>
inline void simplify_insert(Geometry const& geometry, OutputIterator out,
                            Distance const& max_distance)
{
    // Concept: output point type = point type of input geometry
    concepts::check<Geometry const>();
    concepts::check<typename point_type<Geometry>::type>();

    simplify_insert(geometry, out, max_distance, default_strategy());
}

}} // namespace detail::simplify
#endif // DOXYGEN_NO_DETAIL



}} // namespace boost::geometry

#endif // BOOST_GEOMETRY_ALGORITHMS_SIMPLIFY_HPP
