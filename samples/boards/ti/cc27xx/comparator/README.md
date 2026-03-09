# Introduction

This example demonstrates the use of the comparator module present in CC27xx
SoC.

Hardware used: LP_EM_CC2745R10_Q1

# Hardware connections

Before executing the comparator sample code, the following connections have to be made.

|       Comparator pin      |   Control / probe pin     |
|---------------------------|---------------------------|
|   Booster Pack 29 (DIO19) |   Booster Pack 14 (DIO4)  |
|   Booster Pack 30 (DIO20) |   Booster Pack 15 (DIO20) |
|   Booster Pack 2  (DIO21) |   Booster Pack 18 (DIO7)  |
|   Booster Pack 40 (DIO0)  |   Booster Pack 9  (DIO27) |
|   Booster Pack 35 (DIO15) |   Booster Pack 10 (DIO28) |

# Tests

The example code offers three tests to be performed on the comparator.

## External pin input test

For this test to be executed, set `LPCOMP_INPUT_COMBINATION_TEST_ENABLE` to `y`
in `prj.conf`.

The following inputs are given to the comparator and the outputs are
observed

<ul>
<li> LPCOMP(+) = VSS, LPCOMP(-) = VSS </li>
<li> LPCOMP(+) = VDD, LPCOMP(-) = VSS </li>
<li> LPCOMP(+) = VSS, LPCOMP(-) = VDD </li>
<li> LPCOMP(+) = VDD, LPCOMP(-) = VDD </li>
</ul>

The results are validated only for LPCOMP(+) = VDD, LPCOMP(-) = VSS
and LPCOMP(+) = VSS and LPCOMP(-) = VDD.

## Comparator event tests (without interrupts)

For this test to be executed, set `LPCOMP_EVENT_TEST_ENABLE` to `y` and
`LPF3_ENABLE_COMPARATOR_INTERRUPT` to `n` in `prj.conf`.

A comparator trigger event is set first. A rising edge event is generated
followed by falling edge event using the control pins and the comparator event
output is examined by calling return value of `comparator_lpf3_trigger_is_pending`
API for both rising and falling edges. The following table summarized the test
result evaulation.

|  Comparator event trigger  |  Event on rising edge  | Event on falling edge  |
|:--------------------------:|:----------------------:|:----------------------:|
|           NONE             |           NO           |        NO              |
|          RISING            |          YES           |        NO              |
|          FALLING           |           NO           |        YES             |

## Comparator event tests (with interrupts)

For this test to be executed, set `LPCOMP_EVENT_TEST_ENABLE` to `y` and
`LPF3_ENABLE_COMPARATOR_INTERRUPT` to `y` in `prj.conf`.

A comparator trigger event is set first. A rising edge event is generated
followed by falling edge event using the control pins and the comparator event
output is examined by checking `comparator_event_count` variable for both
rising and falling edges. The following table summarized the test result evaulation.

|  Comparator event trigger  |  Event on rising edge  | Event on falling edge  |
|:--------------------------:|:----------------------:|:----------------------:|
|           NONE             |           NO           |        NO              |
|          RISING            |          YES           |        NO              |
|          FALLING           |           NO           |        YES             |