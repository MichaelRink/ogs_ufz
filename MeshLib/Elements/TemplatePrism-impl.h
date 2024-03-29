/**
 * \file
 * \author Karsten Rink
 * \date   2012-05-02
 * \brief  Implementation of the TemplatePrism class.
 *
 * \copyright
 * Copyright (c) 2013, OpenGeoSys Community (http://www.opengeosys.org)
 *            Distributed under a Modified BSD License.
 *              See accompanying file LICENSE.txt or
 *              http://www.opengeosys.org/project/license
 *
 */

// Thirdparty
#include "logog/include/logog.hpp"

#include "Node.h"
#include "Tri.h"
#include "Pyramid.h"
#include "Quad.h"

#include "MathTools.h"

namespace MeshLib {

template <unsigned NNODES, CellType CELLPRISMTYPE>
const unsigned TemplatePrism<NNODES, CELLPRISMTYPE>::n_all_nodes;

template <unsigned NNODES, CellType CELLPRISMTYPE>
const unsigned TemplatePrism<NNODES, CELLPRISMTYPE>::n_base_nodes;

template <unsigned NNODES, CellType CELLPRISMTYPE>
const unsigned TemplatePrism<NNODES,CELLPRISMTYPE>::_face_nodes[5][4] =
{
	{0, 2, 1, 99}, // Face 0
	{0, 1, 4,  3}, // Face 1
	{1, 2, 5,  4}, // Face 2
	{2, 0, 3,  5}, // Face 3
	{3, 4, 5, 99}  // Face 4
};

template <unsigned NNODES, CellType CELLPRISMTYPE>
const unsigned TemplatePrism<NNODES,CELLPRISMTYPE>::_edge_nodes[9][2] =
{
	{0, 1}, // Edge 0
	{1, 2}, // Edge 1
	{0, 2}, // Edge 2
	{0, 3}, // Edge 3
	{1, 4}, // Edge 4
	{2, 5}, // Edge 5
	{3, 4}, // Edge 6
	{4, 5}, // Edge 7
	{3, 5}  // Edge 8
};

template <unsigned NNODES, CellType CELLPRISMTYPE>
const unsigned TemplatePrism<NNODES,CELLPRISMTYPE>::_n_face_nodes[5] = { 3, 4, 4, 4, 3 };

template <unsigned NNODES, CellType CELLPRISMTYPE>
TemplatePrism<NNODES,CELLPRISMTYPE>::TemplatePrism(Node* nodes[NNODES], unsigned value, std::size_t id)
	: Cell(value, id)
{
	_nodes = nodes;
	_neighbors = new Element*[5];
	std::fill(_neighbors, _neighbors + 5, nullptr);
	this->_volume = this->computeVolume();
}

template<unsigned NNODES, CellType CELLPRISMTYPE>
TemplatePrism<NNODES,CELLPRISMTYPE>::TemplatePrism(std::array<Node*, NNODES> const& nodes,
                                                   unsigned value, std::size_t id)
	: Cell(value, id)
{
	_nodes = new Node*[NNODES];
	std::copy(nodes.begin(), nodes.end(), _nodes);

	_neighbors = new Element*[5];
	std::fill(_neighbors, _neighbors + 5, nullptr);

	this->_volume = this->computeVolume();
}

template <unsigned NNODES, CellType CELLPRISMTYPE>
TemplatePrism<NNODES,CELLPRISMTYPE>::TemplatePrism(const TemplatePrism<NNODES,CELLPRISMTYPE> &prism)
	: Cell(prism.getValue(), prism.getID())
{
	_nodes = new Node*[NNODES];
	for (unsigned i=0; i<NNODES; i++)
		_nodes[i] = prism._nodes[i];

	_neighbors = new Element*[5];
	for (unsigned i=0; i<5; i++)
		_neighbors[i] = prism._neighbors[i];

	_volume = prism.getVolume();
}

template <unsigned NNODES, CellType CELLPRISMTYPE>
TemplatePrism<NNODES,CELLPRISMTYPE>::~TemplatePrism()
{
}

template <unsigned NNODES, CellType CELLPRISMTYPE>
double TemplatePrism<NNODES,CELLPRISMTYPE>::computeVolume()
{
	return MathLib::calcTetrahedronVolume(_nodes[0]->getCoords(), _nodes[1]->getCoords(), _nodes[2]->getCoords(), _nodes[3]->getCoords())
		 + MathLib::calcTetrahedronVolume(_nodes[1]->getCoords(), _nodes[4]->getCoords(), _nodes[2]->getCoords(), _nodes[3]->getCoords())
		 + MathLib::calcTetrahedronVolume(_nodes[2]->getCoords(), _nodes[4]->getCoords(), _nodes[5]->getCoords(), _nodes[3]->getCoords());
}

template <unsigned NNODES, CellType CELLPRISMTYPE>
const Element* TemplatePrism<NNODES,CELLPRISMTYPE>::getFace(unsigned i) const
{
	if (i<this->getNFaces())
	{
		unsigned nFaceNodes (this->getNFaceNodes(i));
		Node** nodes = new Node*[nFaceNodes];
		for (unsigned j=0; j<nFaceNodes; j++)
			nodes[j] = _nodes[_face_nodes[i][j]];

		if (i==0 || i==4)
			return new Tri(nodes);
		else
			return new Quad(nodes);
	}
	ERR("Error in MeshLib::Element::getFace() - Index %d does not exist.", i);
	return NULL;
}

template <unsigned NNODES, CellType CELLPRISMTYPE>
unsigned TemplatePrism<NNODES,CELLPRISMTYPE>::getNFaceNodes(unsigned i) const
{
	if (i<5)
		return _n_face_nodes[i];
	ERR("Error in MeshLib::Element::getNFaceNodes() - Index %d does not exist.", i);
	return 0;
}

template <unsigned NNODES, CellType CELLPRISMTYPE>
bool TemplatePrism<NNODES,CELLPRISMTYPE>::isEdge(unsigned idx1, unsigned idx2) const
{
	for (unsigned i(0); i<9; i++)
	{
		if (_edge_nodes[i][0]==idx1 && _edge_nodes[i][1]==idx2) return true;
		if (_edge_nodes[i][1]==idx1 && _edge_nodes[i][0]==idx2) return true;
	}
	return false;
}

template <unsigned NNODES, CellType CELLPRISMTYPE>
Element* TemplatePrism<NNODES,CELLPRISMTYPE>::clone() const
{
	return new TemplatePrism<NNODES,CELLPRISMTYPE>(*this);
}

template <unsigned NNODES, CellType CELLPRISMTYPE>
unsigned TemplatePrism<NNODES,CELLPRISMTYPE>::identifyFace(Node* nodes[3]) const
{
	for (unsigned i=0; i<5; i++)
	{
		unsigned flag(0);
		for (unsigned j=0; j<4; j++)
			for (unsigned k=0; k<3; k++)
				if (_face_nodes[i][j] != 99 && _nodes[_face_nodes[i][j]] == nodes[k])
					flag++;
		if (flag==3)
			return i;
	}
	return std::numeric_limits<unsigned>::max();
}

template <unsigned NNODES, CellType CELLPRISMTYPE>
ElementErrorCode TemplatePrism<NNODES,CELLPRISMTYPE>::validate() const
{
	ElementErrorCode error_code;
	error_code[ElementErrorFlag::ZeroVolume] = this->hasZeroVolume();

	for (unsigned i=1; i<4; ++i)
	{
		const MeshLib::Quad* quad (dynamic_cast<const MeshLib::Quad*>(this->getFace(i)));
		if (quad)
			error_code |= quad->validate();
		else 
			error_code.set(ElementErrorFlag::NodeOrder);
		delete quad;
	}
	error_code[ElementErrorFlag::NodeOrder] = !this->testElementNodeOrder();
	return error_code;
}

} // end namespace MeshLib

