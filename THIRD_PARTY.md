# Third-party components

- include/limine.h is the official Limine Protocol C/C++ header from Limine-Bootloader/limine-protocol, revision 877c2d4c50e170575f8b8e7fbf7b8fa89b1dc157. It carries the upstream 0BSD license notice in the header.
- scripts/get-limine.sh downloads Limine binary release v12.11.1 from the official Limine-Bootloader/Limine GitHub release and checks SHA-256 953aba25c23abc6d6b4e85e20ec8b9f3a8605450a26997103fa3fddf00e658e6 before extraction. The release package contains its own license files under build/deps/limine-binary/.
- The Rust kernel library uses only core; there are no Cargo dependencies.

