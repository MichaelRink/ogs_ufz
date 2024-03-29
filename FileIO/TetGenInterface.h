/**
 * \file
 * \author Thomas Fischer
 * \date   2011-09-12
 * \brief  Definition of the TetGenInterface class.
 *
 * \copyright
 * Copyright (c) 2013, OpenGeoSys Community (http://www.opengeosys.org)
 *            Distributed under a Modified BSD License.
 *              See accompanying file LICENSE.txt or
 *              http://www.opengeosys.org/project/license
 *
 */

#ifndef TETGENINTERFACE_H_
#define TETGENINTERFACE_H_

#include <vector>

#include "GEOObjects.h"

// forward declaration of class Node and Element
namespace MeshLib
{
	class Node;
	class Element;
	class Mesh;
}

namespace FileIO
{
/**
 * class TetGenInterface is used to read file formats used by <a href="http://tetgen.berlios.de/">TetGen</a>.
 * Currently supported formats are:
 *   poly - Geometric point and surface definition
 *   node - mesh node / geometric point definition
 *   ele  - mesh element definition
 */
class TetGenInterface
{
public:
	TetGenInterface();
	virtual ~TetGenInterface();

	/**
	 * Method reads the TetGen mesh from node file and element file.
	 * @param poly_fname   file name of the poly file
	 * @param geo_objects  where the geometry is written to
	 * @return on success  the method returns true, otherwise it returns false
	 */
	bool readTetGenPoly (std::string const& poly_fname,
	                     GeoLib::GEOObjects &geo_objects);

	/**
	 * Method reads the TetGen mesh from node file and element file.
	 * @param nodes_fname  file name of the nodes file
	 * @param ele_fname    file name of the elements file
	 * @return on success  the method returns a (pointer to a) CFEMesh, else the method returns nullptr
	 */
	MeshLib::Mesh* readTetGenMesh (std::string const& nodes_fname,
	                               std::string const& ele_fname);

	/**
	 * Writes the geometry of a given name to TetGen poly-file.
	 * @param file_name    file name of the new poly file
	 * @param geo_objects  the container for the geometry.
	 * @param geo_name     the name for the geometry.
	 * @return returns true on success and false otherwise.
	 */
	bool writeTetGenPoly(const std::string &file_name,
	                     const GeoLib::GEOObjects &geo_objects,
	                     const std::string &geo_name) const;

private:
	/// Returns the declared number of facets in the poly file.
	std::size_t getNFacets(std::ifstream &input) const;

	/**
	 * Method parses the lines reading the facets from TetGen poly file
	 * @param input     the input stream (input)
	 * @param surfaces  the vector of surfaces to be filled (output)
	 * @param points    the point vector needed for creating surfaces (input)
	 * @return true, if the facets have been read correctly, false if the method detects an error
	 */
	bool parseFacets(std::ifstream &input,
	                 std::vector<GeoLib::Surface*> &surfaces,
	                 std::vector<GeoLib::Point*> &points) const;

	/**
	 * Method reads the nodes from stream and stores them in a node vector.
	 * For this purpose it uses methods parseNodesFileHeader() and parseNodes().
	 * @param input  the input stream
	 * @return true, if all information is read, false if the method detects an error
	 */
	bool readNodesFromStream(std::ifstream &input,
	                         std::vector<MeshLib::Node*> &nodes);

	/**
	 * Method parses the header of the nodes file created by TetGen
	 * @param line              the header is in this string (input)
	 * @param n_nodes           number of nodes in the file (output)
	 * @param dim               the spatial dimension of the node (output)
	 * @param n_attributes      the number of attributes for each node (output)
	 * @param boundary_markers  have the nodes boundary information (output)
	 * @return true, if the file header is read, false if the method detects an error
	 */
	bool parseNodesFileHeader(std::string &line,
	                          std::size_t &n_nodes,
	                          std::size_t &dim,
	                          std::size_t &n_attributes,
	                          bool &boundary_markers) const;
	/**
	 * method parses the lines reading the nodes from TetGen nodes file
	 * @param ins      the input stream (input)
	 * @param nodes    the nodes vector to be filled (input)
	 * @param n_nodes  the number of nodes to read (input)
	 * @param dim      the spatial dimension of the node (input)
	 * @return true, if the nodes are read, false if the method detects an error
	 */
	bool parseNodes(std::ifstream &ins,
	                std::vector<MeshLib::Node*> &nodes,
	                std::size_t n_nodes,
	                std::size_t dim);

	/**
	 * Method reads the elements from stream and stores them in an element vector.
	 * For this purpose it uses methods parseElementsFileHeader() and parseElements().
	 * @param input     the input stream
	 * @param elements  the elements vector to be filled
	 * @param nodes     the node information needed for creating elements
	 * @return true, if all information is read, false if the method detects an error
	 */
	bool readElementsFromStream(std::ifstream &input,
	                            std::vector<MeshLib::Element*> &elements,
	                            const std::vector<MeshLib::Node*> &nodes) const;
	/**
	 * Method parses the header of the elements file created by TetGen
	 * @param line              the header is in this string (input)
	 * @param n_tets            the number of tets to read (input)
	 * @param n_nodes_per_tet   the number of nodes per tets (input)
	 * @param region_attribute  is on output true, if there
	 * @return
	 */
	bool parseElementsFileHeader(std::string &line,
	                             std::size_t &n_tets,
	                             std::size_t &n_nodes_per_tet,
	                             bool &region_attribute) const;
	/**
	 * Method parses the tetrahedras and put them in the element vector of the mesh class.
	 * @param ins the input stream
	 * @param elements          the elements vector to be filled
	 * @param nodes             the node information needed for creating elements
	 * @param n_tets            the number of tetrahedras that should be read
	 * @param n_nodes_per_tet   the number of nodes per tetrahedron
	 * @param region_attribute  if region attribute is true, region information is read
	 * @return true, if the tetrahedras are read, false if the method detects an error
	 */
	bool parseElements(std::ifstream& ins,
	                   std::vector<MeshLib::Element*> &elements,
	                   const std::vector<MeshLib::Node*> &nodes,
	                   std::size_t n_tets,
	                   std::size_t n_nodes_per_tet,
	                   bool region_attribute) const;

	/**
	 * the value is true if the indexing is zero based, else false
	 */
	bool _zero_based_idx;
};
}

#endif /* TETGENINTERFACE_H_ */
