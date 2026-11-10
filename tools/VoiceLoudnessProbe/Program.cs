using CUE4Parse.Compression;
using CUE4Parse.Encryption.Aes;
using CUE4Parse.FileProvider;
using CUE4Parse.MappingsProvider.Usmap;
using CUE4Parse.UE4.Objects.Core.Misc;
using CUE4Parse.UE4.Versions;
using CUE4Parse.UE4.Assets;
using CUE4Parse.UE4.Assets.Exports;
using CUE4Parse.UE4.Assets.Objects;
using CUE4Parse.UE4.Assets.Readers;
using CUE4Parse.UE4.Assets.Exports.Wwise;
using CUE4Parse.UE4.Wwise;
using Newtonsoft.Json;
using Newtonsoft.Json.Linq;

var prior = Environment.GetEnvironmentVariable("LEMALIN_REFERENCE_ROOT") ?? Path.Combine(Directory.GetCurrentDirectory(), "private/reference");
var output = Path.GetFullPath(args[0]);
Directory.CreateDirectory(output);
OodleHelper.Initialize(prior + "/tools/oodle/package/bin/oodle-data-shared.dll");
using var provider = new DefaultFileProvider(new DirectoryInfo(Environment.GetEnvironmentVariable("PBZ_PAKS") ?? throw new ArgumentException("Set PBZ_PAKS to the game Content/Paks directory")), SearchOption.TopDirectoryOnly, new VersionContainer(EGame.GAME_UE5_7));
provider.MappingsContainer = new FileUsmapTypeMappingsProvider(prior + "/tools/mappings/ProjectPBZ-5.7.4-301524-full.usmap");
provider.Initialize();
foreach (var key in JArray.Parse(File.ReadAllText(prior + "/work/resource-key-candidates.json")))
{
    try { provider.SubmitKey(new FGuid(), new FAesKey(key["key"].ToString())); } catch { }
    if (provider.Files.Count > 10000) break;
}
provider.PostMount();
OriginalVoiceLibrary.Output = output;
CUE4Parse.Globals.FatalObjectSerializationErrors = true;
ObjectTypeRegistry.RegisterClass("WwiseAssetLibrary", typeof(OriginalVoiceLibrary));
provider.LoadPackage("ProjectPBZ/Content/Wwise/VO_CN.uasset").GetExports().ToArray();
public class OriginalVoiceLibrary : UObject
{
    public static string Output;
    public override void Deserialize(FAssetArchive ar, long validPos)
    {
        base.Deserialize(ar, validPos);
        var preamble = ar.ReadBytes(18);
        if (!preamble.SequenceEqual(Convert.FromHexString("0201FFFFFFFF010000000000040000000100")))
            throw new InvalidDataException("Unsupported PBZ voice-library preamble");
        var files = ar.ReadArray(() => new FWwisePackagedFile(ar));
        long start = ar.Position;
        // Take a small run of ordinary payloads before the next streaming entry.
        for (int index = 2; index <= 12; index++)
        {
            ar.Position = start + index * 8;
            if (ar.Read<int>() != index) throw new InvalidDataException("Sample resource index mismatch");
            ar.Position -= 4;
            var bulk = new FByteBulkData(ar);
            var bytes = bulk.Data ?? throw new InvalidDataException("Sample payload missing");
            File.WriteAllBytes(Path.Combine(Output, $"sample-resource-{index}.wem"), bytes);
        }
        Console.WriteLine("Extracted 11 original Chinese-library sample payloads");
    }
}
