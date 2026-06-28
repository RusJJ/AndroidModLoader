### How do i start an AML core? (AML 1.3 and earlier)
Open classes.dex of an application, head to the class which loads the library you want to modify (System.loadLibrary).
Modify smali code by adding this after the library you want to mod (to ensure its okay):
```
const-string v0, "AML"
invoke-static {v0}, Ljava/lang/System;->loadLibrary(Ljava/lang/String;)V
```
Please make sure register **v0** is not in use or use another one.

### How do i start an AML core? (AML 1.4.1 and later)
Starting with AML v1.4.1 there is an additional way to load AML and its mods "slightly before" an actual game.
To be more correct: before the game's JNI_OnLoad.
If you prefer the old way, just load like before BUT ALSO call an additional native Java function with empty arg:
```
const-string v0, "AML"
invoke-static {v0}, Ljava/lang/System;->loadLibrary(Ljava/lang/String;)V
const-string v0, ""
invoke-static {v0}, Lnet/rusjj/amlcore;->launchAMLCore(Ljava/lang/String;)V
```
Make sure you've already injected a Java-class (path: net/rusjj/amlcore.smali) to your project.

A variant of loading AML with some game libraries looks exactly the same.
The only difference is argument for launchAmlCore(...). Both variants "lib name" and lib*.so are supported here.
```
const-string v0, "AML"
invoke-static {v0}, Ljava/lang/System;->loadLibrary(Ljava/lang/String;)V
const-string v0, "libEngine.so,AMLHelper"
invoke-static {v0, v1}, Lnet/rusjj/amlcore;->launchAMLCore(Ljava/lang/String;)V
```
That way you NEED to replace an original System.loadLibrary call because this function will load the application library.
Currently it only is loading BEFORE JNI_OnLoad. You cannot work with init arrays.
