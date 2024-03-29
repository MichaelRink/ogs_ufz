/**
 * \copyright
 * Copyright (c) 2013, OpenGeoSys Community (http://www.opengeosys.org)
 *            Distributed under a Modified BSD License.
 *              See accompanying file LICENSE.txt or
 *              http://www.opengeosys.org/LICENSE.txt
 */

#include <gtest/gtest.h>

#include <limits>
#include <algorithm>
#include <vector>

#ifdef OGS_USE_EIGEN
#include <Eigen/Eigen>
#endif

#include "NumLib/Fem/CoordinatesMapping/ShapeMatrices.h"
#include "NumLib/Fem/CoordinatesMapping/NaturalCoordinatesMapping.h"

#include "CoordinatesMappingTestData/TestLine2.h"
#include "CoordinatesMappingTestData/TestTri3.h"
#include "CoordinatesMappingTestData/TestQuad4.h"
#include "CoordinatesMappingTestData/TestHex8.h"

#include "../TestTools.h"

using namespace NumLib;
using namespace CoordinatesMappingTestData;


namespace
{

// Element types to be tested
typedef ::testing::Types<
        TestLine2,
        TestTri3,
        TestQuad4,
        TestHex8
        > TestTypes;
}

template <class T_TEST>
class NumLibFemNaturalCoordinatesMappingTest : public ::testing::Test, public T_TEST
{
public:
    typedef typename T_TEST::ElementType ElementType;
    typedef typename T_TEST::ShapeFunctionType ShapeFunctionType;
    static const unsigned dim = T_TEST::dim;
    static const unsigned e_nnodes = T_TEST::e_nnodes;
    // Matrix types
#ifdef OGS_USE_EIGEN
    typedef Eigen::Matrix<double, e_nnodes, 1> NodalVector;
    typedef Eigen::Matrix<double, dim, e_nnodes, Eigen::RowMajor> DimNodalMatrix;
    typedef Eigen::Matrix<double, dim, dim, Eigen::RowMajor> DimMatrix;
#endif
    // Shape data type
    typedef ShapeMatrices<NodalVector,DimNodalMatrix,DimMatrix> ShapeMatricesType;
    // Natural coordinates mapping type
    typedef NaturalCoordinatesMapping<ElementType, ShapeFunctionType, ShapeMatricesType> NaturalCoordsMappingType;

public:
    NumLibFemNaturalCoordinatesMappingTest()
    : eps(std::numeric_limits<double>::epsilon())
    {
        // create four elements used for testing
        naturalEle   = this->createNaturalShape();
        irregularEle = this->createIrregularShape();
        clockwiseEle  = this->createClockWise();
        zeroVolumeEle  = this->createZeroVolume();

        // for destructor
        vec_eles.push_back(naturalEle);
        vec_eles.push_back(irregularEle);
        vec_eles.push_back(clockwiseEle);
        vec_eles.push_back(zeroVolumeEle);
        for (auto e : vec_eles)
            for (unsigned i=0; i<e->getNNodes(true); i++)
                vec_nodes.push_back(e->getNode(i));
    }

    ~NumLibFemNaturalCoordinatesMappingTest()
    {
        for (auto itr = vec_nodes.begin(); itr!=vec_nodes.end(); ++itr )
            delete *itr;
        for (auto itr = vec_eles.begin(); itr!=vec_eles.end(); ++itr )
            delete *itr;
    }

    const double eps;
    std::vector<const MeshLib::Node*> vec_nodes;
    std::vector<const ElementType*> vec_eles;
    ElementType* naturalEle;
    ElementType* irregularEle;
    ElementType* clockwiseEle;
    ElementType* zeroVolumeEle;
};

TYPED_TEST_CASE(NumLibFemNaturalCoordinatesMappingTest, TestTypes);

TYPED_TEST(NumLibFemNaturalCoordinatesMappingTest, CheckFieldSpecification_N)
{
    typedef typename TestFixture::ShapeMatricesType ShapeMatricesType;
    typedef typename TestFixture::NaturalCoordsMappingType NaturalCoordsMappingType;
    ShapeMatricesType shape(this->dim, this->e_nnodes);

    //only N
    NaturalCoordsMappingType::template computeShapeMatrices<ShapeMatrixType::N>(*this->naturalEle, this->r, shape);
    ASSERT_FALSE(shape.N.isZero());
    ASSERT_TRUE(shape.dNdr.isZero());
    ASSERT_TRUE(shape.J.isZero());
    ASSERT_TRUE(shape.detJ == .0);
    ASSERT_TRUE(shape.invJ.isZero());
    ASSERT_TRUE(shape.dNdx.isZero());
}

TYPED_TEST(NumLibFemNaturalCoordinatesMappingTest, CheckFieldSpecification_DNDR)
{
    typedef typename TestFixture::ShapeMatricesType ShapeMatricesType;
    typedef typename TestFixture::NaturalCoordsMappingType NaturalCoordsMappingType;
    ShapeMatricesType shape(this->dim, this->e_nnodes);

    // dNdr
    NaturalCoordsMappingType::template computeShapeMatrices<ShapeMatrixType::DNDR>(*this->naturalEle, this->r, shape);
    ASSERT_TRUE(shape.N.isZero());
    ASSERT_FALSE(shape.dNdr.isZero());
    ASSERT_TRUE(shape.J.isZero());
    ASSERT_TRUE(shape.detJ == .0);
    ASSERT_TRUE(shape.invJ.isZero());
    ASSERT_TRUE(shape.dNdx.isZero());
}

TYPED_TEST(NumLibFemNaturalCoordinatesMappingTest, CheckFieldSpecification_N_J)
{
    typedef typename TestFixture::ShapeMatricesType ShapeMatricesType;
    typedef typename TestFixture::NaturalCoordsMappingType NaturalCoordsMappingType;
    ShapeMatricesType shape(this->dim, this->e_nnodes);

    // N_J
    shape.setZero();
    NaturalCoordsMappingType::template computeShapeMatrices<ShapeMatrixType::N_J>(*this->naturalEle, this->r, shape);
    ASSERT_FALSE(shape.N.isZero());
    ASSERT_FALSE(shape.dNdr.isZero());
    ASSERT_FALSE(shape.J.isZero());
    ASSERT_FALSE(shape.detJ == .0);
    ASSERT_TRUE(shape.invJ.isZero());
    ASSERT_TRUE(shape.dNdx.isZero());
}

TYPED_TEST(NumLibFemNaturalCoordinatesMappingTest, CheckFieldSpecification_DNDR_J)
{
    typedef typename TestFixture::ShapeMatricesType ShapeMatricesType;
    typedef typename TestFixture::NaturalCoordsMappingType NaturalCoordsMappingType;
    ShapeMatricesType shape(this->dim, this->e_nnodes);

    // dNdr, J
    NaturalCoordsMappingType::template computeShapeMatrices<ShapeMatrixType::DNDR_J>(*this->naturalEle, this->r, shape);
    ASSERT_TRUE(shape.N.isZero());
    ASSERT_FALSE(shape.dNdr.isZero());
    ASSERT_FALSE(shape.J.isZero());
    ASSERT_FALSE(shape.detJ == .0);
    ASSERT_TRUE(shape.invJ.isZero());
    ASSERT_TRUE(shape.dNdx.isZero());
}

TYPED_TEST(NumLibFemNaturalCoordinatesMappingTest, CheckFieldSpecification_DNDX)
{
    typedef typename TestFixture::ShapeMatricesType ShapeMatricesType;
    typedef typename TestFixture::NaturalCoordsMappingType NaturalCoordsMappingType;
    ShapeMatricesType shape(this->dim, this->e_nnodes);

    // DNDX
    shape.setZero();
    NaturalCoordsMappingType::template computeShapeMatrices<ShapeMatrixType::DNDX>(*this->naturalEle, this->r, shape);
    ASSERT_TRUE(shape.N.isZero());
    ASSERT_FALSE(shape.dNdr.isZero());
    ASSERT_FALSE(shape.J.isZero());
    ASSERT_FALSE(shape.detJ == .0);
    ASSERT_FALSE(shape.invJ.isZero());
    ASSERT_FALSE(shape.dNdx.isZero());
}

TYPED_TEST(NumLibFemNaturalCoordinatesMappingTest, CheckFieldSpecification_ALL)
{
    typedef typename TestFixture::ShapeMatricesType ShapeMatricesType;
    typedef typename TestFixture::NaturalCoordsMappingType NaturalCoordsMappingType;
    ShapeMatricesType shape(this->dim, this->e_nnodes);

    // ALL
    shape.setZero();
    NaturalCoordsMappingType::computeShapeMatrices(*this->naturalEle, this->r, shape);
    ASSERT_FALSE(shape.N.isZero());
    ASSERT_FALSE(shape.dNdr.isZero());
    ASSERT_FALSE(shape.J.isZero());
    ASSERT_FALSE(shape.detJ == .0);
    ASSERT_FALSE(shape.invJ.isZero());
    ASSERT_FALSE(shape.dNdx.isZero());
}

TYPED_TEST(NumLibFemNaturalCoordinatesMappingTest, CheckNaturalShape)
{
    typedef typename TestFixture::ShapeMatricesType ShapeMatricesType;
    typedef typename TestFixture::NaturalCoordsMappingType NaturalCoordsMappingType;

    // identical to natural coordinates
    ShapeMatricesType shape(this->dim, this->e_nnodes);
    NaturalCoordsMappingType::computeShapeMatrices(*this->naturalEle, this->r, shape);
    double exp_J[TestFixture::dim*TestFixture::dim]= {0.0};
    for (unsigned i=0; i<this->dim; i++)
        exp_J[i+this->dim*i] = 1.0;

    ASSERT_ARRAY_NEAR(this->nat_exp_N, shape.N.data(), shape.N.size(), this->eps);
    ASSERT_ARRAY_NEAR(this->nat_exp_dNdr, shape.dNdr.data(), shape.dNdr.size(), this->eps);
    ASSERT_ARRAY_NEAR(exp_J, shape.J.data(), shape.J.size(), this->eps);
    ASSERT_ARRAY_NEAR(exp_J, shape.invJ.data(), shape.invJ.size(), this->eps);
    ASSERT_NEAR(1.0, shape.detJ, this->eps);
    ASSERT_ARRAY_NEAR(this->nat_exp_dNdr, shape.dNdx.data(), shape.dNdx.size(), this->eps);
}

TYPED_TEST(NumLibFemNaturalCoordinatesMappingTest, CheckIrregularShape)
{
    typedef typename TestFixture::ShapeMatricesType ShapeMatricesType;
    typedef typename TestFixture::NaturalCoordsMappingType NaturalCoordsMappingType;

    // irregular shape
    ShapeMatricesType shape(this->dim, this->e_nnodes);
    NaturalCoordsMappingType::computeShapeMatrices(*this->irregularEle, this->r, shape);
    //std::cout <<  std::setprecision(16) << shape;

    ASSERT_ARRAY_NEAR(this->nat_exp_N, shape.N.data(), shape.N.size(), this->eps);
    ASSERT_ARRAY_NEAR(this->nat_exp_dNdr, shape.dNdr.data(), shape.dNdr.size(), this->eps);
    ASSERT_ARRAY_NEAR(this->ir_exp_J, shape.J.data(), shape.J.size(), this->eps);
    ASSERT_NEAR(this->ir_exp_detJ, shape.detJ, this->eps);
    ASSERT_ARRAY_NEAR(this->ir_exp_invJ, shape.invJ.data(), shape.invJ.size(), this->eps);
    ASSERT_ARRAY_NEAR(this->ir_exp_dNdx, shape.dNdx.data(), shape.dNdx.size(), this->eps);
}

TYPED_TEST(NumLibFemNaturalCoordinatesMappingTest, CheckClockwise)
{
    typedef typename TestFixture::ShapeMatricesType ShapeMatricesType;
    typedef typename TestFixture::NaturalCoordsMappingType NaturalCoordsMappingType;

    // clockwise node ordering, which is invalid)
    ShapeMatricesType shape(this->dim, this->e_nnodes);
    NaturalCoordsMappingType::computeShapeMatrices(*this->clockwiseEle, this->r, shape);
    //std::cout <<  std::setprecision(16) << shape;
    // Inverse of the Jacobian matrix doesn't exist
    double exp_invJ[TestFixture::dim*TestFixture::dim]= {0.0};
    double exp_dNdx[TestFixture::dim*TestFixture::e_nnodes]= {0.0};

    ASSERT_ARRAY_NEAR(this->nat_exp_N, shape.N.data(), shape.N.size(), this->eps);
    ASSERT_ARRAY_NEAR(this->nat_exp_dNdr, shape.dNdr.data(), shape.dNdr.size(), this->eps);
    ASSERT_ARRAY_NEAR(this->cl_exp_J, shape.J.data(), shape.J.size(), this->eps);
    ASSERT_NEAR(this->cl_exp_detJ, shape.detJ, this->eps);
    ASSERT_ARRAY_NEAR(exp_invJ, shape.invJ.data(), shape.invJ.size(), this->eps);
    ASSERT_ARRAY_NEAR(exp_dNdx, shape.dNdx.data(), shape.dNdx.size(), this->eps);
}

TYPED_TEST(NumLibFemNaturalCoordinatesMappingTest, CheckZeroVolume)
{
    typedef typename TestFixture::ShapeMatricesType ShapeMatricesType;
    typedef typename TestFixture::NaturalCoordsMappingType NaturalCoordsMappingType;

    ShapeMatricesType shape(this->dim, this->e_nnodes);
    NaturalCoordsMappingType::computeShapeMatrices(*this->zeroVolumeEle, this->r, shape);
    //std::cout <<  std::setprecision(16) << shape;
    // Inverse of the Jacobian matrix doesn't exist
    double exp_invJ[TestFixture::dim*TestFixture::dim]= {0.0};
    double exp_dNdx[TestFixture::dim*TestFixture::e_nnodes]= {0.0};

    ASSERT_ARRAY_NEAR(this->nat_exp_N, shape.N.data(), shape.N.size(), this->eps);
    ASSERT_ARRAY_NEAR(this->nat_exp_dNdr, shape.dNdr.data(), shape.dNdr.size(), this->eps);
    ASSERT_ARRAY_NEAR(this->ze_exp_J, shape.J.data(), shape.J.size(), this->eps);
    ASSERT_NEAR(0.0, shape.detJ, this->eps);
    ASSERT_ARRAY_NEAR(exp_invJ, shape.invJ.data(), shape.invJ.size(), this->eps);
    ASSERT_ARRAY_NEAR(exp_dNdx, shape.dNdx.data(), shape.dNdx.size(), this->eps);
}


