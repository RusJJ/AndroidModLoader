# What is that?!
AML is a mod loader for almost ANY android game or application!

AML has been started as a GTA-series mod loader AND as a Gorilla Tag mod loader. Since Gorilla Tag is a Unity-application, AML should be "upgraded" with some IL2CPP tools that are NOT ready now. That means you CAN mod for Unity-games but you NEED some external tools.

# How is this working?
AML provides you an interface system (like in Source Engine). You can add your own interfaces or get an AML Interface (does automatically on mod declaration) that can you help with: patching, memory writing, functions hooking & more (it uses ARMPatch that was made using of 4x11's ARMhook and CydiaSubstrate/Rprop's Inline hook).

AML should be loaded after all libraries somehow. For example: added through the smali code (easiest way?).

AML supports hard dependencies and can help you get info about mod (if it's loaded) just like a SOFT dependency. That's really cool.
