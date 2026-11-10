using CUE4Parse.Compression;
using CUE4Parse.Encryption.Aes;
using CUE4Parse.FileProvider;
using CUE4Parse.MappingsProvider.Usmap;
using CUE4Parse.UE4.Objects.Core.Misc;
using CUE4Parse.UE4.Versions;
using CUE4Parse.UE4.Assets.Exports.Texture;
using CUE4Parse_Conversion;
using CUE4Parse_Conversion.Options;
using Newtonsoft.Json.Linq;

var reference = Environment.GetEnvironmentVariable("LEMALIN_REFERENCE_ROOT") ?? Path.Combine(Directory.GetCurrentDirectory(), "private/reference");
var output = Path.Combine(Environment.GetEnvironmentVariable("LEMALIN_ROOT") ?? Directory.GetCurrentDirectory(), "work/native-ui/assets");
Directory.CreateDirectory(output);
OodleHelper.Initialize(Path.Combine(reference, "tools/oodle/package/bin/oodle-data-shared.dll"));
using var provider = new DefaultFileProvider(new DirectoryInfo(Environment.GetEnvironmentVariable("PBZ_PAKS") ?? throw new ArgumentException("Set PBZ_PAKS to the game Content/Paks directory")), SearchOption.TopDirectoryOnly, new VersionContainer(EGame.GAME_UE5_7));
provider.MappingsContainer = new FileUsmapTypeMappingsProvider(Path.Combine(reference, "tools/mappings/ProjectPBZ-5.7.4-301524-full.usmap"));
provider.Initialize();
foreach (var key in JArray.Parse(File.ReadAllText(Path.Combine(reference,"work/resource-key-candidates.json"))))
{
    provider.SubmitKey(new FGuid(), new FAesKey(key["key"].ToString()));
    if (provider.Files.Count > 10000) break;
}
provider.PostMount();
string[] textures = [
    "UI/Menu/StartMenu/T_SelectArrow_Left", "UI/Menu/StartMenu/T_SelectArrow_Right",
    "UI/Menu/StartMenu/T_UI_DifficultySelectButton_Selected",
    "UI/UINew/Hud/T_WhiteLine_Long", "UI/UINew/System/Character/Texture/T_UI_BG"
];
foreach (var path in textures)
{
    var texture = provider.LoadPackage("ProjectPBZ/Content/PBZContent/" + path + ".uasset").GetExports().OfType<UTexture2D>().Single();
    var result = await new ExportSession().Add(texture).RunAsync(output, new ExportOptions());
    Console.WriteLine(Path.GetFileName(path) + " " + System.Text.Json.JsonSerializer.Serialize(result));
}
foreach (var font in new[]{"ZH_ZiKuTangKuangShanKaiShuJF", "YingNum", "AaGuDianKeBenSong-2"})
{
    var bytes = provider.SaveAsset("ProjectPBZ/Content/PBZContent/UI/Fonts/" + font + ".ufont");
    var count = BitConverter.ToInt32(bytes, 0);
    if (count <= 0 || count + 4 > bytes.Length) throw new InvalidDataException("Unexpected font stream header");
    File.WriteAllBytes(Path.Combine(output, font + ".ttf"), bytes[4..(4 + count)]);
    Console.WriteLine(font + " font bytes " + bytes.Length);
}
