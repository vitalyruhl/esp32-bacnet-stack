# Example values for the repository's BACnet/IP test environment. They are not general BACnet defaults.
$BacnetSettings = [ordered]@{
    # Native binaries. Leave empty to search the repository's CMake build output automatically.
    ExeDir = ""

    # Local BACnet/IP interface used by the native examples.
    BindAddress      = "192.168.2.220"
    BroadcastAddress = "192.168.2.255"

    # Default BACnet device used by the examples.
    Target   = "192.168.2.101:47808"
    DeviceId = 1682101

    # Default object instances used by the read, SubscribeCOV, and write examples.
    AvInstance  = 200
    BvInstance  = 320
    MsvInstance = 2020

    # Communication timing for native requests and the COV subscription lifetime.
    TimeoutMs          = 5000
    CovLifetimeSeconds = 120

    # Write examples remain disabled unless explicitly enabled here or with -Execute.
    Priority            = 8
    EnableWriteExamples = $false
}
