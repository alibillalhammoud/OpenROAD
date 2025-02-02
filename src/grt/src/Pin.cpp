/////////////////////////////////////////////////////////////////////////////
//
// BSD 3-Clause License
//
// Copyright (c) 2019, The Regents of the University of California
// All rights reserved.
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are met:
//
// * Redistributions of source code must retain the above copyright notice, this
//   list of conditions and the following disclaimer.
//
// * Redistributions in binary form must reproduce the above copyright notice,
//   this list of conditions and the following disclaimer in the documentation
//   and/or other materials provided with the distribution.
//
// * Neither the name of the copyright holder nor the names of its
//   contributors may be used to endorse or promote products derived from
//   this software without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
// AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
// IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
// ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE
// LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
// CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
// SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
// INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
// CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
// ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
// POSSIBILITY OF SUCH DAMAGE.
//
///////////////////////////////////////////////////////////////////////////////

#include "Pin.h"
#include "grt/GlobalRouter.h"

namespace grt {

Pin::Pin(
    odb::dbITerm* iterm,
    const odb::Point& position,
    const std::vector<odb::dbTechLayer*>& layers,
    const std::map<odb::dbTechLayer*, std::vector<odb::Rect>>& boxes_per_layer,
    bool connected_to_pad_or_macro)
    : iterm_(iterm),
      position_(position),
      edge_(PinEdge::none),
      is_port_(false),
      connected_to_pad_or_macro_(connected_to_pad_or_macro)
{
  for (auto layer : layers) {
    layers_.push_back(layer->getRoutingLevel());
  }
  std::sort(layers_.begin(), layers_.end());

  for (auto& [layer, boxes] : boxes_per_layer) {
    boxes_per_layer_[layer->getRoutingLevel()] = boxes;
  }

  if (connected_to_pad_or_macro) {
    determineEdge(iterm->getInst()->getBBox()->getBox(), position_, layers);
  } else {
    connection_layer_ = layers_.back();
  }
}

Pin::Pin(
    odb::dbBTerm* bterm,
    const odb::Point& position,
    const std::vector<odb::dbTechLayer*>& layers,
    const std::map<odb::dbTechLayer*, std::vector<odb::Rect>>& boxes_per_layer,
    const odb::Point& die_center)
    : bterm_(bterm),
      position_(position),
      edge_(PinEdge::none),
      is_port_(true),
      connected_to_pad_or_macro_(false)
{
  for (auto layer : layers) {
    layers_.push_back(layer->getRoutingLevel());
  }
  std::sort(layers_.begin(), layers_.end());

  for (auto& [layer, boxes] : boxes_per_layer) {
    boxes_per_layer_[layer->getRoutingLevel()] = boxes;
  }

  determineEdge(bterm->getBlock()->getDieArea(), position_, layers);
}

void Pin::determineEdge(const odb::Rect& bounds,
                        const odb::Point& pin_position,
                        const std::vector<odb::dbTechLayer*>& layers)
{
  // Determine which edge is closest to the pin
  const int n_dist = bounds.yMax() - pin_position.y();
  const int s_dist = pin_position.y() - bounds.yMin();
  const int e_dist = bounds.xMax() - pin_position.x();
  const int w_dist = pin_position.x() - bounds.xMin();
  const int min_dist = std::min({n_dist, s_dist, w_dist, e_dist});
  if (n_dist == min_dist) {
    edge_ = PinEdge::north;
  } else if (s_dist == min_dist) {
    edge_ = PinEdge::south;
  } else if (e_dist == min_dist) {
    edge_ = PinEdge::east;
  } else {
    edge_ = PinEdge::west;
  }

  // Find the best connection layer as the top layer may not be in the
  // right direction (eg horizontal layer for a south pin).
  odb::dbTechLayerDir dir = (edge_ == PinEdge::north || edge_ == PinEdge::south)
                                ? odb::dbTechLayerDir::VERTICAL
                                : odb::dbTechLayerDir::HORIZONTAL;
  odb::dbTechLayer* best_layer = nullptr;
  for (auto layer : layers) {
    if (layer->getDirection() == dir) {
      if (!best_layer
          || layer->getRoutingLevel() > best_layer->getRoutingLevel()) {
        best_layer = layer;
      }
    }
  }
  if (best_layer) {
    connection_layer_ = best_layer->getRoutingLevel();
  } else {
    connection_layer_ = layers_.back();
  }
}

odb::dbITerm* Pin::getITerm() const
{
  if (is_port_)
    return nullptr;
  else
    return iterm_;
}

odb::dbBTerm* Pin::getBTerm() const
{
  if (is_port_)
    return bterm_;
  else
    return nullptr;
}

std::string Pin::getName() const
{
  if (is_port_)
    return bterm_->getName();
  else
    return getITermName(iterm_);
}

bool Pin::isDriver()
{
  if (is_port_) {
    return (bterm_->getIoType() == odb::dbIoType::INPUT);
  } else {
    odb::dbMTerm* mterm = iterm_->getMTerm();
    odb::dbIoType type = mterm->getIoType();
    return type == odb::dbIoType::OUTPUT || type == odb::dbIoType::INOUT;
  }
}

int Pin::getConnectionLayer() const
{
  return connection_layer_;
}

}  // namespace grt
