/**
 * \file
 * \author Norihiro Watanabe
 * \date   2013-08-13
 * \brief
 *
 * \copyright
 * Copyright (c) 2013, OpenGeoSys Community (http://www.opengeosys.org)
 *            Distributed under a Modified BSD License.
 *              See accompanying file LICENSE.txt or
 *              http://www.opengeosys.org/project/license
 *
 */

#ifndef SHAPETRI3_H_
#define SHAPETRI3_H_

namespace NumLib
{

/**
 *  Shape function for a triangle element of three nodes in natural coordinates
 *
 *  3 (0,1)
 *    *
 *  s | \
 *    |   \
 *    |     \
 *    *––––––* r
 *  1 (0,0)    2 (1,0)
 */
class ShapeTri3
{
public:
    /**
     * Evaluate the shape function at the given point
     *
     * @param [in]  r    point coordinates
     * @param [out] N   a vector of calculated shape function.
     */
    template <class T_X, class T_N>
    static void computeShapeFunction(const T_X &r, T_N &N3);

    /**
     * Evaluate derivatives of the shape function at the given point
     *
     * @param [in]  r    point coordinates
     * @param [out] dN  a matrix of the derivatives
     */
    template <class T_X, class T_N>
    static void computeGradShapeFunction(const T_X &/*r*/, T_N &dN6);
};

}

#include "ShapeTri3-impl.h"

#endif //SHAPETRI3_H_
