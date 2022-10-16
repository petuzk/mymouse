# app-nrf design notes

## folder structure

The app-nrf uses separate include and src folders for an unknown reason.

On a top-level, the main application components are:
- platform
- services
- transport
- main.c

### platform

The platform folder contains the code which further abstracts HAL based on assumptions from application context.
It is responsible for initializing the hardware, managing HW interrupts (incl. ISR definitions) and providing public API.
Defining ISRs outside of platform directory is not allowed. Kernel services should not be used extensively in platform code.

Internally this folder is subdivided on logical components. They mostly match corresponding hardware subsystems,
but it's not always the case. Actually, the logical component should be responsible for all of its assets in HW
(for example, boot_cond/shutdown code is responsible for controlling the vdd_pwr pin, and not the gpio).
These logical components must be runtime-independent from each other (they can use
public definitions from each other's headers though). Thus, their init functions share the same priority.

Key points:
- no runtime interdependency
- component owns its assets in all HW subsystems
- no ISR definitions outside of platform

### services

The services folder houses isolated runtime logic. Each service may use platform code, kernel services and other services
as long as they do not form dependency cycle.
