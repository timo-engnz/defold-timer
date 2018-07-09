![](logo.png)
# Delay
This is a timer/delay module for the [Defold game engine](http://www.defold.com). Use it to either delay a function call once or call a function repeatedly with an interval.

## Installation
You can use the module in your own project by adding this project as a [Defold library dependency](http://www.defold.com/manuals/libraries/). Open your game.project file and in the dependencies field under project add:

https://github.com/britzl/defold-timer/archive/master.zip

## Usage
The module previously used the `timer` module name, but since Defold 1.2.131 there's a native timer that clashes with this extension. The extension has since been changed to use `delay` as a module name instead:

    local id = delay.seconds(1.5, function(self, id)
      print("I will be called once, unless I'm cancelled")
    end)

    local id = delay.repeating(2.5, function(self, id)
      print("I will be called repeatedly, until cancelled")
    end)

    delay.cancel(id)
    delay.cancel_all()
