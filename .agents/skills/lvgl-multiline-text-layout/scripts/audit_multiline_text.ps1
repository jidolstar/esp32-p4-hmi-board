param(
  [string]$SourcePath = "src/ui"
)

$resolvedSource = Resolve-Path -LiteralPath $SourcePath -ErrorAction Stop

Write-Output "Explicit multiline text candidates:"
rg -n '\\n' $resolvedSource --glob '*.cpp' --glob '*.hpp'

Write-Output "Existing LVGL line-spacing rules:"
rg -n 'text_line_space' $resolvedSource --glob '*.cpp' --glob '*.hpp'
