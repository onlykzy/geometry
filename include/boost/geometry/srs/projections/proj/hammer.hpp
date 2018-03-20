// Boost.Geometry - gis-projections (based on PROJ4)

// Copyright (c) 2008-2015 Barend Gehrels, Amsterdam, the Netherlands.

// This file was modified by Oracle on 2017, 2018.
// Modifications copyright (c) 2017-2018, Oracle and/or its affiliates.
// Contributed and/or modified by Adam Wulkiewicz, on behalf of Oracle.

// Use, modification and distribution is subject to the Boost Software License,
// Version 1.0. (See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

// This file is converted from PROJ4, http://trac.osgeo.org/proj
// PROJ4 is originally written by Gerald Evenden (then of the USGS)
// PROJ4 is maintained by Frank Warmerdam
// PROJ4 is converted to Boost.Geometry by Barend Gehrels

// Last updated version of proj: 5.0.0

// Original copyright notice:

// Permission is hereby granted, free of charge, to any person obtaining a
// copy of this software and associated documentation files (the "Software"),
// to deal in the Software without restriction, including without limitation
// the rights to use, copy, modify, merge, publish, distribute, sublicense,
// and/or sell copies of the Software, and to permit persons to whom the
// Software is furnished to do so, subject to the following conditions:

// The above copyright notice and this permission notice shall be included
// in all copies or substantial portions of the Software.

// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
// OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
// THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
// FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
// DEALINGS IN THE SOFTWARE.

#ifndef BOOST_GEOMETRY_PROJECTIONS_HAMMER_HPP
#define BOOST_GEOMETRY_PROJECTIONS_HAMMER_HPP

#include <boost/geometry/srs/projections/impl/base_static.hpp>
#include <boost/geometry/srs/projections/impl/base_dynamic.hpp>
#include <boost/geometry/srs/projections/impl/projects.hpp>
#include <boost/geometry/srs/projections/impl/factory_entry.hpp>

namespace boost { namespace geometry
{

namespace srs { namespace par4
{
    struct hammer {}; // Hammer & Eckert-Greifendorff

}} //namespace srs::par4

namespace projections
{
    #ifndef DOXYGEN_NO_DETAIL
    namespace detail { namespace hammer
    {
            static const double EPS = 1.0e-10;

            template <typename T>
            struct par_hammer
            {
                T w;
                T m, rm;
            };

            // template class, using CRTP to implement forward/inverse
            template <typename CalculationType, typename Parameters>
            struct base_hammer_spheroid : public base_t_fi<base_hammer_spheroid<CalculationType, Parameters>,
                     CalculationType, Parameters>
            {

                typedef CalculationType geographic_type;
                typedef CalculationType cartesian_type;

                par_hammer<CalculationType> m_proj_parm;

                inline base_hammer_spheroid(const Parameters& par)
                    : base_t_fi<base_hammer_spheroid<CalculationType, Parameters>,
                     CalculationType, Parameters>(*this, par) {}

                // FORWARD(s_forward)  spheroid
                // Project coordinates from geographic (lon, lat) to cartesian (x, y)
                inline void fwd(geographic_type& lp_lon, geographic_type& lp_lat, cartesian_type& xy_x, cartesian_type& xy_y) const
                {
                    CalculationType cosphi, d;

                    d = sqrt(2./(1. + (cosphi = cos(lp_lat)) * cos(lp_lon *= this->m_proj_parm.w)));
                    xy_x = this->m_proj_parm.m * d * cosphi * sin(lp_lon);
                    xy_y = this->m_proj_parm.rm * d * sin(lp_lat);
                }

                // INVERSE(s_inverse)  spheroid
                // Project coordinates from cartesian (x, y) to geographic (lon, lat)
                inline void inv(cartesian_type& xy_x, cartesian_type& xy_y, geographic_type& lp_lon, geographic_type& lp_lat) const
                {
                    CalculationType z;

                    z = sqrt(1. - 0.25*this->m_proj_parm.w*this->m_proj_parm.w*xy_x*xy_x - 0.25*xy_y*xy_y);
                    if (geometry::math::abs(2.*z*z-1.) < EPS) {
                        lp_lon = HUGE_VAL;
                        lp_lat = HUGE_VAL;
                        BOOST_THROW_EXCEPTION( projection_exception(-14) );
                    } else {
                        lp_lon = aatan2(this->m_proj_parm.w * xy_x * z,2. * z * z - 1)/this->m_proj_parm.w;
                        lp_lat = aasin(z * xy_y);
                    }
                }

                static inline std::string get_name()
                {
                    return "hammer_spheroid";
                }

            };

            // Hammer & Eckert-Greifendorff
            template <typename Parameters, typename T>
            inline void setup_hammer(Parameters& par, par_hammer<T>& proj_parm)
            {
                T tmp;

                if (pj_param_f(par.params, "W", tmp)) {
                    if ((proj_parm.w = fabs(tmp)) <= 0.)
                        BOOST_THROW_EXCEPTION( projection_exception(-27) );
                } else
                    proj_parm.w = .5;
                if (pj_param_f(par.params, "M", tmp)) {
                    if ((proj_parm.m = fabs(tmp)) <= 0.)
                        BOOST_THROW_EXCEPTION( projection_exception(-27) );
                } else
                    proj_parm.m = 1.;

                proj_parm.rm = 1. / proj_parm.m;
                proj_parm.m /= proj_parm.w;

                par.es = 0.;
            }

    }} // namespace detail::hammer
    #endif // doxygen

    /*!
        \brief Hammer & Eckert-Greifendorff projection
        \ingroup projections
        \tparam Geographic latlong point type
        \tparam Cartesian xy point type
        \tparam Parameters parameter type
        \par Projection characteristics
         - Miscellaneous
         - Spheroid
         - no inverse
        \par Projection parameters
         - W (real)
         - M (real)
        \par Example
        \image html ex_hammer.gif
    */
    template <typename CalculationType, typename Parameters>
    struct hammer_spheroid : public detail::hammer::base_hammer_spheroid<CalculationType, Parameters>
    {
        inline hammer_spheroid(const Parameters& par) : detail::hammer::base_hammer_spheroid<CalculationType, Parameters>(par)
        {
            detail::hammer::setup_hammer(this->m_par, this->m_proj_parm);
        }
    };

    #ifndef DOXYGEN_NO_DETAIL
    namespace detail
    {

        // Static projection
        BOOST_GEOMETRY_PROJECTIONS_DETAIL_STATIC_PROJECTION(srs::par4::hammer, hammer_spheroid, hammer_spheroid)

        // Factory entry(s)
        template <typename CalculationType, typename Parameters>
        class hammer_entry : public detail::factory_entry<CalculationType, Parameters>
        {
            public :
                virtual base_v<CalculationType, Parameters>* create_new(const Parameters& par) const
                {
                    return new base_v_fi<hammer_spheroid<CalculationType, Parameters>, CalculationType, Parameters>(par);
                }
        };

        template <typename CalculationType, typename Parameters>
        inline void hammer_init(detail::base_factory<CalculationType, Parameters>& factory)
        {
            factory.add_to_factory("hammer", new hammer_entry<CalculationType, Parameters>);
        }

    } // namespace detail
    #endif // doxygen

} // namespace projections

}} // namespace boost::geometry

#endif // BOOST_GEOMETRY_PROJECTIONS_HAMMER_HPP

