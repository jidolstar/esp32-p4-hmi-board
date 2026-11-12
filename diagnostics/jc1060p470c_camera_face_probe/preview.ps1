param(
    [Parameter(Mandatory)]
    [string]$Port
)

$ErrorActionPreference = 'Stop'
Add-Type -AssemblyName System.Windows.Forms, System.Drawing

$width = 160
$height = 90
$encodedSize = 4 * $width * $height / 3
$framePattern = '^PREVIEW (\d{1,2}) ([A-Za-z0-9+/]{' + $encodedSize + '})$'
$state = @{ Pending = ''; ReceivingPreview = $false }
$serial = [System.IO.Ports.SerialPort]::new($Port, 115200)
$timer = [System.Windows.Forms.Timer]::new()
$form = [System.Windows.Forms.Form]::new()
$picture = [System.Windows.Forms.PictureBox]::new()
$status = [System.Windows.Forms.Label]::new()

$form.Text = "C0.5 Camera Preview - $Port"
$form.StartPosition = 'CenterScreen'
$form.ClientSize = [System.Drawing.Size]::new(640, 400)
$picture.Dock = 'Fill'
$picture.SizeMode = 'Zoom'
$picture.BackColor = [System.Drawing.Color]::Black
$status.Dock = 'Bottom'
$status.Height = 40
$status.TextAlign = 'MiddleCenter'
$status.Text = 'Press RESET on the board to begin.'
$form.Controls.Add($picture)
$form.Controls.Add($status)

$timer.Interval = 50
$timer.Add_Tick({
    try {
        $state.Pending += $serial.ReadExisting()
        $lineEnd = $state.Pending.IndexOf("`n")
        while ($lineEnd -ge 0) {
            $line = $state.Pending.Substring(0, $lineEnd).TrimEnd("`r")
            $state.Pending = $state.Pending.Substring($lineEnd + 1)
            if ($line -cmatch $framePattern) {
                $seconds = $Matches[1]
                $pixels = [Convert]::FromBase64String($Matches[2])
                $image = [System.Drawing.Bitmap]::new($width, $height)
                try {
                    for ($y = 0; $y -lt $height; $y++) {
                        for ($x = 0; $x -lt $width; $x++) {
                            $gray = [int]$pixels[$y * $width + $x]
                            $image.SetPixel($x, $y, [System.Drawing.Color]::FromArgb($gray, $gray, $gray))
                        }
                    }
                    $previous = $picture.Image
                    $picture.Image = $image
                    $image = $null
                    if ($previous) { $previous.Dispose() }
                } finally {
                    if ($image) { $image.Dispose() }
                }
                $state.ReceivingPreview = $true
                $status.Text = "Center one face in the image. Detection starts in $seconds s."
            } elseif (!$line.StartsWith('PREVIEW ')) {
                [Console]::WriteLine($line)
                if ($line -match 'C0_5_CAMERA_FACE_(PASS|FAIL)') {
                    $status.Text = "Detection $($Matches[1]). Press RESET to try again."
                    $state.ReceivingPreview = $false
                } elseif ($state.ReceivingPreview -and $line.Contains('camera inference_us=')) {
                    $status.Text = 'Detecting face. See the PowerShell console for details.'
                }
            }
            $lineEnd = $state.Pending.IndexOf("`n")
        }
        if ($state.Pending.Length -gt ($encodedSize + 16)) { $state.Pending = '' }
    } catch {
        $status.Text = "Preview stopped: $($_.Exception.Message)"
        $timer.Stop()
    }
})

try {
    $serial.Open()
    $timer.Start()
    [void]$form.ShowDialog()
} finally {
    $timer.Dispose()
    $serial.Dispose()
    if ($picture.Image) { $picture.Image.Dispose() }
    $form.Dispose()
}
