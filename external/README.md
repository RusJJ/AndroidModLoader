### How do i start an AML core? (AML 1.3 and earlier)
Open classes.dex of an application, head to the class which loads the library you want to modify (System.loadLibrary).
Modify smali code by adding this after the library you want to mod (to ensure its okay):
```
const-string v0, "AML"
invoke-static {v0}, Ljava/lang/System;->loadLibrary(Ljava/lang/String;)V
```
Please make sure register **v0** is not in use or use another one.

### How do i start an AML core? (AML 1.4 and later)
Starting with AML v1.4 there is an additional way to load the library before the application library loads (kinda).
If you prefer the old way, just load the game but also call an additional native Java function:
```
const-string v0, "AML"
invoke-static {v0}, Ljava/lang/System;->loadLibrary(Ljava/lang/String;)V
invoke-static {}, Lnet/rusjj/amlcore;->launchAMLCore()V
```
Make sure you've already added a Java-class (amlcore.smali) to your project.

A variant of loading AML early looks like that:
```
const-string v0, "AML"
invoke-static {v0}, Ljava/lang/System;->loadLibrary(Ljava/lang/String;)V
const-string v0, "libENGINE.so"
const-string v1, "libGAME.so"
invoke-static {v0, v1}, Lnet/rusjj/amlcore;->earlyLaunchAMLCore(Ljava/lang/String;Ljava/lang/String;)V
```
That way you can replace an original System.loadLibrary call because this function will load the application library.
Currently it only is loading BEFORE JNI_OnLoad. You cannot work with init arrays.