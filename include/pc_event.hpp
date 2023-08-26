/*
 *
 */

#pragma once

#include "data_accessor.hpp"

#include <boost/lockfree/spsc_queue.hpp>

#include <chrono>
#include <mutex>

struct PcCompare;

struct PcDataType
{
    data_accessor::DataAccessor accessor;
    std::vector<std::shared_ptr<event_info::EventNode>> eventPtrs;
};

class PcQueueType
{
  public:
    PcQueueType(size_t queueSize) : _queue(queueSize), _mutex{}
    {}

    bool push(PcDataType const& d)
    {
        std::scoped_lock lock(_mutex);
        return _queue.push(d);
    }

    bool pop(PcDataType& d)
    {
        std::scoped_lock lock(_mutex);
        return _queue.pop(d);
    }

    size_t write_available()
    {
        std::scoped_lock lock(_mutex); // TODO: is this necessary?
        return _queue.write_available();
    }

  private:
    boost::lockfree::spsc_queue<PcDataType> _queue;
    std::mutex _mutex;
};

/*
struct PcCompare
{
    bool operator()(const PcDataType& l, const PcDataType& r)
    {
        return false;
    }
};
*/
