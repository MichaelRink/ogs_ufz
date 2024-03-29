/**
 * \file   MeshRevision.cpp
 * \author Karsten Rink
 * \date   2014-02-14
 * \brief  Implementation of the MeshRevision class. 
 *
 * \copyright
 * Copyright (c) 2013, OpenGeoSys Community (http://www.opengeosys.org)
 *            Distributed under a Modified BSD License.
 *              See accompanying file LICENSE.txt or
 *              http://www.opengeosys.org/project/license
 *
 */

#include "MeshRevision.h"

// ThirdParty/logog
#include "logog/include/logog.hpp"

// GeoLib
#include "Grid.h"

// MeshLib
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

#include "DuplicateMeshComponents.h"

namespace MeshLib {

const std::array<unsigned, 8> MeshRevision::_hex_diametral_nodes = {{ 6, 7, 4, 5, 2, 3, 0, 1 }};

MeshRevision::MeshRevision(MeshLib::Mesh &mesh) :
	_mesh(mesh)
{}


MeshLib::Mesh* MeshRevision::collapseNodes(const std::string &new_mesh_name, double eps)
{
	std::vector<MeshLib::Node*> new_nodes (this->constructNewNodesArray(this->collapseNodeIndeces(eps)));
	std::vector<MeshLib::Element*> new_elements (MeshLib::copyElementVector(_mesh.getElements(), new_nodes));
	this->resetNodeIDs();
	return new MeshLib::Mesh(new_mesh_name, new_nodes, new_elements);
}

unsigned MeshRevision::getNCollapsableNodes(double eps) const
{
	std::vector<std::size_t> id_map(this->collapseNodeIndeces(eps));
	std::size_t nNodes(id_map.size());
	unsigned count(0);
	for (std::size_t i = 0; i < nNodes; ++i)
		if (i != id_map[i])
			count++;
	return count;
}

MeshLib::Mesh* MeshRevision::simplifyMesh(const std::string &new_mesh_name, double eps, unsigned min_elem_dim)
{
	if (this->_mesh.getNElements() == 0)
		return nullptr;

	std::vector<MeshLib::Node*> new_nodes = this->constructNewNodesArray(this->collapseNodeIndeces(eps));
	std::vector<MeshLib::Element*> new_elements;

	const std::vector<MeshLib::Element*> &elements(this->_mesh.getElements());
	for (auto elem = elements.begin(); elem != elements.end(); ++elem)
	{
		unsigned n_unique_nodes(this->getNUniqueNodes(*elem));
		if (n_unique_nodes == (*elem)->getNNodes() && (*elem)->getDimension() >= min_elem_dim)
		{
			ElementErrorCode e((*elem)->validate());
			if (e[ElementErrorFlag::NonCoplanar])
			{
				if (!this->subdivideElement(*elem, new_nodes, new_elements))
				{
					ERR("Error: Element %d has unknown element type.", std::distance(elements.begin(), elem));
					this->resetNodeIDs();
					this->cleanUp(new_nodes, new_elements);
					return nullptr;
				}
			}
			else
				new_elements.push_back(MeshLib::copyElement(*elem, new_nodes));
		}
		else if (n_unique_nodes < (*elem)->getNNodes() && n_unique_nodes>1)
			reduceElement(*elem, n_unique_nodes, new_nodes, new_elements, min_elem_dim);
		else
			ERR ("Something is wrong, more unique nodes than actual nodes");

	}
		
	this->resetNodeIDs();
	if (!new_elements.empty())
		return new MeshLib::Mesh(new_mesh_name, new_nodes, new_elements);

	this->cleanUp(new_nodes, new_elements);
	return nullptr;
}

MeshLib::Mesh* MeshRevision::subdivideMesh(const std::string &new_mesh_name) const
{
	if (this->_mesh.getNElements() == 0)
		return nullptr;

	std::vector<MeshLib::Node*> new_nodes = MeshLib::copyNodeVector(_mesh.getNodes());
	std::vector<MeshLib::Element*> new_elements;

	const std::vector<MeshLib::Element*> &elements(this->_mesh.getElements());
	for (auto elem = elements.begin(); elem != elements.end(); ++elem)
	{
		ElementErrorCode e((*elem)->validate());
		if (e[ElementErrorFlag::NonCoplanar])
		{
			if (!this->subdivideElement(*elem, new_nodes, new_elements))
			{
				ERR("Error: Element %d has unknown element type.", std::distance(elements.begin(), elem));
				this->cleanUp(new_nodes, new_elements);
				return nullptr;
			}
		}
		else
			new_elements.push_back(MeshLib::copyElement(*elem, new_nodes));
	}

	if (!new_elements.empty())
		return new MeshLib::Mesh(new_mesh_name, new_nodes, new_elements);

	this->cleanUp(new_nodes, new_elements);
	return nullptr;
}

std::vector<std::size_t> MeshRevision::collapseNodeIndeces(double eps) const
{
	const std::vector<MeshLib::Node*> &nodes(_mesh.getNodes());
	const std::size_t nNodes(_mesh.getNNodes());
	std::vector<std::size_t> id_map(nNodes);
	const double half_eps(eps / 2.0);
	const double sqr_eps(eps*eps);
	for (std::size_t k = 0; k < nNodes; ++k)
		id_map[k] = k;

	GeoLib::Grid<MeshLib::Node> grid(nodes.begin(), nodes.end(), 64);

	for (size_t k = 0; k < nNodes; ++k)
	{
		std::vector<std::vector<MeshLib::Node*> const*> node_vectors;
		MeshLib::Node const*const node(nodes[k]);
		if (node->getID() != k)
			continue;
		grid.getPntVecsOfGridCellsIntersectingCube(node->getCoords(), half_eps, node_vectors);

		const size_t nVectors(node_vectors.size());
		for (size_t i = 0; i < nVectors; ++i)
		{
			const std::vector<MeshLib::Node*> &cell_vector(*node_vectors[i]);
			const size_t nGridCellNodes(cell_vector.size());
			for (size_t j = 0; j < nGridCellNodes; ++j)
			{
				MeshLib::Node* test_node(cell_vector[j]);
				// are node indices already identical (i.e. nodes will be collapsed)
				if (id_map[node->getID()] == id_map[test_node->getID()])
					continue;

				// if test_node has already been collapsed to another node x, ignore it
				// (if the current node would need to be collapsed with x it would already have happened when x was tested)
				if (test_node->getID() != id_map[test_node->getID()])
					continue;

				// calc distance
				if (MathLib::sqrDist(node->getCoords(), test_node->getCoords()) < sqr_eps)
					id_map[test_node->getID()] = node->getID();
			}
		}
	}
	return id_map;
}

std::vector<MeshLib::Node*> MeshRevision::constructNewNodesArray(const std::vector<std::size_t> &id_map) const
{
	const std::vector<MeshLib::Node*> &nodes(_mesh.getNodes());
	const std::size_t nNodes(nodes.size());
	std::vector<MeshLib::Node*> new_nodes;
	new_nodes.reserve(nNodes);
	for (std::size_t k = 0; k < nNodes; ++k)
	{
		// all nodes that have not been collapsed with other nodes are copied into new array
		if (nodes[k]->getID() == id_map[k])
		{
			std::size_t id(new_nodes.size());
			new_nodes.push_back(new MeshLib::Node((*nodes[k])[0], (*nodes[k])[1], (*nodes[k])[2], id));
			nodes[k]->setID(id); // the node in the old array gets the index of the same node in the new array
		}
		// the other nodes are not copied and get the index of the nodes they will have been collapsed with
		else
			nodes[k]->setID(nodes[id_map[k]]->getID());
	}
	return new_nodes;
}

unsigned MeshRevision::getNUniqueNodes(MeshLib::Element const*const element) const
{
	unsigned nNodes(element->getNNodes());
	unsigned count(nNodes);

	for (unsigned i = 0; i < nNodes - 1; ++i)
		for (unsigned j = i + 1; j < nNodes; ++j)
			if (element->getNode(i)->getID() == element->getNode(j)->getID())
			{
				count--;
				break;
			}
	return count;
}

void MeshRevision::resetNodeIDs()
{
	const std::size_t nNodes(this->_mesh.getNNodes());
	const std::vector<MeshLib::Node*> &nodes(_mesh.getNodes());
	for (std::size_t i = 0; i < nNodes; ++i)
		nodes[i]->setID(i);
}



bool MeshRevision::subdivideElement(MeshLib::Element const*const element, const std::vector<MeshLib::Node*> &nodes, std::vector<MeshLib::Element*> &elements) const
{
	unsigned n_new_elems (0);
	if (element->getGeomType() == MeshElemType::QUAD)
		n_new_elems = this->subdivideQuad(element, nodes, elements);
	else if (element->getGeomType() == MeshElemType::HEXAHEDRON)
		n_new_elems = this->subdivideHex(element, nodes, elements);
	else if (element->getGeomType() == MeshElemType::PYRAMID)
		n_new_elems = this->subdividePyramid(element, nodes, elements);
	else if (element->getGeomType() == MeshElemType::PRISM)
		n_new_elems = this->subdividePrism(element, nodes, elements);
	return (n_new_elems > 0);
}

void MeshRevision::reduceElement(MeshLib::Element const*const element, 
								 unsigned n_unique_nodes, 
								 const std::vector<MeshLib::Node*> &nodes,
								 std::vector<MeshLib::Element*> &elements,
								 unsigned min_elem_dim) const
{
	/***************
	 * TODO: modify neighbouring elements if one elements has been subdivided
	 ***************/
	if (element->getGeomType() == MeshElemType::TRIANGLE && min_elem_dim == 1)
		elements.push_back (this->constructLine(element, nodes));
	else if ((element->getGeomType() == MeshElemType::QUAD) || 
		     (element->getGeomType() == MeshElemType::TETRAHEDRON))
	{
		if (n_unique_nodes == 3 && min_elem_dim < 3)
			elements.push_back (this->constructTri(element, nodes));
		else if (min_elem_dim == 1)
			elements.push_back (this->constructLine(element, nodes));
	}
	else if (element->getGeomType() == MeshElemType::HEXAHEDRON)
		this->reduceHex(element, n_unique_nodes, nodes, elements, min_elem_dim);
	else if (element->getGeomType() == MeshElemType::PYRAMID)
		this->reducePyramid(element, n_unique_nodes, nodes, elements, min_elem_dim);
	else if (element->getGeomType() == MeshElemType::PRISM)
		this->reducePrism(element, n_unique_nodes, nodes, elements, min_elem_dim);
	else
		ERR ("Error: Unknown element type.");
}

unsigned MeshRevision::subdivideQuad(MeshLib::Element const*const quad, const std::vector<MeshLib::Node*> &nodes, std::vector<MeshLib::Element*> &new_elements) const
{
	MeshLib::Node** tri1_nodes = new MeshLib::Node*[3];
	tri1_nodes[0] = nodes[quad->getNode(0)->getID()];
	tri1_nodes[1] = nodes[quad->getNode(1)->getID()];
	tri1_nodes[2] = nodes[quad->getNode(2)->getID()];
	new_elements.push_back(new MeshLib::Tri(tri1_nodes, quad->getValue()));

	MeshLib::Node** tri2_nodes = new MeshLib::Node*[3];
	tri2_nodes[0] = nodes[quad->getNode(0)->getID()];
	tri2_nodes[1] = nodes[quad->getNode(2)->getID()];
	tri2_nodes[2] = nodes[quad->getNode(3)->getID()];
	new_elements.push_back(new MeshLib::Tri(tri2_nodes, quad->getValue()));

	return 2;
}

unsigned MeshRevision::subdivideHex(MeshLib::Element const*const hex, const std::vector<MeshLib::Node*> &nodes, std::vector<MeshLib::Element*> &new_elements) const
{
	MeshLib::Node** prism1_nodes = new MeshLib::Node*[6];
	prism1_nodes[0] = nodes[hex->getNode(0)->getID()];
	prism1_nodes[1] = nodes[hex->getNode(2)->getID()];
	prism1_nodes[2] = nodes[hex->getNode(1)->getID()];
	prism1_nodes[3] = nodes[hex->getNode(4)->getID()];
	prism1_nodes[4] = nodes[hex->getNode(6)->getID()];
	prism1_nodes[5] = nodes[hex->getNode(5)->getID()];
	MeshLib::Prism* prism1 (new MeshLib::Prism(prism1_nodes, hex->getValue()));
	this->subdividePrism(prism1, nodes, new_elements);
	delete prism1;

	MeshLib::Node** prism2_nodes = new MeshLib::Node*[6];
	prism2_nodes[0] = nodes[hex->getNode(4)->getID()];
	prism2_nodes[1] = nodes[hex->getNode(6)->getID()];
	prism2_nodes[2] = nodes[hex->getNode(7)->getID()];
	prism2_nodes[3] = nodes[hex->getNode(0)->getID()];
	prism2_nodes[4] = nodes[hex->getNode(2)->getID()];
	prism2_nodes[5] = nodes[hex->getNode(3)->getID()];
	MeshLib::Prism* prism2 (new MeshLib::Prism(prism2_nodes, hex->getValue()));
	this->subdividePrism(prism2, nodes, new_elements);
	delete prism2;

	return 6;
}

unsigned MeshRevision::subdividePyramid(MeshLib::Element const*const pyramid, const std::vector<MeshLib::Node*> &nodes, std::vector<MeshLib::Element*> &new_elements) const
{
	MeshLib::Node** tet1_nodes = new MeshLib::Node*[4];
	tet1_nodes[0] = nodes[pyramid->getNode(0)->getID()];
	tet1_nodes[1] = nodes[pyramid->getNode(1)->getID()];
	tet1_nodes[2] = nodes[pyramid->getNode(2)->getID()];
	tet1_nodes[3] = nodes[pyramid->getNode(4)->getID()];
	new_elements.push_back(new MeshLib::Tet(tet1_nodes, pyramid->getValue()));

	MeshLib::Node** tet2_nodes = new MeshLib::Node*[4];
	tet2_nodes[0] = nodes[pyramid->getNode(0)->getID()];
	tet2_nodes[1] = nodes[pyramid->getNode(2)->getID()];
	tet2_nodes[2] = nodes[pyramid->getNode(3)->getID()];
	tet2_nodes[3] = nodes[pyramid->getNode(4)->getID()];
	new_elements.push_back(new MeshLib::Tet(tet2_nodes, pyramid->getValue()));

	return 2;
}

unsigned MeshRevision::subdividePrism(MeshLib::Element const*const prism, const std::vector<MeshLib::Node*> &nodes, std::vector<MeshLib::Element*> &new_elements) const
{
	MeshLib::Node** tet1_nodes = new MeshLib::Node*[4];
	tet1_nodes[0] = nodes[prism->getNode(0)->getID()];
	tet1_nodes[1] = nodes[prism->getNode(1)->getID()];
	tet1_nodes[2] = nodes[prism->getNode(2)->getID()];
	tet1_nodes[3] = nodes[prism->getNode(3)->getID()];
	new_elements.push_back(new MeshLib::Tet(tet1_nodes, prism->getValue()));

	MeshLib::Node** tet2_nodes = new MeshLib::Node*[4];
	tet2_nodes[0] = nodes[prism->getNode(3)->getID()];
	tet2_nodes[1] = nodes[prism->getNode(2)->getID()];
	tet2_nodes[2] = nodes[prism->getNode(4)->getID()];
	tet2_nodes[3] = nodes[prism->getNode(5)->getID()];
	new_elements.push_back(new MeshLib::Tet(tet2_nodes, prism->getValue()));

	MeshLib::Node** tet3_nodes = new MeshLib::Node*[4];
	tet3_nodes[0] = nodes[prism->getNode(2)->getID()];
	tet3_nodes[1] = nodes[prism->getNode(1)->getID()];
	tet3_nodes[2] = nodes[prism->getNode(3)->getID()];
	tet3_nodes[3] = nodes[prism->getNode(4)->getID()];
	new_elements.push_back(new MeshLib::Tet(tet3_nodes, prism->getValue()));

	return 3;
}

unsigned MeshRevision::reduceHex(MeshLib::Element const*const org_elem, 
							 unsigned n_unique_nodes, 
							 const std::vector<MeshLib::Node*> &nodes,
							 std::vector<MeshLib::Element*> &new_elements,
							 unsigned min_elem_dim) const
{
	// TODO?
	// if two diametral nodes collapse, all kinds of bizarre (2D-)element combinations could be the result.
	// this case is currently not implemented!

	if (n_unique_nodes == 7)
	{
		// reduce to prism + pyramid
		for (unsigned i=0; i<7; ++i)
			for (unsigned j=i+1; j<8; ++j)
				if (org_elem->getNode(i)->getID() == org_elem->getNode(j)->getID())
				{
					const std::array<unsigned,4> base_nodes (this->lutHexCuttingQuadNodes(i,j));
					MeshLib::Node** pyr_nodes = new MeshLib::Node*[5];
					pyr_nodes[0] = nodes[org_elem->getNode(base_nodes[0])->getID()];
					pyr_nodes[1] = nodes[org_elem->getNode(base_nodes[1])->getID()];
					pyr_nodes[2] = nodes[org_elem->getNode(base_nodes[2])->getID()];
					pyr_nodes[3] = nodes[org_elem->getNode(base_nodes[3])->getID()];
					pyr_nodes[4] = nodes[org_elem->getNode(i)->getID()];
					new_elements.push_back (new MeshLib::Pyramid(pyr_nodes, org_elem->getValue()));

					if (i<4 && j>=4) std::swap(i,j);
					MeshLib::Node** prism_nodes = new MeshLib::Node*[6];
					prism_nodes[0] = nodes[org_elem->getNode(base_nodes[0])->getID()];
					prism_nodes[1] = nodes[org_elem->getNode(base_nodes[3])->getID()];
					prism_nodes[2] = nodes[org_elem->getNode(this->lutHexDiametralNode(j))->getID()];
					prism_nodes[3] = nodes[org_elem->getNode(base_nodes[1])->getID()];
					prism_nodes[4] = nodes[org_elem->getNode(base_nodes[2])->getID()];
					prism_nodes[5] = nodes[org_elem->getNode(this->lutHexDiametralNode(i))->getID()];
					new_elements.push_back (new MeshLib::Prism(prism_nodes, org_elem->getValue()));
					return 2;
				}
	}
	else if (n_unique_nodes == 6)
	{
		// reduce to prism
		for (unsigned i=0; i<6; ++i)
		{
			const MeshLib::Element* face (org_elem->getFace(i));
			if (face->getNode(0)->getID() == face->getNode(1)->getID() && face->getNode(2)->getID() == face->getNode(3)->getID())
			{
				MeshLib::Node** prism_nodes = new MeshLib::Node*[6];
				prism_nodes[0] = nodes[org_elem->getNode(this->lutHexDiametralNode(org_elem->getNodeIDinElement(face->getNode(0))))->getID()];
				prism_nodes[1] = nodes[org_elem->getNode(this->lutHexDiametralNode(org_elem->getNodeIDinElement(face->getNode(1))))->getID()];
				prism_nodes[2] = nodes[org_elem->getNode(org_elem->getNodeIDinElement(face->getNode(2)))->getID()];
				prism_nodes[3] = nodes[org_elem->getNode(this->lutHexDiametralNode(org_elem->getNodeIDinElement(face->getNode(2))))->getID()];
				prism_nodes[4] = nodes[org_elem->getNode(this->lutHexDiametralNode(org_elem->getNodeIDinElement(face->getNode(3))))->getID()];
				prism_nodes[5] = nodes[org_elem->getNode(org_elem->getNodeIDinElement(face->getNode(0)))->getID()];
				new_elements.push_back (new MeshLib::Prism(prism_nodes, org_elem->getValue()));
				delete face;
				return 1;
			}
			if (face->getNode(0)->getID() == face->getNode(3)->getID() && face->getNode(1)->getID() == face->getNode(2)->getID())
			{
				MeshLib::Node** prism_nodes = new MeshLib::Node*[6];
				prism_nodes[0] = nodes[org_elem->getNode(this->lutHexDiametralNode(org_elem->getNodeIDinElement(face->getNode(0))))->getID()];
				prism_nodes[1] = nodes[org_elem->getNode(this->lutHexDiametralNode(org_elem->getNodeIDinElement(face->getNode(3))))->getID()];
				prism_nodes[2] = nodes[org_elem->getNode(org_elem->getNodeIDinElement(face->getNode(2)))->getID()];
				prism_nodes[3] = nodes[org_elem->getNode(this->lutHexDiametralNode(org_elem->getNodeIDinElement(face->getNode(1))))->getID()];
				prism_nodes[4] = nodes[org_elem->getNode(this->lutHexDiametralNode(org_elem->getNodeIDinElement(face->getNode(2))))->getID()];
				prism_nodes[5] = nodes[org_elem->getNode(org_elem->getNodeIDinElement(face->getNode(0)))->getID()];
				delete face;
				return 1;
			}
			delete face;
		}
		// reduce to four tets -> divide into 2 prisms such that each has one collapsed node
		for (unsigned i=0; i<7; ++i)
			for (unsigned j=i+1; j<8; ++j)
				if (org_elem->getNode(i)->getID() == org_elem->getNode(j)->getID())
				{
					for (unsigned k=i; k<7; ++k)
						for (unsigned l=k+1; l<8; ++l)
							if (!(i==k && j==l) && org_elem->isEdge(i,j) && org_elem->isEdge(k,l) && 
								org_elem->getNode(k)->getID() == org_elem->getNode(l)->getID())
							{
								const std::pair<unsigned, unsigned> back (this->lutHexBackNodes(i,j,k,l));
								if (back.first == std::numeric_limits<unsigned>::max() || back.second == std::numeric_limits<unsigned>::max())
								{
									ERR ("Unexpected error during Hex reduction");
									return 0;
								}

								std::array<unsigned, 4> cutting_plane (this->lutHexCuttingQuadNodes(back.first, back.second));
								MeshLib::Node** pris1_nodes = new MeshLib::Node*[6];
								pris1_nodes[0] = const_cast<MeshLib::Node*>(org_elem->getNode(back.first));
								pris1_nodes[1] = const_cast<MeshLib::Node*>(org_elem->getNode(cutting_plane[0])); 
								pris1_nodes[2] = const_cast<MeshLib::Node*>(org_elem->getNode(cutting_plane[3]));
								pris1_nodes[3] = const_cast<MeshLib::Node*>(org_elem->getNode(back.second)); 
								pris1_nodes[4] = const_cast<MeshLib::Node*>(org_elem->getNode(cutting_plane[1])); 
								pris1_nodes[5] = const_cast<MeshLib::Node*>(org_elem->getNode(cutting_plane[2]));
								MeshLib::Prism* prism1 (new MeshLib::Prism(pris1_nodes, org_elem->getValue()));
								unsigned nNewElements = this->reducePrism(prism1, 5, nodes, new_elements, min_elem_dim);
								delete prism1;

								MeshLib::Node** pris2_nodes = new MeshLib::Node*[6];
								pris2_nodes[0] = const_cast<MeshLib::Node*>(org_elem->getNode(this->lutHexDiametralNode(back.first)));
								pris2_nodes[1] = const_cast<MeshLib::Node*>(org_elem->getNode(cutting_plane[0]));
								pris2_nodes[2] = const_cast<MeshLib::Node*>(org_elem->getNode(cutting_plane[3]));
								pris2_nodes[3] = const_cast<MeshLib::Node*>(org_elem->getNode(this->lutHexDiametralNode(back.second)));
								pris2_nodes[4] = const_cast<MeshLib::Node*>(org_elem->getNode(cutting_plane[1]));
								pris2_nodes[5] = const_cast<MeshLib::Node*>(org_elem->getNode(cutting_plane[2]));
								MeshLib::Prism* prism2 (new MeshLib::Prism(pris2_nodes, org_elem->getValue()));
								nNewElements += this->reducePrism(prism2, 5, nodes, new_elements, min_elem_dim);
								delete prism2;
								return nNewElements;
							}
				}
	}
	else if (n_unique_nodes == 5)
	{
		MeshLib::Element* tet1 (this->constructFourNodeElement(org_elem, nodes));
		std::array<std::size_t, 4> first_four_nodes = {{ tet1->getNode(0)->getID(), tet1->getNode(1)->getID(), tet1->getNode(2)->getID(), tet1->getNode(3)->getID() }};
		unsigned fifth_node (this->findPyramidTopNode(*org_elem, first_four_nodes));

		bool tet_changed (false);
		if (tet1->getGeomType() == MeshElemType::QUAD)
		{
			delete tet1;
			tet_changed =true;
			MeshLib::Node** tet1_nodes = new MeshLib::Node*[4];
			tet1_nodes[0] = nodes[first_four_nodes[0]];
			tet1_nodes[1] = nodes[first_four_nodes[1]];
			tet1_nodes[2] = nodes[first_four_nodes[2]];
			tet1_nodes[3] = nodes[org_elem->getNode(fifth_node)->getID()];
			new_elements.push_back(new MeshLib::Tet(tet1_nodes, org_elem->getValue()));
		}
		else 
			new_elements.push_back(tet1);

		MeshLib::Node** tet2_nodes = new MeshLib::Node*[4];
		tet2_nodes[0] = (tet_changed) ? nodes[first_four_nodes[0]] : nodes[first_four_nodes[1]];
		tet2_nodes[1] = nodes[first_four_nodes[2]];
		tet2_nodes[2] = nodes[first_four_nodes[3]];
		tet2_nodes[3] = nodes[org_elem->getNode(fifth_node)->getID()];
		new_elements.push_back(new MeshLib::Tet(tet2_nodes, org_elem->getValue()));
		return 2;
	}
	else if (n_unique_nodes == 4)
	{
		MeshLib::Element* elem (this->constructFourNodeElement(org_elem, nodes, min_elem_dim));
		if (elem)
		{
			new_elements.push_back (elem);
			return 1;
		}
	}
	else if (n_unique_nodes == 3 && min_elem_dim < 3)
	{
		new_elements.push_back (this->constructTri(org_elem, nodes));
		return 1;
	}
	else if (min_elem_dim == 1)
	{
		new_elements.push_back (this->constructLine(org_elem, nodes));
		return 1;
	}
	return 0;
}

void MeshRevision::reducePyramid(MeshLib::Element const*const org_elem, 
								 unsigned n_unique_nodes, 
								 const std::vector<MeshLib::Node*> &nodes, 
								 std::vector<MeshLib::Element*> &new_elements,
								 unsigned min_elem_dim) const
{
	if (n_unique_nodes == 4)
	{
		MeshLib::Element* elem (this->constructFourNodeElement(org_elem, nodes, min_elem_dim));
		if (elem)
			new_elements.push_back (elem);
	}
	else if (n_unique_nodes == 3 && min_elem_dim < 3)
		new_elements.push_back (this->constructTri(org_elem, nodes));
	else if (n_unique_nodes == 2 && min_elem_dim == 1)
		new_elements.push_back (this->constructLine(org_elem, nodes));
	return;
}

unsigned MeshRevision::reducePrism(MeshLib::Element const*const org_elem, 
							   unsigned n_unique_nodes, 
							   const std::vector<MeshLib::Node*> &nodes, 
							   std::vector<MeshLib::Element*> &new_elements,
							   unsigned min_elem_dim) const
{
	// TODO?
	// In theory a node from the bottom triangle and a node from the top triangle that are not connected by an edge
	// could collapse, resulting in a combination of tri and quad elements. This case is currently not tested.

	// if one of the non-triangle edges collapsed, elem can be reduced to a pyramid, otherwise it will be two tets
	if (n_unique_nodes == 5)
	{
		for (unsigned i=0; i<5; ++i)
			for (unsigned j=i+1; j<6; ++j)
				if (i!=j && org_elem->getNode(i)->getID() == org_elem->getNode(j)->getID())
				{
					// non triangle edge collapsed
					if (i%3 == j%3)
					{
						MeshLib::Node** tet1_nodes = new MeshLib::Node*[4];
						tet1_nodes[0] = nodes[org_elem->getNode((i+1)%3)->getID()];
						tet1_nodes[1] = nodes[org_elem->getNode((i+2)%3)->getID()];
						tet1_nodes[2] = nodes[org_elem->getNode(i)->getID()];
						tet1_nodes[3] = nodes[org_elem->getNode((i+1)%3+3)->getID()];
						new_elements.push_back (new MeshLib::Tet(tet1_nodes, org_elem->getValue()));

						MeshLib::Node** tet2_nodes = new MeshLib::Node*[4];
						tet2_nodes[0] = nodes[org_elem->getNode((i+1)%3+3)->getID()];
						tet2_nodes[1] = nodes[org_elem->getNode((i+2)%3)->getID()];
						tet2_nodes[2] = nodes[org_elem->getNode(i)->getID()];					
						tet2_nodes[3] = nodes[org_elem->getNode((i+2)%3+3)->getID()];
						new_elements.push_back (new MeshLib::Tet(tet2_nodes, org_elem->getValue()));
						return 2;
					}

					// triangle edge collapsed
					const int offset = (i>2) ? -3 : 3;
					const unsigned k = this->lutPrismThirdNode(i,j);
					if (k == std::numeric_limits<unsigned>::max())
					{
						ERR ("Unexpected error during prism reduction.")
						return 0;
					}
					MeshLib::Node** tet1_nodes = new MeshLib::Node*[4];
					tet1_nodes[0] = nodes[org_elem->getNode(i+offset)->getID()];
					tet1_nodes[1] = nodes[org_elem->getNode(j+offset)->getID()];
					tet1_nodes[2] = nodes[org_elem->getNode(k+offset)->getID()];
					tet1_nodes[3] = nodes[org_elem->getNode(i)->getID()];
					new_elements.push_back (new MeshLib::Tet(tet1_nodes, org_elem->getValue()));

					unsigned l = (GeoLib::isCoplanar(*org_elem->getNode(i+offset), *org_elem->getNode(k+offset), *org_elem->getNode(i), *org_elem->getNode(k))) ? j : i;
					MeshLib::Node** tet2_nodes = new MeshLib::Node*[4];
					tet2_nodes[0] = nodes[org_elem->getNode(l+offset)->getID()];
					tet2_nodes[1] = nodes[org_elem->getNode(k+offset)->getID()];
					tet2_nodes[2] = nodes[org_elem->getNode(i)->getID()];					
					tet2_nodes[3] = nodes[org_elem->getNode(k)->getID()];
					new_elements.push_back (new MeshLib::Tet(tet2_nodes, org_elem->getValue()));
					return 2;
				}
	}
	else if (n_unique_nodes == 4)
	{
		MeshLib::Element* elem (this->constructFourNodeElement(org_elem, nodes, min_elem_dim));
		if (elem)
			new_elements.push_back (elem);
	}
	else if (n_unique_nodes == 3 && min_elem_dim < 3)
		new_elements.push_back (this->constructTri(org_elem, nodes));
	else if (n_unique_nodes == 2 && min_elem_dim == 1)
		new_elements.push_back (this->constructLine(org_elem, nodes));
	return 1;
}

MeshLib::Element* MeshRevision::constructLine(MeshLib::Element const*const element, 
	                                          const std::vector<MeshLib::Node*> &nodes) const
{
	MeshLib::Node** line_nodes = new MeshLib::Node*[2];
	line_nodes[0] = nodes[element->getNode(0)->getID()];
	line_nodes[1] = nullptr;
	for (unsigned i=1; i<element->getNNodes(); ++i)
	{
		if (element->getNode(i)->getID() != element->getNode(0)->getID())
		{
			line_nodes[1] = nodes[element->getNode(i)->getID()];
			break;
		}
	}
	assert(line_nodes[1] != nullptr);
	return new MeshLib::Line(line_nodes, element->getValue());
}

MeshLib::Element* MeshRevision::constructTri(MeshLib::Element const*const element, 
	                                         const std::vector<MeshLib::Node*> &nodes) const
{
	// TODO? 
	// In theory three unique nodes could also be reduced to two lines e.g. with  
	// a quad where two diametral nodes collapse. This case is currently not implemented!
	MeshLib::Node** tri_nodes = new MeshLib::Node*[3];
	tri_nodes[0] = nodes[element->getNode(0)->getID()];
	tri_nodes[2] = nullptr;
	for (unsigned i = 1; i < element->getNNodes(); ++i)
	{
		if (element->getNode(i)->getID() != tri_nodes[0]->getID())
		{
			tri_nodes[1] = nodes[element->getNode(i)->getID()];
			for (unsigned j = i + 1; j < element->getNNodes(); ++j)
			{
				if (element->getNode(j)->getID() != tri_nodes[1]->getID())
				{
					tri_nodes[2] = nodes[element->getNode(j)->getID()];
					break;
				}
			}
			if (tri_nodes[2]) break;
		}
	}
	assert(tri_nodes[2] != nullptr);
	return new MeshLib::Tri(tri_nodes, element->getValue());
}

MeshLib::Element* MeshRevision::constructFourNodeElement(MeshLib::Element const*const element, const std::vector<MeshLib::Node*> &nodes, unsigned min_elem_dim) const
{
	MeshLib::Node** new_nodes = new MeshLib::Node*[4];
	unsigned count(0);
	new_nodes[count++] = nodes[element->getNode(0)->getID()];
	for (unsigned i=1; i<element->getNNodes(); ++i)
	{
		if (count>3)
			break;
		bool unique_node (true);
		for (unsigned j=0; j<i; ++j)
		{
			if (element->getNode(i)->getID() == element->getNode(j)->getID())
			{
				unique_node = false;
				break;
			}
		}
		if (unique_node)
			new_nodes[count++] = nodes[element->getNode(i)->getID()];; 
	}
	
	// test if quad or tet
	const bool isQuad (GeoLib::isCoplanar(*new_nodes[0], *new_nodes[1], *new_nodes[2], *new_nodes[3]));
	if (isQuad && min_elem_dim < 3)
	{
		MeshLib::Element* elem (new MeshLib::Quad(new_nodes, element->getValue()));
		for (unsigned i=1; i<3; ++i)
		{
			if (elem->validate().none())
				return elem;
			else
			{
				// change node order if not convex
				MeshLib::Node* tmp = new_nodes[i+1];
				new_nodes[i+1] = new_nodes[i];
				new_nodes[i] = tmp;
			}
		}
		return elem;
	}
	else if (!isQuad)
		return new MeshLib::Tet(new_nodes, element->getValue());
	else // is quad but min elem dim == 3
		return nullptr;
}

unsigned MeshRevision::findPyramidTopNode(const MeshLib::Element &element, const std::array<std::size_t,4> &base_node_ids) const
{
	const std::size_t nNodes (element.getNNodes());
	for (std::size_t i=0; i<nNodes; ++i)
	{
		bool top_node=true;
		for (unsigned j=0; j<4; ++j)
			if (element.getNode(i)->getID() == base_node_ids[j])
				top_node=false;
		if (top_node)
			return i;
	}
	return std::numeric_limits<unsigned>::max(); // should never be reached if called correctly
}

unsigned MeshRevision::lutHexDiametralNode(unsigned id) const
{
	return _hex_diametral_nodes[id];
}

const std::array<unsigned,4> MeshRevision::lutHexCuttingQuadNodes(unsigned id1, unsigned id2) const
{	
	std::array<unsigned,4> nodes;
	if      (id1==0 && id2==1) { nodes[0]=3; nodes[1]=2; nodes[2]=5; nodes[3]=4; }
	else if (id1==1 && id2==2) { nodes[0]=0; nodes[1]=3; nodes[2]=6; nodes[3]=5; }
	else if (id1==2 && id2==3) { nodes[0]=1; nodes[1]=0; nodes[2]=7; nodes[3]=6; }
	else if (id1==3 && id2==0) { nodes[0]=2; nodes[1]=1; nodes[2]=4; nodes[3]=7; }
	else if (id1==4 && id2==5) { nodes[0]=0; nodes[1]=1; nodes[2]=6; nodes[3]=7; }
	else if (id1==5 && id2==6) { nodes[0]=1; nodes[1]=2; nodes[2]=7; nodes[3]=4; }
	else if (id1==6 && id2==7) { nodes[0]=2; nodes[1]=3; nodes[2]=4; nodes[3]=5; }
	else if (id1==7 && id2==4) { nodes[0]=3; nodes[1]=0; nodes[2]=5; nodes[3]=6; }
	else if (id1==0 && id2==4) { nodes[0]=3; nodes[1]=7; nodes[2]=5; nodes[3]=1; }
	else if (id1==1 && id2==5) { nodes[0]=0; nodes[1]=4; nodes[2]=6; nodes[3]=2; }
	else if (id1==2 && id2==6) { nodes[0]=1; nodes[1]=5; nodes[2]=7; nodes[3]=3; }
	else if (id1==3 && id2==7) { nodes[0]=2; nodes[1]=6; nodes[2]=4; nodes[3]=0; }

	else if (id1==1 && id2==0) { nodes[0]=2; nodes[1]=3; nodes[2]=4; nodes[3]=5; }
	else if (id1==2 && id2==1) { nodes[0]=3; nodes[1]=0; nodes[2]=5; nodes[3]=6; }
	else if (id1==3 && id2==2) { nodes[0]=0; nodes[1]=1; nodes[2]=6; nodes[3]=7; }
	else if (id1==0 && id2==3) { nodes[0]=1; nodes[1]=2; nodes[2]=7; nodes[3]=4; }
	else if (id1==5 && id2==4) { nodes[0]=1; nodes[1]=0; nodes[2]=7; nodes[3]=6; }
	else if (id1==6 && id2==5) { nodes[0]=2; nodes[1]=1; nodes[2]=4; nodes[3]=7; }
	else if (id1==7 && id2==6) { nodes[0]=3; nodes[1]=2; nodes[2]=5; nodes[3]=4; }
	else if (id1==4 && id2==7) { nodes[0]=0; nodes[1]=3; nodes[2]=6; nodes[3]=5; }
	else if (id1==4 && id2==0) { nodes[0]=7; nodes[1]=3; nodes[2]=1; nodes[3]=5; }
	else if (id1==5 && id2==1) { nodes[0]=4; nodes[1]=0; nodes[2]=2; nodes[3]=6; }
	else if (id1==6 && id2==2) { nodes[0]=5; nodes[1]=1; nodes[2]=3; nodes[3]=7; }
	else if (id1==7 && id2==3) { nodes[0]=6; nodes[1]=2; nodes[2]=0; nodes[3]=4; }
	return nodes;
}

const std::pair<unsigned, unsigned> MeshRevision::lutHexBackNodes(unsigned i, unsigned j, unsigned k, unsigned l) const
{
	// collapsed edges are *not* connected
	std::pair<unsigned, unsigned> back(std::numeric_limits<unsigned>::max(), std::numeric_limits<unsigned>::max());
	if      (this->lutHexDiametralNode(i) == k) { back.first = i; back.second = this->lutHexDiametralNode(l); }
	else if (this->lutHexDiametralNode(i) == l) { back.first = i; back.second = this->lutHexDiametralNode(k); }
	else if (this->lutHexDiametralNode(j) == k) { back.first = j; back.second = this->lutHexDiametralNode(l); }
	else if (this->lutHexDiametralNode(j) == l) { back.first = j; back.second = this->lutHexDiametralNode(k); }
	// collapsed edges *are* connected
	else if (i==k) { back.first = this->lutHexDiametralNode(l); back.second = j; }
	else if (i==l) { back.first = this->lutHexDiametralNode(k); back.second = j; }
	else if (j==k) { back.first = this->lutHexDiametralNode(l); back.second = i; }
	else if (j==l) { back.first = this->lutHexDiametralNode(k); back.second = i; }
	return back;
}

unsigned MeshRevision::lutPrismThirdNode(unsigned id1, unsigned id2) const
{
	if      ((id1==0 && id2==1) || (id1==1 && id2==2)) return 2;
	else if ((id1==1 && id2==2) || (id1==2 && id2==1)) return 0;
	else if ((id1==0 && id2==2) || (id1==2 && id2==0)) return 1;
	else if ((id1==3 && id2==4) || (id1==4 && id2==3)) return 5;
	else if ((id1==4 && id2==5) || (id1==5 && id2==4)) return 3;
	else if ((id1==3 && id2==5) || (id1==5 && id2==3)) return 4;
	else return std::numeric_limits<unsigned>::max();
}

void MeshRevision::cleanUp(std::vector<MeshLib::Node*> &new_nodes, std::vector<MeshLib::Element*> &new_elements) const
{
	for (auto elem = new_elements.begin(); elem != new_elements.end(); ++elem)
		delete *elem;

	for (auto node = new_nodes.begin(); node != new_nodes.end(); ++node)
		delete *node;
}

} // end namespace MeshLib
