

$PackageProps = Get-Content "$PSScriptRoot\..\..\src\SpectatorView.Unity\Assets\package.json" | ConvertFrom-Json
$PackageName = $PackageProps.name
$PackageVersion = $PackageProps.version
$PackagePath = "$PSScriptRoot\..\..\packages\$PackageName.$PackageVersion"

Write-Host "`nCreating package $PackageName.$PackageVersion $PackagePath"
if (Test-Path -Path "$PackagePath")
{
    Write-Host "Removing preexisting directory: $PackagePath"
    Remove-Item -Path "$PackagePath" -Recurse
}

New-Item -Path "$PackagePath" -ItemType "directory"
Copy-Item -Path "$PSScriptRoot\..\..\src\SpectatorView.Unity\Assets\*" -Destination $PackagePath -Recurse

Write-Host "Created package: packages\$PackageName.$PackageVersion"