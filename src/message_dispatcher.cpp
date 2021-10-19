
/*
 *
 */

#include "message_dispatcher.hpp"

namespace message_dispatcher
{

MessageDispatcher::MessageDispatcher()
{
}

MessageDispatcher::~MessageDispatcher()
{
}

Status MessageDispatcher::getLongDescription(std::string &longDescription)
{
    return Status::succ;
}

} // namespace message_dispatcher
