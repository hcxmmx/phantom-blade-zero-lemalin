#include "HcxRetargetTools.h"
#include "Animation/AnimBlueprint.h"
#include "Animation/AnimSingleNodeInstance.h"
#include "Animation/AnimSequence.h"
#include "Animation/MorphTarget.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/SkeletalMesh.h"
#include "Engine/SkeletalMeshSocket.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Editor.h"
#include "Engine/World.h"
#include "Retargeter/IKRetargetProcessor.h"
#include "UObject/UnrealType.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonSerializer.h"
#include "Misc/FileHelper.h"
#include "BoneControllers/AnimNode_RigidBody.h"
#include "Blueprint/UserWidget.h"
#include "Components/Slider.h"
#include "Components/Button.h"
#include "Components/Border.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/CheckBox.h"
#include "Components/TextBlock.h"
#include "Kismet/GameplayStatics.h"
#include "GameFramework/SaveGame.h"
#include "PhysicsEngine/SkeletalBodySetup.h"
#include "Engine/TextureRenderTarget2D.h"
#include "Slate/WidgetRenderer.h"
#include "ImageUtils.h"
#include "RenderingThread.h"
#include "Framework/Application/SlateApplication.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Components/PostProcessComponent.h"
#include "HAL/IConsoleManager.h"
#include "AssetCompilingManager.h"
#include "LeMalinPlayerPanelVerification.inl"

FString UHcxRetargetTools::ProbeLeMalinMaterialControls(USkeletalMesh* SourceMesh,USkeletalMesh* TargetMesh,UAnimSequence* Sequence,UAnimBlueprint* Blueprint,const FString& OutputFile)
{
 if(!GEditor||!SourceMesh||!TargetMesh||!Sequence||!Blueprint)return TEXT("material probe assets missing");
 auto* World=GEditor->GetEditorWorldContext().World();auto* Class=LoadClass<UUserWidget>(nullptr,TEXT("/Game/LeMalin/WBP_LeMalinTuning.WBP_LeMalinTuning_C"));if(!World||!Class)return TEXT("material probe widget missing");
 auto* Widget=NewObject<UUserWidget>(World,Class,NAME_None,RF_Transient);if(!Widget->Initialize())return TEXT("widget init failed");
 FActorSpawnParameters Params;Params.ObjectFlags=RF_Transient;auto* Actor=World->SpawnActor<ACharacter>(Params);auto* Part=World->SpawnActor<AActor>(Params);if(!Actor||!Part)return TEXT("probe actor missing");Actor->SetActorEnableCollision(false);
 auto* Source=Actor->GetMesh();Source->SetSkeletalMesh(SourceMesh);Source->SetDisablePostProcessBlueprint(true);Source->SetAnimationMode(EAnimationMode::AnimationSingleNode);Source->SetAnimation(Sequence);Source->GetSingleNodeInstance()->SetPlaying(false);
 auto* Target=NewObject<USkeletalMeshComponent>(Part,NAME_None,RF_Transient);Part->AddInstanceComponent(Target);Part->SetRootComponent(Target);Target->SetupAttachment(Source);Target->RegisterComponent();Target->SetSkeletalMesh(TargetMesh);Target->SetAnimationMode(EAnimationMode::AnimationSingleNode);Target->SetDisablePostProcessBlueprint(false);Target->SetOverridePostProcessAnimBP(Blueprint->GeneratedClass.Get());Target->InitAnim(true);
 auto* Instance=Target->GetPostProcessInstance();auto* Panel=Instance?FindFProperty<FObjectPropertyBase>(Instance->GetClass(),TEXT("LeMalinPanel")):nullptr;if(!Panel){Part->Destroy();Actor->Destroy();return TEXT("probe instance missing");}Panel->SetObjectPropertyValue_InContainer(Instance,Widget);
 if(FPlatformMisc::GetEnvironmentVariable(TEXT("LEMALIN_PROBE_PLAYER_PANEL"))==TEXT("1")){
  FString Result=VerifyLeMalinPlayerPanel(Widget,Source,Target,Instance,OutputFile);
  Part->Destroy();Actor->Destroy();return Result;
 }
 if(FPlatformMisc::GetEnvironmentVariable(TEXT("LEMALIN_PROBE_GAZE"))==TEXT("1")){
  auto* Mode=Cast<USlider>(Widget->GetWidgetFromName(TEXT("GazeModeSlider")));
  auto* Strength=Cast<USlider>(Widget->GetWidgetFromName(TEXT("GazeStrengthSlider")));
  auto Property=[&](const TCHAR* Name){return FindFProperty<FDoubleProperty>(Instance->GetClass(),Name);};
  auto* BlinkClock=Property(TEXT("BlinkClock"));auto* GazeClock=Property(TEXT("GazeClock"));
  auto* GoalH=Property(TEXT("GazeGoalH"));auto* GoalV=Property(TEXT("GazeGoalV"));
  if(!Mode||!Strength||!BlinkClock||!GazeClock||!GoalH||!GoalV){Part->Destroy();Actor->Destroy();return TEXT("gaze controls missing");}
  Source->VisibilityBasedAnimTickOption=Target->VisibilityBasedAnimTickOption=EVisibilityBasedAnimTickOption::AlwaysTickPoseAndRefreshBones;
  auto Step=[&](double Blink){
   BlinkClock->SetPropertyValue_InContainer(Instance,Blink);++GFrameCounter;
   Source->GetSingleNodeInstance()->SetPosition(.5f,false);Source->TickAnimation(0,false);Source->RefreshBoneTransforms();
   Target->TickAnimation(1.f/60,false);Target->RefreshBoneTransforms();
  };
  const TCHAR* Names[]={TEXT("GazeLeft"),TEXT("GazeRight"),TEXT("GazeUp"),TEXT("GazeDown")};bool Good=true;
  for(int32 Setting:{2,3,4,5,0}){
   Mode->SetValue(Setting);Strength->SetValue(100);for(int32 Frame=0;Frame<50;++Frame)Step(-1);
   for(int32 Index=0;Index<4;++Index)Good&=FMath::IsNearlyEqual(Target->GetMorphTarget(Names[Index]),Setting==Index+2?1.:0.,.005);
  }
  Mode->SetValue(2);Strength->SetValue(200);for(int32 Frame=0;Frame<50;++Frame)Step(-1);
  Good&=FMath::IsNearlyEqual(Target->GetMorphTarget(TEXT("GazeLeft")),2.,.005);
  Step(.10);Good&=Target->GetMorphTarget(TEXT("Blink"))>.99f;
  for(const TCHAR* Name:Names)Good&=Target->GetMorphTarget(Name)<.005f;
  Strength->SetValue(0);Step(-1);Good&=Target->GetMorphTarget(TEXT("GazeLeft"))<.005f;
  Mode->SetValue(1);Strength->SetValue(100);GazeClock->SetPropertyValue_InContainer(Instance,-100);
  GoalH->SetPropertyValue_InContainer(Instance,.5);GoalV->SetPropertyValue_InContainer(Instance,-.4);
  for(int32 Frame=0;Frame<50;++Frame)Step(-1);
  Good&=FMath::IsNearlyEqual(Target->GetMorphTarget(TEXT("GazeLeft")),.5,.005)&&FMath::IsNearlyEqual(Target->GetMorphTarget(TEXT("GazeDown")),.4,.005);
  Actor->GetCharacterMovement()->Velocity=FVector(100,0,0);for(int32 Frame=0;Frame<50;++Frame)Step(-1);
  for(const TCHAR* Name:Names)Good&=Target->GetMorphTarget(Name)<.005f;
  GazeClock->SetPropertyValue_InContainer(Instance,0);Step(-1);
  double Timer=GazeClock->GetPropertyValue_InContainer(Instance);Good&=Timer>=-5&&Timer<=-2;
  auto Report=MakeShared<FJsonObject>();Report->SetBoolField(TEXT("passed"),Good);
  Report->SetStringField(TEXT("scope"),TEXT("four directions, off, strength, blink blending, automatic gaze and moving recenter"));
  FString Json;FJsonSerializer::Serialize(Report,TJsonWriterFactory<>::Create(&Json));FFileHelper::SaveStringToFile(Json,*OutputFile,FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM);
  Part->Destroy();Actor->Destroy();return Good?TEXT("OK: gaze controls and blink blending"):TEXT("FAIL: gaze hookup");
 }
 if(FPlatformMisc::GetEnvironmentVariable(TEXT("LEMALIN_PROBE_EXPRESSIONS"))==TEXT("1")){
  auto* Mode=Cast<USlider>(Widget->GetWidgetFromName(TEXT("ExpressionModeSlider")));
  auto* Strength=Cast<USlider>(Widget->GetWidgetFromName(TEXT("ExpressionStrengthSlider")));
  auto* Clock=FindFProperty<FDoubleProperty>(Instance->GetClass(),TEXT("BlinkClock"));
  if(!Mode||!Strength||!Clock){Part->Destroy();Actor->Destroy();return TEXT("expression controls missing");}
  Source->VisibilityBasedAnimTickOption=Target->VisibilityBasedAnimTickOption=EVisibilityBasedAnimTickOption::AlwaysTickPoseAndRefreshBones;
  const TCHAR* Names[]={TEXT("FaceSleepy"),TEXT("FaceSmile"),TEXT("FaceFocus")};
  auto Step=[&](double BlinkClock){
   Clock->SetPropertyValue_InContainer(Instance,BlinkClock);++GFrameCounter;
   Source->GetSingleNodeInstance()->SetPosition(.5f,false);Source->TickAnimation(0,false);Source->RefreshBoneTransforms();
   Target->TickAnimation(1.f/60,false);Target->RefreshBoneTransforms();
  };
  bool Good=true;
  for(int32 Setting:{1,2,3,0}){
   Mode->SetValue(Setting);Strength->SetValue(100);
   for(int32 Frame=0;Frame<50;++Frame)Step(-1);
   for(int32 Index=0;Index<3;++Index){
    FString Prefix=Names[Index];double Expected=Setting==Index+1?1:0;
    Good&=FMath::IsNearlyEqual(Target->GetMorphTarget(FName(*(Prefix+TEXT("Face")))),Expected,.005);
    Good&=FMath::IsNearlyEqual(Target->GetMorphTarget(FName(*(Prefix+TEXT("Eyes")))),Expected,.005);
   }
  }
  Mode->SetValue(2);Strength->SetValue(150);
  for(int32 Frame=0;Frame<50;++Frame)Step(-1);
  Good&=FMath::IsNearlyEqual(Target->GetMorphTarget(TEXT("FaceSmileFace")),1.5,.005);
  Step(.10);
  Good&=Target->GetMorphTarget(TEXT("Blink"))>.99f&&Target->GetMorphTarget(TEXT("FaceSmileEyes"))<.01f;
  Good&=FMath::IsNearlyEqual(Target->GetMorphTarget(TEXT("FaceSmileFace")),1.5,.005);
  Strength->SetValue(0);for(int32 Frame=0;Frame<50;++Frame)Step(-1);
  Good&=Target->GetMorphTarget(TEXT("FaceSmileFace"))<.005f;
  auto* AutoClock=FindFProperty<FDoubleProperty>(Instance->GetClass(),TEXT("FaceAutoClock"));
  auto* AutoChoice=FindFProperty<FDoubleProperty>(Instance->GetClass(),TEXT("FaceAutoChoice"));
  if(!AutoClock||!AutoChoice){Part->Destroy();Actor->Destroy();return TEXT("automatic expression state missing");}
  Mode->SetValue(4);Strength->SetValue(100);
  for(int32 Setting:{0,1,2}){
   AutoClock->SetPropertyValue_InContainer(Instance,-100);AutoChoice->SetPropertyValue_InContainer(Instance,Setting+.5);
   for(int32 Frame=0;Frame<120;++Frame)Step(-1);
   for(int32 Index=0;Index<3;++Index){
    FString Prefix=Names[Index];double Expected=Setting==Index+1?1:0;
    Good&=FMath::IsNearlyEqual(Target->GetMorphTarget(FName(*(Prefix+TEXT("Face")))),Expected,.04);
   }
  }
  AutoClock->SetPropertyValue_InContainer(Instance,0);Step(-1);
  Good&=AutoClock->GetPropertyValue_InContainer(Instance)<=-4&&AutoClock->GetPropertyValue_InContainer(Instance)>=-8;
  Good&=AutoChoice->GetPropertyValue_InContainer(Instance)>=0&&AutoChoice->GetPropertyValue_InContainer(Instance)<=3;
  auto Report=MakeShared<FJsonObject>();Report->SetBoolField(TEXT("passed"),Good);
  Report->SetStringField(TEXT("scope"),TEXT("manual presets, strength, blink blending, automatic choices and timer"));
  FString Json;FJsonSerializer::Serialize(Report,TJsonWriterFactory<>::Create(&Json));FFileHelper::SaveStringToFile(Json,*OutputFile,FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM);
  Part->Destroy();Actor->Destroy();return Good?TEXT("OK: expression preview and blink blending"):TEXT("FAIL: expression preview hookup");
 }
 if(FPlatformMisc::GetEnvironmentVariable(TEXT("LEMALIN_PROBE_OUTFIT"))==TEXT("1")){
  auto* Style=Cast<USlider>(Widget->GetWidgetFromName(TEXT("OutfitStyleSlider")));
  auto* Level=Cast<USlider>(Widget->GetWidgetFromName(TEXT("ReflectanceLevelSlider")));
  if(!Style||!Level){Part->Destroy();Actor->Destroy();return TEXT("outfit controls missing");}
  bool Good=Style->GetValue()==0;
  Source->VisibilityBasedAnimTickOption=Target->VisibilityBasedAnimTickOption=EVisibilityBasedAnimTickOption::AlwaysTickPoseAndRefreshBones;
  const TCHAR* Levels[]={TEXT("Original"),TEXT("Half"),TEXT("Quarter"),TEXT("Eighth"),TEXT("Sixteenth"),TEXT("Black")};
  TArray<TPair<int32,int32>> Settings={{0,2},{1,2},{2,2},{3,2},{4,2},{5,2},{6,2},{3,0},{6,4},{4,5},{2,0},{2,4},{2,5},{1,2},{0,2}};
  TArray<TSharedPtr<FJsonValue>> Rows;
  for(auto Setting:Settings){
   Style->SetValue(Setting.Key);Level->SetValue(Setting.Value);
   for(int32 Frame=0;Frame<3;++Frame){++GFrameCounter;Source->GetSingleNodeInstance()->SetPosition(.5f,false);Source->TickAnimation(0,false);Source->RefreshBoneTransforms();Target->TickAnimation(1.f/60,false);Target->RefreshBoneTransforms();}
   bool FrameGood=true;int32 OutfitSlots=0;
   for(int32 Slot=0;Slot<TargetMesh->GetMaterials().Num();++Slot){
    FString Name=TargetMesh->GetMaterials()[Slot].MaterialSlotName.ToString();Name.RemoveFromStart(TEXT("Preview_"));
    bool Outfit=Name==TEXT("Cloth")||Name==TEXT("Obj")||Name==TEXT("RabbitEar")||Name==TEXT("Shoes")||Name==TEXT("Belt");
    FString PartName;
    if(Outfit){PartName=Name==TEXT("Cloth")?TEXT("Cloth"):TEXT("Obj");++OutfitSlots;}
    else if(Name==TEXT("Body"))PartName=TEXT("Skin");
    else if(Name==TEXT("Hair"))PartName=TEXT("Hair");
    else if(Name==TEXT("Belt"))PartName=TEXT("Obj");
    else continue;
    FString Expected;
    if(Outfit&&Setting.Key>=1&&Setting.Value<5)Expected=(Setting.Key>=2&&Name==TEXT("Cloth")?TEXT("MI_StyleSheer_"):TEXT("MI_StyleBlack_"))+FString(Levels[Setting.Value])+TEXT("_")+PartName;
    else Expected=Setting.Value==0?TEXT("MI_")+PartName:TEXT("MI_Sun")+FString(Levels[Setting.Value])+TEXT("_")+PartName;
    if(Name==TEXT("Cloth")&&Setting.Key>=3&&Setting.Value<5){
     const TCHAR* Modes[]={TEXT("Smooth"),TEXT("Matte"),TEXT("Flat"),TEXT("Ridges")};
     Expected=FString(TEXT("MI_StockingProbe_"))+Modes[Setting.Key-3]+TEXT("_")+Levels[Setting.Value]+TEXT("_Cloth");
    }
    auto* Actual=Target->GetMaterial(Slot);FrameGood&=Actual&&Actual->GetName()==Expected;
   }
   Good&=FrameGood&&OutfitSlots==5;
   auto Row=MakeShared<FJsonObject>();Row->SetNumberField(TEXT("style"),Setting.Key);Row->SetNumberField(TEXT("reflectance"),Setting.Value);Row->SetBoolField(TEXT("correct_materials"),FrameGood&&OutfitSlots==5);Rows.Add(MakeShared<FJsonValueObject>(Row));
  }
  auto Report=MakeShared<FJsonObject>();Report->SetBoolField(TEXT("passed"),Good);Report->SetArrayField(TEXT("states"),Rows);
  FString Json;FJsonSerializer::Serialize(Report,TJsonWriterFactory<>::Create(&Json));FFileHelper::SaveStringToFile(Json,*OutputFile,FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM);
  Part->Destroy();Actor->Destroy();return Good?TEXT("OK: outfit switch, reflectance integration and white restoration"):TEXT("FAIL: outfit material hookup");
 }
 if(FPlatformMisc::GetEnvironmentVariable(TEXT("LEMALIN_PROBE_BACK_VISIBILITY"))==TEXT("1")){
  auto* Control=Cast<USlider>(Widget->GetWidgetFromName(TEXT("BackVisibleSlider")));
  if(!Control){Part->Destroy();Actor->Destroy();return TEXT("back visibility control missing");}
  auto* DiagnosticPanel=Cast<UBorder>(Widget->GetWidgetFromName(TEXT("ControlsPanel")));
  DiagnosticPanel->SetVisibility(ESlateVisibility::Visible);
  const auto DiagnosticSlate=Widget->TakeWidget();
  Source->VisibilityBasedAnimTickOption=Target->VisibilityBasedAnimTickOption=EVisibilityBasedAnimTickOption::AlwaysTickPoseAndRefreshBones;
  auto* BladeActor=World->SpawnActor<AActor>(Params);
  auto* KnifeActor=World->SpawnActor<AActor>(Params);
  auto Root=[&](AActor* Owner,const TCHAR* Socket){
   auto* Component=NewObject<USceneComponent>(Owner,NAME_None,RF_Transient);
   Owner->AddInstanceComponent(Component);Owner->SetRootComponent(Component);
   Component->SetupAttachment(Source,FName(Socket));Component->RegisterComponent();
   return Component;
  };
  auto Mesh=[&](AActor* Owner,USceneComponent* Parent){
   auto* Component=NewObject<UStaticMeshComponent>(Owner,NAME_None,RF_Transient);
   Owner->AddInstanceComponent(Component);Component->SetupAttachment(Parent);Component->RegisterComponent();
   return Component;
  };
  auto* BladeRoot=Root(BladeActor,TEXT("Pt_LongBlade_Back"));
  auto* KnifeRoot=NewObject<UStaticMeshComponent>(KnifeActor,NAME_None,RF_Transient);
  KnifeActor->AddInstanceComponent(KnifeRoot);KnifeActor->SetRootComponent(KnifeRoot);
  KnifeRoot->SetupAttachment(Source,TEXT("Pt_Knife_Back_Socket"));KnifeRoot->RegisterComponent();
  auto* Blade=Mesh(BladeActor,BladeRoot);
  auto* Knife=KnifeRoot;
  auto* Scabbard=Mesh(KnifeActor,Source);
  Scabbard->AttachToComponent(Source,FAttachmentTransformRules::KeepRelativeTransform,TEXT("Pt_KnifeScabbard_Back_Socket"));
  auto* KnifeChild=Mesh(KnifeActor,Knife);
  auto* AlreadyHidden=Mesh(KnifeActor,KnifeRoot);AlreadyHidden->SetHiddenInGame(true);
  auto* Other=Mesh(Actor,Source);
  auto Tick=[&](){
   for(int32 Frame=0;Frame<3;++Frame){
    ++GFrameCounter;
    Source->GetSingleNodeInstance()->SetPosition(.5f,false);
    Source->TickAnimation(0,false);Source->RefreshBoneTransforms();
    Target->TickAnimation(1.f/60,false);Target->RefreshBoneTransforms();
   }
  };
  Control->SetValue(0);Tick();
  bool Initial=!Blade->IsVisible()&&!Knife->IsVisible()&&!KnifeChild->IsVisible()&&!Scabbard->IsVisible()&&Other->IsVisible();
  // Weapon logic may refresh Visible after animation evaluation.
  Knife->SetVisibility(true);KnifeChild->SetVisibility(true);
  bool LateRefresh=!Knife->IsVisible()&&!KnifeChild->IsVisible();
  Tick();bool Maintained=!Knife->GetVisibleFlag()&&!KnifeChild->GetVisibleFlag();
  KnifeRoot->AttachToComponent(Source,FAttachmentTransformRules::KeepRelativeTransform,TEXT("Pt_Knife_Hand"));Tick();
  bool Draw=Knife->IsVisible()&&KnifeChild->IsVisible()&&!Blade->IsVisible()&&!AlreadyHidden->IsVisible();
  KnifeRoot->AttachToComponent(Source,FAttachmentTransformRules::KeepRelativeTransform,TEXT("Pt_Knife_Back_Socket"));Tick();
  bool Sheath=!Knife->IsVisible()&&!KnifeChild->IsVisible();
  Control->SetValue(1);Tick();
  bool Show=Blade->IsVisible()&&Knife->IsVisible()&&KnifeChild->IsVisible()&&Scabbard->IsVisible()&&!AlreadyHidden->IsVisible()&&Other->IsVisible();
  auto* DiagnosticProperty=FindFProperty<FStrProperty>(Widget->GetClass(),TEXT("BackDiagnostics"));
  FString Capture=DiagnosticProperty?DiagnosticProperty->GetPropertyValue_InContainer(Widget):TEXT("");
  FString DiagnosticSlot=TEXT("LeMalinBackCaptureProbe_")+FGuid::NewGuid().ToString(EGuidFormats::Digits);
  FindFProperty<FStrProperty>(Widget->GetClass(),TEXT("SettingsSlot"))->SetPropertyValue_InContainer(Widget,DiagnosticSlot);
  Cast<UButton>(Widget->GetWidgetFromName(TEXT("SaveButton")))->OnClicked.Broadcast();
  auto* Saved=UGameplayStatics::LoadGameFromSlot(DiagnosticSlot,0);
  auto* SavedProperty=Saved?FindFProperty<FStrProperty>(Saved->GetClass(),TEXT("BackDiagnostics")):nullptr;
  bool Captured=Capture.Contains(TEXT("Pt_Knife_Back"))&&Capture.Contains(TEXT("PLAYER_COMPONENTS"))&&SavedProperty&&SavedProperty->GetPropertyValue_InContainer(Saved)==Capture;
  UGameplayStatics::DeleteGameInSlot(DiagnosticSlot,0);
  bool Good=Initial&&LateRefresh&&Maintained&&Draw&&Sheath&&Show&&Captured;
  auto Report=MakeShared<FJsonObject>();Report->SetBoolField(TEXT("passed"),Good);
  Report->SetBoolField(TEXT("blade_knife_and_descendants_hidden"),Initial);
  Report->SetBoolField(TEXT("late_game_visibility_refresh_stays_hidden"),LateRefresh);
  Report->SetBoolField(TEXT("hidden_flags_maintained"),Maintained);
  Report->SetBoolField(TEXT("draw_restores_knife"),Draw);Report->SetBoolField(TEXT("sheath_hides_knife"),Sheath);
  Report->SetBoolField(TEXT("show_restores_and_preserves_game_hidden_parts"),Show);
  Report->SetBoolField(TEXT("attachment_capture_saved"),Captured);
  Report->SetStringField(TEXT("attachment_capture"),Capture);
  Report->SetBoolField(TEXT("saved_capture_property_found"),SavedProperty!=nullptr);
  FString Json;FJsonSerializer::Serialize(Report,TJsonWriterFactory<>::Create(&Json));
  FFileHelper::SaveStringToFile(Json,*OutputFile,FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM);
  Other->DestroyComponent();KnifeActor->Destroy();BladeActor->Destroy();Part->Destroy();Actor->Destroy();
  return Good?TEXT("OK: knife subtree visibility, late refresh and draw/sheath restoration"):TEXT("FAIL: back visibility regression");
 }
 if(FPlatformMisc::GetEnvironmentVariable(TEXT("LEMALIN_PROBE_HDR"))==TEXT("1")){
  Source->VisibilityBasedAnimTickOption=Target->VisibilityBasedAnimTickOption=EVisibilityBasedAnimTickOption::AlwaysTickPoseAndRefreshBones;
  auto* Mode=Cast<USlider>(Widget->GetWidgetFromName(TEXT("HDRModeSlider")));if(!Mode){Part->Destroy();Actor->Destroy();return TEXT("HDR control missing");}
  auto* DepthProp=FindFProperty<FBoolProperty>(UPrimitiveComponent::StaticClass(),TEXT("bRenderCustomDepth"));auto* StencilProp=FindFProperty<FIntProperty>(UPrimitiveComponent::StaticClass(),TEXT("CustomDepthStencilValue"));auto* MaskProp=FindFProperty<FProperty>(UPrimitiveComponent::StaticClass(),TEXT("CustomDepthStencilWriteMask"));auto ReadMask=[&](){FString Value;MaskProp->ExportTextItem_Direct(Value,MaskProp->ContainerPtrToValuePtr<void>(Target),nullptr,Target,PPF_None);return Value;};
  bool OldDepth=DepthProp->GetPropertyValue_InContainer(Target);int32 OldStencil=StencilProp->GetPropertyValue_InContainer(Target);FString OldMask=ReadMask();auto* CVar=IConsoleManager::Get().FindConsoleVariable(TEXT("r.CustomDepth"));int32 OldCVar=CVar->GetInt();bool Good=true;TArray<TSharedPtr<FJsonValue>> Rows;
  auto Object=[&](const TCHAR* Name){return FindFProperty<FObjectPropertyBase>(Widget->GetClass(),Name)->GetObjectPropertyValue_InContainer(Widget);};
  for(int32 Value:{0,1,2,0}){
   Mode->SetValue(Value);
   for(int32 I=0;I<3;++I){if(Target->GetPostProcessInstance()!=Instance){Instance=Target->GetPostProcessInstance();FindFProperty<FObjectPropertyBase>(Instance->GetClass(),TEXT("LeMalinPanel"))->SetObjectPropertyValue_InContainer(Instance,Widget);}++GFrameCounter;Source->GetSingleNodeInstance()->SetPosition(.5f,false);Source->TickAnimation(0,false);Source->RefreshBoneTransforms();Target->TickAnimation(1.f/60,false);Target->RefreshBoneTransforms();}
   auto* Volume=Cast<UPostProcessComponent>(Object(TEXT("HDRVolume")));auto* MID=Cast<UMaterialInstanceDynamic>(Object(TEXT("HDRMID")));bool Depth=DepthProp->GetPropertyValue_InContainer(Target);int32 Stencil=StencilProp->GetPropertyValue_InContainer(Target);FString Mask=ReadMask();
   auto Row=MakeShared<FJsonObject>();Row->SetNumberField(TEXT("mode"),Value);Row->SetBoolField(TEXT("same_instance"),Target->GetPostProcessInstance()==Instance);Row->SetNumberField(TEXT("runtime_mode"),FindFProperty<FDoubleProperty>(Instance->GetClass(),TEXT("LeMalinHDRMode"))->GetPropertyValue_InContainer(Instance));Row->SetBoolField(TEXT("depth"),Depth);Row->SetNumberField(TEXT("stencil"),Stencil);Row->SetNumberField(TEXT("buffer_mode"),CVar->GetInt());Row->SetBoolField(TEXT("volume_enabled"),Volume&&Volume->bEnabled);Row->SetNumberField(TEXT("blend_weight"),Volume?Volume->BlendWeight:0);Row->SetNumberField(TEXT("shader_mode"),MID?MID->K2_GetScalarParameterValue(TEXT("Mode")):-1);Row->SetStringField(TEXT("skin_material"),Target->GetMaterial(6)->GetName());Rows.Add(MakeShared<FJsonValueObject>(Row));
   if(Value){Good&=Volume&&Volume->bEnabled&&Volume->BlendWeight==1&&MID&&MID->K2_GetScalarParameterValue(TEXT("Mode"))==Value&&Depth&&Stencil==231&&CVar->GetInt()==3&&Target->GetMaterial(6)->GetName()==TEXT("MI_Skin")&&Volume->Settings.WeightedBlendables.Array.Num()==1;}
   else Good&=(!Volume||!Volume->bEnabled)&&Depth==OldDepth&&Stencil==OldStencil&&Mask==OldMask&&CVar->GetInt()==OldCVar;
  }
  auto Report=MakeShared<FJsonObject>();Report->SetBoolField(TEXT("passed"),Good);Report->SetArrayField(TEXT("frames"),Rows);FString Json;FJsonSerializer::Serialize(Report,TJsonWriterFactory<>::Create(&Json));FFileHelper::SaveStringToFile(Json,*OutputFile,FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM);CVar->Set(OldCVar,ECVF_SetByConsole);Part->Destroy();Actor->Destroy();return Good?TEXT("OK: HDR load, stencil, blendable and disable restoration"):TEXT("FAIL: HDR runtime hookup or restoration");
 }
 if(FPlatformMisc::GetEnvironmentVariable(TEXT("LEMALIN_PROBE_EYES"))==TEXT("1")){
  auto* EyeControl=Cast<USlider>(Widget->GetWidgetFromName(TEXT("EyeFinishSlider")));
  auto* BodyControl=Cast<USlider>(Widget->GetWidgetFromName(TEXT("ReflectanceLevelSlider")));
  if(!EyeControl||!BodyControl){Part->Destroy();Actor->Destroy();return TEXT("eye comparison controls missing");}
  Source->VisibilityBasedAnimTickOption=Target->VisibilityBasedAnimTickOption=EVisibilityBasedAnimTickOption::AlwaysTickPoseAndRefreshBones;
  BodyControl->SetValue(2);
  bool Good=true;
  TArray<TSharedPtr<FJsonValue>> Rows;
  const TCHAR* ExpectedMaterials[]={TEXT("MI_Eye"),TEXT("MI_Eye_Gloss"),TEXT("MI_Eye_Crystal"),TEXT("MI_Eye_Gem")};
  const TCHAR* ExpectedMix[]={TEXT("T_NeutralARM"),TEXT("T_Eye_Gloss_ARM"),TEXT("T_Eye_Crystal_ARM"),TEXT("T_Eye_Crystal_ARM")};
  for(int32 Value:{0,1,2,3,0}){
   EyeControl->SetValue(Value);
   for(int32 Frame=0;Frame<3;++Frame){
    ++GFrameCounter;
    Source->GetSingleNodeInstance()->SetPosition(.5f,false);
    Source->TickAnimation(0,false);Source->RefreshBoneTransforms();
    Target->TickAnimation(1.f/60,false);Target->RefreshBoneTransforms();
   }
   auto Row=MakeShared<FJsonObject>();Row->SetNumberField(TEXT("mode"),Value);
   TArray<TSharedPtr<FJsonValue>> Materials;
   for(int32 Slot=0;Slot<TargetMesh->GetMaterials().Num();++Slot){
    FString Name=TargetMesh->GetMaterials()[Slot].MaterialSlotName.ToString();Name.RemoveFromStart(TEXT("Preview_"));
    if(Name!=TEXT("Eye")&&Name!=TEXT("EyeLight"))continue;
    auto* Material=Target->GetMaterial(Slot);
    UTexture* Mix=nullptr;UTexture* Albedo=nullptr;
    if(Material){
     Material->GetTextureParameterValue(FMaterialParameterInfo(TEXT("Mix"),EMaterialParameterAssociation::LayerParameter,0),Mix);
     Material->GetTextureParameterValue(FMaterialParameterInfo(TEXT("Albedo"),EMaterialParameterAssociation::LayerParameter,0),Albedo);
    }
    FString ExpectedAlbedo=Value==3?TEXT("T_Eye_Gem_Albedo"):TEXT("T_Eye");
    Good&=Material&&Material->GetName()==ExpectedMaterials[Value]&&Mix&&Mix->GetName()==ExpectedMix[Value]&&Albedo&&Albedo->GetName()==ExpectedAlbedo;
    auto Entry=MakeShared<FJsonObject>();Entry->SetStringField(TEXT("slot"),Name);
    Entry->SetStringField(TEXT("material"),Material?Material->GetName():TEXT("none"));
    Entry->SetStringField(TEXT("mix"),Mix?Mix->GetName():TEXT("none"));
    Entry->SetStringField(TEXT("albedo"),Albedo?Albedo->GetName():TEXT("none"));
    Materials.Add(MakeShared<FJsonValueObject>(Entry));
   }
   Good&=Materials.Num()==2&&Target->GetMaterial(6)&&Target->GetMaterial(6)->GetName()==TEXT("MI_SunQuarter_Skin");
   Row->SetArrayField(TEXT("eyes"),Materials);Rows.Add(MakeShared<FJsonValueObject>(Row));
  }
  auto Report=MakeShared<FJsonObject>();Report->SetBoolField(TEXT("passed"),Good);Report->SetArrayField(TEXT("frames"),Rows);
  FString Json;FJsonSerializer::Serialize(Report,TJsonWriterFactory<>::Create(&Json));
  FFileHelper::SaveStringToFile(Json,*OutputFile,FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM);
  Part->Destroy();Actor->Destroy();
  return Good?TEXT("OK: eye materials and textures switch 0/1/2/3/0, body stays at tier 2"):TEXT("FAIL: eye material comparison hookup");
 }
 auto* Mode=Cast<USlider>(Widget->GetWidgetFromName(TEXT("SunModeSlider")));if(!Mode){Part->Destroy();Actor->Destroy();return TEXT("probe mode control missing");}
 TArray<TSharedPtr<FJsonValue>> Rows;bool Good=true;
 for(int32 Value:{0,1,2,3,0}){
  Mode->SetValue(Value);
  for(int32 I=0;I<3;++I){Source->GetSingleNodeInstance()->SetPosition(.5f,false);Source->TickAnimation(0,false);Source->RefreshBoneTransforms();Target->TickAnimation(1.f/60,false);Target->RefreshBoneTransforms();}
  auto R=MakeShared<FJsonObject>();R->SetNumberField(TEXT("mode"),Value);TArray<TSharedPtr<FJsonValue>> Mats;
  const TCHAR* Parts[]={TEXT("Face"),TEXT("Cloth"),TEXT("Skin"),TEXT("Hair"),TEXT("Hair")};int32 J=0;
  for(int32 I:{1,5,6,7,8}){auto M=MakeShared<FJsonObject>();M->SetNumberField(TEXT("slot"),I);auto* Mat=Target->GetMaterial(I);FString Expected=Value==0?FString(TEXT("MI_"))+Parts[J]:FString(TEXT("MI_Sun"))+(Value==1?TEXT("NoSpec"):Value==2?TEXT("Half"):TEXT("Black"))+TEXT("_")+Parts[J];++J;Good&=Mat&&Mat->GetName()==Expected;M->SetStringField(TEXT("material"),Mat?Mat->GetName():TEXT("none"));UTexture* Tex=nullptr;if(Mat)Mat->GetTextureParameterValue(FMaterialParameterInfo(TEXT("Albedo"),EMaterialParameterAssociation::LayerParameter,0),Tex);M->SetStringField(TEXT("albedo"),Tex?Tex->GetName():TEXT("none"));if(Value==2||Value==3)Good&=Tex&&Tex->GetName().StartsWith(Value==2?TEXT("T_SunHalf_"):TEXT("T_SunBlack_"));Mats.Add(MakeShared<FJsonValueObject>(M));}
  R->SetArrayField(TEXT("actual_materials"),Mats);Rows.Add(MakeShared<FJsonValueObject>(R));
 }
 auto Report=MakeShared<FJsonObject>();Report->SetArrayField(TEXT("frames"),Rows);FString Json;FJsonSerializer::Serialize(Report,TJsonWriterFactory<>::Create(&Json));FFileHelper::SaveStringToFile(Json,*OutputFile,FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM);Part->Destroy();Actor->Destroy();return Good?TEXT("OK: actual comparison materials/textures switched and restored"):TEXT("FAIL: material comparison did not reach target");
}

FString UHcxRetargetTools::VerifyLeMalinLiveliness(USkeletalMesh* SourceMesh,USkeletalMesh* TargetMesh,UAnimSequence* Sequence,UAnimBlueprint* Blueprint,const FString& OutputFile){
 if(!GEditor||!SourceMesh||!TargetMesh||!Sequence||!Blueprint||!TargetMesh->FindMorphTarget(TEXT("Blink")))return TEXT("verification assets missing");
 UWorld* World=GEditor->GetEditorWorldContext().World();if(!World)return TEXT("verification world missing");
 FActorSpawnParameters Params;Params.ObjectFlags=RF_Transient;auto* Actor=World->SpawnActor<ACharacter>(Params);if(!Actor)return TEXT("actor spawn failed");Actor->SetActorEnableCollision(false);
 auto* Source=Actor->GetMesh();Source->SetSkeletalMesh(SourceMesh);Source->SetDisablePostProcessBlueprint(true);Source->SetAnimationMode(EAnimationMode::AnimationSingleNode);Source->SetAnimation(Sequence);Source->GetSingleNodeInstance()->SetPlaying(false);
 auto* PartActor=World->SpawnActor<AActor>(Params);if(!PartActor){Actor->Destroy();return TEXT("part actor spawn failed");}
 auto* Target=NewObject<USkeletalMeshComponent>(PartActor,NAME_None,RF_Transient);PartActor->AddInstanceComponent(Target);PartActor->SetRootComponent(Target);Target->SetupAttachment(Source);Target->RegisterComponent();Target->SetSkeletalMesh(TargetMesh);Target->SetAnimationMode(EAnimationMode::AnimationSingleNode);Target->SetDisablePostProcessBlueprint(false);Target->SetOverridePostProcessAnimBP(Blueprint->GeneratedClass.Get());Target->InitAnim(true);
 Source->VisibilityBasedAnimTickOption=Target->VisibilityBasedAnimTickOption=EVisibilityBasedAnimTickOption::AlwaysTickPoseAndRefreshBones;
 auto* Instance=Target->GetPostProcessInstance();auto* Clock=Instance?FindFProperty<FDoubleProperty>(Instance->GetClass(),TEXT("BlinkClock")):nullptr;if(!Clock){PartActor->Destroy();Actor->Destroy();return TEXT("postprocess blink clock missing");}
 auto* RTG=LoadObject<UIKRetargeter>(nullptr,TEXT("/Game/LeMalin/RTG_Hun_LeMalin"));FIKRetargetProcessor Processor;FRetargetProfile Profile;Processor.Initialize(SourceMesh,TargetMesh,RTG,Profile,false);if(!Processor.IsInitialized()){PartActor->Destroy();Actor->Destroy();return TEXT("reference processor missing");}
 const auto& Ref=TargetMesh->GetRefSkeleton();
 auto Pose=[](const TArray<FTransform>& Values){TArray<TSharedPtr<FJsonValue>> Out;for(const auto& T:Values){auto P=T.GetLocation(),S=T.GetScale3D();auto Q=T.GetRotation();TArray<TSharedPtr<FJsonValue>> V;for(double D:{P.X,P.Y,P.Z,Q.X,Q.Y,Q.Z,Q.W,S.X,S.Y,S.Z})V.Add(MakeShared<FJsonValueNumber>(D));Out.Add(MakeShared<FJsonValueArray>(V));}return Out;};
 auto Tick=[&](float Time){Source->GetSingleNodeInstance()->SetPosition(FMath::Fmod(Time,Sequence->GetPlayLength()),false);Source->TickAnimation(0,false);Source->RefreshBoneTransforms();Target->TickAnimation(1.f/60,false);Target->RefreshBoneTransforms();};
 for(int32 I=0;I<60;++I)Tick(0);
 auto* ScaleControl=FindFProperty<FDoubleProperty>(Instance->GetClass(),TEXT("LeMalinScale"));double DefaultScale=ScaleControl?ScaleControl->GetPropertyValue_InContainer(Instance):1.,DefaultScaleError=0;
 auto DefaultPose=Target->GetComponentSpaceTransforms();if(ScaleControl)ScaleControl->SetPropertyValue_InContainer(Instance,1.);Tick(0);auto NeutralPose=Target->GetComponentSpaceTransforms();
 for(const TCHAR* Name:{TEXT("head"),TEXT("hand_l"),TEXT("hand_r"),TEXT("foot_l"),TEXT("foot_r")}){int32 B=Ref.FindBoneIndex(Name);DefaultScaleError=FMath::Max(DefaultScaleError,FVector::Distance(DefaultPose[B].GetLocation(),NeutralPose[B].GetLocation()*DefaultScale));}
 double BlinkError=0;
 for(double Time:{-.5,0.,.025,.05,.10,.14,.23,.32}){
  Clock->SetPropertyValue_InContainer(Instance,Time-1./60);Tick(.5f);double C=Clock->GetPropertyValue_InContainer(Instance),X=FMath::Clamp(FMath::Min(C/.10,(.32-C)/.18),0.,1.);double Expected=X*X*(3-2*X);BlinkError=FMath::Max(BlinkError,FMath::Abs(Target->GetMorphTarget(TEXT("Blink"))-Expected));
 }
 Clock->SetPropertyValue_InContainer(Instance,-1.);TArray<TSharedPtr<FJsonValue>> Frames,Blinks,Names,Parents;auto Global=Ref.GetRefBonePose();
 for(int32 I=0;I<Global.Num();++I){Names.Add(MakeShared<FJsonValueString>(Ref.GetBoneName(I).ToString()));int32 P=Ref.GetParentIndex(I);Parents.Add(MakeShared<FJsonValueNumber>(P));if(P>=0)Global[I]*=Global[P];}
 double MaxMainError=0,MaxEar=0,MaxHair=0,MaxStep=0,TotalMS=0,MaxScaleError=0,MaxBraidLengthError=0;bool Finite=true;TArray<FTransform> Previous;
 const int32 Count=480;
 for(int32 F=0;F<Count;++F){
  double T=F/60.;Actor->SetActorLocation(FVector(18*FMath::Sin(2*T),10*FMath::Sin(T),0));double Yaw=22*FMath::Sin(1.4*T);if(T>=4)Yaw=120*FMath::Clamp((T-4)/.3,0.,1.)*(1-FMath::Clamp((T-5)/.3,0.,1.));Actor->SetActorRotation(FRotator(0,Yaw,0));FVector Velocity(36*FMath::Cos(2*T),10*FMath::Cos(T),0);if(T>=2&&T<4)Velocity=FVector(500*FMath::Clamp((T-2)/.5,0.,1.)*(1-FMath::Clamp((T-3)/.2,0.,1.)),0,0);Actor->GetCharacterMovement()->Velocity=Velocity;double Start=FPlatformTime::Seconds();Tick(T);TotalMS+=(FPlatformTime::Seconds()-Start)*1000;
  auto Input=Source->GetComponentSpaceTransforms();Processor.ApplySourceScaleToPose(Input);auto Base=Processor.RunRetargeter(Input,Profile,1.f/60);auto Current=Target->GetComponentSpaceTransforms();
  for(int32 I=0;I<Current.Num();++I){Finite&=!Current[I].ContainsNaN();auto S=Current[I].GetScale3D();MaxScaleError=FMath::Max(MaxScaleError,FMath::Max3(FMath::Abs(S.X-1),FMath::Abs(S.Y-1),FMath::Abs(S.Z-1)));}
  for(int32 N=2;N<16;++N){int32 B=Ref.FindBoneIndex(FName(*FString::Printf(TEXT("aux_%03d"),N))),P=Ref.GetParentIndex(B);MaxBraidLengthError=FMath::Max(MaxBraidLengthError,FMath::Abs(FVector::Distance(Current[B].GetLocation(),Current[P].GetLocation())-FVector::Distance(Global[B].GetLocation(),Global[P].GetLocation())));}
  for(const TCHAR* Name:{TEXT("pelvis"),TEXT("spine_02"),TEXT("head"),TEXT("hand_l"),TEXT("hand_r"),TEXT("foot_l"),TEXT("foot_r")}){int32 I=Ref.FindBoneIndex(Name);MaxMainError=FMath::Max(MaxMainError,FVector::Distance(Current[I].GetLocation(),Base[I].GetLocation()));}
  for(int32 Index:{28,29,12,13}){int32 I=Ref.FindBoneIndex(FName(*FString::Printf(TEXT("aux_%03d"),Index)));double D=FVector::Distance(Current[I].GetLocation(),Base[I].GetLocation());if(Index>=24)MaxEar=FMath::Max(MaxEar,D);else MaxHair=FMath::Max(MaxHair,D);if(Previous.Num())MaxStep=FMath::Max(MaxStep,FVector::Distance(Current[I].GetLocation(),Previous[I].GetLocation()));}
  Previous=Current;if(F%2==0){Frames.Add(MakeShared<FJsonValueArray>(Pose(Current)));Blinks.Add(MakeShared<FJsonValueNumber>(Target->GetMorphTarget(TEXT("Blink"))));}
 }
 // A necessary gravity check: identical fixed, tilted head pose with/without
 // gravity. This distinguishes a live physical chain from an inert node/asset.
 FAnimNode_RigidBody* Rigid=nullptr;
 for(TFieldIterator<FStructProperty> It(Instance->GetClass());It;++It)if(It->Struct==FAnimNode_RigidBody::StaticStruct()){Rigid=It->ContainerPtrToValuePtr<FAnimNode_RigidBody>(Instance);break;}
 double GravityResponse=0,GravityWorldDrop=0,A1GravityAngle=0;
 auto* GravityControl=FindFProperty<FDoubleProperty>(Instance->GetClass(),TEXT("LeMalinGravity"));
 if(Rigid){
  Actor->SetActorLocation(FVector::ZeroVector);Actor->SetActorRotation(FRotator::ZeroRotator);Actor->GetCharacterMovement()->Velocity=FVector::ZeroVector;Source->SetRelativeRotation(FRotator(0,0,40));
  if(GravityControl)GravityControl->SetPropertyValue_InContainer(Instance,1.);Rigid->OverrideWorldGravity=FVector(0,0,-980);Rigid->ResetDynamics(ETeleportType::ResetPhysics);for(int32 I=0;I<180;++I)Tick(.5f);auto GravityPose=Target->GetComponentSpaceTransforms();
  if(GravityControl)GravityControl->SetPropertyValue_InContainer(Instance,0.);Rigid->OverrideWorldGravity=FVector::ZeroVector;Rigid->ResetDynamics(ETeleportType::ResetPhysics);for(int32 I=0;I<180;++I)Tick(.5f);auto ZeroPose=Target->GetComponentSpaceTransforms();
  for(int32 N:{12,13,14,15}){int32 B=Ref.FindBoneIndex(FName(*FString::Printf(TEXT("aux_%03d"),N)));GravityResponse=FMath::Max(GravityResponse,FVector::Distance(GravityPose[B].GetLocation(),ZeroPose[B].GetLocation()));}
  for(int32 N:{2,3}){int32 B=Ref.FindBoneIndex(FName(*FString::Printf(TEXT("aux_%03d"),N)));A1GravityAngle=FMath::Max(A1GravityAngle,FMath::RadiansToDegrees(GravityPose[B].GetRotation().AngularDistance(ZeroPose[B].GetRotation())));}
  for(int32 N:{14,15}){int32 B=Ref.FindBoneIndex(FName(*FString::Printf(TEXT("aux_%03d"),N)));GravityWorldDrop+=Target->GetComponentTransform().TransformVectorNoScale(ZeroPose[B].GetLocation()-GravityPose[B].GetLocation()).Z*.5;}
  if(GravityControl)GravityControl->SetPropertyValue_InContainer(Instance,1.);Rigid->OverrideWorldGravity=FVector(0,0,-980);
 }
 // Exercise the actual new slider -> blueprint -> tagged native node path.
 // Lean forward so gravity pushes the braids toward the torso, then compare
 // identical shapes/parameters with only the contact pairs enabled/disabled.
 double PenetrationOn=0,PenetrationOff=0,CollisionDifference=0;bool CollisionSwitch=false;
 auto* WidgetClass=LoadClass<UUserWidget>(nullptr,TEXT("/Game/LeMalin/WBP_LeMalinTuning.WBP_LeMalinTuning_C"));
 auto* Widget=WidgetClass?NewObject<UUserWidget>(World,WidgetClass,NAME_None,RF_Transient):nullptr;
 auto* PanelProperty=FindFProperty<FObjectPropertyBase>(Instance->GetClass(),TEXT("LeMalinPanel"));
 if(Rigid&&Widget&&Widget->Initialize()&&PanelProperty){
  auto* Slider=Cast<USlider>(Widget->GetWidgetFromName(TEXT("CollisionSlider")));
  auto* OnAsset=LoadObject<UPhysicsAsset>(nullptr,TEXT("/Game/LeMalin/PA_LeMalin_Braids"));
  PanelProperty->SetObjectPropertyValue_InContainer(Instance,Widget);Source->SetRelativeRotation(FRotator(0,0,60));
  auto Run=[&](float Value){Slider->SetValue(Value);for(int32 I=0;I<3;++I)Tick(.5f);Rigid->ResetDynamics(ETeleportType::ResetPhysics);for(int32 I=0;I<180;++I)Tick(.5f);return Target->GetComponentSpaceTransforms();};
  if(Slider&&OnAsset){
   auto On=Run(1);bool OnSelected=Rigid->GetPhysicsAsset()==OnAsset;
   auto Off=Run(0);bool OffSelected=Rigid->GetPhysicsAsset()&&Rigid->GetPhysicsAsset()->GetName()==TEXT("PA_LeMalin_Braids_NoCollision");CollisionSwitch=OnSelected&&OffSelected;
   const double DisplayScale=ScaleControl?ScaleControl->GetPropertyValue_InContainer(Instance):1;
   struct Capsule {FVector A,B;double R;};
   auto Penetration=[&](const TArray<FTransform>& Pose){
    TArray<TArray<Capsule>> Geometry;Geometry.SetNum(OnAsset->SkeletalBodySetups.Num());
    for(int32 I=0;I<Geometry.Num();++I){auto* Body=OnAsset->SkeletalBodySetups[I].Get();int32 B=Ref.FindBoneIndex(Body->BoneName);if(B<0)continue;
     for(const auto& S:Body->AggGeom.SphylElems){FVector C=Pose[B].TransformPosition(S.Center)/DisplayScale;FVector D=Pose[B].TransformVectorNoScale(S.Rotation.RotateVector(FVector::UpVector))*S.Length*.5;Geometry[I].Add({C-D,C+D,S.Radius});}
     for(const auto& S:Body->AggGeom.SphereElems){FVector C=Pose[B].TransformPosition(S.Center)/DisplayScale;Geometry[I].Add({C,C,S.Radius});}
    }
    double Worst=0;
    for(int32 I=0;I<Geometry.Num();++I)if(OnAsset->SkeletalBodySetups[I]->PhysicsType==PhysType_Simulated)for(int32 J=0;J<Geometry.Num();++J)if(OnAsset->SkeletalBodySetups[J]->PhysicsType==PhysType_Kinematic&&!OnAsset->CollisionDisableTable.Contains(FRigidBodyIndexPair(I,J))){
     for(const auto& A:Geometry[I])for(const auto& B:Geometry[J]){FVector P,Q;FMath::SegmentDistToSegmentSafe(A.A,A.B,B.A,B.B,P,Q);Worst=FMath::Max(Worst,A.R+B.R-FVector::Distance(P,Q));}
    }return Worst;
   };
   PenetrationOn=Penetration(On);PenetrationOff=Penetration(Off);
   for(int32 N=2;N<14;++N){int32 B=Ref.FindBoneIndex(FName(*FString::Printf(TEXT("aux_%03d"),N)));CollisionDifference=FMath::Max(CollisionDifference,FVector::Distance(On[B].GetLocation(),Off[B].GetLocation())/DisplayScale);}
   Slider->SetValue(1);for(int32 I=0;I<3;++I)Tick(.5f);
  }
 }
 auto Skeleton=MakeShared<FJsonObject>();Skeleton->SetArrayField(TEXT("bones"),Names);Skeleton->SetArrayField(TEXT("parents"),Parents);Skeleton->SetArrayField(TEXT("reference_global"),Pose(Global));Skeleton->SetArrayField(TEXT("frames"),Frames);
 auto Result=MakeShared<FJsonObject>();Result->SetStringField(TEXT("name"),TEXT("LeMalin_Lively_PostProcess"));Result->SetNumberField(TEXT("fps"),30);Result->SetNumberField(TEXT("duration"),8);Result->SetObjectField(TEXT("target"),Skeleton);Result->SetArrayField(TEXT("blink"),Blinks);
 Result->SetNumberField(TEXT("blink_error"),BlinkError);Result->SetNumberField(TEXT("main_bone_error_cm"),MaxMainError);Result->SetNumberField(TEXT("ear_tip_offset_cm"),MaxEar);Result->SetNumberField(TEXT("braid_tip_offset_cm"),MaxHair);Result->SetNumberField(TEXT("max_tip_frame_step_cm"),MaxStep);Result->SetNumberField(TEXT("max_bone_scale_error"),MaxScaleError);Result->SetNumberField(TEXT("editor_tick_mean_ms"),TotalMS/Count);Result->SetBoolField(TEXT("finite"),Finite);Result->SetStringField(TEXT("limitation"),TEXT("Editor postprocess instance with motion and actor acceleration; actual game visual/performance validation pending."));
 Result->SetNumberField(TEXT("braid_length_error_cm"),MaxBraidLengthError);Result->SetNumberField(TEXT("tilted_pose_gravity_response_cm"),GravityResponse);Result->SetNumberField(TEXT("gravity_tip_world_drop_cm"),GravityWorldDrop);Result->SetBoolField(TEXT("native_rigid_body_found"),Rigid!=nullptr);
 Result->SetNumberField(TEXT("a1_gravity_angle_degrees"),A1GravityAngle);Result->SetNumberField(TEXT("default_visual_scale"),DefaultScale);Result->SetNumberField(TEXT("default_scale_error_cm"),DefaultScaleError);Result->SetNumberField(TEXT("turn_angular_velocity_limit"),Rigid?Rigid->SimSpaceSettings.MaxAngularVelocity:-1);Result->SetNumberField(TEXT("turn_angular_acceleration_limit"),Rigid?Rigid->SimSpaceSettings.MaxAngularAcceleration:-1);
 Result->SetBoolField(TEXT("collision_slider_switch"),CollisionSwitch);Result->SetNumberField(TEXT("body_penetration_on_cm"),PenetrationOn);Result->SetNumberField(TEXT("body_penetration_off_cm"),PenetrationOff);Result->SetNumberField(TEXT("collision_pose_difference_cm"),CollisionDifference);
 bool OK=Finite&&BlinkError<.001&&MaxMainError<.05&&MaxScaleError<.001&&MaxEar>.05&&MaxHair>.05&&MaxEar<25&&MaxHair<100&&MaxStep<35&&MaxBraidLengthError<.1&&Rigid&&GravityResponse>1&&GravityWorldDrop>1&&A1GravityAngle>1&&DefaultScaleError<.02&&FMath::IsNearlyEqual(DefaultScale,1.25,.0001)&&Rigid->SimSpaceSettings.MaxAngularVelocity==0&&Rigid->SimSpaceSettings.MaxAngularAcceleration==0&&CollisionSwitch&&PenetrationOff>1&&PenetrationOn+.1<PenetrationOff&&CollisionDifference>.1;Result->SetBoolField(TEXT("passed"),OK);
 Result->SetBoolField(TEXT("separate_display_actor_owner"),true);FString Json;FJsonSerializer::Serialize(Result,TJsonWriterFactory<TCHAR,TCondensedJsonPrintPolicy<TCHAR>>::Create(&Json));bool Written=FFileHelper::SaveStringToFile(Json,*OutputFile,FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM);PartActor->Destroy();Actor->Destroy();return Written&&OK?TEXT("OK"):FString::Printf(TEXT("verification failed: blink %.4f main %.4f ear %.3f hair %.3f step %.3f length %.3f gravity %.3f finite %d"),BlinkError,MaxMainError,MaxEar,MaxHair,MaxStep,MaxBraidLengthError,GravityResponse,Finite);
}

FString UHcxRetargetTools::VerifyLeMalinTuningPanel(USkeletalMesh* SourceMesh,USkeletalMesh* TargetMesh,UAnimSequence* Sequence,UAnimBlueprint* Blueprint,const FString& OutputFile)
{
 if(!GEditor||!SourceMesh||!TargetMesh||!Sequence||!Blueprint)return TEXT("panel verification assets missing");
 auto* Class=LoadClass<UUserWidget>(nullptr,TEXT("/Game/LeMalin/WBP_LeMalinTuning.WBP_LeMalinTuning_C"));if(!Class)return TEXT("widget class missing");
 auto* World=GEditor->GetEditorWorldContext().World();
 auto MakeWidget=[&](){auto* W=NewObject<UUserWidget>(World,Class,NAME_None,RF_Transient);return W->Initialize()?W:nullptr;};
 auto* Widget=MakeWidget();if(!Widget)return TEXT("widget initialize failed");
 const FString TestSlot=TEXT("LeMalinPanelVerification_")+FGuid::NewGuid().ToString(EGuidFormats::Digits);
 auto SetSlot=[&](UUserWidget* W){auto* P=FindFProperty<FStrProperty>(W->GetClass(),TEXT("SettingsSlot"));if(P)P->SetPropertyValue_InContainer(W,TestSlot);return P!=nullptr;};
 if(!SetSlot(Widget))return TEXT("settings slot missing");
 Widget->ProcessEvent(Widget->FindFunctionChecked(TEXT("Construct")),nullptr);
 FActorSpawnParameters Params;Params.ObjectFlags=RF_Transient;auto* Actor=World->SpawnActor<ACharacter>(Params);auto* Part=World->SpawnActor<AActor>(Params);if(!Actor||!Part)return TEXT("panel test actor missing");Actor->SetActorEnableCollision(false);
 auto* Source=Actor->GetMesh();Source->SetSkeletalMesh(SourceMesh);Source->SetDisablePostProcessBlueprint(true);Source->SetAnimationMode(EAnimationMode::AnimationSingleNode);Source->SetAnimation(Sequence);Source->GetSingleNodeInstance()->SetPlaying(false);
 auto* Target=NewObject<USkeletalMeshComponent>(Part,NAME_None,RF_Transient);Part->AddInstanceComponent(Target);Part->SetRootComponent(Target);Target->SetupAttachment(Source);Target->RegisterComponent();Target->SetSkeletalMesh(TargetMesh);Target->SetAnimationMode(EAnimationMode::AnimationSingleNode);Target->SetDisablePostProcessBlueprint(false);Target->SetOverridePostProcessAnimBP(Blueprint->GeneratedClass.Get());Target->InitAnim(true);
 Source->VisibilityBasedAnimTickOption=Target->VisibilityBasedAnimTickOption=EVisibilityBasedAnimTickOption::AlwaysTickPoseAndRefreshBones;
 auto Tick=[&](){Source->GetSingleNodeInstance()->SetPosition(.5f,false);Source->TickAnimation(0,false);Source->RefreshBoneTransforms();Target->TickAnimation(1.f/60,false);Target->RefreshBoneTransforms();};
 for(int32 I=0;I<20;++I)Tick();auto Baseline=Target->GetComponentSpaceTransforms();auto SourceBefore=Source->GetComponentSpaceTransforms();auto* Instance=Target->GetPostProcessInstance();
 auto* PanelProperty=Instance?FindFProperty<FObjectPropertyBase>(Instance->GetClass(),TEXT("LeMalinPanel")):nullptr;
 if(!PanelProperty){Part->Destroy();Actor->Destroy();return TEXT("runtime panel reference missing");}PanelProperty->SetObjectPropertyValue_InContainer(Instance,Widget);
 const TCHAR* Names[]={TEXT("Scale"),TEXT("Inertia"),TEXT("Collision"),TEXT("Damping"),TEXT("Gravity")};const double Defaults[]={125,55,1,2,100},Requested[]={130,25,1,3,120};bool Sliders=true;
 auto Slider=[&](UUserWidget* W,int32 I){return Cast<USlider>(W->GetWidgetFromName(FName(*(FString(Names[I])+TEXT("Slider")))));};
 auto Change=[&](int32 I,double Value){auto* S=Slider(Widget,I);if(!S){Sliders=false;return;}S->SetValue(Value);S->OnValueChanged.Broadcast(Value);};
 for(int32 I=0;I<5;++I)Change(I,Requested[I]);for(int32 I=0;I<8;++I)Tick();
 auto Read=[&](const TCHAR* Name){auto* P=FindFProperty<FDoubleProperty>(Instance->GetClass(),Name);return P?P->GetPropertyValue_InContainer(Instance):-999.;};
 bool Live=FMath::IsNearlyEqual(Read(TEXT("LeMalinScale")),1.3,.0001)&&FMath::IsNearlyEqual(Read(TEXT("LeMalinInertia")),.25,.0001)&&FMath::IsNearlyEqual(Read(TEXT("LeMalinCollision")),1.,.0001)&&FMath::IsNearlyEqual(Read(TEXT("LeMalinDamping")),3.,.0001)&&FMath::IsNearlyEqual(Read(TEXT("LeMalinGravity")),1.2,.0001);
 FAnimNode_RigidBody* Rigid=nullptr;for(TFieldIterator<FStructProperty> It(Instance->GetClass());It;++It)if(It->Struct==FAnimNode_RigidBody::StaticStruct()){Rigid=It->ContainerPtrToValuePtr<FAnimNode_RigidBody>(Instance);break;}
 bool Physics=Rigid&&FMath::IsNearlyEqual(Rigid->SimSpaceSettings.WorldAlpha,.25f,.0001f)&&FMath::IsNearlyEqual(Rigid->SimSpaceSettings.MaxAngularVelocity,0.f,.0001f)&&FMath::IsNearlyEqual(Rigid->SimSpaceSettings.MaxAngularAcceleration,0.f,.0001f)&&FMath::IsNearlyEqual(Rigid->OverrideWorldGravity.Z,-1176.,.01)&&Rigid->GetPhysicsAsset()&&Rigid->GetPhysicsAsset()->GetName()==TEXT("PA_LeMalin_Braids_Firm");
 TArray<TSharedPtr<FJsonValue>> DampingAssets;
 for(int32 Level:{1,2,3,4}){Change(3,Level);for(int32 I=0;I<4;++I)Tick();auto* PA=Rigid?Rigid->GetPhysicsAsset():nullptr;DampingAssets.Add(MakeShared<FJsonValueString>(PA?PA->GetName():TEXT("missing")));const TCHAR* Expected[]={TEXT("PA_LeMalin_Braids_Light"),TEXT("PA_LeMalin_Braids"),TEXT("PA_LeMalin_Braids_Firm"),TEXT("PA_LeMalin_Braids_Heavy")};Physics&=PA&&PA->GetName()==Expected[Level-1];}
 Change(3,Requested[3]);
 // Back-mount fixture uses real skeletal sockets with deliberately rotated
 // frames. It exercises the shipped blueprint on scene-component roots.
 double BackError=0,BackRotationError=0;bool BackPersistence=false;
 auto* BackSlider=Cast<USlider>(Widget->GetWidgetFromName(TEXT("BackDistanceSlider")));
 TArray<USkeletalMeshSocket*> FixtureSockets;
 auto AddSocket=[&](const TCHAR* Name,const TCHAR* Bone,FVector Location,FRotator Rotation){auto* S=NewObject<USkeletalMeshSocket>(SourceMesh,NAME_None,RF_Transient);S->SocketName=Name;S->BoneName=Bone;S->RelativeLocation=Location;S->RelativeRotation=Rotation;SourceMesh->GetMeshOnlySocketList().Add(S);FixtureSockets.Add(S);return S->SocketName;};
 const FName BackSocket=AddSocket(TEXT("Pt_LongBlade_Back_LeMalinFixture"),TEXT("spine_02"),FVector(2,-3,8),FRotator(11,27,-19));
 const FName SheathSocket=AddSocket(TEXT("Pt_LongBladeScabbard_Back_LeMalinFixture"),TEXT("spine_02"),FVector(-2,5,4),FRotator(-10,-42,25));
 const FName HandSocket=AddSocket(TEXT("LeMalin_HandFixture"),TEXT("hand_r"),FVector::ZeroVector,FRotator(0,15,0));
 const FName OtherSocket=AddSocket(TEXT("Pt_Accessory_Back_LeMalinFixture"),TEXT("spine_02"),FVector::ZeroVector,FRotator::ZeroRotator);SourceMesh->RebuildSocketMap();
 TArray<USceneComponent*> FixtureComponents;
 auto Component=[&](USceneComponent* Parent,FName Socket,FVector Local){auto* C=NewObject<USceneComponent>(Actor,NAME_None,RF_Transient);Actor->AddInstanceComponent(C);C->SetupAttachment(Parent,Socket);C->RegisterComponent();C->SetRelativeLocation(Local);FixtureComponents.Add(C);return C;};
 FVector BladeBase(1.5,-2.25,3.5),SheathBase(-1,3,2),OtherBase(3,4,5);FRotator BladeRotation(5,10,15);FVector BladeScale(1.1);
 auto* Blade=Component(Source,BackSocket,BladeBase);Blade->SetRelativeRotation(BladeRotation);Blade->SetRelativeScale3D(BladeScale);auto* Sheath=Component(Source,SheathSocket,SheathBase);auto* Child=Component(Blade,NAME_None,FVector(0,0,7));auto* Other=Component(Source,OtherSocket,OtherBase);
 auto BackTick=[&](float Value,int32 Frames=4){BackSlider->SetValue(Value);for(int32 I=0;I<Frames;++I)Tick();};
 auto CheckBack=[&](double Distance){FVector Rear=-Actor->GetActorForwardVector()*Distance;BackError=FMath::Max(BackError,FVector::Distance(Blade->GetComponentLocation(),Source->GetSocketTransform(BackSocket).TransformPosition(BladeBase)+Rear));BackError=FMath::Max(BackError,FVector::Distance(Sheath->GetComponentLocation(),Source->GetSocketTransform(SheathSocket).TransformPosition(SheathBase)+Rear));FTransform Original(BladeRotation,BladeBase,BladeScale);Original*=Source->GetSocketTransform(BackSocket);BackError=FMath::Max(BackError,FVector::Distance(Child->GetComponentLocation(),Original.TransformPosition(FVector(0,0,7))+Rear));BackError=FMath::Max(BackError,FVector::Distance(Other->GetRelativeLocation(),OtherBase));BackRotationError=FMath::Max(BackRotationError,Blade->GetRelativeRotation().Quaternion().AngularDistance(BladeRotation.Quaternion()));BackError=FMath::Max(BackError,FVector::Distance(Blade->GetRelativeScale3D(),BladeScale));};
 if(BackSlider){
  for(double Value:{0.,6.,20.,6.,0.,9.}){BackTick(Value);CheckBack(Value);}
  BackTick(9,120);CheckBack(9);
  for(double Yaw:{45.,90.,-135.}){Actor->SetActorRotation(FRotator(0,Yaw,0));BackTick(9);CheckBack(9);}
  BladeBase=FVector(3,-1,4);Blade->SetRelativeLocation(BladeBase);BackTick(6);CheckBack(6);
  const FVector HandBase(.2,-.5,1.);Blade->AttachToComponent(Source,FAttachmentTransformRules::SnapToTargetNotIncludingScale,HandSocket);Blade->SetRelativeLocation(HandBase);BackTick(6);BackError=FMath::Max(BackError,FVector::Distance(Blade->GetRelativeLocation(),HandBase));BackTick(20);BackError=FMath::Max(BackError,FVector::Distance(Blade->GetRelativeLocation(),HandBase));
  Blade->AttachToComponent(Source,FAttachmentTransformRules::SnapToTargetNotIncludingScale,BackSocket);Blade->SetRelativeLocation(BladeBase);Blade->SetRelativeRotation(BladeRotation);BackTick(6);CheckBack(6);
  Blade->AttachToComponent(Source,FAttachmentTransformRules::KeepRelativeTransform,HandSocket);BackTick(6);BackError=FMath::Max(BackError,FVector::Distance(Blade->GetRelativeLocation(),BladeBase));
  BackSlider->SetValue(11);
 }else BackError=999;
 for(auto* C:FixtureComponents)C->DestroyComponent();for(auto* S:FixtureSockets)SourceMesh->GetMeshOnlySocketList().Remove(S);SourceMesh->RebuildSocketMap();Actor->SetActorRotation(FRotator::ZeroRotator);
 double ScaleError=0,SourceError=0;
 const auto& Ref=TargetMesh->GetRefSkeleton();
 for(double Percent:{125.,130.,140.,150.}){Change(0,Percent);for(int32 I=0;I<3;++I)Tick();const auto& Pose=Target->GetComponentSpaceTransforms();for(const TCHAR* Name:{TEXT("pelvis"),TEXT("head"),TEXT("hand_l"),TEXT("hand_r"),TEXT("foot_l"),TEXT("foot_r")}){int32 B=Ref.FindBoneIndex(Name);ScaleError=FMath::Max(ScaleError,FVector::Distance(Pose[B].GetLocation(),Baseline[B].GetLocation()*Percent/125.));}}
 const auto& SourceAfter=Source->GetComponentSpaceTransforms();for(int32 I=0;I<SourceBefore.Num();++I)SourceError=FMath::Max(SourceError,FVector::Distance(SourceBefore[I].GetLocation(),SourceAfter[I].GetLocation()));
 for(int32 I=0;I<5;++I)Change(I,Requested[I]);
 auto* SaveButton=Cast<UButton>(Widget->GetWidgetFromName(TEXT("SaveButton")));if(SaveButton)SaveButton->OnClicked.Broadcast();
 bool Saved=UGameplayStatics::DoesSaveGameExist(TestSlot,0),Loaded=false;auto* Reopened=MakeWidget();
 if(Reopened&&SetSlot(Reopened)){Reopened->ProcessEvent(Reopened->FindFunctionChecked(TEXT("Construct")),nullptr);Loaded=true;for(int32 I=0;I<5;++I){auto* S=Slider(Reopened,I);Loaded&=S&&FMath::Abs(S->GetValue()-Requested[I])<.0001;}auto* B=Cast<USlider>(Reopened->GetWidgetFromName(TEXT("BackDistanceSlider")));BackPersistence=B&&FMath::Abs(B->GetValue()-11)<.0001;}
 auto* ResetButton=Cast<UButton>(Widget->GetWidgetFromName(TEXT("ResetButton")));if(ResetButton)ResetButton->OnClicked.Broadcast();bool Reset=ResetButton!=nullptr;for(int32 I=0;I<5;++I){auto* S=Slider(Widget,I);Reset&=S&&FMath::Abs(S->GetValue()-Defaults[I])<.0001;}
 auto* Border=Cast<UBorder>(Widget->GetWidgetFromName(TEXT("ControlsPanel")));auto* CloseButton=Cast<UButton>(Widget->GetWidgetFromName(TEXT("CloseButton")));if(Border)Border->SetVisibility(ESlateVisibility::Visible);if(CloseButton)CloseButton->OnClicked.Broadcast();bool Closed=Border&&CloseButton&&Border->GetVisibility()==ESlateVisibility::Collapsed;
 // First-time layout preview is optional: ordinary -nullrhi generation never
 // spends time rendering. A rendering-enabled call emits this exact UMG tree.
 bool Preview=true;
 if(FSlateApplication::IsInitialized()&&!FParse::Param(FCommandLine::Get(),TEXT("nullrhi"))){
  Border->SetVisibility(ESlateVisibility::Visible);const auto SlateWidget=Widget->TakeWidget();ResetButton->OnClicked.Broadcast();FWidgetRenderer Renderer(true);auto* RT=Renderer.DrawWidget(SlateWidget,FVector2D(560,820));FlushRenderingCommands();TArray<FColor> Pixels;TArray64<uint8> Png;Preview=RT&&RT->GameThread_GetRenderTargetResource()->ReadPixels(Pixels);if(Preview){FImageUtils::PNGCompressImageArray(560,820,Pixels,Png);Preview=FFileHelper::SaveArrayToFile(Png,*FPaths::ChangeExtension(OutputFile,TEXT("png")));}
  const auto Bounds=Border->GetCachedGeometry();const auto Origin=Bounds.GetAbsolutePosition(),End=Origin+Bounds.GetAbsoluteSize();
  for(const TCHAR* N:{TEXT("ResetButton"),TEXT("SaveButton"),TEXT("CloseButton")}){auto B=Widget->GetWidgetFromName(N)->GetCachedGeometry();auto P=B.GetAbsolutePosition(),Q=P+B.GetAbsoluteSize();Preview&=P.X>=Origin.X&&P.Y>=Origin.Y&&Q.X<=End.X+1&&Q.Y<=End.Y+1;}
 }
 // This slot is unique to this verification and never names a user setting.
 if(Saved)UGameplayStatics::DeleteGameInSlot(TestSlot,0);
 bool OK=Sliders&&Live&&Physics&&ScaleError<.02&&SourceError<.001&&Saved&&Loaded&&Reset&&Closed&&Preview&&BackError<.02&&BackRotationError<.0001&&BackPersistence;
 auto Report=MakeShared<FJsonObject>();Report->SetNumberField(TEXT("back_mount_error_cm"),BackError);Report->SetNumberField(TEXT("back_rotation_error_radians"),BackRotationError);Report->SetBoolField(TEXT("back_distance_save_load"),BackPersistence);Report->SetBoolField(TEXT("passed"),OK);Report->SetBoolField(TEXT("slider_to_runtime_variables"),Live);Report->SetBoolField(TEXT("native_physics_inputs_and_damping_switch"),Physics);Report->SetArrayField(TEXT("damping_assets"),DampingAssets);Report->SetNumberField(TEXT("visual_scale_error_cm"),ScaleError);Report->SetNumberField(TEXT("source_pose_error_cm"),SourceError);Report->SetBoolField(TEXT("save_load_round_trip"),Saved&&Loaded);Report->SetBoolField(TEXT("reset_recommended_values"),Reset);Report->SetBoolField(TEXT("close_button"),Closed);Report->SetStringField(TEXT("limitation"),TEXT("Initialized UMG events and actual PostProcess runtime inputs tested headlessly. In-game F8, cursor, font/layout and weapon alignment at changed scale await user test."));FString Json;FJsonSerializer::Serialize(Report,TJsonWriterFactory<>::Create(&Json));FFileHelper::SaveStringToFile(Json,*OutputFile,FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM);Part->Destroy();Actor->Destroy();return OK?TEXT("OK"):FString::Printf(TEXT("panel verify failed: sliders %d live %d physics %d scale %.4f source %.4f save %d load %d reset %d close %d"),Sliders,Live,Physics,ScaleError,SourceError,Saved,Loaded,Reset,Closed);
}
