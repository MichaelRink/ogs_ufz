/**
 * \file   DuplicateMeshComponents.cpp
 * \author Karsten Rink
 * \date   2014-03-25
 * \brief  Implementation of Duplicate functions.
 *
 * \copyright
 * Copyright (c) 2013, OpenGeoSys Community (http://www.opengeosys.org)
 *            Distributed under a Modified BSD License.
 *              See accompanying file LICENSE.txt or
 *              http://www.opengeosys.org/project/license
 *
 */

#include "DuplicateMeshComponents.h"

#include "Mesh.h"
#include "Node.h"
#include "Elements/Element.h"
#include "Elements/Line.h"
#include "Elements/Tri.h"
#include "Elements/Quad.h"
#include "Elements/Tet.h"
#include "Elements/Hex.h"
#include "Elements/Pyramid.h"
#include "Elements/Prism.h"

namespace MeshLib
{

std::vector<MeshLib::Node*> copyNodeVector(const std::vector<MeshLib::Node*> &nodes)
{
	const std::size_t nNodes(nodes.size());
	std::vector<MeshLib::Node*> new_nodes;
	new_nodes.reserve(nNodes);
	for (std::size_t k = 0; k < nNodes; ++k)
		new_nodes.push_back(new MeshLib::Node(nodes[k]->getCoords(), new_nodes.size()));
	return new_nodes;
}

std::vector<MeshLib::Element*> copyElementVector(const std::vector<MeshLib::Element*> &elements, const std::vector<MeshLib::Node*> &nodes)
{
	const std::size_t nElements(elements.size());
	std::vector<MeshLib::Element*> new_elements;
	new_elements.reserve(nElements);
	for (std::size_t k = 0; k < nElements; ++k)
		new_elements.push_back(copyElement(elements[k], nodes));
	return new_elements;
}

MeshLib::Element* copyElement(MeshLib::Element const*const element, const std::vector<MeshLib::Node*> &nodes)
{
	if (element->getGeomType() == MeshElemType::LINE)
		return copyElement<MeshLib::Line>(element, nodes);
	else if (element->getGeomType() == MeshElemType::TRIANGLE)
		return copyElement<MeshLib::Tri>(element, nodes);
	else if (element->getGeomType() == MeshElemType::QUAD)
		return copyElement<MeshLib::Quad>(element, nodes);
	else if (element->getGeomType() == MeshElemType::TETRAHEDRON)
		return copyElement<MeshLib::Tet>(element, nodes);
	else if (element->getGeomType() == MeshElemType::HEXAHEDRON)
		return copyElement<MeshLib::Hex>(element, nodes);
	else if (element->getGeomType() == MeshElemType::PYRAMID)
		return copyElement<MeshLib::Pyramid>(element, nodes);
	else if (element->getGeomType() == MeshElemType::PRISM)
		return copyElement<MeshLib::Prism>(element, nodes);

	ERR ("Error: Unknown element type.");
	return nullptr;
}

template <typename E>
MeshLib::Element* copyElement(MeshLib::Element const*const element, const std::vector<MeshLib::Node*> &nodes)
{
	MeshLib::Node** new_nodes = new MeshLib::Node*[element->getNNodes()];
	for (unsigned i=0; i<element->getNNodes(); ++i)
		new_nodes[i] = nodes[element->getNode(i)->getID()];
	return new E(new_nodes, element->getValue());
}

} // namespace MeshLib
