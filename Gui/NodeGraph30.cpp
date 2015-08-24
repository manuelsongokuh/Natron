//  Natron
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
/*
 * Created by Alexandre GAUTHIER-FOICHAT on 6/1/2012.
 * contact: immarespond at gmail dot com
 *
 */

// from <https://docs.python.org/3/c-api/intro.html#include-files>:
// "Since Python may define some pre-processor definitions which affect the standard headers on some systems, you must include Python.h before any standard headers are included."
#include <Python.h>

#include "NodeGraph.h"
#include "NodeGraphPrivate.h"

#include <cstdlib>
#include <set>
#include <map>
#include <vector>
#include <locale>
#include <algorithm> // min, max

GCC_DIAG_UNUSED_PRIVATE_FIELD_OFF
// /opt/local/include/QtGui/qmime.h:119:10: warning: private field 'type' is not used [-Wunused-private-field]
#include <QGraphicsProxyWidget>
GCC_DIAG_UNUSED_PRIVATE_FIELD_ON
#include <QGraphicsTextItem>
#include <QFileSystemModel>
#include <QScrollBar>
#include <QVBoxLayout>
#include <QGraphicsLineItem>
#include <QGraphicsPixmapItem>
#include <QDialogButtonBox>
#include <QUndoStack>
#include <QToolButton>
#include <QThread>
#include <QDropEvent>
#include <QApplication>
#include <QCheckBox>
#include <QMimeData>
#include <QLineEdit>
#include <QDebug>
#include <QtCore/QRectF>
#include <QRegExp>
#include <QtCore/QTimer>
#include <QAction>
#include <QPainter>
CLANG_DIAG_OFF(deprecated)
CLANG_DIAG_OFF(uninitialized)
#include <QMutex>
CLANG_DIAG_ON(deprecated)
CLANG_DIAG_ON(uninitialized)

#include <SequenceParsing.h>

#include "Engine/AppManager.h"
#include "Engine/BackDrop.h"
#include "Engine/Dot.h"
#include "Engine/FrameEntry.h"
#include "Engine/Hash64.h"
#include "Engine/KnobFile.h"
#include "Engine/Node.h"
#include "Engine/NodeGroup.h"
#include "Engine/NodeSerialization.h"
#include "Engine/OfxEffectInstance.h"
#include "Engine/OutputSchedulerThread.h"
#include "Engine/Plugin.h"
#include "Engine/Project.h"
#include "Engine/Settings.h"
#include "Engine/ViewerInstance.h"

#include "Gui/ActionShortcuts.h"
#include "Gui/BackDropGui.h"
#include "Gui/CurveEditor.h"
#include "Gui/CurveWidget.h"
#include "Gui/DockablePanel.h"
#include "Gui/Edge.h"
#include "Gui/FloatingWidget.h"
#include "Gui/Gui.h"
#include "Gui/Gui.h"
#include "Gui/GuiAppInstance.h"
#include "Gui/GuiApplicationManager.h"
#include "Gui/GuiMacros.h"
#include "Gui/Histogram.h"
#include "Gui/KnobGui.h"
#include "Gui/Label.h"
#include "Gui/Menu.h"
#include "Gui/NodeBackDropSerialization.h"
#include "Gui/NodeClipBoard.h"
#include "Gui/NodeCreationDialog.h"
#include "Gui/NodeGraphUndoRedo.h"
#include "Gui/NodeGui.h"
#include "Gui/NodeGuiSerialization.h"
#include "Gui/NodeSettingsPanel.h"
#include "Gui/SequenceFileDialog.h"
#include "Gui/TabWidget.h"
#include "Gui/TimeLineGui.h"
#include "Gui/ToolButton.h"
#include "Gui/ViewerGL.h"
#include "Gui/ViewerTab.h"

#include "Global/QtCompat.h"

using namespace Natron;
using std::cout; using std::endl;


void
NodeGraph::connectCurrentViewerToSelection(int inputNB)
{
    if ( !getLastSelectedViewer() ) {
        _imp->_gui->getApp()->createNode(  CreateNodeArgs(PLUGINID_NATRON_VIEWER,
                                                          "",
                                                          -1,-1,
                                                          true,
                                                          INT_MIN,INT_MIN,
                                                          true,
                                                          true,
                                                          true,
                                                          QString(),
                                                          CreateNodeArgs::DefaultValuesList(),
                                                          getGroup()) );
    }

    ///get a pointer to the last user selected viewer
    boost::shared_ptr<InspectorNode> v = boost::dynamic_pointer_cast<InspectorNode>( getLastSelectedViewer()->
                                                                                     getInternalNode()->getNode() );

    ///if the node is no longer active (i.e: it was deleted by the user), don't do anything.
    if ( !v->isActivated() ) {
        return;
    }

    ///get a ptr to the NodeGui
    boost::shared_ptr<NodeGuiI> gui_i = v->getNodeGui();
    boost::shared_ptr<NodeGui> gui = boost::dynamic_pointer_cast<NodeGui>(gui_i);
    assert(gui);

    ///if there's no selected node or the viewer is selected, then try refreshing that input nb if it is connected.
    bool viewerAlreadySelected = std::find(_imp->_selection.begin(),_imp->_selection.end(),gui) != _imp->_selection.end();
    if (_imp->_selection.empty() || (_imp->_selection.size() > 1) || viewerAlreadySelected) {
        v->setActiveInputAndRefresh(inputNB);
        gui->refreshEdges();

        return;
    }

    boost::shared_ptr<NodeGui> selected = _imp->_selection.front();


    if ( !selected->getNode()->canOthersConnectToThisNode() ) {
        return;
    }

    ///if the node doesn't have the input 'inputNb' created yet, populate enough input
    ///so it can be created.
    Edge* foundInput = gui->getInputArrow(inputNB);
    assert(foundInput);
  
    ///and push a connect command to the selected node.
    pushUndoCommand( new ConnectCommand(this,foundInput,foundInput->getSource(),selected) );

    ///Set the viewer as the selected node (also wipe the current selection)
    selectNode(gui,false);
} // connectCurrentViewerToSelection

void
NodeGraph::enterEvent(QEvent* e)
{
    QGraphicsView::enterEvent(e);
    
    QWidget* currentFocus = qApp->focusWidget();
    
    bool canSetFocus = !currentFocus ||
    dynamic_cast<ViewerGL*>(currentFocus) ||
    dynamic_cast<CurveWidget*>(currentFocus) ||
    dynamic_cast<Histogram*>(currentFocus) ||
    dynamic_cast<NodeGraph*>(currentFocus) ||
    dynamic_cast<QToolButton*>(currentFocus) ||
    currentFocus->objectName() == "Properties" ||
    currentFocus->objectName() == "SettingsPanel" ||
    currentFocus->objectName() == "qt_tabwidget_tabbar";
    
    if (canSetFocus) {
        setFocus();
    }

    _imp->_nodeCreationShortcutEnabled = true;
   
}

void
NodeGraph::leaveEvent(QEvent* e)
{
    QGraphicsView::leaveEvent(e);

    _imp->_nodeCreationShortcutEnabled = false;
   // setFocus();
}

void
NodeGraph::setVisibleNodeDetails(bool visible)
{
    if (visible == _imp->_detailsVisible) {
        return;
    }
    _imp->_detailsVisible = visible;
    QMutexLocker k(&_imp->_nodesMutex);
    for (std::list<boost::shared_ptr<NodeGui> >::const_iterator it = _imp->_nodes.begin(); it != _imp->_nodes.end(); ++it) {
        (*it)->setVisibleDetails(visible);
    }
}

void
NodeGraph::wheelEventInternal(bool ctrlDown,double delta)
{
    double scaleFactor = pow( NATRON_WHEEL_ZOOM_PER_DELTA, delta);
    QTransform transfo = transform();
    
    double currentZoomFactor = transfo.mapRect( QRectF(0, 0, 1, 1) ).width();
    double newZoomfactor = currentZoomFactor * scaleFactor;
    if ((newZoomfactor < 0.01 && scaleFactor < 1.) || (newZoomfactor > 50 && scaleFactor > 1.)) {
        return;
    }
    if (newZoomfactor < 0.4) {
        setVisibleNodeDetails(false);
    } else if (newZoomfactor >= 0.4) {
        setVisibleNodeDetails(true);
    }
    
    if (ctrlDown && _imp->_magnifiedNode) {
        if (!_imp->_magnifOn) {
            _imp->_magnifOn = true;
            _imp->_nodeSelectedScaleBeforeMagnif = _imp->_magnifiedNode->scale();
        }
        _imp->_magnifiedNode->setScale_natron(_imp->_magnifiedNode->scale() * scaleFactor);
    } else {

        _imp->_accumDelta += delta;
        if (std::abs(_imp->_accumDelta) > 60) {
            scaleFactor = pow( NATRON_WHEEL_ZOOM_PER_DELTA, _imp->_accumDelta );
           // setSceneRect(NATRON_SCENE_MIN,NATRON_SCENE_MIN,NATRON_SCENE_MAX,NATRON_SCENE_MAX);
            scale(scaleFactor,scaleFactor);
            _imp->_accumDelta = 0;
        }
        _imp->_refreshOverlays = true;
        
    }

}

void
NodeGraph::wheelEvent(QWheelEvent* e)
{
    if (e->orientation() != Qt::Vertical) {
        return;
    }
    wheelEventInternal(modCASIsControl(e), e->delta());
    _imp->_lastMousePos = e->pos();
    update();
}

void
NodeGraph::keyReleaseEvent(QKeyEvent* e)
{
    if (e->key() == Qt::Key_Control) {
        if (_imp->_magnifOn) {
            _imp->_magnifOn = false;
            _imp->_magnifiedNode->setScale_natron(_imp->_nodeSelectedScaleBeforeMagnif);
        }
        if (_imp->_bendPointsVisible) {
            _imp->setNodesBendPointsVisible(false);
        }
    }
}

void
NodeGraph::removeNode(const boost::shared_ptr<NodeGui> & node)
{
 
    NodeGroup* isGrp = dynamic_cast<NodeGroup*>(node->getNode()->getLiveInstance());
    const std::vector<boost::shared_ptr<KnobI> > & knobs = node->getNode()->getKnobs();

    
    for (U32 i = 0; i < knobs.size(); ++i) {
        std::list<boost::shared_ptr<KnobI> > listeners;
        knobs[i]->getListeners(listeners);
        ///For all listeners make sure they belong to a node
        bool foundEffect = false;
        for (std::list<boost::shared_ptr<KnobI> >::iterator it2 = listeners.begin(); it2 != listeners.end(); ++it2) {
            EffectInstance* isEffect = dynamic_cast<EffectInstance*>( (*it2)->getHolder() );
            if (!isEffect) {
                continue;
            }
            if (isGrp && isEffect->getNode()->getGroup().get() == isGrp) {
                continue;
            }
            
            if ( isEffect && ( isEffect != node->getNode()->getLiveInstance() ) ) {
                foundEffect = true;
                break;
            }
        }
        if (foundEffect) {
            Natron::StandardButtonEnum reply = Natron::questionDialog( tr("Delete").toStdString(), tr("This node has one or several "
                                                                                                  "parameters from which other parameters "
                                                                                                  "of the project rely on through expressions "
                                                                                                  "or links. Deleting this node will "
                                                                                                  "remove these expressions  "
                                                                                                  "and undoing the action will not recover "
                                                                                                  "them. Do you wish to continue ?")
                                                                   .toStdString(), false );
            if (reply == Natron::eStandardButtonNo) {
                return;
            }
            break;
        }
    }

    node->setUserSelected(false);
    std::list<boost::shared_ptr<NodeGui> > nodesToRemove;
    nodesToRemove.push_back(node);
    pushUndoCommand( new RemoveMultipleNodesCommand(this,nodesToRemove) );
}

void
NodeGraph::deleteSelection()
{
    if ( !_imp->_selection.empty()) {
        NodeGuiList nodesToRemove = _imp->_selection;

        
        ///For all backdrops also move all the nodes contained within it
        for (NodeGuiList::iterator it = nodesToRemove.begin(); it != nodesToRemove.end(); ++it) {
            NodeGuiList nodesWithinBD = getNodesWithinBackDrop(*it);
            for (NodeGuiList::iterator it2 = nodesWithinBD.begin(); it2 != nodesWithinBD.end(); ++it2) {
                NodeGuiList::iterator found = std::find(nodesToRemove.begin(),nodesToRemove.end(),*it2);
                if ( found == nodesToRemove.end()) {
                    nodesToRemove.push_back(*it2);
                }
            }
        }


        for (NodeGuiList::iterator it = nodesToRemove.begin(); it != nodesToRemove.end(); ++it) {
            
            const std::vector<boost::shared_ptr<KnobI> > & knobs = (*it)->getNode()->getKnobs();
            bool mustBreak = false;
            
            NodeGroup* isGrp = dynamic_cast<NodeGroup*>((*it)->getNode()->getLiveInstance());
            
            for (U32 i = 0; i < knobs.size(); ++i) {
                std::list<boost::shared_ptr<KnobI> > listeners;
                knobs[i]->getListeners(listeners);

                ///For all listeners make sure they belong to a node
                bool foundEffect = false;
                for (std::list<boost::shared_ptr<KnobI> >::iterator it2 = listeners.begin(); it2 != listeners.end(); ++it2) {
                    EffectInstance* isEffect = dynamic_cast<EffectInstance*>( (*it2)->getHolder() );
                    
                    if (!isEffect) {
                        continue;
                    }
                    if (isGrp && isEffect->getNode()->getGroup().get() == isGrp) {
                        continue;
                    }
                    
                    if ( isEffect && ( isEffect != (*it)->getNode()->getLiveInstance() ) ) {
                        foundEffect = true;
                        break;
                    }
                }
                if (foundEffect) {
                    Natron::StandardButtonEnum reply = Natron::questionDialog( tr("Delete").toStdString(),
                                                                           tr("This node has one or several "
                                                                              "parameters from which other parameters "
                                                                              "of the project rely on through expressions "
                                                                              "or links. Deleting this node will "
                                                                              "remove these expressions. "
                                                                              ". Undoing the action will not recover "
                                                                              "them. \nContinue anyway ?")
                                                                           .toStdString(), false );
                    if (reply == Natron::eStandardButtonNo) {
                        return;
                    }
                    mustBreak = true;
                    break;
                }
            }
            if (mustBreak) {
                break;
            }
        }


        for (NodeGuiList::iterator it = nodesToRemove.begin(); it != nodesToRemove.end(); ++it) {
            (*it)->setUserSelected(false);
        }


        pushUndoCommand( new RemoveMultipleNodesCommand(this,nodesToRemove) );
        _imp->_selection.clear();
    }
} // deleteSelection

void
NodeGraph::selectNode(const boost::shared_ptr<NodeGui> & n,
                      bool addToSelection)
{
    if ( !n->isVisible() ) {
        return;
    }
    bool alreadyInSelection = std::find(_imp->_selection.begin(),_imp->_selection.end(),n) != _imp->_selection.end();


    assert(n);
    if (addToSelection && !alreadyInSelection) {
        _imp->_selection.push_back(n);
    } else if (!addToSelection) {
        clearSelection();
        _imp->_selection.push_back(n);
    }

    n->setUserSelected(true);

    ViewerInstance* isViewer = dynamic_cast<ViewerInstance*>( n->getNode()->getLiveInstance() );
    if (isViewer) {
        OpenGLViewerI* viewer = isViewer->getUiContext();
        const std::list<ViewerTab*> & viewerTabs = _imp->_gui->getViewersList();
        for (std::list<ViewerTab*>::const_iterator it = viewerTabs.begin(); it != viewerTabs.end(); ++it) {
            if ( (*it)->getViewer() == viewer ) {
                setLastSelectedViewer( (*it) );
            }
        }
    }

    bool magnifiedNodeSelected = false;
    if (_imp->_magnifiedNode) {
        magnifiedNodeSelected = std::find(_imp->_selection.begin(),_imp->_selection.end(),_imp->_magnifiedNode)
                                != _imp->_selection.end();
    }
    if (magnifiedNodeSelected && _imp->_magnifOn) {
        _imp->_magnifOn = false;
        _imp->_magnifiedNode->setScale_natron(_imp->_nodeSelectedScaleBeforeMagnif);
    }
}

void
NodeGraph::setLastSelectedViewer(ViewerTab* tab)
{
    _imp->lastSelectedViewer = tab;
}

ViewerTab*
NodeGraph::getLastSelectedViewer() const
{
    return _imp->lastSelectedViewer;
}

void
NodeGraph::setSelection(const std::list<boost::shared_ptr<NodeGui> >& nodes)
{
    clearSelection();
    for (std::list<boost::shared_ptr<NodeGui> >::const_iterator it = nodes.begin(); it != nodes.end(); ++it) {
        selectNode(*it, true);
    }
}

void
NodeGraph::clearSelection()
{
    {
        QMutexLocker l(&_imp->_nodesMutex);
        for (std::list<boost::shared_ptr<NodeGui> >::iterator it = _imp->_selection.begin(); it != _imp->_selection.end(); ++it) {
            (*it)->setUserSelected(false);
        }
    }

    _imp->_selection.clear();

}

void
NodeGraph::updateNavigator()
{
    if ( !areAllNodesVisible() ) {
        _imp->_navigator->setPixmap( QPixmap::fromImage( getFullSceneScreenShot() ) );
        _imp->_navigator->show();
    } else {
        _imp->_navigator->hide();
    }
}

bool
NodeGraph::areAllNodesVisible()
{
    QRectF rect = visibleSceneRect();
    QMutexLocker l(&_imp->_nodesMutex);

    for (std::list<boost::shared_ptr<NodeGui> >::iterator it = _imp->_nodes.begin(); it != _imp->_nodes.end(); ++it) {
        if ( (*it)->isVisible() ) {
            if ( !rect.contains( (*it)->boundingRectWithEdges() ) ) {
                return false;
            }
        }
    }
    return true;
}

QImage
NodeGraph::getFullSceneScreenShot()
{
    ///The bbox of all nodes in the nodegraph
    QRectF sceneR = _imp->calcNodesBoundingRect();

    ///The visible portion of the nodegraph
    QRectF viewRect = visibleSceneRect();

    ///Make sure the visible rect is included in the scene rect
    sceneR = sceneR.united(viewRect);

    int navWidth = std::ceil(width() * NATRON_NAVIGATOR_BASE_WIDTH);
    int navHeight = std::ceil(height() * NATRON_NAVIGATOR_BASE_HEIGHT);

    ///Make sceneR and viewRect keep the same aspect ratio as the navigator
    double xScale = navWidth / sceneR.width();
    double yScale =  navHeight / sceneR.height();
    double scaleFactor = std::max(0.001,std::min(xScale,yScale));
    
    int sceneW_navPixelCoord = std::floor(sceneR.width() * scaleFactor);
    int sceneH_navPixelCoord = std::floor(sceneR.height() * scaleFactor);

    ///Render the scene in an image with the same aspect ratio  as the scene rect
    QImage renderImage(sceneW_navPixelCoord,sceneH_navPixelCoord,QImage::Format_ARGB32_Premultiplied);
    
    ///Fill the background
    renderImage.fill( QColor(71,71,71,255) );

    ///Offset the visible rect corner as an offset relative to the scene rect corner
    viewRect.setX( viewRect.x() - sceneR.x() );
    viewRect.setY( viewRect.y() - sceneR.y() );
    viewRect.setWidth( viewRect.width() - sceneR.x() );
    viewRect.setHeight( viewRect.height() - sceneR.y() );

    QRectF viewRect_navCoordinates = viewRect;
    viewRect_navCoordinates.setLeft(viewRect.left() * scaleFactor);
    viewRect_navCoordinates.setBottom(viewRect.bottom() * scaleFactor);
    viewRect_navCoordinates.setRight(viewRect.right() * scaleFactor);
    viewRect_navCoordinates.setTop(viewRect.top() * scaleFactor);

    ///Paint the visible portion with a highlight
    QPainter painter(&renderImage);

    ///Remove the overlays from the scene before rendering it
    scene()->removeItem(_imp->_cacheSizeText);
    scene()->removeItem(_imp->_navigator);

    ///Render into the QImage with downscaling
    scene()->render(&painter,renderImage.rect(),sceneR,Qt::KeepAspectRatio);

    ///Add the overlays back
    scene()->addItem(_imp->_navigator);
    scene()->addItem(_imp->_cacheSizeText);

    ///Fill the highlight with a semi transparant whitish grey
    painter.fillRect( viewRect_navCoordinates, QColor(200,200,200,100) );
    
    ///Draw a border surrounding the
    QPen p;
    p.setWidth(2);
    p.setBrush(Qt::yellow);
    painter.setPen(p);
    ///Make sure the border is visible
    viewRect_navCoordinates.adjust(2, 2, -2, -2);
    painter.drawRect(viewRect_navCoordinates);

    ///Now make an image of the requested size of the navigator and center the render image into it
    QImage img(navWidth, navHeight, QImage::Format_ARGB32_Premultiplied);
    img.fill( QColor(71,71,71,255) );

    int xOffset = ( img.width() - renderImage.width() ) / 2;
    int yOffset = ( img.height() - renderImage.height() ) / 2;

    int yDest = yOffset;
    for (int y = 0; y < renderImage.height(); ++y, ++yDest) {
       
        if (yDest >= img.height()) {
            break;
        }
        QRgb* dst_pixels = (QRgb*)img.scanLine(yDest);
        const QRgb* src_pixels = (const QRgb*)renderImage.scanLine(y);
        int xDest = xOffset;
        for (int x = 0; x < renderImage.width(); ++x, ++xDest) {
            if (xDest >= img.width()) {
                dst_pixels[xDest] = qRgba(0, 0, 0, 0);
            } else {
                dst_pixels[xDest] = src_pixels[x];
            }
        }
    }

    return img;
} // getFullSceneScreenShot

const std::list<boost::shared_ptr<NodeGui> > &
NodeGraph::getAllActiveNodes() const
{
    return _imp->_nodes;
}

std::list<boost::shared_ptr<NodeGui> >
NodeGraph::getAllActiveNodes_mt_safe() const
{
    QMutexLocker l(&_imp->_nodesMutex);

    return _imp->_nodes;
}

void
NodeGraph::moveToTrash(NodeGui* node)
{
    assert(node);
    for (std::list<boost::shared_ptr<NodeGui> >::iterator it = _imp->_selection.begin();
         it != _imp->_selection.end(); ++it) {
        if (it->get() == node) {
            (*it)->setUserSelected(false);
            _imp->_selection.erase(it);
            break;
        }
    }
    
    QMutexLocker l(&_imp->_nodesMutex);
    for (std::list<boost::shared_ptr<NodeGui> >::iterator it = _imp->_nodes.begin(); it != _imp->_nodes.end(); ++it) {
        if ( (*it).get() == node ) {
            _imp->_nodesTrash.push_back(*it);
            _imp->_nodes.erase(it);
            break;
        }
    }
}

void
NodeGraph::restoreFromTrash(NodeGui* node)
{
    assert(node);
    QMutexLocker l(&_imp->_nodesMutex);
    for (std::list<boost::shared_ptr<NodeGui> >::iterator it = _imp->_nodesTrash.begin(); it != _imp->_nodesTrash.end(); ++it) {
        if ( (*it).get() == node ) {
            _imp->_nodes.push_back(*it);
            _imp->_nodesTrash.erase(it);
            break;
        }
    }
}

void
NodeGraph::refreshAllEdges()
{
    QMutexLocker l(&_imp->_nodesMutex);

    for (std::list<boost::shared_ptr<NodeGui> >::iterator it = _imp->_nodes.begin(); it != _imp->_nodes.end(); ++it) {
        (*it)->refreshEdges();
    }
}

// grabbed from QDirModelPrivate::size() in qtbase/src/widgets/itemviews/qdirmodel.cpp
static
QString
QDirModelPrivate_size(quint64 bytes)
{
    // According to the Si standard KB is 1000 bytes, KiB is 1024
    // but on windows sizes are calulated by dividing by 1024 so we do what they do.
    const quint64 kb = 1024;
    const quint64 mb = 1024 * kb;
    const quint64 gb = 1024 * mb;
    const quint64 tb = 1024 * gb;

    if (bytes >= tb) {
        return QFileSystemModel::tr("%1 TB").arg( QLocale().toString(qreal(bytes) / tb, 'f', 3) );
    }
    if (bytes >= gb) {
        return QFileSystemModel::tr("%1 GB").arg( QLocale().toString(qreal(bytes) / gb, 'f', 2) );
    }
    if (bytes >= mb) {
        return QFileSystemModel::tr("%1 MB").arg( QLocale().toString(qreal(bytes) / mb, 'f', 1) );
    }
    if (bytes >= kb) {
        return QFileSystemModel::tr("%1 KB").arg( QLocale().toString(bytes / kb) );
    }

    return QFileSystemModel::tr("%1 byte(s)").arg( QLocale().toString(bytes) );
}

void
NodeGraph::updateCacheSizeText()
{
    if (!getGui() || getGui()->isGUIFrozen()) {
        return;
    }
    QString oldText = _imp->_cacheSizeText->toPlainText();
    quint64 cacheSize = appPTR->getCachesTotalMemorySize();
    QString cacheSizeStr = QDirModelPrivate_size(cacheSize);
    QString newText = tr("Memory cache size: ") + cacheSizeStr;
    if (newText != oldText) {
        _imp->_cacheSizeText->setPlainText(newText);
    }
}


void
NodeGraph::toggleCacheInfo()
{
    if ( _imp->_cacheSizeText->isVisible() ) {
        _imp->_cacheSizeText->hide();
    } else {
        _imp->_cacheSizeText->show();
    }
}

void
NodeGraph::toggleKnobLinksVisible()
{
    _imp->_knobLinksVisible = !_imp->_knobLinksVisible;
    {
        QMutexLocker l(&_imp->_nodesMutex);
        for (std::list<boost::shared_ptr<NodeGui> >::iterator it = _imp->_nodes.begin(); it != _imp->_nodes.end(); ++it) {
            (*it)->setKnobLinksVisible(_imp->_knobLinksVisible);
        }
        for (std::list<boost::shared_ptr<NodeGui> >::iterator it = _imp->_nodesTrash.begin(); it != _imp->_nodesTrash.end(); ++it) {
            (*it)->setKnobLinksVisible(_imp->_knobLinksVisible);
        }
    }
}

void
NodeGraph::toggleAutoPreview()
{
    _imp->_gui->getApp()->getProject()->toggleAutoPreview();
}

void
NodeGraph::forceRefreshAllPreviews()
{
    _imp->_gui->forceRefreshAllPreviews();
}

void
NodeGraph::showMenu(const QPoint & pos)
{
    _imp->_menu->clear();
    
    QAction* findAction = new ActionWithShortcut(kShortcutGroupNodegraph,kShortcutIDActionGraphFindNode,
                                                 kShortcutDescActionGraphFindNode,_imp->_menu);
    _imp->_menu->addAction(findAction);
    _imp->_menu->addSeparator();
    
    //QFont font(appFont,appFontSize);
    Natron::Menu* editMenu = new Natron::Menu(tr("Edit"),_imp->_menu);
    //editMenu->setFont(font);
    _imp->_menu->addAction( editMenu->menuAction() );
    
    QAction* copyAction = new ActionWithShortcut(kShortcutGroupNodegraph,kShortcutIDActionGraphCopy,
                                                 kShortcutDescActionGraphCopy,editMenu);
    QObject::connect( copyAction,SIGNAL( triggered() ),this,SLOT( copySelectedNodes() ) );
    editMenu->addAction(copyAction);
    
    QAction* cutAction = new ActionWithShortcut(kShortcutGroupNodegraph,kShortcutIDActionGraphCut,
                                                kShortcutDescActionGraphCut,editMenu);
    QObject::connect( cutAction,SIGNAL( triggered() ),this,SLOT( cutSelectedNodes() ) );
    editMenu->addAction(cutAction);
    
    
    QAction* pasteAction = new ActionWithShortcut(kShortcutGroupNodegraph,kShortcutIDActionGraphPaste,
                                                  kShortcutDescActionGraphPaste,editMenu);
    pasteAction->setEnabled( !appPTR->isNodeClipBoardEmpty() );
    editMenu->addAction(pasteAction);
    
    QAction* deleteAction = new ActionWithShortcut(kShortcutGroupNodegraph,kShortcutIDActionGraphRemoveNodes,
                                                   kShortcutDescActionGraphRemoveNodes,editMenu);
    QObject::connect( deleteAction,SIGNAL( triggered() ),this,SLOT( deleteSelection() ) );
    editMenu->addAction(deleteAction);
    
    QAction* duplicateAction = new ActionWithShortcut(kShortcutGroupNodegraph,kShortcutIDActionGraphDuplicate,
                                                      kShortcutDescActionGraphDuplicate,editMenu);
    editMenu->addAction(duplicateAction);
    
    QAction* cloneAction = new ActionWithShortcut(kShortcutGroupNodegraph,kShortcutIDActionGraphClone,
                                                  kShortcutDescActionGraphClone,editMenu);
    editMenu->addAction(cloneAction);
    
    QAction* decloneAction = new ActionWithShortcut(kShortcutGroupNodegraph,kShortcutIDActionGraphDeclone,
                                                    kShortcutDescActionGraphDeclone,editMenu);
    QObject::connect( decloneAction,SIGNAL( triggered() ),this,SLOT( decloneSelectedNodes() ) );
    editMenu->addAction(decloneAction);
    
    QAction* switchInputs = new ActionWithShortcut(kShortcutGroupNodegraph,kShortcutIDActionGraphExtractNode,
                                                   kShortcutDescActionGraphExtractNode,editMenu);
    QObject::connect( switchInputs, SIGNAL( triggered() ), this, SLOT( extractSelectedNode() ) );
    editMenu->addAction(switchInputs);
    
    QAction* extractNode = new ActionWithShortcut(kShortcutGroupNodegraph,kShortcutIDActionGraphSwitchInputs,
                                                   kShortcutDescActionGraphSwitchInputs,editMenu);
    QObject::connect( extractNode, SIGNAL( triggered() ), this, SLOT( switchInputs1and2ForSelectedNodes() ) );
    editMenu->addAction(extractNode);
    
    QAction* disableNodes = new ActionWithShortcut(kShortcutGroupNodegraph,kShortcutIDActionGraphDisableNodes,
                                                   kShortcutDescActionGraphDisableNodes,editMenu);
    QObject::connect( disableNodes, SIGNAL( triggered() ), this, SLOT( toggleSelectedNodesEnabled() ) );
    editMenu->addAction(disableNodes);
    
    QAction* groupFromSel = new ActionWithShortcut(kShortcutGroupNodegraph, kShortcutIDActionGraphMakeGroup,
                                                   kShortcutDescActionGraphMakeGroup,editMenu);
    QObject::connect( groupFromSel, SIGNAL( triggered() ), this, SLOT( createGroupFromSelection() ) );
    editMenu->addAction(groupFromSel);
    
    QAction* expandGroup = new ActionWithShortcut(kShortcutGroupNodegraph, kShortcutIDActionGraphExpandGroup,
                                                   kShortcutDescActionGraphExpandGroup,editMenu);
    QObject::connect( expandGroup, SIGNAL( triggered() ), this, SLOT( expandSelectedGroups() ) );
    editMenu->addAction(expandGroup);
    
    QAction* displayCacheInfoAction = new ActionWithShortcut(kShortcutGroupNodegraph,kShortcutIDActionGraphShowCacheSize,
                                                             kShortcutDescActionGraphShowCacheSize,_imp->_menu);
    displayCacheInfoAction->setCheckable(true);
    displayCacheInfoAction->setChecked( _imp->_cacheSizeText->isVisible() );
    QObject::connect( displayCacheInfoAction,SIGNAL( triggered() ),this,SLOT( toggleCacheInfo() ) );
    _imp->_menu->addAction(displayCacheInfoAction);
    
    QAction* turnOffPreviewAction = new ActionWithShortcut(kShortcutGroupNodegraph,kShortcutIDActionGraphTogglePreview,
                                                           kShortcutDescActionGraphTogglePreview,_imp->_menu);
    turnOffPreviewAction->setCheckable(true);
    turnOffPreviewAction->setChecked(false);
    QObject::connect( turnOffPreviewAction,SIGNAL( triggered() ),this,SLOT( togglePreviewsForSelectedNodes() ) );
    _imp->_menu->addAction(turnOffPreviewAction);
    
    QAction* connectionHints = new ActionWithShortcut(kShortcutGroupNodegraph,kShortcutIDActionGraphEnableHints,
                                                      kShortcutDescActionGraphEnableHints,_imp->_menu);
    connectionHints->setCheckable(true);
    connectionHints->setChecked( appPTR->getCurrentSettings()->isConnectionHintEnabled() );
    QObject::connect( connectionHints,SIGNAL( triggered() ),this,SLOT( toggleConnectionHints() ) );
    _imp->_menu->addAction(connectionHints);
    
    QAction* autoHideInputs = new ActionWithShortcut(kShortcutGroupNodegraph,kShortcutIDActionGraphAutoHideInputs,
                                                      kShortcutDescActionGraphAutoHideInputs,_imp->_menu);
    autoHideInputs->setCheckable(true);
    autoHideInputs->setChecked( appPTR->getCurrentSettings()->areOptionalInputsAutoHidden() );
    QObject::connect( autoHideInputs,SIGNAL( triggered() ),this,SLOT( toggleAutoHideInputs() ) );
    _imp->_menu->addAction(autoHideInputs);
    
    QAction* knobLinks = new ActionWithShortcut(kShortcutGroupNodegraph,kShortcutIDActionGraphShowExpressions,
                                                kShortcutDescActionGraphShowExpressions,_imp->_menu);
    knobLinks->setCheckable(true);
    knobLinks->setChecked( areKnobLinksVisible() );
    QObject::connect( knobLinks,SIGNAL( triggered() ),this,SLOT( toggleKnobLinksVisible() ) );
    _imp->_menu->addAction(knobLinks);
    
    QAction* autoPreview = new ActionWithShortcut(kShortcutGroupNodegraph,kShortcutIDActionGraphToggleAutoPreview,
                                                  kShortcutDescActionGraphToggleAutoPreview,_imp->_menu);
    autoPreview->setCheckable(true);
    autoPreview->setChecked( _imp->_gui->getApp()->getProject()->isAutoPreviewEnabled() );
    QObject::connect( autoPreview,SIGNAL( triggered() ),this,SLOT( toggleAutoPreview() ) );
    QObject::connect( _imp->_gui->getApp()->getProject().get(),SIGNAL( autoPreviewChanged(bool) ),autoPreview,SLOT( setChecked(bool) ) );
    _imp->_menu->addAction(autoPreview);
    
    QAction* autoTurbo = new ActionWithShortcut(kShortcutGroupNodegraph,kShortcutIDActionGraphToggleAutoTurbo,
                                               kShortcutDescActionGraphToggleAutoTurbo,_imp->_menu);
    autoTurbo->setCheckable(true);
    autoTurbo->setChecked( appPTR->getCurrentSettings()->isAutoTurboEnabled() );
    QObject::connect( autoTurbo,SIGNAL( triggered() ),this,SLOT( toggleAutoTurbo() ) );
    _imp->_menu->addAction(autoTurbo);

    
    QAction* forceRefreshPreviews = new ActionWithShortcut(kShortcutGroupNodegraph,kShortcutIDActionGraphForcePreview,
                                                           kShortcutDescActionGraphForcePreview,_imp->_menu);
    QObject::connect( forceRefreshPreviews,SIGNAL( triggered() ),this,SLOT( forceRefreshAllPreviews() ) );
    _imp->_menu->addAction(forceRefreshPreviews);
    
    QAction* frameAllNodes = new ActionWithShortcut(kShortcutGroupNodegraph,kShortcutIDActionGraphFrameNodes,
                                                    kShortcutDescActionGraphFrameNodes,_imp->_menu);
    QObject::connect( frameAllNodes,SIGNAL( triggered() ),this,SLOT( centerOnAllNodes() ) );
    _imp->_menu->addAction(frameAllNodes);
    
    _imp->_menu->addSeparator();
    
    std::list<ToolButton*> orederedToolButtons = _imp->_gui->getToolButtonsOrdered();
    for (std::list<ToolButton*>::iterator it = orederedToolButtons.begin(); it != orederedToolButtons.end(); ++it) {
        (*it)->getMenu()->setIcon( (*it)->getIcon() );
        _imp->_menu->addAction( (*it)->getMenu()->menuAction() );
    }
    
    QAction* ret = _imp->_menu->exec(pos);
    if (ret == findAction) {
        popFindDialog();
    } else if (ret == duplicateAction) {
        QRectF rect = visibleSceneRect();
        duplicateSelectedNodes(rect.center());
    } else if (ret == cloneAction) {
        QRectF rect = visibleSceneRect();
        cloneSelectedNodes(rect.center());
    } else if (ret == pasteAction) {
        QRectF rect = visibleSceneRect();
        pasteNodeClipBoards(rect.center());
    }
}

void
NodeGraph::dropEvent(QDropEvent* e)
{
    if ( !e->mimeData()->hasUrls() ) {
        return;
    }

    QStringList filesList;
    QList<QUrl> urls = e->mimeData()->urls();
    for (int i = 0; i < urls.size(); ++i) {
        const QUrl rl = Natron::toLocalFileUrlFixed(urls.at(i));
        QString path = rl.toLocalFile();

#ifdef __NATRON_WIN32__
        if ( !path.isEmpty() && ( path.at(0) == QChar('/') ) || ( path.at(0) == QChar('\\') ) ) {
            path = path.remove(0,1);
        }

#endif
        
        QDir dir(path);

        //if the path dropped is not a directory append it
        if ( !dir.exists() ) {
            filesList << path;
        } else {
            //otherwise append everything inside the dir recursively
            SequenceFileDialog::appendFilesFromDirRecursively(&dir,&filesList);
        }
    }

    QStringList supportedExtensions;
    std::map<std::string,std::string> writersForFormat;
    appPTR->getCurrentSettings()->getFileFormatsForWritingAndWriter(&writersForFormat);
    for (std::map<std::string,std::string>::const_iterator it = writersForFormat.begin(); it != writersForFormat.end(); ++it) {
        supportedExtensions.push_back( it->first.c_str() );
    }
    QPointF scenePos = mapToScene(e->pos());
    std::vector< boost::shared_ptr<SequenceParsing::SequenceFromFiles> > files = SequenceFileDialog::fileSequencesFromFilesList(filesList,supportedExtensions);
    std::locale local;
    for (U32 i = 0; i < files.size(); ++i) {
        ///get all the decoders
        std::map<std::string,std::string> readersForFormat;
        appPTR->getCurrentSettings()->getFileFormatsForReadingAndReader(&readersForFormat);

        boost::shared_ptr<SequenceParsing::SequenceFromFiles> & sequence = files[i];

        ///find a decoder for this file type
        std::string ext = sequence->fileExtension();
        std::string extLower;
        for (size_t j = 0; j < ext.size(); ++j) {
            extLower.append( 1,std::tolower( ext.at(j),local ) );
        }
        std::map<std::string,std::string>::iterator found = readersForFormat.find(extLower);
        if ( found == readersForFormat.end() ) {
            errorDialog("Reader", "No plugin capable of decoding " + extLower + " was found.");
        } else {
            
            std::string pattern = sequence->generateValidSequencePattern();
            CreateNodeArgs::DefaultValuesList defaultValues;
            defaultValues.push_back(createDefaultValueForParam<std::string>(kOfxImageEffectFileParamName, pattern));
            
            CreateNodeArgs args(found->second.c_str(),
                                "",
                                -1,
                                -1,
                                false,
                                scenePos.x(),scenePos.y(),
                                true,
                                true,
                                false,
                                QString(),
                                defaultValues,
                                getGroup());
            boost::shared_ptr<Natron::Node>  n = getGui()->getApp()->createNode(args);
            
            //And offset scenePos by the Width of the previous node created if several nodes are created
            double w,h;
            n->getSize(&w, &h);
            scenePos.rx() += (w + 10);
        }
    }
} // dropEvent

void
NodeGraph::dragEnterEvent(QDragEnterEvent* e)
{
    e->accept();
}

void
NodeGraph::dragLeaveEvent(QDragLeaveEvent* e)
{
    e->accept();
}

void
NodeGraph::dragMoveEvent(QDragMoveEvent* e)
{
    e->accept();
}