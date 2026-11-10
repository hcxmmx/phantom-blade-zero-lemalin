// These packages support editor preview and are excluded by the existing
// PBZContent packaging guard. Runtime references resolve to the installed game.
static UFont* LeMalinNativeFont(const TCHAR* Name)
{
 FString Directory=TEXT("/Game/PBZContent/UI/Fonts/");
 FString FontName=FString(Name)+TEXT("_Font"),Path=Directory+FontName;
 if(auto* Existing=LoadObject<UFont>(nullptr,*Path))return Existing;
 TArray<uint8> Bytes;
 FString Source=FPaths::ConvertRelativePathToFull(FPaths::ProjectDir()/TEXT("../../work/native-ui/assets/")+Name+TEXT(".ttf"));
 if(!FFileHelper::LoadFileToArray(Bytes,*Source))return nullptr;
 auto* Face=NewObject<UFontFace>(CreatePackage(*(Directory+Name)),Name,RF_Public|RF_Standalone);
 Face->FontFaceData=FFontFaceData::MakeFontFaceData(MoveTemp(Bytes));Face->LoadingPolicy=EFontLoadingPolicy::Inline;
 Face->MarkPackageDirty();FAssetRegistryModule::AssetCreated(Face);
 auto* Font=NewObject<UFont>(CreatePackage(*Path),*FontName,RF_Public|RF_Standalone);
 Font->FontCacheType=EFontCacheType::Runtime;
 FTypefaceEntry Entry(TEXT("Default"));Entry.Font=FFontData(Face);
 Font->GetMutableInternalCompositeFont().DefaultTypeface.Fonts.Add(Entry);
 Font->MarkPackageDirty();FAssetRegistryModule::AssetCreated(Font);return Font;
}

static FSlateBrush LeMalinNativeBrush(UObject* Resource,FVector2D Size)
{
 FSlateBrush Brush;Brush.DrawAs=ESlateBrushDrawType::Image;Brush.ImageType=ESlateBrushImageType::FullColor;Brush.ImageSize=Size;
 Brush.SetResourceObject(Resource);Brush.TintColor=FLinearColor::White;return Brush;
}
