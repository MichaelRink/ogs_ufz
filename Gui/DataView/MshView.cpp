/**
 * Copyright (c) 2012, OpenGeoSys Community (http://www.opengeosys.net)
 *            Distributed under a Modified BSD License.
 *              See accompanying file LICENSE.txt or
 *              http://www.opengeosys.org/LICENSE.txt
 *
 * \file MshView.cpp
 *
 * Created on 2009-09-24 by Lars Bilke
 */

#include "MshView.h"
#include "Mesh.h"
#include "MshEditDialog.h"
#include "MshItem.h"
#include "MshModel.h"
#include "OGSError.h"
#include "ImportFileTypes.h"
#include <QHeaderView>

#include "VtkMeshSource.h"

#include <QContextMenuEvent>
#include <QFileDialog>
#include <QMenu>
#include <QObject>
#include <QSettings>

#include "Legacy/MeshIO.h"
#include "RapidXmlIO/RapidVtuInterface.h"
#include "Writer.h" // necessary to avoid Linker Error in Windows

MshView::MshView( QWidget* parent /*= 0*/ )
	: QTreeView(parent)
{
	//resizeColumnsToContents();
	//resizeRowsToContents();
}

MshView::~MshView()
{
}

void MshView::updateView()
{
	setAlternatingRowColors(true);
	setColumnWidth(0,125);
	size_t nColumns = (this->model() != NULL) ? this->model()->columnCount() : 0;
	for (size_t i = 1; i < nColumns; i++)
		resizeColumnToContents(i);
}

void MshView::selectionChanged( const QItemSelection &selected, const QItemSelection &deselected )
{
	Q_UNUSED(deselected);
	if (!selected.isEmpty())
	{
		const QModelIndex idx = *(selected.indexes().begin());
		const TreeItem* tree_item = static_cast<TreeModel*>(this->model())->getItem(idx);

		const MshItem* list_item = dynamic_cast<const MshItem*>(tree_item);
		if (list_item)
		{
			emit enableSaveButton(true);
			emit enableRemoveButton(true);
		}
		else
		{
			emit enableSaveButton(false);
			emit enableRemoveButton(false);
		}
	}
	//emit itemSelectionChanged(selected, deselected);
	//return QTreeView::selectionChanged(selected, deselected);
}
void MshView::addMesh()
{
	emit openMeshFile(ImportFileType::OGS_MSH);
}

void MshView::removeMesh()
{
	QModelIndex index (this->selectionModel()->currentIndex());
	if (!index.isValid())
		OGSError::box("No mesh selected.");
	else
		emit requestMeshRemoval(index);
}

/*
// Removed functionality. Do we still need this?
void MshView::removeAllMeshes()
{
	TreeItem* root = static_cast<MshModel*>(this->model())->getItem(QModelIndex());
	int nChildren = root->childCount() - 1;
	for (int i = nChildren; i >= 0; i--)
		emit requestMeshRemoval(this->model()->index(i, 0, QModelIndex()));
}
*/

void MshView::contextMenuEvent( QContextMenuEvent* event )
{
	QModelIndex index = this->selectionModel()->currentIndex();
	MshItem* item = dynamic_cast<MshItem*>(static_cast<TreeItem*>(index.internalPointer()));

	if (item)
	{
		QMenu menu;
		QAction* editMeshAction   = menu.addAction("Edit mesh...");
		QAction* checkMeshAction  = menu.addAction("Check mesh quality...");
		//QAction* saveMeshAction   = menu.addAction("Save mesh...");
		menu.addSeparator();
		QMenu direct_cond_menu("DIRECT Conditions");
		menu.addMenu(&direct_cond_menu);
		QAction* addDirectAction  = direct_cond_menu.addAction("Add...");
		QAction* loadDirectAction = direct_cond_menu.addAction("Load...");
		menu.addSeparator();
		//QAction* removeMeshAction = menu.addAction("Remove mesh");
		connect(editMeshAction, SIGNAL(triggered()), this, SLOT(openMshEditDialog()));
		connect(checkMeshAction, SIGNAL(triggered()), this, SLOT(checkMeshQuality()));
		//connect(saveMeshAction, SIGNAL(triggered()), this, SLOT(writeMeshToFile()));
		connect(addDirectAction, SIGNAL(triggered()), this, SLOT(addDIRECTSourceTerms()));
		connect(loadDirectAction, SIGNAL(triggered()), this, SLOT(loadDIRECTSourceTerms()));
		//connect(removeMeshAction, SIGNAL(triggered()), this, SLOT(removeMesh()));
		menu.exec(event->globalPos());
	}
}

void MshView::openMshEditDialog()
{
	MshModel* model = static_cast<MshModel*>(this->model());
	QModelIndex index = this->selectionModel()->currentIndex();
	const MeshLib::Mesh* mesh =
	        static_cast<MshModel*>(this->model())->getMesh(index);

	MshEditDialog meshEdit(mesh);
	connect(&meshEdit, SIGNAL(mshEditFinished(MeshLib::Mesh*)),
		    model, SLOT(addMesh(MeshLib::Mesh*)));
	meshEdit.exec();
}

int MshView::writeToFile() const
{
	QModelIndex index = this->selectionModel()->currentIndex();

	if (!index.isValid())
	{
		OGSError::box("No mesh selected.");
		return 0;
	}

	const MeshLib::Mesh* mesh = static_cast<MshModel*>(this->model())->getMesh(index);

	if (mesh)
	{
		QSettings settings;
		QString mshName = QString::fromStdString(
		        static_cast<MshModel*>(this->model())->getMesh(index)->getName());
		QString fileName = QFileDialog::getSaveFileName(NULL, "Save mesh as",
		                                    settings.value("lastOpenedMeshFileDirectory").toString(),
											"VTK Unstructured Grid (*.vtu);;GeoSys legacy mesh file (*.msh)");

		if (!fileName.isEmpty())
		{
			QFileInfo fi(fileName);
			if (fi.suffix().toLower() == "vtu")
			{
				FileIO::RapidVtuInterface vtkIO;
				vtkIO.setMesh(mesh);
				vtkIO.writeToFile(fileName.toStdString().c_str());
			}
			if (fi.suffix().toLower() == "msh")
			{
				FileIO::MeshIO meshIO;
				meshIO.setMesh(mesh);
				meshIO.writeToFile(fileName.toStdString().c_str());
			}
			QDir dir = QDir(fileName);
			settings.setValue("lastOpenedMeshFileDirectory", dir.absolutePath());
			return 1;
		}
		else
			OGSError::box("No file name entered.");
	}
	return 0;
}

void MshView::addDIRECTSourceTerms()
{
	QModelIndex index = this->selectionModel()->currentIndex();
	const MeshLib::Mesh* grid = static_cast<MshModel*>(this->model())->getMesh(index);
	emit requestCondSetupDialog(grid->getName(), GeoLib::INVALID, 0, false);
}


void MshView::loadDIRECTSourceTerms()
{
	QModelIndex index = this->selectionModel()->currentIndex();
	const MeshLib::Mesh* grid = static_cast<MshModel*>(this->model())->getMesh(index);
	// TODO6 const std::vector<MeshLib::Node*>* nodes = grid->getNodes();
	// TODO6 emit requestDIRECTSourceTerms(grid->getName(), nodes);
}

void MshView::checkMeshQuality ()
{
	QModelIndex index = this->selectionModel()->currentIndex();
	MshItem* item = static_cast<MshItem*>(static_cast<MshModel*>(this->model())->getItem(index));
	emit qualityCheckRequested(item->vtkSource());
}

/*
   void DataView::selectionChangedFromOutside( const QItemSelection &selected, const QItemSelection &deselected )
   {
    QItemSelectionModel* selModel = this->selectionModel();

    Q_ASSERT(selModel);

    selModel->blockSignals(true);
    selModel->select(deselected, QItemSelectionModel::Deselect);
    selModel->select(selected, QItemSelectionModel::Select);
    selModel->blockSignals(false);

    Model* model = static_cast<Model*>(this->model());
    //model->setSelectionFromOutside(selected, deselected);

    return QTreeView::selectionChanged(selected, deselected);
   }

   void DataView::clearSelection()
   {
    selectionModel()->clearSelection();
   }
 */
