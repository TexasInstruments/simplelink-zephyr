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

This release in the downstream repository of TI Zephyr is based on a tagged
release from the Zephyr upstream. TI has built on top of that Zephyr version
to add support for specific SimpleLink Wireless MCUs. Please see the
[Release Notes](#releases) for information about which upstream Zephyr release
this TI release is based on. Other dependency info is included there as well.

This release only contains
support for the `CC2340R5` device. The `CC32xx` and `CC13xx_CC26xx` devices are
not supported in this release. However, the Zephyr community continues to
support them in the upstream repositories.

### Devices

Supported by this release:

- [CC2340R52](https://www.ti.com/product/CC2340R5)

Beta support:

Limited testing was also performed on the `CC2340R53`, a variant of the CC2340R5
with 64kB of SRAM. Please note that some examples that require more RAM should
be run on the CC2340R53. Please see the release notes for more details.

To use this device, change [`cc2340r5.dtsi`](dts/arm/ti/cc2340r5.dtsi)
as follows:

```dts
sram0: memory@20000000 {
  reg = <0x20000000 DT_SIZE_K(64)>;
};
```

Supported by the Zephyr community:

- CC1352R
- CC2652P
- CC2652R
- CC1352P7
- CC1352R7
- CC2652P7
- CC2652R7
- CC3220SF
- CC3235SF

### Boards

Supported by this release:

- [lp_em_cc2340r5](https://www.ti.com/tool/LP-EM-CC2340R5)

Supported by the Zephyr community:

- cc1352p1_launchxl
- cc1352p7_launchpad
- cc1352r1_launchxl
- cc26x2r1_launchxl
- cc3220sf_launchxl
- cc3235sf_launchxl

## Getting Started

For getting started, please refer to the
[Zephyr Getting Started Guide](https://docs.zephyrproject.org/3.7.0/develop/getting_started/index.html)
for the Zephyr project and follow the same getting-started guide for setting up
the environment and building your first application.

> **_NOTE:_** When running `west init` in the getting-started guide it's
> important to instead run `west init -m https://github.com/TexasInstruments/simplelink-zephyr --mr {tag-name} zephyrproject`
> in order to use the TI Zephyr repository.
> The `{tag-name}` changes with each release from TI.
>
> You can look up the latest tag from the following link:
> https://github.com/TexasInstruments/simplelink-zephyr/tags


## Tools support

Currently the XDS110 debugger supplied with TI Launchpads is not natively
supported in the `west` Zephyr tool for all devices. In order to flash/debug the
`CC2340R5` device with `west`, only [JLink](https://www.segger.com/downloads/jlink/)
is available. The recommended version to use is V7.94f which has been used for
validation. Note that it is also possible to build an application in Zephyr
targeting `CC2340R5`, and to use [Code Composer Studio](https://www.ti.com/tool/CCSTUDIO)
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

### Disclaimer

This release is provided as-is and should be considered Beta quality. This
product is meant for demonstration purposes only. Please refer to the Release Notes for
details on specific limitations and known issues.

## Need help?

- For technical support with TI Zephyr, including bugs and feature requests -
  submit a ticket to [TI's Wireless Connectivity E2E forum](https://e2e.ti.com/support/wireless-connectivity/)
  > **_NOTE:_** Please do not use the Github issue tracker for this project.

Additionally, we welcome any feedback that you can give to improve the
documentation!
