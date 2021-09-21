/*
 * Copyright (C) 2021 Open Source Robotics Foundation
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
*/

#include <rmf_traffic_ros2/agv/Graph.hpp>

namespace rmf_traffic_ros2 {

//==============================================================================
rmf_traffic::agv::Graph convert(const rmf_building_map_msgs::msg::Graph& from,
  int waypoint_offset)
{
  rmf_traffic::agv::Graph graph;
  // Iterate over vertices / waypoints
  // Graph params are not used for now
  for (const auto& vertex : from.vertices)
  {
    const Eigen::Vector2d location{
      vertex.x, vertex.y};
    auto& wp = graph.add_waypoint(from.name, location);
    // Add waypoint name if in the message
    if (vertex.name.size() > 0 && !graph.add_key(vertex.name, wp.index()))
    {
      throw std::runtime_error(
              "Duplicated waypoint name [" + vertex.name + "]");
    }
    for (const auto& param : vertex.params)
    {
      if (param.name == "is_parking_spot")
        wp.set_parking_spot(param.value_bool);
      else if (param.name == "is_holding_point")
        wp.set_holding_point(param.value_bool);
      else if (param.name == "is_passthrough_point")
        wp.set_passthrough_point(param.value_bool);
      else if (param.name == "is_charger")
        wp.set_charger(param.value_bool);
    }
  }
  // Iterate over edges / lanes
  for (const auto& edge : from.edges)
  {
    using Lane = rmf_traffic::agv::Graph::Lane;
    using Event = Lane::Event;
    // TODO(luca) Add lifts, doors, orientation constraints
    rmf_utils::clone_ptr<Event> entry_event;
    rmf_utils::clone_ptr<Event> exit_event;
    // Waypoint offset is applied to ensure unique IDs when multiple levels
    // are present
    const std::size_t start_wp = edge.v1_idx + waypoint_offset;
    const std::size_t end_wp = edge.v2_idx + waypoint_offset;
    std::string dock_name;
    std::optional<double> speed_limit;
    for (const auto& param : edge.params)
    {
      if (param.name == "dock_name")
        dock_name = param.value_string;
      if (param.name == "speed_limit")
        speed_limit = param.value_float;
    }
    // dock_name is only applied to the lane going to the waypoint, not exiting
    if (edge.edge_type == edge.EDGE_TYPE_BIDIRECTIONAL)
    {
      auto& lane = graph.add_lane({end_wp, entry_event},
          {start_wp, exit_event});
      lane.properties().speed_limit(speed_limit);
    }

    const rmf_traffic::Duration duration = std::chrono::seconds(5);
    if (dock_name.size() > 0)
      entry_event = Event::make(Lane::Dock(dock_name, duration));
    auto& lane = graph.add_lane({start_wp, entry_event},
      {end_wp, exit_event});
    lane.properties().speed_limit(speed_limit);
  }
  return graph;
}

} // namespace rmf_traffic_ros2
