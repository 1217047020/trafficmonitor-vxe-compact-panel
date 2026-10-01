# Read-only UI snapshot of the running BatteryMonitor; no button interaction.
[CmdletBinding()]
param([string]$Output = '')
$ErrorActionPreference = 'Stop'
Add-Type -AssemblyName UIAutomationClient
Add-Type -AssemblyName UIAutomationTypes
$process = Get-Process BatteryMonitor -ErrorAction Stop | Select-Object -First 1
$condition = [Windows.Automation.PropertyCondition]::new(
    [Windows.Automation.AutomationElement]::ProcessIdProperty, $process.Id)
$windows = [Windows.Automation.AutomationElement]::RootElement.FindAll(
    [Windows.Automation.TreeScope]::Children, $condition)
$elements = foreach ($window in $windows) {
    foreach ($element in $window.FindAll([Windows.Automation.TreeScope]::Descendants,
                                        [Windows.Automation.Condition]::TrueCondition)) {
        $range = $null
        $hasRange = $element.TryGetCurrentPattern([Windows.Automation.RangeValuePattern]::Pattern,[ref]$range)
        [pscustomobject]@{ Name=$element.Current.Name; Type=$element.Current.ControlType.ProgrammaticName;
            Value=$(if($hasRange){$range.Current.Value}else{$null}) }
    }
}
$result = [pscustomobject]@{
    Timestamp=(Get-Date -Format o); Responding=$process.Responding;
    MouseRowPresent=(@($elements | Where-Object Name -eq 'VXE R1 星闪版').Count -eq 1);
    Elements=@($elements | Where-Object { $_.Name -or $null -ne $_.Value })
}
$json = $result | ConvertTo-Json -Depth 5
$json
if ($Output) { $json | Set-Content -LiteralPath $Output -Encoding utf8 }
