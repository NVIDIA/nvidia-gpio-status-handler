
/*
 *
 */

#include "event_handler.hpp"

namespace event_handler
{

EventHandler::EventHandler()
{
}

EventHandler::~EventHandler()
{
}

Status EventHandler::getLongDescription(std::string &longDescription)
{
    return Status::succ;
}

} // namespace event_handler
