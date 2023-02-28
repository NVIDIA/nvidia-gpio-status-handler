/*
 *
 */

#pragma once

#include <chrono>

#include <boost/lockfree/queue.hpp>
#include <boost/lockfree/spsc_queue.hpp>

struct PcCompare;

// using PcTimestampType = std::chrono::time_point<std::chrono::steady_clock>;

// using PcDataType = std::tuple<PcTimestampType, std::string, std::string,
//         std::string, std::string, PropertyVariant>;

struct PcDataType {
    //PcTimestampType timestamp;
    std::string sender;
    std::string path;
    std::string interface;
    std::string propertyName;
    PropertyVariant value;
};

//using PcQueueType = PrioQueue<::PcDataType, std::vector<::PcDataType>, ::PcCompare>;
//using PcQueueType = PrioQueue<PcDataType>;
using PcQueueType = boost::lockfree::spsc_queue<PcDataType>;

/*
struct PcCompare
{
    bool operator()(const PcDataType& l, const PcDataType& r)
    {
        return false;
    }
};
*/
