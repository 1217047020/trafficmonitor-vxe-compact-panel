#Requires -Version 7.2
[CmdletBinding()]param([string]$Output='')
$ErrorActionPreference='Stop'
$port=(Get-ItemProperty 'HKCU:\Software\BatteryMonitor\BatteryMonitor').RpcPort
if(!$port){$port=19211}
$socket=[Net.WebSockets.ClientWebSocket]::new()
$timeout=[Threading.CancellationTokenSource]::new(4000)
try {
    $socket.ConnectAsync([Uri]"ws://127.0.0.1:$port/",$timeout.Token).GetAwaiter().GetResult() | Out-Null
    $query=[Text.Encoding]::UTF8.GetBytes('{"jsonrpc":"2.0","method":"devices.list","params":{},"id":1}')
    $socket.SendAsync([ArraySegment[byte]]::new($query),[Net.WebSockets.WebSocketMessageType]::Text,$true,$timeout.Token).GetAwaiter().GetResult() | Out-Null
    $buffer=[byte[]]::new(4096)
    $data=[IO.MemoryStream]::new()
    try {
        do {
            $part=$socket.ReceiveAsync([ArraySegment[byte]]::new($buffer),$timeout.Token).GetAwaiter().GetResult()
            if($part.MessageType -ne [Net.WebSockets.WebSocketMessageType]::Text){throw 'Expected text response'}
            $data.Write($buffer,0,$part.Count)
            if($data.Length -gt 65536){throw 'Response exceeds limit'}
        } while(!$part.EndOfMessage)
        $response=[Text.Encoding]::UTF8.GetString($data.ToArray()) | ConvertFrom-Json
    } finally {$data.Dispose()}
    if($response.error){throw "RPC error $($response.error.code)"}
    $mouse=@($response.result.devices | Where-Object id -eq 'js:vxe/vxe-r1-nearlink:373B:10C9')
    $result=[ordered]@{Timestamp=(Get-Date -Format o);Endpoint="ws://127.0.0.1:$port/";UpdatedAt=$response.result.updatedAt;Mouse=@($mouse|Select-Object id,name,percentage,connected,stale,lastSeen)}
    $json=$result|ConvertTo-Json -Depth 5
    $json
    if($Output){$json|Set-Content -LiteralPath $Output -Encoding utf8}
} finally {$socket.Dispose();$timeout.Dispose()}
