/**
 * Copyright © 2019 IBM Corporation
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
 */
#include <string>
#include <filesystem>
#include <fstream>
#include <nlohmann/json.hpp>
#include <phosphor-logging/log.hpp>

#include "json_config.hpp"
#include "tach.hpp"
#include "gpio.hpp"
#include "anyof.hpp"
#include "fallback.hpp"

namespace phosphor
{
namespace fan
{
namespace presence
{

using json = nlohmann::json;
namespace fs = std::filesystem;
using namespace phosphor::logging;

policies JsonConfig::_policies;
const std::map<std::string, methodHandler> JsonConfig::_methods =
{
    {"tach", method::getTach},
    {"gpio", method::getGpio}
};
const std::map<std::string, rpolicyHandler> JsonConfig::_rpolicies =
{
    {"anyof", rpolicy::getAnyof},
    {"fallback", rpolicy::getFallback}
};

JsonConfig::JsonConfig(const std::string& jsonFile)
{
    fs::path confFile{jsonFile};
    std::ifstream file;
    json jsonConf;

    if (fs::exists(confFile))
    {
        file.open(confFile);
        try
        {
            jsonConf = json::parse(file);
        }
        catch (std::exception& e)
        {
            log<level::ERR>("Failed to parse JSON config file",
                            entry("JSON_FILE=%s", jsonFile.c_str()),
                            entry("JSON_ERROR=%s", e.what()));
            throw std::runtime_error("Failed to parse JSON config file");
        }
    }
    else
    {
        log<level::ERR>("Unable to open JSON config file",
                        entry("JSON_FILE=%s", jsonFile.c_str()));
        throw std::runtime_error("Unable to open JSON config file");
    }

    process(jsonConf);
}

const policies& JsonConfig::get()
{
    return _policies;
}

void JsonConfig::process(const json& jsonConf)
{
    // Set the expected number of fan entries
    // to be size of the list of fan json config entries
    // (Must be done to eliminate vector reallocation of fan references)
    _fans.reserve(jsonConf.size());
    for (auto& member : jsonConf)
    {
        if (!member.contains("name") || !member.contains("path") ||
            !member.contains("methods") || !member.contains("rpolicy"))
        {
            log<level::ERR>(
                "Missing required fan presence properties",
                entry("REQUIRED_PROPERTIES=%s",
                      "{name, path, methods, rpolicy}"));
            throw std::runtime_error(
                "Missing required fan presence properties");
        }

        // Loop thru the configured methods of presence detection
        std::vector<std::unique_ptr<PresenceSensor>> sensors;
        for (auto& method : member["methods"].items())
        {
            if (!method.value().contains("type"))
            {
                log<level::ERR>(
                    "Missing required fan presence method type",
                    entry("FAN_NAME=%s",
                        member["name"].get<std::string>().c_str()));
                throw std::runtime_error(
                    "Missing required fan presence method type");
            }
            // The method type of fan presence detection
            // (Must have a supported function within the method namespace)
            auto type = method.value()["type"].get<std::string>();
            std::transform(type.begin(), type.end(), type.begin(), tolower);
            auto func = _methods.find(type);
            if (func != _methods.end())
            {
                // Call function for method type
                auto sensor = func->second(_fans.size(), method.value());
                if (sensor)
                {
                    sensors.emplace_back(std::move(sensor));
                }
            }
            else
            {
                log<level::ERR>("Invalid fan presence method type",
                                entry("FAN_NAME=%s",
                                    member["name"].get<std::string>().c_str()),
                                entry("METHOD_TYPE=%s", type.c_str()));
                throw std::runtime_error("Invalid fan presence method type");
            }
        }
        auto fan = std::make_tuple(member["name"], member["path"]);
        // Create a fan object
        _fans.emplace_back(std::make_tuple(fan, std::move(sensors)));

        // Add fan presence policy
        addPolicy(member["rpolicy"]);
    }
}

void JsonConfig::addPolicy(const json& rpolicy)
{
    if (!rpolicy.contains("type"))
    {
        log<level::ERR>("Missing required fan presence policy type",
                        entry("FAN_NAME=%s",
                            std::get<fanPolicyFanPos>(
                                std::get<Fan>(_fans.back())).c_str()),
                        entry("REQUIRED_PROPERTIES=%s", "{type}"));
        throw std::runtime_error("Missing required fan presence policy type");
    }

    // The redundancy policy type for fan presence detection
    // (Must have a supported function within the rpolicy namespace)
    auto type = rpolicy["type"].get<std::string>();
    std::transform(type.begin(), type.end(), type.begin(), tolower);
    auto func = _rpolicies.find(type);
    if (func != _rpolicies.end())
    {
        // Call function for redundancy policy type
        auto policy = func->second(_fans.back());
        if (policy)
        {
            _policies.emplace_back(std::move(policy));
        }
    }
    else
    {
        log<level::ERR>("Invalid fan presence policy type",
            entry("FAN_NAME=%s",
                std::get<fanPolicyFanPos>(std::get<Fan>(_fans.back())).c_str()),
            entry("RPOLICY_TYPE=%s", type.c_str()));
        throw std::runtime_error("Invalid fan presence methods policy type");
    }
}

/**
 * Methods of fan presence detection function definitions
 */
namespace method
{
    // Get a constructed presence sensor for fan presence detection by tach
    std::unique_ptr<PresenceSensor> getTach(size_t fanIndex, const json& method)
    {
        if (!method.contains("sensors") ||
            method["sensors"].size() == 0)
        {
            log<level::ERR>(
                "Missing required tach method properties",
                entry("FAN_ENTRY=%d", fanIndex),
                entry("REQUIRED_PROPERTIES=%s", "{sensors}"));
            throw std::runtime_error("Missing required tach method properties");
        }

        std::vector<std::string> sensors;
        for (auto& sensor : method["sensors"])
        {
            sensors.emplace_back(sensor.get<std::string>());
        }

        return std::make_unique<PolicyAccess<Tach, JsonConfig>>(
            fanIndex, std::move(sensors));
    }

    // Get a constructed presence sensor for fan presence detection by gpio
    std::unique_ptr<PresenceSensor> getGpio(size_t fanIndex, const json& method)
    {
        if (!method.contains("physpath") ||
            !method.contains("devpath") ||
            !method.contains("key"))
        {
            log<level::ERR>(
                "Missing required gpio method properties",
                entry("FAN_ENTRY=%d", fanIndex),
                entry("REQUIRED_PROPERTIES=%s", "{physpath, devpath, key}"));
            throw std::runtime_error("Missing required gpio method properties");
        }

        auto physpath = method["physpath"].get<std::string>();
        auto devpath = method["devpath"].get<std::string>();
        auto key = method["key"].get<unsigned int>();

        return std::make_unique<PolicyAccess<Gpio, JsonConfig>>(
            fanIndex, physpath, devpath, key);
    }

} // namespace method

/**
 * Redundancy policies for fan presence detection function definitions
 */
namespace rpolicy
{
    // Get an `Anyof` redundancy policy for the fan
    std::unique_ptr<RedundancyPolicy> getAnyof(const fanPolicy& fan)
    {
        std::vector<std::reference_wrapper<PresenceSensor>> pSensors;
        for (auto& fanSensor : std::get<fanPolicySensorListPos>(fan))
        {
            pSensors.emplace_back(*fanSensor);
        }

        return std::make_unique<AnyOf>(
            std::get<fanPolicyFanPos>(fan), pSensors);
    }

    // Get a `Fallback` redundancy policy for the fan
    std::unique_ptr<RedundancyPolicy> getFallback(const fanPolicy& fan)
    {
        std::vector<std::reference_wrapper<PresenceSensor>> pSensors;
        for (auto& fanSensor : std::get<fanPolicySensorListPos>(fan))
        {
            // Place in the order given to fallback correctly
            pSensors.emplace_back(*fanSensor);
        }

        return std::make_unique<Fallback>(
            std::get<fanPolicyFanPos>(fan), pSensors);
    }

} // namespace policy

} // namespace presence
} // namespace fan
} // namespace phosphor
