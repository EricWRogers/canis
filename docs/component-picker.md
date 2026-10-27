# Adding components and scripts

The entity Inspector has one **Add Component** button. Its searchable popup lists
native components, registered C++ scripts, and compiled C# script types together.
C# entries have a `(C#)` label to distinguish names shared with native types.
Search by name and press Enter, double-click a row, or select it and press Add.
Escape or Cancel closes the popup.

Only types not already attached to the selected entity are offered. Saved C# aliases
count as the same type, so renaming a script does not allow duplicate attachment.
Adding a C# script uses its canonical identity and serialized field defaults. Native
entries retain their registered Add callbacks and required-component behavior.

C# assets can also be dropped on the shared Add Component button. All available
script types from that source file are attached; existing attachments and saved
values are preserved. There is no separate C# dropdown or drop-only button.

C# types come from the current compiled script manifest, just as C++ entries come
from their registered types. Projects without available C# types still use the same
picker for native components and scripts.
