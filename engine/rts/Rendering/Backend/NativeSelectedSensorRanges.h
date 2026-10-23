/* This file is part of the Spring engine (GPL v2 or later), see LICENSE.html */
#pragma once
#include <array>
#include <cmath>
#include <stdexcept>

namespace rendering {
// Presentation-only state read afresh from a currently selected unit. Never
// use UnitDef nominal radii or retain a previous frame's eligibility/radii.
struct SelectedSensorState {
 bool selected, alive, allied, fullView, inLos, beingBuilt, activated;
 float radarRadius, sonarRadius;
};
inline std::array<float,2> SelectedSensorRadii(const SelectedSensorState& s) {
 if (!s.selected || !s.alive || (!s.fullView && (!s.allied || !s.inLos)) || s.beingBuilt || !s.activated)
  return {};
 const auto positive=[](float r) { return std::isfinite(r) && r>0.f ? r : 0.f; };
 return {positive(s.radarRadius),positive(s.sonarRadius)};
}
// Selection-only presentation contract: preserve the configured sensor RGB,
// but use an independent 0.7 opacity. Legacy sensor alpha can suppress
// engine ranges for other presentation policies; it does not control
// this explicitly requested selection overlay. Never mutate cmdColors.
inline constexpr float SelectedSensorOpacity = 0.7f;
inline std::array<float,4> SelectedSensorColor(const float* legacyColor) {
 return {legacyColor[0], legacyColor[1], legacyColor[2], SelectedSensorOpacity};
}
// Match the original PiP full-map pixel convention. Its existing viewport
// then maps the visible world rectangle to the panel; scissor clips the panel.
inline std::array<float,2> SelectedSensorMiniMapPoint(const std::array<float,2>& p,
 float mapWidth,float mapHeight,float width,float height,unsigned rotation) {
 if (!(std::isfinite(mapWidth)&&std::isfinite(mapHeight)&&std::isfinite(width)&&std::isfinite(height)) ||
     mapWidth<=0 || mapHeight<=0 || width<=0 || height<=0 || rotation>3)
  throw std::invalid_argument("Invalid selected sensor minimap geometry");
 const float x=p[0]/mapWidth,z=p[1]/mapHeight;
 switch(rotation) {
  case 0:return {x*width,(1-z)*height};
  case 1:return {z*width,x*height};
  case 2:return {(1-x)*width,z*height};
  default:return {(1-z)*width,(1-x)*height};
 }
}
} // namespace rendering
