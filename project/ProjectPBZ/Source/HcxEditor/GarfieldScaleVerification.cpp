#include "HcxRetargetTools.h"
#include "Animation/AnimBlueprint.h"
#include "Animation/AnimInstance.h"
#include "Animation/AnimSingleNodeInstance.h"
#include "Animation/AnimSequence.h"
#include "Components/SkeletalMeshComponent.h"
#include "Editor.h"
#include "Engine/SkeletalMesh.h"
#include "Engine/World.h"
#include "GameFramework/Character.h"
#include "UObject/UnrealType.h"

FString UHcxRetargetTools::VerifyGarfieldScale(USkeletalMesh* SourceMesh, USkeletalMesh* TargetMesh,
                                             UAnimSequence* Sequence, UAnimBlueprint* Blueprint)
{
    if (!GEditor || !SourceMesh || !TargetMesh || !Sequence || !Blueprint)
        return TEXT("missing scale verification assets");
    UWorld* World = GEditor->GetEditorWorldContext().World();
    FActorSpawnParameters Parameters;
    Parameters.ObjectFlags = RF_Transient;
    auto* Actor = World->SpawnActor<ACharacter>(Parameters);
    if (!Actor) return TEXT("verification actor creation failed");
    Actor->SetActorEnableCollision(false);
    auto* Source = Actor->GetMesh();
    Source->SetSkeletalMesh(SourceMesh);
    Source->SetDisablePostProcessBlueprint(true);
    Source->SetAnimationMode(EAnimationMode::AnimationSingleNode);
    Source->SetAnimation(Sequence);
    Source->GetSingleNodeInstance()->SetPlaying(false);
    auto* Target = NewObject<USkeletalMeshComponent>(Actor, NAME_None, RF_Transient);
    Actor->AddInstanceComponent(Target);
    Target->SetupAttachment(Source);
    Target->RegisterComponent();
    Target->SetSkeletalMesh(TargetMesh);
    Target->SetDisablePostProcessBlueprint(true);
    Target->SetAnimInstanceClass(Blueprint->GeneratedClass);
    Target->InitAnim(true);
    Source->VisibilityBasedAnimTickOption = EVisibilityBasedAnimTickOption::AlwaysTickPoseAndRefreshBones;
    Target->VisibilityBasedAnimTickOption = EVisibilityBasedAnimTickOption::AlwaysTickPoseAndRefreshBones;
    auto* Instance = Target->GetAnimInstance();
    auto* ScaleProperty = Instance ? FindFProperty<FDoubleProperty>(Instance->GetClass(), TEXT("GarfieldScale")) : nullptr;
    if (!ScaleProperty)
    {
        Actor->Destroy();
        return TEXT("GarfieldScale property missing");
    }
    auto Evaluate = [&](double Scale)
    {
        ScaleProperty->SetPropertyValue_InContainer(Instance, Scale);
        if (auto* Clock = FindFProperty<FDoubleProperty>(Instance->GetClass(), TEXT("LifeClock")))
            Clock->SetPropertyValue_InContainer(Instance, 0);
        Source->GetSingleNodeInstance()->SetPosition(.5f, false);
        Source->TickAnimation(0, false);
        Source->RefreshBoneTransforms();
        Target->TickAnimation(1.f / 60, false);
        Target->RefreshBoneTransforms();
        return Target->GetComponentSpaceTransforms();
    };
    Evaluate(1);
    const auto Baseline = Evaluate(1);
    double MaxError = 0;
    double Span = 0;
    for (const auto& Bone : Baseline) Span = FMath::Max(Span, Bone.GetLocation().Length());
    if (Span < 50)
    {
        Actor->Destroy();
        return TEXT("verification pose did not evaluate");
    }
    bool Valid = Baseline.Num() == TargetMesh->GetRefSkeleton().GetNum();
    for (double Ratio : {.6, .85, 1., 1.1})
    {
        const auto Scaled = Evaluate(Ratio);
        Valid &= Scaled.Num() == Baseline.Num();
        if (!Valid) break;
        const FVector Pivot = Baseline[0].GetLocation();
        for (int32 Index = 0; Index < Scaled.Num(); ++Index)
        {
            const FVector Expected = Pivot + Ratio * (Baseline[Index].GetLocation() - Pivot);
            MaxError = FMath::Max(MaxError, FVector::Distance(Expected, Scaled[Index].GetLocation()));
            Valid &= !Scaled[Index].ContainsNaN();
            Valid &= Scaled[Index].GetScale3D().Equals(FVector(Ratio), .001);
        }
    }
    Actor->Destroy();
    return Valid && MaxError < .02 ? FString::Printf(TEXT("OK max_position_error_cm=%.8f span_cm=%.4f"), MaxError, Span)
                                  : FString::Printf(TEXT("scale verification failed valid=%d max_error_cm=%.8f"), Valid, MaxError);
}

#include "Animation/MorphTarget.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonSerializer.h"
#include "Misc/FileHelper.h"

FString UHcxRetargetTools::VerifyGarfieldLiveliness(USkeletalMesh* SourceMesh, USkeletalMesh* TargetMesh,
    UAnimSequence* Sequence, UAnimBlueprint* Blueprint, const FString& OutputFile)
{
    if (!GEditor || !SourceMesh || !TargetMesh || !Sequence || !Blueprint) return TEXT("missing life test assets");
    for (int32 Stage = 1; Stage <= 4; ++Stage)
    {
        auto* Morph = TargetMesh->FindMorphTarget(FName(*FString::Printf(TEXT("Blink%d"), Stage * 25)));
        if (!Morph || Morph->GetMorphLODModels().IsEmpty() || Morph->GetMorphLODModels()[0].Vertices.IsEmpty())
            return TEXT("blink morph has no deltas");
    }
    UWorld* World = GEditor->GetEditorWorldContext().World();
    FActorSpawnParameters Parameters; Parameters.ObjectFlags = RF_Transient;
    auto* Actor = World->SpawnActor<ACharacter>(Parameters);
    if (!Actor) return TEXT("life test actor creation failed");
    Actor->SetActorEnableCollision(false);
    auto* Source = Actor->GetMesh();
    Source->SetSkeletalMesh(SourceMesh);
    Source->SetDisablePostProcessBlueprint(true);
    Source->SetAnimationMode(EAnimationMode::AnimationSingleNode);
    Source->SetAnimation(Sequence);
    Source->GetSingleNodeInstance()->SetPlaying(false);
    auto* Target = NewObject<USkeletalMeshComponent>(Actor, NAME_None, RF_Transient);
    Actor->AddInstanceComponent(Target);
    Target->SetupAttachment(Source);
    Target->RegisterComponent();
    Target->SetSkeletalMesh(TargetMesh);
    Target->SetDisablePostProcessBlueprint(false);
    Target->SetAnimationMode(EAnimationMode::AnimationSingleNode);
    Target->SetOverridePostProcessAnimBP(Blueprint->GeneratedClass.Get());
    Target->InitAnim(true);
    Source->VisibilityBasedAnimTickOption = EVisibilityBasedAnimTickOption::AlwaysTickPoseAndRefreshBones;
    Target->VisibilityBasedAnimTickOption = EVisibilityBasedAnimTickOption::AlwaysTickPoseAndRefreshBones;
    auto* Instance = Target->GetPostProcessInstance();
    if (!Instance) { Actor->Destroy(); return TEXT("post-process instance missing"); }
    auto* BlinkClock = FindFProperty<FDoubleProperty>(Instance->GetClass(), TEXT("BlinkClock"));
    auto* LifeClock = FindFProperty<FDoubleProperty>(Instance->GetClass(), TEXT("LifeClock"));
    if (!BlinkClock || !LifeClock) { Actor->Destroy(); return TEXT("life clock properties missing"); }
    auto Tick = [&](float Delta)
    {
        Source->GetSingleNodeInstance()->SetPosition(.5f, false);
        Source->TickAnimation(0, false);
        Source->RefreshBoneTransforms();
        Target->TickAnimation(Delta, false);
        Target->RefreshBoneTransforms();
    };
    Tick(1.f / 60);
    double MaxError = 0;
    for (double Time : {-.5, 0., .025, .05, .075, .1, .15, .23, .32})
    {
        const double Delta = 1. / 60;
        BlinkClock->SetPropertyValue_InContainer(Instance, Time - Delta);
        Tick(Delta);
        const double Clock = BlinkClock->GetPropertyValue_InContainer(Instance);
        if (Time == .32 && (Clock > -3 || Clock < -6))
        { Actor->Destroy(); return TEXT("blink wait outside expected range"); }
        const double Progress = FMath::Clamp(FMath::Min(Clock / .1, (.32 - Clock) / .18), 0., 1.);
        MaxError = FMath::Max(MaxError, FMath::Abs(Target->GetMorphTarget(TEXT("EyesOpen")) - FMath::Clamp(1 - Progress * 4, 0., 1.)));
        for (int32 Stage = 1; Stage <= 4; ++Stage)
        {
            const double Expected = FMath::Clamp(1 - FMath::Abs(Progress * 4 - Stage), 0., 1.);
            const double Weight = Target->GetMorphTarget(FName(*FString::Printf(TEXT("Blink%d"), Stage * 25)));
            MaxError = FMath::Max(MaxError, FMath::Abs(Expected - Weight));
        }
    }
    auto JsonPose = [](const TArray<FTransform>& Pose)
    {
        TArray<TSharedPtr<FJsonValue>> Values;
        for (const auto& Transform : Pose)
        {
            const auto P = Transform.GetLocation(), S = Transform.GetScale3D(); const auto Q = Transform.GetRotation();
            TArray<TSharedPtr<FJsonValue>> Value;
            for (double Number : {P.X, P.Y, P.Z, Q.X, Q.Y, Q.Z, Q.W, S.X, S.Y, S.Z}) Value.Add(MakeShared<FJsonValueNumber>(Number));
            Values.Add(MakeShared<FJsonValueArray>(Value));
        }
        return Values;
    };
    const auto& Reference = TargetMesh->GetRefSkeleton();
    auto Global = Reference.GetRefBonePose();
    TArray<TSharedPtr<FJsonValue>> Names, Parents;
    for (int32 Index = 0; Index < Global.Num(); ++Index)
    {
        Names.Add(MakeShared<FJsonValueString>(Reference.GetBoneName(Index).ToString()));
        const int32 Parent = Reference.GetParentIndex(Index);
        Parents.Add(MakeShared<FJsonValueNumber>(Parent));
        if (Parent >= 0) Global[Index] = Global[Index] * Global[Parent];
    }
    LifeClock->SetPropertyValue_InContainer(Instance, 0);
    BlinkClock->SetPropertyValue_InContainer(Instance, -.5);
    TArray<TSharedPtr<FJsonValue>> Frames, MorphFrames;
    TArray<FTransform> First, Last;
    bool Finite = true;
    for (int32 Frame = 0; Frame <= 120; ++Frame)
    {
        Tick(1.f / 30);
        const auto Pose = Target->GetComponentSpaceTransforms();
        for (const auto& Transform : Pose) Finite &= !Transform.ContainsNaN();
        if (Frame == 0) First = Pose;
        if (Frame == 120) Last = Pose;
        Frames.Add(MakeShared<FJsonValueArray>(JsonPose(Pose)));
        auto Morphs = MakeShared<FJsonObject>();
        for (int32 Stage = 1; Stage <= 4; ++Stage)
        {
            const FString Name = FString::Printf(TEXT("Blink%d"), Stage * 25);
            Morphs->SetNumberField(Name, Target->GetMorphTarget(FName(*Name)));
        }
        MorphFrames.Add(MakeShared<FJsonValueObject>(Morphs));
    }
    const int32 Tail = Reference.FindBoneIndex(TEXT("tail6")), Head = Reference.FindBoneIndex(TEXT("head"));
    const double TailMovement = Tail >= 0 ? FVector::Distance(First[Tail].GetLocation(), Last[Tail].GetLocation()) : 0;
    const double HeadMovement = Head >= 0 ? FVector::Distance(First[Head].GetLocation(), Last[Head].GetLocation()) : 1;
    auto Skeleton = MakeShared<FJsonObject>();
    Skeleton->SetArrayField(TEXT("bones"), Names); Skeleton->SetArrayField(TEXT("parents"), Parents);
    Skeleton->SetArrayField(TEXT("reference_global"), JsonPose(Global)); Skeleton->SetArrayField(TEXT("frames"), Frames);
    auto Report = MakeShared<FJsonObject>();
    Report->SetStringField(TEXT("name"), TEXT("Garfield_Life")); Report->SetNumberField(TEXT("duration"), 4);
    Report->SetNumberField(TEXT("fps"), 30); Report->SetObjectField(TEXT("target"), Skeleton);
    Report->SetArrayField(TEXT("morph_frames"), MorphFrames);
    Report->SetNumberField(TEXT("morph_weight_error"), MaxError);
    Report->SetNumberField(TEXT("tail_movement_cm"), TailMovement);
    Report->SetNumberField(TEXT("head_drift_cm"), HeadMovement);
    FString Json; FJsonSerializer::Serialize(Report, TJsonWriterFactory<TCHAR,TCondensedJsonPrintPolicy<TCHAR>>::Create(&Json));
    const bool Written = FFileHelper::SaveStringToFile(Json, *OutputFile, FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM);
    Actor->Destroy();
    return Finite && Written && MaxError < .001 && TailMovement > .01 && HeadMovement < .01
        ? FString::Printf(TEXT("OK morph_error=%.6f tail_cm=%.4f head_drift_cm=%.6f"), MaxError, TailMovement, HeadMovement)
        : FString::Printf(TEXT("life test failed finite=%d written=%d morph_error=%.6f tail_cm=%.4f head_drift_cm=%.6f"), Finite, Written, MaxError, TailMovement, HeadMovement);
}

#include "Blueprint/UserWidget.h"
#include "Blueprint/WidgetTree.h"
#include "WidgetBlueprint.h"
#include "Components/Border.h"
#include "Components/Button.h"
#include "Engine/TextureRenderTarget2D.h"
#include "Slate/WidgetRenderer.h"
#include "ImageUtils.h"
#include "RenderingThread.h"

FString UHcxRetargetTools::RenderGarfieldPanelPreview(const FString& OutputFile)
{
    auto* Blueprint = LoadObject<UWidgetBlueprint>(nullptr, TEXT("/Game/Garfield/WBP_GarfieldScale"));
    if (!Blueprint) return TEXT("panel preview class missing");
    UWorld* World = UWorld::CreateWorld(EWorldType::GamePreview, false);
    auto* Widget = CreateWidget<UUserWidget>(World, Blueprint->GeneratedClass.Get());
    if (!Widget) { World->DestroyWorld(false); return TEXT("panel preview creation failed"); }
    auto* Panel = Widget->GetWidgetFromName(TEXT("ControlsPanel"));
    Panel->SetVisibility(ESlateVisibility::Visible);
    const auto SlateWidget = Widget->TakeWidget();
    FWidgetRenderer Renderer(true);
    auto* Target = Renderer.DrawWidget(SlateWidget, FVector2D(560, 560));
    FlushRenderingCommands();
    TArray<FColor> Pixels;
    bool Success = Target && Target->GameThread_GetRenderTargetResource()->ReadPixels(Pixels);
    TArray64<uint8> Png;
    if (Success)
    {
        FImageUtils::PNGCompressImageArray(560, 560, Pixels, Png);
        Success = FFileHelper::SaveArrayToFile(Png, *OutputFile);
    }
    // Compare the arranged widget bounds, including both buttons, with the panel.
    const auto Geometry = Panel->GetCachedGeometry();
    const auto Origin = Geometry.GetAbsolutePosition(), Size = Geometry.GetAbsoluteSize();
    bool Contained = Size.X > 0 && Size.Y > 0;
    for (const TCHAR* Name : {TEXT("ResetButton"), TEXT("SaveButton")})
    {
        const auto Button = Widget->GetWidgetFromName(Name)->GetCachedGeometry();
        const auto Position = Button.GetAbsolutePosition(), End = Position + Button.GetAbsoluteSize();
        Contained &= Position.X >= Origin.X && Position.Y >= Origin.Y;
        Contained &= End.X <= Origin.X + Size.X + 1 && End.Y <= Origin.Y + Size.Y + 1;
    }
    Widget->RemoveFromParent();
    World->DestroyWorld(false);
    return Success && Contained ? TEXT("OK panel image rendered and buttons contained") : TEXT("panel render or containment failed");
}
