#pragma once

#include "../manager.hpp"

#include <sdbusplus/message.hpp>

#include <algorithm>
#include <map>
#include <tuple>
#include <vector>

namespace phosphor::fan::control::json::trigger::signal
{

using namespace sdbusplus::message;

struct Handlers
{

  public:
    /**
     * @brief Processes a properties changed signal and updates the property's
     * value in the manager's object cache
     *
     * @param[in] msg - The sdbusplus signal message
     * @param[in] obj - Object data associated with the signal
     * @param[in] mgr - Manager that stores the object cache
     */
    static bool propertiesChanged(message& msg, const SignalObject& obj,
                                  Manager& mgr)
    {
        std::string intf;
        msg.read(intf);
        if (intf != std::get<Intf>(obj))
        {
            // Interface name does not match object's interface
            return false;
        }

        std::map<std::string, PropertyVariantType> props;
        msg.read(props);
        auto itProp = props.find(std::get<Prop>(obj));
        if (itProp == props.cend())
        {
            // Object's property not in dictionary of properties changed
            return false;
        }

        mgr.setProperty(std::get<Path>(obj), std::get<Intf>(obj),
                        std::get<Prop>(obj), itProp->second);
        return true;
    }

    /**
     * @brief Processes an interfaces added signal and adds the interface
     * (including property & property value) to the manager's object cache
     *
     * @param[in] msg - The sdbusplus signal message
     * @param[in] obj - Object data associated with the signal
     * @param[in] mgr - Manager that stores the object cache
     */
    static bool interfacesAdded(message& msg, const SignalObject& obj,
                                Manager& mgr)
    {
        sdbusplus::message::object_path op;
        msg.read(op);
        if (static_cast<const std::string&>(op) != std::get<Path>(obj))
        {
            // Path name does not match object's path
            return false;
        }

        std::map<std::string, std::map<std::string, PropertyVariantType>>
            intfProps;
        msg.read(intfProps);
        auto itIntf = intfProps.find(std::get<Intf>(obj));
        if (itIntf == intfProps.cend())
        {
            // Object's interface not in dictionary of interfaces added
            return false;
        }

        auto itProp = itIntf->second.find(std::get<Prop>(obj));
        if (itProp == itIntf->second.cend())
        {
            // Object's property not in dictionary of properties of interface
            return false;
        }

        mgr.setProperty(std::get<Path>(obj), std::get<Intf>(obj),
                        std::get<Prop>(obj), itProp->second);
        return true;
    }
};

} // namespace phosphor::fan::control::json::trigger::signal
