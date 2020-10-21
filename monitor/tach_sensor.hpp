#pragma once

#include <sdbusplus/bus.hpp>
#include <sdbusplus/server.hpp>
#include <sdeventplus/clock.hpp>
#include <sdeventplus/event.hpp>
#include <sdeventplus/utility/timer.hpp>

#include <chrono>

namespace phosphor
{
namespace fan
{
namespace monitor
{

class Fan;

constexpr auto FAN_SENSOR_PATH = "/xyz/openbmc_project/sensors/fan_tach/";

/**
 * The mode fan monitor will run in:
 *   - init - only do the initialization steps
 *   - monitor - run normal monitoring algorithm
 */
enum class Mode
{
    init,
    monitor
};

/**
 * The mode that the timer is running in:
 *   - func - Transition to functional state timer
 *   - nonfunc - Transition to nonfunctional state timer
 */
enum class TimerMode
{
    func,
    nonfunc
};

/**
 * The mode that the method is running in:
 *   - time - Use a percentage based deviation
 *   - count - Run up/down count fault detection
 */
enum MethodMode
{
    timebased = 0,
    count
};

/**
 * @class TachSensor
 *
 * This class represents the sensor that reads a tach value.
 * It may also support a Target, which is the property used to
 * set a speed.  Since it doesn't necessarily have a Target, it
 * won't for sure know if it is running too slow, so it leaves
 * that determination to other code.
 *
 * This class has a parent Fan object that knows about all
 * sensors for that fan.
 */
class TachSensor
{
  public:
    TachSensor() = delete;
    TachSensor(const TachSensor&) = delete;
    // TachSensor is not moveable since the this pointer is used as systemd
    // callback context.
    TachSensor(TachSensor&&) = delete;
    TachSensor& operator=(const TachSensor&) = delete;
    TachSensor& operator=(TachSensor&&) = delete;
    ~TachSensor() = default;

    /**
     * @brief Constructor
     *
     * @param[in] mode - mode of fan monitor
     * @param[in] bus - the dbus object
     * @param[in] fan - the parent fan object
     * @param[in] id - the id of the sensor
     * @param[in] hasTarget - if the sensor supports
     *                        setting the speed
     * @param[in] funcDelay - Delay to mark functional
     * @param[in] interface - the interface of the target
     * @param[in] factor - the factor of the sensor target
     * @param[in] offset - the offset of the sensor target
     * @param[in] method - the method of out of range
     * @param[in] threshold - the threshold of counter method
     * @param[in] timeout - Normal timeout value to use
     * @param[in] errorDelay - Delay in seconds before creating an error
     *                         or std::nullopt if no errors.
     *
     * @param[in] event - Event loop reference
     */
    TachSensor(Mode mode, sdbusplus::bus::bus& bus, Fan& fan,
               const std::string& id, bool hasTarget, size_t funcDelay,
               const std::string& interface, double factor, int64_t offset,
               size_t method, size_t threshold, size_t timeout,
               const std::optional<size_t>& errorDelay,
               const sdeventplus::Event& event);

    /**
     * @brief Returns the target speed value
     */
    uint64_t getTarget() const;

    /**
     * @brief Returns the input speed value
     */
    inline double getInput() const
    {
        return _tachInput;
    }

    /**
     * @brief Returns true if sensor has a target
     */
    inline bool hasTarget() const
    {
        return _hasTarget;
    }

    /**
     * @brief Returns the interface of the sensor target
     */
    inline std::string getInterface() const
    {
        return _interface;
    }

    /**
     * @brief Returns the factor of the sensor target
     */
    inline double getFactor() const
    {
        return _factor;
    }

    /**
     * @brief Returns the offset of the sensor target
     */
    inline int64_t getOffset() const
    {
        return _offset;
    }

    /**
     * @brief Returns the method of out of range
     */
    inline size_t getMethod() const
    {
        return _method;
    }

    /**
     * @brief Returns the threshold of count method
     */
    inline size_t getThreshold() const
    {
        return _threshold;
    }

    /**
     * Set the sensor faulted counter
     */
    void setCounter(bool count);

    /**
     * @brief Returns the sensor faulted count
     */
    inline size_t getCounter() const
    {
        return _counter;
    }

    /**
     * Returns true if the hardware behind this
     * sensor is considered working OK/functional.
     */
    inline bool functional() const
    {
        return _functional;
    }

    /**
     * Set the functional status and update inventory to match
     */
    void setFunctional(bool functional);

    /**
     * @brief Says if the timer is running or not
     *
     * @return bool - if timer is currently running
     */
    inline bool timerRunning()
    {
        return _timer.isEnabled();
    }

    /**
     * @brief Stops the timer when the given mode differs and starts
     * the associated timer for the mode given if not already running
     *
     * @param[in] mode - mode of timer to start
     */
    void startTimer(TimerMode mode);

    /**
     * @brief Stops the timer
     */
    inline void stopTimer()
    {
        _timer.setEnabled(false);
    }

    /**
     * @brief Return the given timer mode's delay time
     *
     * @param[in] mode - mode of timer to get delay time for
     */
    std::chrono::microseconds getDelay(TimerMode mode);

    /**
     * Returns the sensor name
     */
    inline const std::string& name() const
    {
        return _name;
    };

    /**
     * @brief Says if the error timer is running
     *
     * @return bool - If the timer is running
     */
    bool errorTimerRunning() const
    {
        if (_errorTimer && _errorTimer->isEnabled())
        {
            return true;
        }
        return false;
    }

  private:
    /**
     * @brief Returns the match string to use for matching
     *        on a properties changed signal.
     */
    std::string getMatchString(const std::string& interface);

    /**
     * @brief Reads the Target property and stores in _tachTarget.
     *        Also calls Fan::tachChanged().
     *
     * @param[in] msg - the dbus message
     */
    void handleTargetChange(sdbusplus::message::message& msg);

    /**
     * @brief Reads the Value property and stores in _tachInput.
     *        Also calls Fan::tachChanged().
     *
     * @param[in] msg - the dbus message
     */
    void handleTachChange(sdbusplus::message::message& msg);

    /**
     * @brief Updates the Functional property in the inventory
     *        for this tach sensor based on the value passed in.
     *
     * @param[in] functional - If the Functional property should
     *                         be set to true or false.
     */
    void updateInventory(bool functional);

    /**
     * @brief the dbus object
     */
    sdbusplus::bus::bus& _bus;

    /**
     * @brief Reference to the parent Fan object
     */
    Fan& _fan;

    /**
     * @brief The name of the sensor, including the full path
     *
     * For example /xyz/openbmc_project/sensors/fan_tach/fan0
     */
    const std::string _name;

    /**
     * @brief The inventory name of the sensor, including the full path
     */
    const std::string _invName;

    /**
     * @brief If functional (not too slow).  The parent
     *        fan object sets this.
     */
    bool _functional;

    /**
     * @brief If the sensor has a Target property (can set speed)
     */
    const bool _hasTarget;

    /**
     * @brief Amount of time to delay updating to functional
     */
    const size_t _funcDelay;

    /**
     * @brief The interface that the target implements
     */
    const std::string _interface;

    /**
     * @brief The factor of target to get fan rpm
     */
    const double _factor;

    /**
     * @brief The offset of target to get fan rpm
     */
    const int64_t _offset;

    /**
     * @brief The method of out of range
     */
    const size_t _method;

    /**
     * @brief The threshold for count method
     */
    const size_t _threshold;

    /**
     * @brief The counter for count method
     */
    size_t _counter = 0;

    /**
     * @brief The input speed, from the Value dbus property
     */
    double _tachInput = 0;

    /**
     * @brief The current target speed, from the Target dbus property
     *        (if applicable)
     */
    uint64_t _tachTarget = 0;

    /**
     * @brief The timeout value to use
     */
    const size_t _timeout;

    /**
     * @brief Mode that current timer is in
     */
    TimerMode _timerMode;

    /**
     * The timer object
     */
    sdeventplus::utility::Timer<sdeventplus::ClockId::Monotonic> _timer;

    /**
     * @brief The match object for the Value properties changed signal
     */
    std::unique_ptr<sdbusplus::server::match::match> tachSignal;

    /**
     * @brief The match object for the Target properties changed signal
     */
    std::unique_ptr<sdbusplus::server::match::match> targetSignal;

    /**
     * @brief The number of seconds to wait between a sensor being set
     *        to nonfunctional and creating an error for it.
     *
     * If std::nullopt, no errors will be created.
     */
    const std::optional<size_t> _errorDelay;

    /**
     * @brief The timer that uses _errorDelay.  When it expires an error
     *        will be created for a faulted fan sensor (rotor).
     *
     * If _errorDelay is std::nullopt, then this won't be created.
     */
    std::unique_ptr<
        sdeventplus::utility::Timer<sdeventplus::ClockId::Monotonic>>
        _errorTimer;
};

} // namespace monitor
} // namespace fan
} // namespace phosphor
