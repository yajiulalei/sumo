/****************************************************************************/
// Eclipse SUMO, Simulation of Urban MObility; see https://eclipse.org/sumo
// Copyright (C) 2001-2019 German Aerospace Center (DLR) and others.
// This program and the accompanying materials
// are made available under the terms of the Eclipse Public License v2.0
// which accompanies this distribution, and is available at
// http://www.eclipse.org/legal/epl-v20.html
// SPDX-License-Identifier: EPL-2.0
/****************************************************************************/
/// @file    GNEStop.cpp
/// @author  Pablo Alvarez Lopez
/// @date    March 2019
/// @version $Id$
///
// Representation of Stops in NETEDIT
/****************************************************************************/

// ===========================================================================
// included modules
// ===========================================================================

#include <cmath>
#include <microsim/MSVehicle.h>
#include <microsim/devices/MSDevice_BTreceiver.h>
#include <netedit/additionals/GNEStoppingPlace.h>
#include <netedit/GNENet.h>
#include <netedit/GNEUndoList.h>
#include <netedit/GNEViewNet.h>
#include <netedit/GNEViewParent.h>
#include <netedit/changes/GNEChange_Attribute.h>
#include <netedit/frames/GNESelectorFrame.h>
#include <netedit/netelements/GNEEdge.h>
#include <netedit/netelements/GNELane.h>
#include <netedit/additionals/GNEAdditional.h>
#include <utils/geom/GeomHelper.h>
#include <utils/gui/div/GLHelper.h>
#include <utils/gui/div/GUIGlobalSelection.h>
#include <utils/gui/div/GUIBaseVehicleHelper.h>
#include <utils/gui/globjects/GLIncludes.h>

#include "GNEStop.h"

// ===========================================================================
// member method definitions
// ===========================================================================

GNEStop::GNEStop(SumoXMLTag tag, GNEViewNet* viewNet, const SUMOVehicleParameter::Stop &stopParameter, GNEAdditional* stoppingPlace, GNEDemandElement* stopParent) :
    GNEDemandElement(stopParent, viewNet, GLO_STOP, tag, {}, {}, {stoppingPlace}, {stopParent}, {}, {}, {}, {}),
    SUMOVehicleParameter::Stop(stopParameter),
    myFriendlyPosition(false) {
}


GNEStop::GNEStop(GNEViewNet* viewNet, const SUMOVehicleParameter::Stop &stopParameter, GNELane* lane, const std::string &startPosition, const std::string &endPosition, bool friendlyPosition, GNEDemandElement* stopParent) :
    GNEDemandElement(stopParent, viewNet, GLO_STOP, SUMO_TAG_STOP_LANE, {}, {lane}, {}, {stopParent}, {}, {}, {}, {}),
    SUMOVehicleParameter::Stop(stopParameter),
    myFriendlyPosition(friendlyPosition) { 
}


GNEStop::~GNEStop() {}


std::string 
GNEStop::getBegin() const {
    return "";
}


const RGBColor &
GNEStop::getColor() const {
    return RGBColor::BLACK;
}


void 
GNEStop::writeDemandElement(OutputDevice& device) const {
    write(device);
}


void
GNEStop::moveGeometry(const Position& offset) {
    // only move if at leats start or end positions is defined
    if ((myLaneParents.size() > 0) && ((parametersSet |= STOP_START_SET) || (parametersSet |= STOP_END_SET))) {
        // Calculate new position using old position
        Position newPosition = myMove.originalViewPosition;
        newPosition.add(offset);
        // filtern position using snap to active grid
        newPosition = myViewNet->snapToActiveGrid(newPosition);
        double offsetLane = myLaneParents.front()->getShape().nearest_offset_to_point2D(newPosition, false) - myLaneParents.front()->getShape().nearest_offset_to_point2D(myMove.originalViewPosition, false);
        // check if both position has to be moved
        if ((parametersSet |= STOP_START_SET) && (parametersSet |= STOP_END_SET)) {
            // calculate stoppingPlace lenght and lane lenght (After apply geometry factor)
            double stoppingPlaceLenght = fabs(parse<double>(myMove.secondOriginalPosition) - parse<double>(myMove.firstOriginalLanePosition));
            double laneLengt = myLaneParents.front()->getParentEdge().getNBEdge()->getFinalLength() * myLaneParents.front()->getLengthGeometryFactor();
            // avoid changing stopping place's lenght
            if ((parse<double>(myMove.firstOriginalLanePosition) + offsetLane) < 0) {
                startPos = 0;
                endPos = stoppingPlaceLenght;
            } else if ((parse<double>(myMove.secondOriginalPosition) + offsetLane) > laneLengt) {
                startPos = laneLengt - stoppingPlaceLenght;
                endPos = laneLengt;
            } else {
                startPos = parse<double>(myMove.firstOriginalLanePosition) + offsetLane;
                endPos = parse<double>(myMove.secondOriginalPosition) + offsetLane;
            }
        } else {
            // check if start position must be moved
            if ((parametersSet |= STOP_START_SET)) {
                startPos = parse<double>(myMove.firstOriginalLanePosition) + offsetLane;
            }
            // check if start position must be moved
            if ((parametersSet |= STOP_END_SET)) {
                endPos = parse<double>(myMove.secondOriginalPosition) + offsetLane;
            }
        }
        // Update geometry
        updateGeometry(false);
    }
}


void
GNEStop::commitGeometryMoving(GNEUndoList* undoList) {
    // only commit geometry moving if at leats start or end positions is defined
    if ((myLaneParents.size() > 0) && ((parametersSet |= STOP_START_SET) || (parametersSet |= STOP_END_SET))) {
        undoList->p_begin("position of " + getTagStr());
        if (parametersSet |= STOP_START_SET) {
            undoList->p_add(new GNEChange_Attribute(this, myViewNet->getNet(), SUMO_ATTR_STARTPOS, toString(startPos), true, myMove.firstOriginalLanePosition));
        }
        if (parametersSet |= STOP_END_SET) {
            undoList->p_add(new GNEChange_Attribute(this, myViewNet->getNet(), SUMO_ATTR_ENDPOS, toString(endPos), true, myMove.secondOriginalPosition));
        }
        undoList->p_end();
    }
}


void
GNEStop::updateGeometry(bool updateGrid) {
    // first check if object has to be removed from grid (SUMOTree)
    if (updateGrid) {
        myViewNet->getNet()->removeGLObjectFromGrid(this);
    }
    // Clear all containers
    myGeometry.clearGeometry();
    //only update Stops over lanes, because other uses the geometry of stopping place parent
    if (myLaneParents.size() > 0) {
        // Cut shape using as delimitators fixed start position and fixed end position
        myGeometry.shape = myLaneParents.front()->getShape().getSubpart(getStartGeometryPositionOverLane(), getEndGeometryPositionOverLane());
        // Get calculate lenghts and rotations
        myGeometry.calculateShapeRotationsAndLengths();
    } else if (myAdditionalParents.size() > 0) {
        // copy geometry of additional
        myGeometry.shape = myAdditionalParents.at(0)->getAdditionalGeometry().shape;
        myGeometry.shapeLengths = myAdditionalParents.at(0)->getAdditionalGeometry().shapeLengths;
        myGeometry.shapeRotations = myAdditionalParents.at(0)->getAdditionalGeometry().shapeRotations;
    }
    // last step is to check if object has to be added into grid (SUMOTree) again
    if (updateGrid) {
        myViewNet->getNet()->addGLObjectIntoGrid(this);
    }
}


Position
GNEStop::getPositionInView() const {
    if (myLaneParents.size() > 0) {
        if (myLaneParents.front()->getShape().length() < 2.5) {
            return myLaneParents.front()->getShape().front();
        } else {
            Position A = myLaneParents.front()->getShape().positionAtOffset(2.5);
            Position B = myLaneParents.front()->getShape().positionAtOffset(2.5);
            // return Middle point
            return Position((A.x() + B.x()) / 2, (A.y() + B.y()) / 2);
        }
    } else if (myDemandElementParents.size() > 0) {
        return myDemandElementParents.front()->getPositionInView();
    } else {
        throw ProcessError("Invalid Stop parent");
    }
}


std::string
GNEStop::getParentName() const {
    if (myDemandElementParents.size() > 0) {
        return myDemandElementParents.front()->getID();
    } else if (myAdditionalParents.size() > 0) {
        return myAdditionalParents.front()->getID();
    } else if (myLaneParents.size() > 0) {
        return myLaneParents.front()->getID();
    } else {
        throw ProcessError("Invalid parent");
    }
}


void
GNEStop::drawGL(const GUIVisualizationSettings& s) const {
    // only drawn in super mode demand
    if (myViewNet->getViewOptions().showDemandElements()) {
        // Obtain exaggeration of the draw
        const double exaggeration = s.addSize.getExaggeration(s, this);
        // Start drawing adding an gl identificator
        glPushName(getGlID());
        // Add a draw matrix
        glPushMatrix();
        // Start with the drawing of the area traslating matrix to origin
        glTranslated(0, 0, getType());
        // Set color of the base
        if (drawUsingSelectColor()) {
            GLHelper::setColor(s.selectedAdditionalColor);
        } else {
            GLHelper::setColor(s.SUMO_color_stops);
        }
        // Draw the area using shape, shapeRotations, shapeLengths and value of exaggeration
        GLHelper::drawBoxLines(myGeometry.shape, myGeometry.shapeRotations, myGeometry.shapeLengths, exaggeration*0.6);
        // pop draw matrix
        glPopMatrix();
        // Draw name if isn't being drawn for selecting
        drawName(getCenteringBoundary().getCenter(), s.scale, s.addName);
        // check if dotted contour has to be drawn
        if (!s.drawForSelecting && (myViewNet->getDottedAC() == this)) {
            GLHelper::drawShapeDottedContour(getType(), myGeometry.shape, exaggeration);
        }
        // Pop name
        glPopName();
    }
}


void
GNEStop::selectAttributeCarrier(bool changeFlag) {
    if (!myViewNet) {
        throw ProcessError("ViewNet cannot be nullptr");
    } else {
        gSelected.select(dynamic_cast<GUIGlObject*>(this)->getGlID());
        // add object of list into selected objects
        myViewNet->getViewParent()->getSelectorFrame()->getLockGLObjectTypes()->addedLockedObject(GLO_VEHICLE);
        if (changeFlag) {
            mySelected = true;
        }
    }
}


void
GNEStop::unselectAttributeCarrier(bool changeFlag) {
    if (!myViewNet) {
        throw ProcessError("ViewNet cannot be nullptr");
    } else {
        gSelected.deselect(dynamic_cast<GUIGlObject*>(this)->getGlID());
        // remove object of list of selected objects
        myViewNet->getViewParent()->getSelectorFrame()->getLockGLObjectTypes()->removeLockedObject(GLO_VEHICLE);
        if (changeFlag) {
            mySelected = false;

        }
    }
}


std::string
GNEStop::getAttribute(SumoXMLAttr key) const {
    // declare string error
    std::string error;
    switch (key) {
        case SUMO_ATTR_ID:
            return getDemandElementID();
        case SUMO_ATTR_DURATION:
            return toString(duration);
        case SUMO_ATTR_UNTIL:
            return toString(until);
        case SUMO_ATTR_INDEX:
            return toString(index);
        case SUMO_ATTR_TRIGGERED:
            return toString(triggered);
        case SUMO_ATTR_CONTAINER_TRIGGERED:
            return toString(containerTriggered);
        case SUMO_ATTR_EXPECTED:
            return toString(awaitedPersons);
        case SUMO_ATTR_EXPECTED_CONTAINERS:
            return toString(awaitedContainers);
        case SUMO_ATTR_PARKING:
            return toString(parking);
        case SUMO_ATTR_ACTTYPE:
            return "";  // CHECK
        case SUMO_ATTR_TRIP_ID:
            return tripId;
        // specific of Stops over stoppingPlaces
        case SUMO_ATTR_BUS_STOP:
        case SUMO_ATTR_CONTAINER_STOP:
        case SUMO_ATTR_CHARGING_STATION:
        case SUMO_ATTR_PARKING_AREA:
            return myAdditionalParents.front()->getID();
        // specific of stops over lanes
        case SUMO_ATTR_LANE:
            return myLaneParents.front()->getID();
        case SUMO_ATTR_STARTPOS:
            if (parametersSet & STOP_START_SET) {
                return toString(startPos);
            } else {
                return "";
            }
        case SUMO_ATTR_ENDPOS:
            if (parametersSet & STOP_END_SET) {
                return toString(endPos);
            } else {
                return "";
            }
        case SUMO_ATTR_FRIENDLY_POS:
            return toString(myFriendlyPosition);
        //
        case GNE_ATTR_SELECTED:
            return toString(isAttributeCarrierSelected());
        case GNE_ATTR_GENERIC:
            return getGenericParametersStr();
        default:
            throw InvalidArgument(getTagStr() + " doesn't have an attribute of type '" + toString(key) + "'");
    }
}


void
GNEStop::setAttribute(SumoXMLAttr key, const std::string& value, GNEUndoList* undoList) {
    if (value == getAttribute(key)) {
        return; //avoid needless changes, later logic relies on the fact that attributes have changed
    }
    switch (key) {
        case SUMO_ATTR_ID:
        case SUMO_ATTR_DURATION:
        case SUMO_ATTR_UNTIL:
        case SUMO_ATTR_INDEX:
        case SUMO_ATTR_TRIGGERED:
        case SUMO_ATTR_CONTAINER_TRIGGERED:
        case SUMO_ATTR_EXPECTED:
        case SUMO_ATTR_EXPECTED_CONTAINERS:
        case SUMO_ATTR_PARKING:
        case SUMO_ATTR_ACTTYPE:
        case SUMO_ATTR_TRIP_ID:
        // specific of Stops over stoppingPlaces
        case SUMO_ATTR_BUS_STOP:
        case SUMO_ATTR_CONTAINER_STOP:
        case SUMO_ATTR_CHARGING_STATION:
        case SUMO_ATTR_PARKING_AREA:
        // specific of stops over lanes
        case SUMO_ATTR_LANE:
        case SUMO_ATTR_STARTPOS:
        case SUMO_ATTR_ENDPOS:
        case SUMO_ATTR_FRIENDLY_POS:
        //
        case GNE_ATTR_GENERIC:
        case GNE_ATTR_SELECTED:
            undoList->p_add(new GNEChange_Attribute(this, myViewNet->getNet(), key, value));
            break;
        default:
            throw InvalidArgument(getTagStr() + " doesn't have an attribute of type '" + toString(key) + "'");
    }
}


bool
GNEStop::isValid(SumoXMLAttr key, const std::string& value) {
    // declare string error
    std::string error;
    switch (key) {
        case SUMO_ATTR_ID:
            return isValidDemandElementID(value);
        case SUMO_ATTR_DURATION:
            return canParse<SUMOTime>(value);
        case SUMO_ATTR_UNTIL:
            return canParse<SUMOTime>(value);
        case SUMO_ATTR_INDEX:
            return canParse<int>(value);
        case SUMO_ATTR_TRIGGERED:
            return canParse<bool>(value);
        case SUMO_ATTR_CONTAINER_TRIGGERED:
            return canParse<bool>(value);
        case SUMO_ATTR_EXPECTED:
        case SUMO_ATTR_EXPECTED_CONTAINERS:
            if (value.empty()) {
                return true;
            } else {
                return canParse<std::vector<std::string> >(value);
            }
        case SUMO_ATTR_PARKING:
            return canParse<bool>(value);
        case SUMO_ATTR_ACTTYPE:
            return false;  // CHECK
        case SUMO_ATTR_TRIP_ID:
            return SUMOXMLDefinitions::isValidAttribute(value);
        // specific of Stops over stoppingPlaces
        case SUMO_ATTR_BUS_STOP:
            return (myViewNet->getNet()->retrieveAdditional(SUMO_TAG_BUS_STOP, value, false) != nullptr);
        case SUMO_ATTR_CONTAINER_STOP:
            return (myViewNet->getNet()->retrieveAdditional(SUMO_TAG_CONTAINER_STOP, value, false) != nullptr);
        case SUMO_ATTR_CHARGING_STATION:
            return (myViewNet->getNet()->retrieveAdditional(SUMO_TAG_CHARGING_STATION, value, false) != nullptr);
        case SUMO_ATTR_PARKING_AREA:
            return (myViewNet->getNet()->retrieveAdditional(SUMO_TAG_PARKING_AREA, value, false) != nullptr);
        // specific of stops over lanes
        case SUMO_ATTR_LANE:
            if (myViewNet->getNet()->retrieveLane(value, false) != nullptr) {
                return true;
            } else {
                return false;
            }
        case SUMO_ATTR_STARTPOS:
            if (value.empty()) {
                return true;
            } else if (canParse<double>(value)) {
                return GNEStoppingPlace::checkStoppinPlacePosition(value, toString(endPos), myLaneParents.front()->getParentEdge().getNBEdge()->getFinalLength(), myFriendlyPosition);
            } else {
                return false;
            }
        case SUMO_ATTR_ENDPOS:
            if (value.empty()) {
                return true;
            } else if (canParse<double>(value)) {
                return GNEStoppingPlace::checkStoppinPlacePosition(toString(startPos), value, myLaneParents.front()->getParentEdge().getNBEdge()->getFinalLength(), myFriendlyPosition);
            } else {
                return false;
            }
        case SUMO_ATTR_FRIENDLY_POS:
            return canParse<bool>(value);
        //
        case GNE_ATTR_SELECTED:
            return canParse<bool>(value);
        case GNE_ATTR_GENERIC:
            return isGenericParametersValid(value);
        default:
            throw InvalidArgument(getTagStr() + " doesn't have an attribute of type '" + toString(key) + "'");
    }
}


bool 
GNEStop::isDisjointAttributeSet(const SumoXMLAttr attr) const {
    switch (attr) {
        case SUMO_ATTR_END:
            return (parametersSet & VEHPARS_END_SET) != 0;
        case SUMO_ATTR_NUMBER:
            return (parametersSet & VEHPARS_NUMBER_SET) != 0;
        case SUMO_ATTR_VEHSPERHOUR:
            return (parametersSet & VEHPARS_VPH_SET) != 0;
        case SUMO_ATTR_PERIOD:
            return (parametersSet & VEHPARS_PERIOD_SET) != 0;
        case SUMO_ATTR_PROB:
            return (parametersSet & VEHPARS_PROB_SET) != 0;
        default:
            return true;
    };
}


void 
GNEStop::setDisjointAttribute(const SumoXMLAttr attr, GNEUndoList* undoList) {
    // obtain a copy of parameter sets
    int parametersSetCopy = parametersSet;
    // modify parametersSetCopy depending of attr
    switch (attr) {
        case SUMO_ATTR_END: {
            // give more priority to end
            parametersSetCopy = VEHPARS_END_SET | VEHPARS_NUMBER_SET;
            break;
        }
        case SUMO_ATTR_NUMBER:
            parametersSetCopy ^= VEHPARS_END_SET;
            parametersSetCopy |= VEHPARS_NUMBER_SET;
            break;
        case SUMO_ATTR_VEHSPERHOUR: {
            // give more priority to end
            if ((parametersSetCopy & VEHPARS_END_SET) && (parametersSetCopy & VEHPARS_NUMBER_SET)) {
                parametersSetCopy = VEHPARS_END_SET;
            } else if (parametersSetCopy & VEHPARS_END_SET) {
                parametersSetCopy = VEHPARS_END_SET;
            } else if (parametersSetCopy & VEHPARS_NUMBER_SET) {
                parametersSetCopy = VEHPARS_NUMBER_SET;
            }
            // set VehsPerHour
            parametersSetCopy |= VEHPARS_VPH_SET;
            break;
        }
        case SUMO_ATTR_PERIOD: {
            // give more priority to end
            if ((parametersSetCopy & VEHPARS_END_SET) && (parametersSetCopy & VEHPARS_NUMBER_SET)) {
                parametersSetCopy = VEHPARS_END_SET;
            } else if (parametersSetCopy & VEHPARS_END_SET) {
                parametersSetCopy = VEHPARS_END_SET;
            } else if (parametersSetCopy & VEHPARS_NUMBER_SET) {
                parametersSetCopy = VEHPARS_NUMBER_SET;
            }
            // set period
            parametersSetCopy |= VEHPARS_PERIOD_SET;
            break;
        }
        case SUMO_ATTR_PROB: {
            // give more priority to end
            if ((parametersSetCopy & VEHPARS_END_SET) && (parametersSetCopy & VEHPARS_NUMBER_SET)) {
                parametersSetCopy = VEHPARS_END_SET;
            } else if (parametersSetCopy & VEHPARS_END_SET) {
                parametersSetCopy = VEHPARS_END_SET;
            } else if (parametersSetCopy & VEHPARS_NUMBER_SET) {
                parametersSetCopy = VEHPARS_NUMBER_SET;
            }
            // set probability
            parametersSetCopy |= VEHPARS_PROB_SET;
            break;
        }
        default:
            break;
    }
    undoList->p_add(new GNEChange_Attribute(this, myViewNet->getNet(), parametersSet, parametersSetCopy));
}


std::string
GNEStop::getPopUpID() const {
    return getTagStr();
}


std::string
GNEStop::getHierarchyName() const {
    return getTagStr() + ": " + getAttribute(SUMO_ATTR_ID) ;
}


double
GNEStop::getStartGeometryPositionOverLane() const {
    if (parametersSet & STOP_START_SET) {
        double fixedPos = endPos;
        const double len = myLaneParents.front()->getParentEdge().getNBEdge()->getFinalLength();
        if (fixedPos < 0) {
            fixedPos += len;
        }
        return fixedPos * myLaneParents.front()->getLengthGeometryFactor();
    } else {
        return 0;
    }
}


double
GNEStop::getEndGeometryPositionOverLane() const {
    if (parametersSet & STOP_END_SET) {
        double fixedPos = startPos;
        const double len = myLaneParents.front()->getParentEdge().getNBEdge()->getFinalLength();
        if (fixedPos < 0) {
            fixedPos += len;
        }
        return fixedPos * myLaneParents.front()->getLengthGeometryFactor();
    } else {
        return 0;
    }
}

// ===========================================================================
// private
// ===========================================================================

void
GNEStop::setAttribute(SumoXMLAttr key, const std::string& value) {
    // declare string error
    std::string error;
    switch (key) {
        case SUMO_ATTR_ID:
            changeDemandElementID(value);
            break;
        case SUMO_ATTR_DURATION:
            duration = parse<SUMOTime>(value);
            break;
        case SUMO_ATTR_UNTIL:
            until = parse<SUMOTime>(value);
            break;
        case SUMO_ATTR_INDEX:
            index = parse<int>(value);
            break;
        case SUMO_ATTR_TRIGGERED:
            triggered = parse<bool>(value);
            break;
        case SUMO_ATTR_CONTAINER_TRIGGERED:
            containerTriggered = parse<bool>(value);
            break;
        case SUMO_ATTR_EXPECTED:
            awaitedPersons = parse<std::set<std::string> >(value);
            break;
        case SUMO_ATTR_EXPECTED_CONTAINERS:
            awaitedContainers = parse<std::set<std::string> >(value);
            break;
        case SUMO_ATTR_PARKING:
            parking = parse<bool>(value);
            break;
        case SUMO_ATTR_ACTTYPE:
            // CHECK
            break;
        case SUMO_ATTR_TRIP_ID:
            tripId = value;
            break;
        // specific of Stops over stoppingPlaces
        case SUMO_ATTR_BUS_STOP:
        case SUMO_ATTR_CONTAINER_STOP:
        case SUMO_ATTR_CHARGING_STATION:
        case SUMO_ATTR_PARKING_AREA:
            changeAdditionalParent(this, value, 0);
            break;
        // specific of Stops over lanes
        case SUMO_ATTR_LANE:
            changeLaneParents(this, value);
            break;
        case SUMO_ATTR_STARTPOS:
            if (value.empty()) {
                parametersSet &= ~STOP_START_SET;
            } else {
                startPos = parse<double>(value);
                parametersSet |= STOP_START_SET;
            }
            break;
        case SUMO_ATTR_ENDPOS:
            if (value.empty()) {
                parametersSet &= ~STOP_END_SET;
            } else {
                endPos = parse<double>(value);
                parametersSet |= STOP_END_SET;
            }
            break;
        case SUMO_ATTR_FRIENDLY_POS:
            myFriendlyPosition = parse<bool>(value);
            break;
        //
        case GNE_ATTR_SELECTED:
            if (parse<bool>(value)) {
                selectAttributeCarrier();
            } else {
                unselectAttributeCarrier();
            }
            break;
        case GNE_ATTR_GENERIC:
            setGenericParametersStr(value);
            break;
        default:
            throw InvalidArgument(getTagStr() + " doesn't have an attribute of type '" + toString(key) + "'");
    }
    // check if updated attribute requieres update geometry
    if (myTagProperty.hasAttribute(key) && myTagProperty.getAttributeProperties(key).requiereUpdateGeometry()) {
        updateGeometry(true);
    }
}

/****************************************************************************/
