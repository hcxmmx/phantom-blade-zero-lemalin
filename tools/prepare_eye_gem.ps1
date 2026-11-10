param(
    [string]$GeneratedAtlas = (Join-Path (Split-Path -Parent $PSScriptRoot) 'work/eye-gem/generated-atlas.png')
)
$ErrorActionPreference = 'Stop'
$repo = Split-Path -Parent $PSScriptRoot
Add-Type -AssemblyName System.Drawing

# Pack the generated iris artwork into the original game's UV/alpha layout.
$references = @(
    [System.Drawing.Bitmap].Assembly.Location,
    [System.Drawing.Color].Assembly.Location,
    [System.ComponentModel.Component].Assembly.Location,
    (Join-Path $PSHOME 'ref/System.Runtime.dll')
)
$references += Get-ChildItem -LiteralPath $PSHOME -Filter 'System.Private.Windows.*.dll' | ForEach-Object FullName
Add-Type -ReferencedAssemblies $references -TypeDefinition @'
using System;
using System.Drawing;
using System.Drawing.Drawing2D;
using System.Drawing.Imaging;

public static class LeMalinEyeAtlas
{
    public static void Pack(string originalPath, string artworkPath, string outputPath)
    {
        using (var original = new Bitmap(originalPath))
        using (var artwork = new Bitmap(artworkPath))
        using (var scaled = new Bitmap(original.Width, original.Height, PixelFormat.Format32bppArgb))
        using (var output = new Bitmap(original.Width, original.Height, PixelFormat.Format32bppArgb))
        {
            using (var graphics = Graphics.FromImage(scaled))
            {
                graphics.CompositingMode = CompositingMode.SourceCopy;
                graphics.InterpolationMode = InterpolationMode.HighQualityBicubic;
                graphics.DrawImage(artwork, new Rectangle(0, 0, scaled.Width, scaled.Height));
            }
            for (int y = 0; y < original.Height; ++y)
            {
                for (int x = 0; x < original.Width; ++x)
                {
                    Color source = original.GetPixel(x, y);
                    Color result = source;
                    // Eye uses the upper-right island. EyeLight uses the lower utility area.
                    if (x >= original.Width / 2 && y < original.Height * 0.64 && source.A > 0)
                    {
                        Color paint = scaled.GetPixel(x, y);
                        double coverage = paint.A / 255.0;
                        result = Color.FromArgb(source.A,
                            (int)Math.Round(paint.R * coverage + source.R * (1 - coverage)),
                            (int)Math.Round(paint.G * coverage + source.G * (1 - coverage)),
                            (int)Math.Round(paint.B * coverage + source.B * (1 - coverage)));
                    }
                    output.SetPixel(x, y, result);
                }
            }
            output.Save(outputPath, ImageFormat.Png);
        }
    }
}
'@

$output = Join-Path $repo 'work/textures/Eye_Gem_Albedo.png'
[LeMalinEyeAtlas]::Pack((Join-Path $repo 'source/tex/TEX_LML_EyeA.png'), $GeneratedAtlas, $output)
Write-Output $output
