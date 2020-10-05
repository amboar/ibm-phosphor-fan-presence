# phosphor-fan-presence
Phosphor fan provides a set of fan monitoring and control applications.

## To Build
By default, YAML configuration file(s) are used at build time for each fan
application. The location of the YAML configuration file(s) are provided at
configure time to each application.

To build this package using YAML, do the following steps:
```
    1. ./bootstrap.sh
    2. ./configure ${CONFIGURE_FLAGS}
    3. make
```

To enable the use of JSON configuration file(s) at runtime, provide the
`--enable-json` flag at configure time.
```
    1. ./bootstrap.sh
    2. ./configure ${CONFIGURE_FLAGS} --enable-json
    3. make
```
*Note: The following fan applications support the use of a JSON configuration
file.*
* Fan presence detection(presence)
  * The error logging feature is only available via the JSON configuration.
* Fan monitoring(monitor)
  * [JSON Documentation](monitor/json)
* Fan control(control) - limited functionality
    * Currently only supports setting fans to the configured `full_speed`  
      (If you require more than just setting fans to the configured
       `full_speed`, it is recommended to continue using YAML based
       configurations until the final work can be completed to enable
       full fan control JSON support.)

To clean the repository run `./bootstrap.sh clean`.
