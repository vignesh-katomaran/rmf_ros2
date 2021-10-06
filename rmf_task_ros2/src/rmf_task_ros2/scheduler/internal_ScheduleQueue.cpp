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

#include "internal_ScheduleQueue.hpp"

using namespace rmf_task_ros2;

uint32_t SimpleSchedulerQueue::enqueue_request(const TaskRequest& request)
{
  uint32_t request_id = _increment_id;
  _increment_id++;
  _request_table[request_id] = request;
  _priority_queue.push({request._start_time, request_id});

  return request_id;
}


void SimpleSchedulerQueue::cancel_request(const uint32_t request)
{
  _to_be_deleted.insert(request);
}


std::unordered_map<uint32_t, TaskRequest>
  SimpleSchedulerQueue::get_queued() const
{
  // TODO(arjo): This is terribly inefficient. We should return some type
  // of custom view instead.
  std::unordered_map<uint32_t, TaskRequest> tasks;
  for (auto& [req_id, task_req] : _request_table)
  {
    if (_to_be_deleted.count(req_id) > 0)
    {
      continue;
    }
    tasks[req_id] = task_req;
  }
  return tasks;
}

std::vector<TaskRequest> 
  SimpleSchedulerQueue::check_and_deque(const rclcpp::Time time)
{
  std::vector<TaskRequest> to_be_submitted;
  while(
    !_priority_queue.empty() && 
    time > _priority_queue.top()._time)
  {
    if (_to_be_deleted.count(_priority_queue.top()._request_index) != 0)
    {
      auto idx = _priority_queue.top()._request_index;
      _priority_queue.pop();
      _request_table.erase(idx);
      _to_be_deleted.erase(idx);
      continue;
    }

    auto req_idx = _priority_queue.top()._request_index;
    auto req = _request_table[req_idx];
    auto time_of_execution = _priority_queue.top()._time;
    req._start_time = time_of_execution;
    to_be_submitted.push_back(req);
    _priority_queue.pop();

    if (req._repeat_interval.has_value())
    {
      auto next_time_of_exec =
        req._repeat_interval.value() + time_of_execution;

      if (req._end_time.has_value()
      && next_time_of_exec >= req._end_time.value())
      {
        _request_table.erase(req_idx);
        continue;
      }

      _priority_queue.push({next_time_of_exec, req_idx});
    }
  }

  return to_be_submitted;
}