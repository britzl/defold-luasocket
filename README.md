# defold-luasocket
The Defold engine already includes the socket module from [LuaSocket](http://w3.impa.br/~diego/software/luasocket/) and the accompanying socket.lua module (located in builtins/scripts/). The builtins folder of a Defold project does however not contain any of the other .lua files from the LuaSocket project and the Defold engine doesn't provide the mime.core files either. This project aims to provide the missing LuaSocket .lua files, modified to play nicely with Defold (the require calls for Lua base libraries have been changed to work with Defold). This project also provides the mime.core part of LuaSocket as a native extension. The version of LuaSocket used in Defold is [3.0-rc1](https://github.com/diegonehab/luasocket/releases).

# Installation
You can use the extension in your own project by adding this project as a [Defold library dependency](http://www.defold.com/manuals/libraries/). Open your game.project file and in the dependencies field under project add:

https://www.github.com/britzl/defold-luasocket/archive/master.zip

Or point to the ZIP file of a [specific release](https://github.com/britzl/defold-luasocket/releases).

# Usage
See the [documentation for LuaSocket](http://w3.impa.br/~diego/software/luasocket/) for an in-depth API specification and example use cases. Also refer to the example folder inside this project.
