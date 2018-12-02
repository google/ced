ced - A C++ code editor
-----------------------

This is not an official Google product.

A text editor exploring ideas around editing and optimizing C++ codebases.
It's still very much in development, and will most likely destroy your files.
Use at your own risk.

FUTURE PLANS
------------

* Support other than C++ editing (allow configuration of collaborator set based
  on file extension)
* Proper editing: ~~copy/paste~~, multicursor, etc...
* Update woot.h to allow multiple consecutive ID's to be stored on the same AVL
  leaf node - this should allow large file editing
* Implement collaborative editing
* ~~Implement TextMate theme & language support~~
* Optimize optimize optimize
* ~~Struct/class size annotations (where are the cache lines, how big is this
  class, etc)~~
* clang-tidy integration
* Multi-file editing/navigation
* GUI mode for Linux, Mac, Windows
* Macro support - perhaps via Lua
* Undo/redo
* Out-of-process collaborators (via gRPC)
* Template files for projects
* Project configuration discovery: source control, build system, etc... should
  be auto-discovered and used


BUILD REQUIREMENTS
------------------

- libncurses-dev

Linux only:
- opengl headers

