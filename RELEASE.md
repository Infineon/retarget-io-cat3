# Retarget IO (CAT3)

A utility library to retarget the standard input/output (STDIO) messages to a UART port on XMC™ 1000 and XMC™ 4000 devices. With this library, you can directly print messages on a UART terminal using `printf()`.

### What's Included?
* printf() support over a UART terminal
* Support for GCC, IAR, and ARM toolchains
* Thread safe write for NewLib

### What Changed?
#### v1.2.0
* Updated `_read` and `_write` function attributes to support GCC 14.
#### v1.1.0
* Add a new macro `CY_RETARGET_IO_NO_FLOAT`. When defined, floating point string formatting support will be disabled,
  allowing for flash savings in applications which do not need this functionality.
#### v1.0.0
* Initial release

### Supported Software and Tools
This version of the Retarget IO was validated for compatibility with the following Software and Tools:

| Software and Tools                        | Version |
| :---                                      | :----:  |
| ModusToolbox™ Software Environment        | 3.5.0   |
| GCC Compiler                              | 14.2.1  |
| IAR Compiler                              | 9.40.2  |
| ARM Compiler 6                            | 6.16    |

Minimum required ModusToolbox™ Software Environment: v3.5.0

### More information

* [API Reference Guide](https://infineon.github.io/retarget-io-cat3/html/index.html)
* [Infineon](http://www.infineon.com)
* [Infineon GitHub](https://github.com/infineon)
* [ModusToolbox Software Environment, Quick Start Guide, Documentation, and Videos](https://www.infineon.com/cms/en/design-support/tools/sdk/modustoolbox-software/)

---
© Cypress Semiconductor Corporation (an Infineon company) or an affiliate of Cypress Semiconductor Corporation, 2021-2025.
