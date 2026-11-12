param(
  [string[]]$SourcePath = @("apps", "samples", "modules/ui")
)

$repositoryRoot = Resolve-Path -LiteralPath (Join-Path $PSScriptRoot "../../../..")

function Find-AuditMatches {
  param([string]$Heading, [string]$Pattern, [string[]]$Globs)

  Write-Output $Heading
  $arguments = @('-n')
  foreach ($glob in $Globs) {
    $arguments += @('--glob', $glob)
  }
  $arguments += @('--glob', '!**/generated/**', '--glob', '!**/tests/**')
  & rg @arguments -- $Pattern @SourcePath
  if ($LASTEXITCODE -gt 1) {
    throw "ripgrep failed with exit code $LASTEXITCODE"
  }
}

Push-Location -LiteralPath $repositoryRoot
try {
  $null = Resolve-Path -LiteralPath $SourcePath -ErrorAction Stop
  $codeGlobs = @('*.c', '*.cpp', '*.h', '*.hpp')
  Find-AuditMatches 'Explicit multiline text in source:' '\\n' $codeGlobs
  Find-AuditMatches 'Multiline catalog candidates:' '\\n|:\s*[|>][+-]?\s*(?:#.*)?$' @('*.yaml', '*.yml')
  Find-AuditMatches 'LVGL multiline helpers and line spacing:' 'configure_multiline|text_line_space' $codeGlobs
} finally {
  Pop-Location
}
exit 0
