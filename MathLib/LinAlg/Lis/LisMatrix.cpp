/**
 * \file
 * \author Norihiro Watanabe
 * \date   2013-04-16
 * \brief  Implementation of the LisMatrix class.
 *
 * \copyright
 * Copyright (c) 2013, OpenGeoSys Community (http://www.opengeosys.org)
 *            Distributed under a Modified BSD License.
 *              See accompanying file LICENSE.txt or
 *              http://www.opengeosys.org/project/license
 *
 */

#include "LisMatrix.h"

#include <stdexcept>

#include "LisVector.h"
#include "LisCheck.h"

namespace MathLib
{

LisMatrix::LisMatrix(std::size_t n_rows, LisOption::MatrixType mat_type)
    : _n_rows(n_rows), _mat_type(mat_type), _is_assembled(false)
{
    int ierr = lis_matrix_create(0, &_AA);
    checkLisError(ierr);
    ierr = lis_matrix_set_size(_AA, 0, n_rows);
    checkLisError(ierr);
    lis_matrix_get_range(_AA, &_is, &_ie);
    ierr = lis_vector_duplicate(_AA, &_diag);
    checkLisError(ierr);
}

LisMatrix::~LisMatrix()
{
    int ierr = lis_matrix_destroy(_AA);
    checkLisError(ierr);
    ierr = lis_vector_destroy(_diag);
    checkLisError(ierr);
}

void LisMatrix::setZero()
{
    // A matrix has to be destroyed and created again because Lis doesn't provide a
    // function to set matrix entries to zero
    int ierr = lis_matrix_destroy(_AA);
    checkLisError(ierr);
    ierr = lis_matrix_create(0, &_AA);
    checkLisError(ierr);
    ierr = lis_matrix_set_size(_AA, 0, _n_rows);
    checkLisError(ierr);
    ierr = lis_vector_set_all(0.0, _diag);
    checkLisError(ierr);

    _is_assembled = false;
}

int LisMatrix::setValue(std::size_t rowId, std::size_t colId, double v)
{
    lis_matrix_set_value(LIS_INS_VALUE, rowId, colId, v, _AA);
    if (rowId==colId)
        lis_vector_set_value(LIS_INS_VALUE, rowId, v, _diag);
    _is_assembled = false;
    return 0;
}

int LisMatrix::add(std::size_t rowId, std::size_t colId, double v)
{
    lis_matrix_set_value(LIS_ADD_VALUE, rowId, colId, v, _AA);
    if (rowId==colId)
        lis_vector_set_value(LIS_ADD_VALUE, rowId, v, _diag);
    _is_assembled = false;
    return 0;
}

void LisMatrix::write(const std::string &filename) const
{
    if (!_is_assembled)
        throw std::logic_error("LisMatrix::write(): matrix not assembled.");
    lis_output_matrix(_AA, LIS_FMT_MM, const_cast<char*>(filename.c_str()));
}

double LisMatrix::getMaxDiagCoeff()
{
    double abs_max_entry;
    int ierr = lis_vector_get_value(_diag, 0, &abs_max_entry);
    checkLisError(ierr);
    abs_max_entry = std::abs(abs_max_entry);
    for (std::size_t k(1); k<_n_rows; ++k) {
        double tmp;
        ierr = lis_vector_get_value(_diag, k, &tmp);
        checkLisError(ierr);
        if (abs_max_entry < std::abs(tmp)) {
            abs_max_entry = std::abs(tmp);
        }
    }

    return abs_max_entry;
}

void LisMatrix::multiply(const LisVector &x, LisVector &y) const
{
    if (!_is_assembled)
        throw std::logic_error("LisMatrix::matvec(): matrix not assembled.");
    int ierr = lis_matvec(_AA, const_cast<LisVector*>(&x)->getRawVector(), y.getRawVector());
    checkLisError(ierr);
}

bool finalizeMatrixAssembly(LisMatrix &mat)
{
    LIS_MATRIX &A = mat.getRawMatrix();

    if (!mat.isAssembled()) {
        int ierr = lis_matrix_set_type(A, static_cast<int>(mat.getMatrixType()));
        checkLisError(ierr);
        ierr = lis_matrix_assemble(A);
        checkLisError(ierr);
        mat._is_assembled = true;
    }
    return true;
}


} //MathLib
