# DSMotion

Henkaku plugins which adds motion control support for PlayStation TV with DualShock controllers

There are 2 plugins:
 * Kernel plugin **dsmotion.skprx**: it hooks BlueTooth calls to catch DualShock controller packets and exposes user services to get the intercepted motion data
 * User plugin **dsmotion.suprx**: it calls the kernel plugin services and reinterprets the given data for SceMotion functions return


### Installation

In order to activate those plugins featues, you must edit your `ux0:tai/config.txt` file:

```
*KERNEL
ux0:tai/dsmotion.skprx

*TITLEID00
ux0:tai/dsmotion.suprx
```

Replace **TITLEID00** by a title identifier which needs motion control or by **ALL** to affect all titles.


### Compatibility

 * NPXS10007 - Welcome Park - The skate board game is playable.
 * PCSF00214 - Tearaway - Introduction which asks to shake the PS Vita can now be passed by shaking the controller.
 * PCSF00349 - Flower - The game is playable.
 * PCSF00007 - WipEout 2048 - The game can be played with motion commands option.
 * PCSA00029 - Uncharted Golden Abyss - River game at chapter 12 and other balance mini-games can be completed.
 * PCSB00031 - Virtua Tennis 4 - "Match VR" mode has inverted horizontal controls.
 * PCSF00024 - Gravity Rush - "No gravity" mode has inverted horizontal controls.


### Limitations

 * If a DualShock 3 controller is used, it must not be directly plugged with USB on the PS TV otherwise, signal will be sent through USB instead of BlueTooth (and it won't be catched): use an external charger for the controller.
 * It doesn't work well on classic PS Vita with "ds4vita": for an unknown reason, motion control samples seems to be too much spaced over time.
 * It hooks documented "SceMotion" user functions instead of undocumented "SceMotionDev" kernel functions: if we could understand those kernel functions, we could have more compatibility with a single kernel plugin (no more need for a user plugin).
 * Device orientation quaternion and rotation matrix are not currently accurate: they give unstable result and they don't support full orientation, feel free to give help if you have some maths/IMU skills!
 * Some games (specially those with inverted horizontal controls) call functions to activate the PS Vita magnetometer: DualShock controllers don't have this feature.


### Credits

 * **xerpi** for his "ds3vita" and "ds4vita" plugins source code which helped me a lot to understand BlueTooth communication!
 * **TheFlow** for his "VitaShell" source code which helped me to understand how to export functions from kernel plugin and call them in user plugin
 * **YifanLu** for Henkaku which makes everything possible (except coffee) on this device!
