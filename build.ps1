$NDKPath = Get-Content $PSScriptRoot/NDKPath.txt
Write-Output "NDK located at: $NDKPath"

$buildScript = "$NDKPath/build/ndk-build"
if (-not ($PSVersionTable.PSEdition -eq "Core"))
{
    $buildScript += ".cmd"
}

Write-Output "[BUILD] Starting NDK..."
Write-Output "[BUILD] ARMPatch:"
& $buildScript NDK_PROJECT_PATH=$PSScriptRoot APP_BUILD_SCRIPT=$PSScriptRoot/ARMPatch/armpatch_src/Android.mk NDK_APPLICATION_MK=$PSScriptRoot/ARMPatch/armpatch_src/Application.mk NDK_DEBUG=0 -j12
Write-Output "[BUILD] AML:"
& $buildScript NDK_PROJECT_PATH=$PSScriptRoot APP_BUILD_SCRIPT=$PSScriptRoot/Android.mk NDK_APPLICATION_MK=$PSScriptRoot/Application.mk NDK_DEBUG=0 -j12
Write-Output "[BUILD] Done!"

Exit $LASTEXITCODE