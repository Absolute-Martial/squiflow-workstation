# ADR 0011: OS-reported network state

Status: Accepted

Use Qt 6.11.1 `QNetworkInformation::loadDefaultBackend()` and its native Windows `networklistmanager` plugin. Never probe the SquiFlow server merely to decide whether the machine is online. Unsupported, unknown, local-only, site-only, and captive-portal states are conservative and do not authorize online-only operations. Metered links permit correctness-critical work but defer optional bulk transfer. OS reachability never claims that the application server is healthy.
