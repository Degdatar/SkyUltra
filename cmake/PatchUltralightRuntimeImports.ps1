param(
    [Parameter(Mandatory = $true)][string]$PluginPath,
    [Parameter(Mandatory = $true)][string]$RuntimeDirectory
)

$ErrorActionPreference = 'Stop'

Add-Type -TypeDefinition @'
using System;
using System.IO;
using System.Text;

public static class SkyUltraImportNamePatcher
{
    private static bool Contains(byte[] image, byte[] value)
    {
        int cursor = 0;
        while (cursor <= image.Length - value.Length)
        {
            int index = Array.IndexOf(image, value[0], cursor);
            if (index < 0 || index > image.Length - value.Length)
                return false;

            bool matches = true;
            for (int offset = 1; offset < value.Length; ++offset)
            {
                if (image[index + offset] != value[offset])
                {
                    matches = false;
                    break;
                }
            }

            if (matches)
                return true;
            cursor = index + 1;
        }
        return false;
    }

    public static int ReplaceNullTerminatedAscii(string path, string oldName, string newName)
    {
        byte[] image = File.ReadAllBytes(path);
        byte[] search = Encoding.ASCII.GetBytes(oldName + "\0");
        byte[] replacement = new byte[search.Length];
        byte[] newValue = Encoding.ASCII.GetBytes(newName + "\0");

        if (newValue.Length > replacement.Length)
            throw new InvalidDataException("Replacement DLL name is longer than its original name.");

        Buffer.BlockCopy(newValue, 0, replacement, 0, newValue.Length);
        int count = 0;
        int cursor = 0;

        while (cursor <= image.Length - search.Length)
        {
            int index = Array.IndexOf(image, search[0], cursor);
            if (index < 0 || index > image.Length - search.Length)
                break;

            bool matches = true;
            for (int offset = 1; offset < search.Length; ++offset)
            {
                if (image[index + offset] != search[offset])
                {
                    matches = false;
                    break;
                }
            }

            if (matches)
            {
                Buffer.BlockCopy(replacement, 0, image, index, replacement.Length);
                ++count;
                cursor = index + search.Length;
            }
            else
            {
                cursor = index + 1;
            }
        }

        if (count == 0 && Contains(image, newValue))
            return 0;

        if (count == 0)
            throw new InvalidDataException("Expected import name was not found in " + path + ": " + oldName);

        File.WriteAllBytes(path, image);
        return count;
    }
}
'@

$patches = @(
    @{ Path = $PluginPath; Old = 'Ultralight.dll'; New = 'SUL.dll' },
    @{ Path = $PluginPath; Old = 'UltralightCore.dll'; New = 'SULCore.dll' },
    @{ Path = (Join-Path $RuntimeDirectory 'SUL.dll'); Old = 'UltralightCore.dll'; New = 'SULCore.dll' },
    @{ Path = (Join-Path $RuntimeDirectory 'SUL.dll'); Old = 'WebCore.dll'; New = 'SULWeb.dll' },
    @{ Path = (Join-Path $RuntimeDirectory 'SULWeb.dll'); Old = 'UltralightCore.dll'; New = 'SULCore.dll' }
)

foreach ($patch in $patches) {
    if (-not (Test-Path -LiteralPath $patch.Path -PathType Leaf)) {
        throw "Private Ultralight runtime file is missing: $($patch.Path)"
    }
    [void][SkyUltraImportNamePatcher]::ReplaceNullTerminatedAscii(
        $patch.Path, $patch.Old, $patch.New)
}
