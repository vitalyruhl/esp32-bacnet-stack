$port = 47808
$timeoutMs = 5000

# Optional: set your real LAN IP here, for example "192.168.2.220"
# Leave empty to bind to all interfaces.
$localIp = ""

# BACnet/IP broadcast address of your BACnet subnet
$targetBroadcast = "192.168.2.255"

# BACnet/IP Who-Is broadcast:
# BVLC Original-Broadcast-NPDU + NPDU Global Broadcast + APDU Who-Is
[byte[]]$whois = 0x81,0x0B,0x00,0x0C,0x01,0x20,0xFF,0xFF,0x00,0xFF,0x10,0x08

if ($localIp -ne "") {
    $localEndpoint = [System.Net.IPEndPoint]::new([System.Net.IPAddress]::Parse($localIp), $port)
    $udp = [System.Net.Sockets.UdpClient]::new($localEndpoint)
} else {
    $udp = [System.Net.Sockets.UdpClient]::new($port)
}

$udp.EnableBroadcast = $true
$udp.Client.ReceiveTimeout = 1000

$target = [System.Net.IPEndPoint]::new([System.Net.IPAddress]::Parse($targetBroadcast), $port)

Write-Host "Sending BACnet Who-Is broadcast to $targetBroadcast`:$port ..."
[void]$udp.Send($whois, $whois.Length, $target)

$start = Get-Date
$found = $false

while (((Get-Date) - $start).TotalMilliseconds -lt $timeoutMs) {
    try {
        $remote = [System.Net.IPEndPoint]::new([System.Net.IPAddress]::Any, 0)
        $bytes = $udp.Receive([ref]$remote)

        $hex = ($bytes | ForEach-Object { $_.ToString("X2") }) -join " "

        # Ignore our own Who-Is echo.
        if ($bytes.Length -eq $whois.Length) {
            $same = $true

            for ($i = 0; $i -lt $bytes.Length; $i++) {
                if ($bytes[$i] -ne $whois[$i]) {
                    $same = $false
                    break
                }
            }

            if ($same) {
                Write-Host "Ignoring own Who-Is echo from $($remote.Address):$($remote.Port)"
                continue
            }
        }

        Write-Host ""
        Write-Host "Response from $($remote.Address):$($remote.Port)"
        Write-Host "Bytes: $hex"

        if ($bytes.Length -ge 12 -and $bytes[0] -eq 0x81) {
            $byteString = $bytes -join ","

            if ($byteString -match "16,0") {
                Write-Host "Looks like a BACnet I-Am response."
                $found = $true
            } elseif ($byteString -match "16,8") {
                Write-Host "This is a Who-Is packet, not an I-Am response."
            } else {
                Write-Host "Received a BACnet/IP packet, but it is not clearly an I-Am response."
            }
        } else {
            Write-Host "Received a packet, but it does not look like BACnet/IP."
        }
    }
    catch [System.Net.Sockets.SocketException] {
        # Receive timeout for this iteration.
        # Continue until the total timeout has elapsed.
    }
}

$udp.Close()

Write-Host ""

if ($found) {
    Write-Host "Done. At least one BACnet I-Am response was received."
} else {
    Write-Host "Done. No real BACnet I-Am response was received."
}
