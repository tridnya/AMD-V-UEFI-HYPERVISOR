# AMD-V UEFI Hypervisor

A minimal Type-1 AMD-V hypervisor launched from UEFI, with an example Windows bridge for communicating with the VMM.

## Overview

This project is built around AMD-V/SVM and starts before Windows through UEFI. The bootloader side is intended to live in a [tianocore/edk2](https://github.com/tianocore/edk2) workspace, while this repository contains a small Windows kernel-mode example for calling into the VMM.

## Information

- Windows boots under the UEFI-launched VMM.
- Basic `vmmcall` communication works from a Windows kernel driver.
- Example hypercalls currently return:
  - version: `0x10000`
  - ping: `0xDEADBEEF`
- The example driver uses MASM/ML64 for the hypercall stub.
