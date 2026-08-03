# Phase 6.6 supplied toolchain inputs

The supplied archives were inspected, not copied into Git because they total more than 100 MiB and are upstream source distributions.

- qtbase.zip: `78704b09494f3a012528f36e93234875d920c5721faba871838af243d498e6fb`; contains QNetworkInformation and the Windows networklistmanager plugin.
- qttools.zip: `959ecd39fde7971db0b61f96178f11e42cb4158ea6c8d7ee98c50838596563f6`; source for deployment tools including the Qt tool suite.
- vcpkg-2026.07.29.tar.gz: `6b1a5b0170fda8e585b258fa416e4251197bba4633414d2ac00f02625c78194e`; package-manager source snapshot.
- mingw-w64-v14.0.0.zip: `2f1eb3c15cad0e64e0dc4348cf560165e2d6918b7ef68f22b9202838765ebfea`; MinGW-w64 source, not a ready cross-compiler binary.

The Qt adapter is wired for a real Qt build. A Windows runtime gate still needs built Qt libraries and an actual MinGW/MSVC compiler toolchain.
