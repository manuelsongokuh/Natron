/* ***** BEGIN LICENSE BLOCK *****
 * This file is part of Natron <http://www.natron.fr/>,
 * Copyright (C) 2016 INRIA and Alexandre Gauthier-Foichat
 *
 * Natron is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * Natron is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with Natron.  If not, see <http://www.gnu.org/licenses/gpl-2.0.html>
 * ***** END LICENSE BLOCK ***** */

// ***** BEGIN PYTHON BLOCK *****
// from <https://docs.python.org/3/c-api/intro.html#include-files>:
// "Since Python may define some pre-processor definitions which affect the standard headers on some systems, you must include Python.h before any standard headers are included."
#include <Python.h>
// ***** END PYTHON BLOCK *****

#include "PyNode.h"

#include <cassert>
#include <stdexcept>

#include "Engine/Node.h"
#include "Engine/KnobTypes.h"
#include "Engine/KnobFile.h"
#include "Engine/AppInstance.h"
#include "Engine/EffectInstance.h"
#include "Engine/NodeGroup.h"
#include "Engine/PyRoto.h"
#include "Engine/Hash64.h"

NATRON_NAMESPACE_ENTER;

ImageLayer::ImageLayer(const std::string& layerName,
           const std::string& componentsPrettyName,
           const std::vector<std::string>& componentsName)
: _comps(layerName,componentsPrettyName, componentsName)
{
    
}

ImageLayer::ImageLayer(const ImageComponents& internalComps)
: _comps(internalComps)
{
    
}

int
ImageLayer::getHash(const ImageLayer& layer)
{
    Hash64 h;
    Hash64_appendQString(&h, layer._comps.getLayerName().c_str());
    const std::vector<std::string>& comps = layer._comps.getComponentsNames();
    for (std::size_t i = 0; i < comps.size(); ++i) {
        Hash64_appendQString(&h, comps[i].c_str());
    }
    return (int)h.value();
}

bool
ImageLayer::isColorPlane() const
{
    return _comps.isColorPlane();
}

int
ImageLayer::getNumComponents() const
{
    return _comps.getNumComponents();
}

const std::string&
ImageLayer::getLayerName() const
{
    return _comps.getLayerName();
}

const std::vector<std::string>&
ImageLayer::getComponentsNames() const
{
    return _comps.getComponentsNames();
}

const std::string&
ImageLayer::getComponentsPrettyName() const
{
    return _comps.getComponentsGlobalName();
}

bool ImageLayer::operator==(const ImageLayer& other) const
{
    return _comps == other._comps;
}


bool
ImageLayer::operator<(const ImageLayer& other) const
{
    return _comps < other._comps;
}

/*
 * These are default presets image components
 */
ImageLayer ImageLayer::getNoneComponents()
{
    return ImageLayer(ImageComponents::getNoneComponents());
}

ImageLayer ImageLayer::getRGBAComponents()
{
    return ImageLayer(ImageComponents::getRGBAComponents());
}

ImageLayer ImageLayer::getRGBComponents()
{
    return ImageLayer(ImageComponents::getRGBComponents());
}

ImageLayer ImageLayer::getAlphaComponents()
{
    return ImageLayer(ImageComponents::getAlphaComponents());
}

ImageLayer ImageLayer::getBackwardMotionComponents()
{
    return ImageLayer(ImageComponents::getBackwardMotionComponents());
}

ImageLayer ImageLayer::getForwardMotionComponents()
{
    return ImageLayer(ImageComponents::getForwardMotionComponents());
}

ImageLayer ImageLayer::getDisparityLeftComponents()
{
    return ImageLayer(ImageComponents::getDisparityLeftComponents());
}

ImageLayer ImageLayer::getDisparityRightComponents()
{
    return ImageLayer(ImageComponents::getDisparityRightComponents());
}

UserParamHolder::UserParamHolder()
: _holder(0)
{
    
}

UserParamHolder::UserParamHolder(KnobHolder* holder)
: _holder(holder)
{
    
}

void
UserParamHolder::setHolder(KnobHolder* holder)
{
    assert(!_holder);
    _holder = holder;
}

Effect::Effect(const NodePtr& node)
: Group()
, UserParamHolder(node ? node->getEffectInstance().get() : 0)
, _node(node)
{
    if (node) {
        boost::shared_ptr<NodeGroup> grp;
        if (node->getEffectInstance()) {
            grp = boost::dynamic_pointer_cast<NodeGroup>(node->getEffectInstance()->shared_from_this());
            init(boost::dynamic_pointer_cast<NodeCollection>(grp));
        }
        
    }
}

Effect::~Effect()
{
    
}

NodePtr
Effect::getInternalNode() const
{
    return _node.lock();
}

void
Effect::destroy(bool autoReconnect)
{
    getInternalNode()->destroyNode(autoReconnect);
}

int
Effect::getMaxInputCount() const
{
    return getInternalNode()->getMaxInputCount();
}

bool
Effect::canConnectInput(int inputNumber,const Effect* node) const
{

    if (!node) {
        return false;
    }
    
    if (!node->getInternalNode()) {
        return false;
    }
    Node::CanConnectInputReturnValue ret = getInternalNode()->canConnectInput(node->getInternalNode(),inputNumber);
    return ret == Node::eCanConnectInput_ok ||
    ret == Node::eCanConnectInput_differentFPS ||
    ret == Node::eCanConnectInput_differentPars;
}

bool
Effect::connectInput(int inputNumber,const Effect* input)
{
    if (canConnectInput(inputNumber, input)) {
        return getInternalNode()->connectInput(input->getInternalNode(), inputNumber);
    } else {
        return false;
    }
}

void
Effect::disconnectInput(int inputNumber)
{
    getInternalNode()->disconnectInput(inputNumber);
}

Effect*
Effect::getInput(int inputNumber) const
{
    NodePtr node = getInternalNode()->getRealInput(inputNumber);
    if (node) {
        return new Effect(node);
    }
    return NULL;
}

std::string
Effect::getScriptName() const
{
    return getInternalNode()->getScriptName_mt_safe();
}

bool
Effect::setScriptName(const std::string& scriptName)
{
    try {
        getInternalNode()->setScriptName(scriptName);
    } catch (...) {
        return false;
    }
    return true;
}

std::string
Effect::getLabel() const
{
    return getInternalNode()->getLabel_mt_safe();
}


void
Effect::setLabel(const std::string& name)
{
    return getInternalNode()->setLabel(name);
}

std::string
Effect::getInputLabel(int inputNumber)
{
    try {
        return getInternalNode()->getInputLabel(inputNumber);
    } catch (const std::exception& e) {
        getInternalNode()->getApp()->appendToScriptEditor(e.what());
    }
    return std::string();
}

std::string
Effect::getPluginID() const
{
    return getInternalNode()->getPluginID();
}

Param*
Effect::createParamWrapperForKnob(const KnobPtr& knob)
{
    int dims = knob->getDimension();
    boost::shared_ptr<KnobInt> isInt = boost::dynamic_pointer_cast<KnobInt>(knob);
    boost::shared_ptr<KnobDouble> isDouble = boost::dynamic_pointer_cast<KnobDouble>(knob);
    boost::shared_ptr<KnobBool> isBool = boost::dynamic_pointer_cast<KnobBool>(knob);
    boost::shared_ptr<KnobChoice> isChoice = boost::dynamic_pointer_cast<KnobChoice>(knob);
    boost::shared_ptr<KnobColor> isColor = boost::dynamic_pointer_cast<KnobColor>(knob);
    boost::shared_ptr<KnobString> isString = boost::dynamic_pointer_cast<KnobString>(knob);
    boost::shared_ptr<KnobFile> isFile = boost::dynamic_pointer_cast<KnobFile>(knob);
    boost::shared_ptr<KnobOutputFile> isOutputFile = boost::dynamic_pointer_cast<KnobOutputFile>(knob);
    boost::shared_ptr<KnobPath> isPath = boost::dynamic_pointer_cast<KnobPath>(knob);
    boost::shared_ptr<KnobButton> isButton = boost::dynamic_pointer_cast<KnobButton>(knob);
    boost::shared_ptr<KnobGroup> isGroup = boost::dynamic_pointer_cast<KnobGroup>(knob);
    boost::shared_ptr<KnobPage> isPage = boost::dynamic_pointer_cast<KnobPage>(knob);
    boost::shared_ptr<KnobParametric> isParametric = boost::dynamic_pointer_cast<KnobParametric>(knob);
    boost::shared_ptr<KnobSeparator> isSep = boost::dynamic_pointer_cast<KnobSeparator>(knob);
    
    if (isInt) {
        switch (dims) {
            case 1:
                return new IntParam(isInt);
            case 2:
                return new Int2DParam(isInt);
            case 3:
                return new Int3DParam(isInt);
            default:
                break;
        }
    } else if (isDouble) {
        switch (dims) {
            case 1:
                return new DoubleParam(isDouble);
            case 2:
                return new Double2DParam(isDouble);
            case 3:
                return new Double3DParam(isDouble);
            default:
                break;
        }
    } else if (isBool) {
        return new BooleanParam(isBool);
    } else if (isChoice) {
        return new ChoiceParam(isChoice);
    } else if (isColor) {
        return new ColorParam(isColor);
    } else if (isString) {
        return new StringParam(isString);
    } else if (isFile) {
        return new FileParam(isFile);
    } else if (isOutputFile) {
        return new OutputFileParam(isOutputFile);
    } else if (isPath) {
        return new PathParam(isPath);
    } else if (isGroup) {
        return new GroupParam(isGroup);
    } else if (isPage) {
        return new PageParam(isPage);
    } else if (isParametric) {
        return new ParametricParam(isParametric);
    } else if (isButton) {
        return new ButtonParam(isButton);
    } else if (isSep) {
        return new SeparatorParam(isSep);
    }
    return NULL;
}

std::list<Param*>
Effect::getParams() const
{
    std::list<Param*> ret;
    const KnobsVec& knobs = getInternalNode()->getKnobs();
    for (KnobsVec::const_iterator it = knobs.begin(); it != knobs.end(); ++it) {
        Param* p = createParamWrapperForKnob(*it);
        if (p) {
            ret.push_back(p);
        }
    }
    return ret;
}

Param*
Effect::getParam(const std::string& name) const
{
    KnobPtr knob = getInternalNode()->getKnobByName(name);
    if (knob) {
        return createParamWrapperForKnob(knob);
    } else {
        return NULL;
    }
}

int
Effect::getCurrentTime() const
{
    return getInternalNode()->getEffectInstance()->getCurrentTime();
}

void
Effect::setPosition(double x,double y)
{
    getInternalNode()->setPosition(x, y);
}

void
Effect::getPosition(double* x, double* y) const
{
    getInternalNode()->getPosition(x, y);
}

void
Effect::setSize(double w,double h)
{
    getInternalNode()->setSize(w, h);
}

void
Effect::getSize(double* w, double* h) const
{
    getInternalNode()->getSize(w, h);
}

void
Effect::getColor(double* r,double *g, double* b) const
{
    getInternalNode()->getColor(r, g, b);
}

void
Effect::setColor(double r, double g, double b)
{
    getInternalNode()->setColor(r, g, b);
}

bool
Effect::isNodeSelected() const
{
    return getInternalNode()->isUserSelected();
}

void
Effect::beginChanges()
{
    getInternalNode()->getEffectInstance()->beginChanges();
    getInternalNode()->beginInputEdition();
}

void
Effect::endChanges()
{
    getInternalNode()->getEffectInstance()->endChanges();
    getInternalNode()->endInputEdition(true);
}

IntParam*
UserParamHolder::createIntParam(const std::string& name, const std::string& label)
{
    boost::shared_ptr<KnobInt> knob = _holder->createIntKnob(name, label, 1);
    if (knob) {
        return new IntParam(knob);
    } else {
        return 0;
    }
}

Int2DParam*
UserParamHolder::createInt2DParam(const std::string& name, const std::string& label)
{
    boost::shared_ptr<KnobInt> knob = _holder->createIntKnob(name, label, 2);
    if (knob) {
        return new Int2DParam(knob);
    } else {
        return 0;
    }

}

Int3DParam*
UserParamHolder::createInt3DParam(const std::string& name, const std::string& label)
{
    boost::shared_ptr<KnobInt> knob = _holder->createIntKnob(name, label, 3);
    if (knob) {
        return new Int3DParam(knob);
    } else {
        return 0;
    }

}

DoubleParam*
UserParamHolder::createDoubleParam(const std::string& name, const std::string& label)
{
    boost::shared_ptr<KnobDouble> knob = _holder->createDoubleKnob(name, label, 1);
    if (knob) {
        return new DoubleParam(knob);
    } else {
        return 0;
    }
   
}

Double2DParam*
UserParamHolder::createDouble2DParam(const std::string& name, const std::string& label)
{
    boost::shared_ptr<KnobDouble> knob = _holder->createDoubleKnob(name, label, 2);
    if (knob) {
        return new Double2DParam(knob);
    } else {
        return 0;
    }
}

Double3DParam*
UserParamHolder::createDouble3DParam(const std::string& name, const std::string& label)
{
    boost::shared_ptr<KnobDouble> knob = _holder->createDoubleKnob(name, label, 3);
    if (knob) {
        return new Double3DParam(knob);
    } else {
        return 0;
    }
}

BooleanParam*
UserParamHolder::createBooleanParam(const std::string& name, const std::string& label)
{
    boost::shared_ptr<KnobBool> knob = _holder->createBoolKnob(name, label);
    if (knob) {
        return new BooleanParam(knob);
    } else {
        return 0;
    }
}

ChoiceParam*
UserParamHolder::createChoiceParam(const std::string& name, const std::string& label)
{
    boost::shared_ptr<KnobChoice> knob = _holder->createChoiceKnob(name, label);
    if (knob) {
        return new ChoiceParam(knob);
    } else {
        return 0;
    }
}

ColorParam*
UserParamHolder::createColorParam(const std::string& name, const std::string& label, bool useAlpha)
{
    boost::shared_ptr<KnobColor> knob = _holder->createColorKnob(name, label, useAlpha ? 4 : 3);
    if (knob) {
        return new ColorParam(knob);
    } else {
        return 0;
    }
}

StringParam*
UserParamHolder::createStringParam(const std::string& name, const std::string& label)
{
    boost::shared_ptr<KnobString> knob = _holder->createStringKnob(name, label);
    if (knob) {
        return new StringParam(knob);
    } else {
        return 0;
    }
}

FileParam*
UserParamHolder::createFileParam(const std::string& name, const std::string& label)
{
    boost::shared_ptr<KnobFile> knob = _holder->createFileKnob(name, label);
    if (knob) {
        return new FileParam(knob);
    } else {
        return 0;
    }
}

OutputFileParam*
UserParamHolder::createOutputFileParam(const std::string& name, const std::string& label)
{
    boost::shared_ptr<KnobOutputFile> knob = _holder->createOuptutFileKnob(name, label);
    if (knob) {
        return new OutputFileParam(knob);
    } else {
        return 0;
    }
}

PathParam*
UserParamHolder::createPathParam(const std::string& name, const std::string& label)
{
    boost::shared_ptr<KnobPath> knob = _holder->createPathKnob(name, label);
    if (knob) {
        return new PathParam(knob);
    } else {
        return 0;
    }
}

ButtonParam*
UserParamHolder::createButtonParam(const std::string& name, const std::string& label)
{
    boost::shared_ptr<KnobButton> knob = _holder->createButtonKnob(name, label);
    if (knob) {
        return new ButtonParam(knob);
    } else {
        return 0;
    }
}

SeparatorParam*
UserParamHolder::createSeparatorParam(const std::string& name, const std::string& label)
{
    boost::shared_ptr<KnobSeparator> knob = _holder->createSeparatorKnob(name, label);
    if (knob) {
        return new SeparatorParam(knob);
    } else {
        return 0;
    }
}

GroupParam*
UserParamHolder::createGroupParam(const std::string& name, const std::string& label)
{
    boost::shared_ptr<KnobGroup> knob = _holder->createGroupKnob(name, label);
    if (knob) {
        return new GroupParam(knob);
    } else {
        return 0;
    }
}

PageParam*
UserParamHolder::createPageParam(const std::string& name, const std::string& label)
{
    if (!_holder) {
        assert(false);
        return 0;
    }
    boost::shared_ptr<KnobPage> knob = _holder->createPageKnob(name, label);
    if (knob) {
        return new PageParam(knob);
    } else {
        return 0;
    }
}

ParametricParam*
UserParamHolder::createParametricParam(const std::string& name, const std::string& label, int nbCurves)
{
    boost::shared_ptr<KnobParametric> knob = _holder->createParametricKnob(name, label, nbCurves);
    if (knob) {
        return new ParametricParam(knob);
    } else {
        return 0;
    }
}

bool
UserParamHolder::removeParam(Param* param)
{
    if (!param) {
        return false;
    }
    if (!param->getInternalKnob()) {
        return false;
    }
    if (!param->getInternalKnob()->isUserKnob()) {
        return false;
    }
    
    _holder->removeDynamicKnob(param->getInternalKnob().get());
    
    return true;
}

PageParam*
Effect::getUserPageParam() const
{
    boost::shared_ptr<KnobPage> page = getInternalNode()->getEffectInstance()->getOrCreateUserPageKnob();
    assert(page);
    return new PageParam(page);
}

void
UserParamHolder::refreshUserParamsGUI()
{
    _holder->refreshKnobs(false);
}

Effect*
Effect::createChild()
{
    if (!getInternalNode()->isMultiInstance()) {
        return 0;
    }
    
    CreateNodeArgs args(getInternalNode()->getPluginID().c_str(), eCreateNodeReasonInternal, getInternalNode()->getGroup());
    args.multiInstanceParentName = getInternalNode()->getScriptName();
    NodePtr child = getInternalNode()->getApp()->createNode(args);
    if (child) {
        return new Effect(child);
    }
    return 0;
}

Roto*
Effect::getRotoContext() const
{
    boost::shared_ptr<RotoContext> roto = getInternalNode()->getRotoContext();
    if (roto) {
        return new Roto(roto);
    }
    return 0;
}

RectD
Effect::getRegionOfDefinition(double time,
                              int view) const
{
    RectD rod;
    if (!getInternalNode() || !getInternalNode()->getEffectInstance()) {
        return rod;
    }
    U64 hash = getInternalNode()->getHashValue();
    RenderScale s(1.);
    bool isProject;
    StatusEnum stat = getInternalNode()->getEffectInstance()->getRegionOfDefinition_public(hash, time, s, ViewIdx(view), &rod, &isProject);
    if (stat != eStatusOK) {
        return RectD();
    }
    return rod;
}

void
Effect::setSubGraphEditable(bool editable)
{
    if (!getInternalNode()) {
        return;
    }
    NodeGroup* isGroup = getInternalNode()->isEffectGroup();
    if (isGroup) {
        isGroup->setSubGraphEditable(editable);
    }
}

bool
Effect::addUserPlane(const std::string& planeName, const std::vector<std::string>& channels)
{
    if (planeName.empty() ||
        channels.size() < 1 ||
        channels.size() > 4) {
        return false;
    }
    std::string compsGlobal;
    for (std::size_t i = 0; i < channels.size(); ++i) {
        compsGlobal.append(channels[i]);
    }
    ImageComponents comp(planeName,compsGlobal,channels);
    return getInternalNode()->addUserComponents(comp);
}

std::map<ImageLayer,Effect*>
Effect::getAvailableLayers() const
{
    std::map<ImageLayer,Effect*> ret;
    if (!getInternalNode()) {
        return ret;
    }
    EffectInstance::ComponentsAvailableMap availComps;
    getInternalNode()->getEffectInstance()->getComponentsAvailable(true, true, getInternalNode()->getEffectInstance()->getCurrentTime(), &availComps);
    for (EffectInstance::ComponentsAvailableMap::iterator it = availComps.begin(); it != availComps.end(); ++it) {
        NodePtr node = it->second.lock();
        if (node) {
            Effect* effect = new Effect(node);
            ImageLayer layer(it->first);
            ret.insert(std::make_pair(layer, effect));
        }
    }
    return ret;
}

void
Effect::setPagesOrder(const std::list<std::string>& pages)
{
    if (!getInternalNode()) {
        return;
    }
    getInternalNode()->setPagesOrder(pages);
}

NATRON_NAMESPACE_EXIT;
