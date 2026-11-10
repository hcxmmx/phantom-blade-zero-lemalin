#pragma once
#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "HcxRetargetTools.generated.h"
class UAnimBlueprint; class UIKRetargeter; class USkeletalMesh; class UAnimSequence;
UCLASS() class HCXEDITOR_API UHcxRetargetTools : public UBlueprintFunctionLibrary {
 GENERATED_BODY()
public:
 UFUNCTION(BlueprintCallable,Category="Hcx") static FString BuildLeMalinLiveliness(UAnimBlueprint* Blueprint, USkeletalMesh* Mesh);
 UFUNCTION(BlueprintCallable,Category="Hcx") static FString VerifyLeMalinLiveliness(USkeletalMesh* Source, USkeletalMesh* Target, UAnimSequence* Sequence, UAnimBlueprint* Blueprint, const FString& OutputFile);
 UFUNCTION(BlueprintCallable,Category="Hcx") static FString MakePlayerBuild(UAnimBlueprint* Blueprint);
 UFUNCTION(BlueprintCallable,Category="Hcx") static FString BuildChestMotion(UAnimBlueprint* Blueprint);
 UFUNCTION(BlueprintCallable,Category="Hcx") static FString BuildSkirtAvoidance(UAnimBlueprint* Blueprint);
 UFUNCTION(BlueprintCallable,Category="Hcx") static FString RefineSkirtCloth(USkeletalMesh* Mesh);
 UFUNCTION(BlueprintCallable,Category="Hcx") static FString CreateSkirtCloth(USkeletalMesh* Mesh);
 UFUNCTION(BlueprintCallable,Category="Hcx") static FString EnableSkirtCloth(UAnimBlueprint* Blueprint);
 UFUNCTION(BlueprintCallable,Category="Hcx") static FString BuildSkirtGraph(UAnimBlueprint* Blueprint);
 UFUNCTION(BlueprintCallable,Category="Hcx") static bool BuildRetargetGraph(UAnimBlueprint* Blueprint, UIKRetargeter* Retargeter);
 UFUNCTION(BlueprintCallable,Category="Hcx") static void SyncReferencePose(USkeletalMesh* Mesh);
 UFUNCTION(BlueprintCallable,Category="Hcx") static FString CheckRetargetMotion(USkeletalMesh* Source, USkeletalMesh* Target, UIKRetargeter* Retargeter);
 UFUNCTION(BlueprintCallable,Category="Hcx") static bool BakeDiagnosticAnimation(USkeletalMesh* Source, USkeletalMesh* Target, UIKRetargeter* Retargeter, const FString& InputFile, const FString& AssetDirectory, const FString& OutputFile);
 UFUNCTION(BlueprintCallable,Category="Hcx") static AActor* SpawnPreviewActor(TSubclassOf<AActor> ActorClass, const FTransform& Transform);
 UFUNCTION(BlueprintCallable,Category="Hcx") static bool RestorePhysicsReference(USkeletalMesh* Mesh, const FString& AssetPackage);
 UFUNCTION(BlueprintCallable,Category="Hcx") static FString BuildGaitGraph(UAnimBlueprint* Blueprint, UIKRetargeter* Base, UIKRetargeter* Left, UIKRetargeter* Right);
 UFUNCTION(BlueprintCallable,Category="Hcx") static FString CheckGaitGate(UAnimBlueprint* Blueprint, USkeletalMesh* Source, USkeletalMesh* Target, UAnimSequence* Sequence);
 UFUNCTION(BlueprintCallable,Category="Hcx") static FString BuildShoeGraph(UAnimBlueprint* Blueprint, const FString& SettingsJson);
 UFUNCTION(BlueprintCallable,Category="Hcx") static FString BuildHingeGraph(UAnimBlueprint* Blueprint, const FString& SettingsJson);
 UFUNCTION(BlueprintCallable,Category="Hcx") static FString BuildHipGraph(UAnimBlueprint* Blueprint, const FString& SettingsJson);
 UFUNCTION(BlueprintCallable,Category="Hcx") static bool CaptureGaitCycle(USkeletalMesh* Source, USkeletalMesh* Target, UAnimSequence* Sequence, UAnimBlueprint* Baseline, UAnimBlueprint* Current, const FString& OutputFile);
 UFUNCTION(BlueprintCallable,Category="Hcx") static FString BuildMovementLegGraph(UAnimBlueprint* Blueprint, UIKRetargeter* Baseline, UIKRetargeter* Direct);
 UFUNCTION(BlueprintCallable,Category="Hcx") static FString BuildWidthGraph(UAnimBlueprint* Blueprint, const FString& SettingsJson);
 UFUNCTION(BlueprintCallable,Category="Hcx") static FString CreateFootTuningPanel();
 UFUNCTION(BlueprintCallable,Category="Hcx") static FString AttachFootTuningPanel(UAnimBlueprint* Blueprint);
 UFUNCTION(BlueprintCallable,Category="Hcx") static FString BuildGripGraph(UAnimBlueprint* Blueprint, const FString& ReferenceJson);
 UFUNCTION(BlueprintCallable,Category="Hcx") static FString BuildWeaponFollowGraph(UAnimBlueprint* Blueprint, const FString& ReferenceJson);
 UFUNCTION(BlueprintCallable,Category="Hcx") static FString VerifyGarfieldScale(USkeletalMesh* Source, USkeletalMesh* Target, UAnimSequence* Sequence, UAnimBlueprint* Blueprint);
 UFUNCTION(BlueprintCallable,Category="Hcx") static FString BuildGarfieldLiveliness(UAnimBlueprint* Blueprint);
 UFUNCTION(BlueprintCallable,Category="Hcx") static FString PolishGarfieldScalePanel();
 UFUNCTION(BlueprintCallable,Category="Hcx") static FString RenderGarfieldPanelPreview(const FString& OutputFile);
 UFUNCTION(BlueprintCallable,Category="Hcx") static FString VerifyGarfieldLiveliness(USkeletalMesh* Source, USkeletalMesh* Target, UAnimSequence* Sequence, UAnimBlueprint* Blueprint, const FString& OutputFile);
 UFUNCTION(BlueprintCallable,Category="Hcx") static FString BuildGarfieldScalePanel(UAnimBlueprint* Blueprint);
 UFUNCTION(BlueprintCallable,Category="Hcx") static FString BuildLeMalinTuningPanel(UAnimBlueprint* Blueprint);
 UFUNCTION(BlueprintCallable,Category="Hcx") static FString ProbeLeMalinMaterialControls(USkeletalMesh* Source, USkeletalMesh* Target, UAnimSequence* Sequence, UAnimBlueprint* Blueprint, const FString& OutputFile);
 UFUNCTION(BlueprintCallable,Category="Hcx") static FString VerifyLeMalinTuningPanel(USkeletalMesh* Source, USkeletalMesh* Target, UAnimSequence* Sequence, UAnimBlueprint* Blueprint, const FString& OutputFile);
 UFUNCTION(BlueprintCallable,Category="Hcx") static FString ScaleGaitReferenceVectors(UAnimBlueprint* Blueprint, double Ratio);
};
