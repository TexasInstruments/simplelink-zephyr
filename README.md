# Texas Instruments Support for Zephyr

<p align="center">
  <img src="doc/images/ti_logo.png" />
</p>

The Texas Instruments Zephyr GitHub repository is the starting point for Zephyr
development on supported Texas Instruments devices. TI's Zephyr solution is
based on the Zephyr project and utilizes the same familiar environment, tools,
and dependencies.

## What is Zephyr?

The Zephyr Project is a scalable real-time operating system (RTOS) supporting
multiple hardware architectures, optimized for resource constrained devices,
and built with security in mind.

The Zephyr OS is based on a small-footprint kernel designed for use on
resource-constrained systems: from simple embedded environmental sensors and
LED wearables to sophisticated smart watches and IoT wireless gateways.

This release of TI Zephyr is based on v3.6.0 and includes support for the following
Texas Instruments boards and devices. This release specifically adds support for
CC2340R5 and the LP_EM_CC2340R5 Launchpad.

#### Devices

- CC1352P
- CC1352R
- CC2652P
- CC2652R
- CC1352P7
- CC1352R7
- CC2652P7
- CC2652R7
- CC3220SF
- CC3235SF
- CC2340R5

#### Boards

- cc1352p1_launchxl
- cc1352p7_launchpad
- cc1352r1_launchxl
- cc26x2r1_launchxl
- cc3220sf_launchxl
- cc3235sf_launchxl
- lp_em_cc2340r5

## Getting Started

For getting started, please refer to the [Upstream Zephyr Readme](https://github.com/zephyrproject-rtos/zephyr/blob/main/README.rst)
for the Zephyr project and follow the same getting-started guide for setting up
the environment and building your first application.

> **_NOTE:_** When running `west init` in the getting-started guide it's
> important to instead run `west init -m https://github.com/TexasInstruments/simplelink-zephyr -mr v3.6.0-d0ae1a8b105-ti-8.20.00_ea zephyrproject`
> in order to use the TI Zephyr repository.

## Tools support

Currently the XDS110 debugger supplied with TI Launchpads is not natively
supported in the `west` Zephyr tool for all devices. In order to flash/debug the
CC2340R5 device with `west`, only [JLink](https://www.segger.com/downloads/jlink/)
is available. The recommended version to use is V7.94f which has been used for
validation. Note that it is also possible to build an application in Zephyr
targeting CC2340R5, and to use [Code Composer Studio](https://www.ti.com/tool/CCSTUDIO)
to both flash and debug the application using the XDS110 debugger.

## Versioning

TI will tag each release with the following format: {upstream-tag}-ti-M.mm.pp(\_optional-qualifier)

This tag can be broken down into 4 components:

- upstream-tag: This is the tag or commit of the [Zephyr](https://github.com/zephyrproject-rtos/zephyr)
  repo that the TI release is based on
- -ti-: Separator
- TI release version: This is TI's version on top of the upstream Zephyr
  version. The version scheme is explained below.
- Qualifier. The qualifier keyword is described below.

### TI Versioning Scheme

The TI version follows a version format, M.mm.pp, where:

- M is a 1 digit major number,
- mm is a 2 digit minor number,
- pp is a 2 digit patch number.

M.mm will follow TI's SimpleLink SDK version and is an indicator that the TI
added content is based on the SimpleLink SDK with matching M.mm.

### Qualifier

Tags that are appended with \_ea are for demo only and are beta quality, while
tags without the \_ea keyword should be treated as production worthy releases.

## Releases

All releases will be tagged using the version format above. Release notes are
provided in the form of GitHub's release notices. Read the release notes for
your selected version here:

https://github.com/TexasInstruments/simplelink-zephyr/releases/

#### Disclaimer

This release is provided as-is and should be considered Beta quality. This
product is meant for demonstration purposes only.

## Need help?

- For technical support with TI Zephyr, including bugs and feature requests -
  submit a ticket to [TI's Wireless Connectivity E2E forum](https://e2e.ti.com/support/wireless-connectivity/)
  > **_NOTE:_** Please do not use the Github issue tracker for this project.

Additionally, we welcome any feedback that you can give to improve the
documentation!
