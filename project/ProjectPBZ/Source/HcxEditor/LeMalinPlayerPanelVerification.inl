static FString VerifyLeMalinPlayerPanel(UUserWidget* Widget,USkeletalMeshComponent* Source,USkeletalMeshComponent* Target,UAnimInstance* Instance,const FString& OutputFile)
{
 auto* Class=Widget->GetClass();
 auto* SlotProperty=FindFProperty<FStrProperty>(Class,TEXT("SettingsSlot"));
 if(!SlotProperty)return TEXT("player settings slot missing");
 bool Schema=SlotProperty->GetPropertyValue_InContainer(Widget)==TEXT("Hcxmmx_LeMalin_Player_v1");
 const FString TestSlot=TEXT("LeMalinPlayerPanelTest_")+FGuid::NewGuid().ToString(EGuidFormats::Digits);
 SlotProperty->SetPropertyValue_InContainer(Widget,TestSlot);
 Widget->ProcessEvent(Widget->FindFunctionChecked(TEXT("Construct")),nullptr);
 auto Read=[&](UObject* Object,const TCHAR* Name){
  auto* Property=FindFProperty<FDoubleProperty>(Object->GetClass(),Name);
  return Property?Property->GetPropertyValue_InContainer(Object):-999.;
 };
 auto Click=[&](const TCHAR* Name){
  auto* Button=Cast<UButton>(Widget->GetWidgetFromName(Name));if(!Button)return false;
  Button->OnClicked.Broadcast();return true;
 };
 auto Check=[&](const TCHAR* Name,bool Enabled){
  auto* Box=Cast<UCheckBox>(Widget->GetWidgetFromName(Name));if(!Box)return false;
  Box->SetIsChecked(Enabled);Box->OnCheckStateChanged.Broadcast(Enabled);return true;
 };
 Source->VisibilityBasedAnimTickOption=Target->VisibilityBasedAnimTickOption=EVisibilityBasedAnimTickOption::AlwaysTickPoseAndRefreshBones;
 auto Tick=[&](){
  for(int32 Frame=0;Frame<3;++Frame){
   ++GFrameCounter;Source->GetSingleNodeInstance()->SetPosition(.5f,false);Source->TickAnimation(0,false);Source->RefreshBoneTransforms();
   Target->TickAnimation(1.f/60,false);Target->RefreshBoneTransforms();
  }
 };
 const TCHAR* Fields[]={TEXT("OutfitStyle"),TEXT("EarStyle"),TEXT("EyeFinish"),TEXT("BackVisible"),TEXT("BackDistance"),TEXT("ExpressionMode"),TEXT("GazeMode")};
 const double Defaults[]={0,0,3,0,25,4,1};bool DefaultValues=true;
 for(int32 I=0;I<7;++I)DefaultValues&=Read(Widget,Fields[I])==Defaults[I];
 for(const TCHAR* Name:{TEXT("ScaleSlider"),TEXT("InertiaSlider"),TEXT("DampingSlider"),TEXT("GravitySlider"),TEXT("CollisionSlider"),TEXT("ReflectanceLevelSlider"),TEXT("GazeModeSlider"),TEXT("GazeStrengthSlider"),TEXT("ExpressionModeSlider"),TEXT("ExpressionStrengthSlider"),TEXT("BackVisibleSlider"),TEXT("OutfitStyleSlider")})Schema&=Widget->GetWidgetFromName(Name)==nullptr;
 auto* SaveClass=LoadClass<USaveGame>(nullptr,TEXT("/Game/LeMalin/SG_LeMalinSettings.SG_LeMalinSettings_C"));
 int32 SaveFields=0;if(SaveClass)for(TFieldIterator<FProperty> It(SaveClass);It;++It)if(It->HasAnyPropertyFlags(CPF_SaveGame))++SaveFields;
 Schema&=SaveClass&&SaveFields==7;
 bool Controls=Click(TEXT("OutfitStyleNext"))&&Click(TEXT("EarStylePrevious"))&&Click(TEXT("EyeFinishNext"));
 Controls&=Check(TEXT("BackVisibleCheck"),true)&&Check(TEXT("ExpressionModeCheck"),false)&&Check(TEXT("GazeModeCheck"),false);
 auto* Distance=Cast<USlider>(Widget->GetWidgetFromName(TEXT("BackDistanceSlider")));
 if(Distance){Distance->SetValue(40);Distance->OnValueChanged.Broadcast(40);}else Controls=false;
 Tick();
 bool Live=Read(Instance,TEXT("LeMalinOutfitStyle"))==2&&Read(Instance,TEXT("LeMalinEarStyle"))==1&&Read(Instance,TEXT("LeMalinEyeFinish"))==1;
 Live&=Read(Instance,TEXT("LeMalinBackVisible"))==1&&Read(Instance,TEXT("LeMalinBackDistance"))==40&&Read(Instance,TEXT("LeMalinExpressionMode"))==0&&Read(Instance,TEXT("LeMalinGazeMode"))==0;
 auto Material=[&](const TCHAR* Suffix,const TCHAR* Expected){
  for(int32 I=0;I<Target->GetNumMaterials();++I)if(Target->GetSkeletalMeshAsset()->GetMaterials()[I].MaterialSlotName.ToString().EndsWith(Suffix)){
   auto* Actual=Target->GetMaterial(I);return Actual&&Actual->GetName()==Expected;
  }
  return false;
 };
 bool Materials=Material(TEXT("Cloth"),TEXT("MI_StyleSheer_Quarter_Cloth"))&&Material(TEXT("Eye"),TEXT("MI_Eye_Gloss"));
 Click(TEXT("SaveButton"));
 auto* Reopened=NewObject<UUserWidget>(Widget->GetWorld(),Class,NAME_None,RF_Transient);bool Persistence=Reopened->Initialize();
 if(Persistence){
  SlotProperty->SetPropertyValue_InContainer(Reopened,TestSlot);Reopened->ProcessEvent(Reopened->FindFunctionChecked(TEXT("Construct")),nullptr);
  for(const TCHAR* Name:Fields)Persistence&=Read(Reopened,Name)==Read(Widget,Name);
  auto* Box=Cast<UCheckBox>(Reopened->GetWidgetFromName(TEXT("BackVisibleCheck")));Persistence&=Box&&Box->IsChecked();
  auto* Text=Cast<UTextBlock>(Reopened->GetWidgetFromName(TEXT("OutfitStyleValue")));Persistence&=Text&&Text->GetText().ToString()==TEXT("黑色");
 }
 bool Reset=Click(TEXT("ResetButton"));Tick();
 for(int32 I=0;I<7;++I)Reset&=Read(Widget,Fields[I])==Defaults[I];
 Reset&=Material(TEXT("Cloth"),TEXT("MI_SunQuarter_Cloth"))&&Material(TEXT("Eye"),TEXT("MI_Eye_Gem"));
 bool Fixed=FMath::IsNearlyEqual(Read(Instance,TEXT("LeMalinScale")),1.25,.0001)&&FMath::IsNearlyEqual(Read(Instance,TEXT("LeMalinInertia")),.55,.0001)&&Read(Instance,TEXT("LeMalinReflectanceLevel"))==2&&Read(Instance,TEXT("LeMalinCollision"))==1&&Read(Instance,TEXT("LeMalinDamping"))==2&&Read(Instance,TEXT("LeMalinGravity"))==1;
 auto* Border=Cast<UBorder>(Widget->GetWidgetFromName(TEXT("ControlsPanel")));if(!Border)return TEXT("player panel border missing");
 Border->SetVisibility(ESlateVisibility::Visible);bool Close=Click(TEXT("CloseButton"))&&Border->GetVisibility()==ESlateVisibility::Collapsed;
 UGameplayStatics::DeleteGameInSlot(TestSlot,0);
 bool Preview=true;
 if(FSlateApplication::IsInitialized()&&!FParse::Param(FCommandLine::Get(),TEXT("nullrhi"))){
  FAssetCompilingManager::Get().FinishAllCompilation();FlushRenderingCommands();
  Border->SetVisibility(ESlateVisibility::Visible);const auto Slate=Widget->TakeWidget();
  FWidgetRenderer Renderer(true);auto* RT=Renderer.DrawWidget(Slate,FVector2D(800,900));FlushRenderingCommands();
  FReadSurfaceDataFlags ReadFlags;ReadFlags.SetLinearToGamma(false);
  TArray<FColor> Pixels;TArray64<uint8> Png;Preview=RT&&RT->GameThread_GetRenderTargetResource()->ReadPixels(Pixels,ReadFlags);
  if(Preview){FImageUtils::PNGCompressImageArray(800,900,Pixels,Png);Preview=FFileHelper::SaveArrayToFile(Png,*FPaths::ChangeExtension(OutputFile,TEXT("png")));}
  auto Bounds=Border->GetCachedGeometry();FVector2D Origin=Bounds.GetAbsolutePosition(),End=Origin+Bounds.GetAbsoluteSize();
  for(const TCHAR* Name:{TEXT("OutfitStylePrevious"),TEXT("GazeModeCheck"),TEXT("SaveButton"),TEXT("VoiceHint")}){
   auto Geometry=Widget->GetWidgetFromName(Name)->GetCachedGeometry();auto P=Geometry.GetAbsolutePosition(),Q=P+Geometry.GetAbsoluteSize();
   Preview&=P.X>=Origin.X&&P.Y>=Origin.Y&&Q.X<=End.X+1&&Q.Y<=End.Y+1;
  }
 }
 auto Report=MakeShared<FJsonObject>();
 Report->SetBoolField(TEXT("player_only_schema"),Schema);Report->SetBoolField(TEXT("defaults"),DefaultValues);
 Report->SetBoolField(TEXT("controls"),Controls&&Live);Report->SetBoolField(TEXT("black_and_eye_materials"),Materials);
 Report->SetBoolField(TEXT("save_load"),Persistence);Report->SetBoolField(TEXT("reset"),Reset);Report->SetBoolField(TEXT("fixed_parameters"),Fixed);
 Report->SetBoolField(TEXT("close"),Close);Report->SetBoolField(TEXT("layout"),Preview);
 bool Good=Schema&&DefaultValues&&Controls&&Live&&Materials&&Persistence&&Reset&&Fixed&&Close&&Preview;Report->SetBoolField(TEXT("passed"),Good);
 FString Json;FJsonSerializer::Serialize(Report,TJsonWriterFactory<>::Create(&Json));FFileHelper::SaveStringToFile(Json,*OutputFile,FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM);
 return Good?TEXT("OK: player controls, materials, save/load and layout"):TEXT("FAIL: player panel hookup");
}
